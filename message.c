//system includes
#include <string.h>
#include <glib/gi18n.h>
#include <errno.h>

//purple includes
#include <xmlnode.h>
#include <notify.h>
#include <debug.h>

//local includes
#include "message.h"

void campfire_message_send(CampfireMessage *cm)
{
	xmlnode *message, *child;

	message = xmlnode_new("message");

	if(cm->type)
		xmlnode_set_attrib(message, "type", cm->type);
	else
		xmlnode_set_attrib(message, "type", CAMPFIRE_MESSAGE_TEXT);
	
	if(cm->body) {
		child = xmlnode_new_child(message, "body");
		xmlnode_insert_data(child, cm->body, -1);
	}
	
	//jabber_send(message);

	xmlnode_free(message);
}

void campfire_ssl_failure(PurpleSslConnection *gsc, PurpleSslErrorType error, gpointer data)
{
	purple_debug_info("campfire", "ssl connect failure\n");
}

void do_nothing_cb(gpointer data, PurpleSslConnection *gsc, PurpleInputCondition cond)
{
}

void campfire_add_callback(gpointer data, PurpleSslConnection *gsc, PurpleInputCondition cond)
{
	PurpleSslInputFunction cb = (PurpleSslInputFunction)data;
	purple_ssl_input_add(gsc, cb, gsc);
}

void campfire_send_and_respond(PurpleSslConnection *gsc, CampfireSslTransaction *xaction)
{
	purple_debug_info("campfire", "http_request: %p, response_cb: %p\n",
	                  xaction->http_request, xaction->response_cb);
	/* only add input and write to ssl connection if
	 * you have a http_request string and a handler function
	 */
	if (xaction->http_request && xaction->response_cb)
	{
		/* remove older input handler, if any exists */
		if (gsc->inpa > 0) {
			purple_input_remove(gsc->inpa);
		}
		purple_ssl_input_add(gsc, xaction->response_cb, xaction->response_cb_data);
		purple_ssl_write(gsc, xaction->http_request->str, xaction->http_request->len);
		g_string_free(xaction->http_request, TRUE);

	}
	/* this must be 'free'd somewhere bu not here
	 * g_free(xaction);
	 */
}

void campfire_do_new_connection_xaction_cb(gpointer data, PurpleSslConnection *gsc, PurpleInputCondition cond)
{
	CampfireSslTransaction *xaction = (CampfireSslTransaction *)data;
	purple_debug_info("campfire", "new connection xaction\n");
	campfire_send_and_respond(gsc, xaction);
}

void campfire_renew_connection(CampfireSslTransaction *xaction)
{
	CampfireConn *conn = xaction->campfire;

	if(!conn->gsc) {
		purple_debug_info("campfire", "Renewing connnection\n");
		conn->gsc = purple_ssl_connect(conn->account,
		                               conn->hostname,
		                               443,
					       xaction->connect_cb,
		                               campfire_ssl_failure,
		                               xaction->connect_cb_data);

	} else {
		purple_debug_info("campfire", "connection is still open\n");
		campfire_send_and_respond(conn->gsc, xaction);
	}

}

void campfire_http_request(CampfireSslTransaction *xaction, gchar *uri, gchar *method)
{
	CampfireConn *conn = xaction->campfire;
	const char *api_token = purple_account_get_string(conn->account,
			"api_token", "");

	xaction->http_request = g_string_new(method);
	g_string_append(xaction->http_request, " ");
	g_string_append(xaction->http_request, uri);
	g_string_append(xaction->http_request, " HTTP/1.1\r\n");

	g_string_append(xaction->http_request, "Content-Type: application/xml\r\n");
	
	g_string_append(xaction->http_request, "Authorization: Basic ");
	gsize auth_len = strlen(api_token);
	gchar *encoded = purple_base64_encode((const guchar *)api_token, auth_len);
	g_string_append(xaction->http_request, encoded);
	g_string_append(xaction->http_request, "\r\n");	
	g_free(encoded);
	
	g_string_append(xaction->http_request, "Host: ");
	g_string_append(xaction->http_request, conn->hostname);
	g_string_append(xaction->http_request, "\r\n");

	g_string_append(xaction->http_request, "Accept: */*\r\n\r\n");

	campfire_renew_connection(xaction);
}

gint campfire_http_response(CampfireSslTransaction *xaction, PurpleInputCondition cond,
                            xmlnode **node)
{
	CampfireConn *conn = xaction->campfire;
	gchar buf[1024];
	gchar *blank_line = "\r\n\r\n";
	gchar *status_header = "\r\nStatus: ";
	gchar *xml_header = "<?xml";
	gchar *content, *rawxml, *node_str;
	xmlnode *tmpnode;
	gint len;
	static gint size_response = 0;

	if (size_response == 0) {
		if (xaction->http_response) {
			g_string_free(xaction->http_response, TRUE);
		}
		xaction->http_response = g_string_new("");
	}

	errno = 0;
	/* We need a while loop here if/when the response is larger
	 * than our 'static gchar buf'
	 * NOTE: jabber is using a while loop here and parsing chunks of
	 *       xml each loop with libxml call xmlParseChunk()
	 */
	while ((len = purple_ssl_read(conn->gsc, buf, sizeof(buf))) > 0) {
		purple_debug_info("campfire",
				  "read %d bytes from HTTP Response\n",
				  len);
		xaction->http_response = g_string_append_len(xaction->http_response, buf, len);
		size_response += len;
	}


	if (len < 0) {
		if (errno == EAGAIN) {
			if (size_response == 0) {
				purple_debug_info("campfire", "TRY AGAIN\n");
				return CAMPFIRE_HTTP_RESPONSE_STATUS_TRY_AGAIN;
			} else {
				/* DO NOT RETURN */
				purple_debug_info("campfire", "GOT SOMETHING\n");
			}
		} else {
			purple_debug_info("campfire", "LOST CONNECTION\n");
			purple_debug_info("campfire", "errno: %d\n", errno);
			purple_ssl_close(conn->gsc);
			conn->gsc = NULL;
			if (node) {
				*node = NULL;
			}
			return CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION;
		}
	} else if (len == 0) {
		purple_debug_info("campfire", "SERVER CLOSED CONNECTION\n");
		purple_ssl_close(conn->gsc);
		conn->gsc = NULL;
		if (size_response == 0) {
			return CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED;
		}
	}


	/*
	 * only continue here when len >= 0 and size_response > 0
	 * below we parse the response and pull out the
	 * xml we need
	 */
	g_string_append(xaction->http_response, "\n");
	purple_debug_info("campfire", "HTTP response size: %d bytes\n", size_response);
	purple_debug_info("campfire", "HTTP response string:\n%s\n", xaction->http_response->str);

	/*
	 *look for the status
	 */
	gchar *status_and_after = g_strstr_len(xaction->http_response->str, size_response, status_header);
	gchar *status = g_malloc0(4); //status is 3-digits plus NULL
	g_strlcpy (status, &status_and_after[strlen(status_header)], 4);
	purple_debug_info("campfire", "HTTP status: %s\n", status);
	
	/*
	 *look for the content
	 */
	content = g_strstr_len(xaction->http_response->str, size_response, blank_line);

	if (content) {
		purple_debug_info("campfire", "content: %s\n", content);
	}

	size_response = 0; /* reset */
	if(content == NULL) {
		purple_debug_info("campfire", "no content found\n");
		if (node) {
			*node = NULL;
		}
		return CAMPFIRE_HTTP_RESPONSE_STATUS_NO_CONTENT;
	}

	rawxml = g_strstr_len(content, strlen(content), xml_header);

	if(rawxml == NULL) {
		if (node) {
			*node = NULL;
		}
		if( g_strcmp0( status, "200" ) == 0 ) {
			purple_debug_info("campfire", "no xml found, status OK\n");
			return CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML;
		}
		purple_debug_info("campfire", "no xml found\n");
		return CAMPFIRE_HTTP_RESPONSE_STATUS_NO_XML;
	}

	purple_debug_info("campfire", "raw xml: %s\n", rawxml);

	tmpnode = xmlnode_from_str(rawxml, -1);
	node_str = xmlnode_to_str(tmpnode, NULL);
	purple_debug_info("campfire", "xml: %s\n", node_str);
	g_free(node_str);
	g_string_free(xaction->http_response, TRUE);
	xaction->http_response = NULL;
	if (node) {
		*node = tmpnode;
	}
	return CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK;
}


//see
//protocols/jabber/chat.c:864 roomlist_ok_cb() AND
//protocols/jabber/chat.c:800 roomlist_disco_result_cb()
void campfire_room_query_callback(gpointer data, PurpleSslConnection *gsc,
                                  PurpleInputCondition cond)
{
	CampfireSslTransaction *xaction = (CampfireSslTransaction *)data;
	CampfireConn *conn = xaction->campfire;
	gint status;

	xmlnode *xmlrooms = NULL;
	xmlnode *xmlroom = NULL;

	//if (conn->roomlist)
	//	purple_roomlist_unref(conn->roomlist);

	status = campfire_http_response(xaction, cond, &xmlrooms);

	if(status == CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK)
	{
		purple_debug_info("campfire", "processing xml...\n");
		xmlroom = xmlnode_get_child(xmlrooms, "room");
		while (xmlroom != NULL)
		{
			xmlnode *xmlname = xmlnode_get_child(xmlroom, "name");
			gchar *name = xmlnode_get_data(xmlname);
			xmlnode *xmltopic = xmlnode_get_child(xmlroom, "topic");
			gchar *topic = xmlnode_get_data(xmltopic);
			xmlnode *xmlid = xmlnode_get_child(xmlroom, "id");
			gchar *id = xmlnode_get_data(xmlid);

	
			PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, NULL);
			purple_roomlist_room_add_field(conn->roomlist, room, topic);
			purple_roomlist_room_add_field(conn->roomlist, room, id);
			purple_roomlist_room_add(conn->roomlist, room);
			xmlroom = xmlnode_get_next_twin(xmlroom);
		}
		purple_roomlist_set_in_progress(conn->roomlist, FALSE);
	}		   
}

void campfire_room_query(CampfireConn *conn)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	xaction->campfire = conn;
	xaction->connect_cb = campfire_do_new_connection_xaction_cb;
	xaction->connect_cb_data = xaction;
	xaction->response_cb = campfire_room_query_callback;
	xaction->response_cb_data = xaction;
	xaction->http_response = NULL;
	campfire_http_request(xaction, "/rooms.xml", "GET");
}


void campfire_userlist_callback(gpointer data, PurpleSslConnection *gsc,
                                PurpleInputCondition cond)
{
	CampfireSslTransaction *xaction = (CampfireSslTransaction *)data;
	CampfireConn *conn = xaction->campfire;
	PurpleConversation *convo;
	xmlnode *xmlroom = NULL;
	xmlnode *xmlusers = NULL;
	xmlnode *xmluser = NULL;
	
	if(campfire_http_response(xaction, cond, &xmlroom) == CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK)
	{
		purple_debug_info("campfire", "locating room: %s\n", conn->room_name);
		convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, conn->room_name, purple_connection_get_account(conn->gc));
		xmlusers = xmlnode_get_child(xmlroom, "users");
		xmluser = xmlnode_get_child(xmlusers, "user");
		GList *users = NULL;

		while (xmluser != NULL)
		{
			xmlnode *xmlname = xmlnode_get_child(xmluser, "name");
			gchar *name = xmlnode_get_data(xmlname);
			purple_debug_info("campfire", "user in room: %s\n", name);

			if ( ! purple_conv_chat_find_user(PURPLE_CONV_CHAT(convo), name) )
			{
				purple_debug_info("campfire", "adding user %s to room\n", name);
				purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), name, NULL, PURPLE_CBFLAGS_NONE, TRUE);
			}
			users = g_list_prepend(users, name);
			xmluser = xmlnode_get_next_twin(xmluser);
		}

		purple_debug_info("campfire", "Getting all users in room\n");
		GList *chatusers = purple_conv_chat_get_users(PURPLE_CONV_CHAT(convo));
		purple_debug_info("campfire", "got all users in room %p\n", chatusers);

		if (users == NULL) //probably shouldn't happen
		{
			purple_debug_info("campfire", "removing all users from room");
			purple_conv_chat_remove_users(PURPLE_CONV_CHAT(convo), chatusers, NULL);
		}
		else if (chatusers != NULL) //also probably shouldn't happen
		{
			purple_debug_info("campfire", "iterating chat users\n");
			for (; chatusers != NULL; chatusers = chatusers->next)
			{
				PurpleConvChatBuddy *buddy = chatusers->data;
				gboolean found = FALSE;
				purple_debug_info("campfire", "checking to see if user %s has left\n", buddy->name);
				for (; users; users = users->next)
				{
					if ( g_strcmp0( users->data, buddy->name ) == 0 )
					{
						purple_debug_info("campfire", "user %s is still here\n", buddy->name);
						found = TRUE;
						break;
					}
				}

				if (!found)
				{
					purple_debug_info("campfire", "removing user %s that has left\n", buddy->name);
					purple_conv_chat_remove_user(PURPLE_CONV_CHAT(convo), buddy->name, NULL);
				}
				
				//g_free(c->data);
			}

			//g_list_free(c);
			//g_list_free(users);			
		}

		//g_list_free(chatusers);
	}
	
}

gboolean campfire_room_check(gpointer data)	
{
	CampfireConn *conn = (CampfireConn *)data;
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	xaction->campfire = conn;
	xaction->connect_cb = campfire_do_new_connection_xaction_cb;
	xaction->connect_cb_data = xaction;
	xaction->response_cb = campfire_userlist_callback;
	xaction->response_cb_data = conn;
	
	purple_debug_info("campfire", "checking for users in room: %s\n", conn->room_name);
	GString *uri = g_string_new("/room/");
	g_string_append(uri, conn->room_id);
	g_string_append(uri, ".xml");
	campfire_http_request(xaction, uri->str, "GET");
	g_string_free(uri, TRUE);
	
	return TRUE;
}

void campfire_message_callback(gpointer data, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	CampfireSslTransaction *xaction = (CampfireSslTransaction *)data;
	CampfireConn *conn = xaction->campfire;
	PurpleConversation *convo;
	xmlnode *xmlmessages = NULL;
	xmlnode *xmlmessage = NULL;
	
	if(campfire_http_response(xaction, cond, &xmlmessages) == CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK)
	{
		purple_debug_info("campfire", "retrieving messages from: %s\n", conn->room_name);
		convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, conn->room_name, purple_connection_get_account(conn->gc));
		xmlmessage = xmlnode_get_child(xmlmessages, "message");

		while (xmlmessage != NULL)
		{
			xmlnode *xmlbody = xmlnode_get_child(xmlmessage, "body");
			gchar *body = xmlnode_get_data(xmlbody);
			xmlnode *xmltype = xmlnode_get_child(xmlmessage, "type");
			xmlnode *xmluser_id = xmlnode_get_child(xmlmessage, "user-id");
			gchar *user_id = xmlnode_get_data(xmluser_id);
			xmlnode *xmltime = xmlnode_get_child(xmlmessage, "created-at");
			GTimeVal time;
			time_t mtime = 0;
			if(g_time_val_from_iso8601(xmlnode_get_data(xmltime), &time))
			{
				mtime = time.tv_sec;
			}
									
			gchar *msgtype = xmlnode_get_data(xmltype);
			
			purple_debug_info("campfire", "got message of type: %s\n", msgtype);

			if ( g_strcmp0( msgtype, CAMPFIRE_MESSAGE_ENTER ) == 0 )
			{
			}
			else if ( g_strcmp0( msgtype, CAMPFIRE_MESSAGE_LEAVE ) == 0 )
			{
			}
			else if ( g_strcmp0( msgtype, CAMPFIRE_MESSAGE_KICK ) == 0 )
			{
			}
			else if ( g_strcmp0( msgtype, CAMPFIRE_MESSAGE_TIME ) == 0 )
			{
			}
			else
			{
				purple_conv_chat_write(PURPLE_CONV_CHAT(convo), user_id,
									   body, PURPLE_CBFLAGS_NONE,
									   mtime);
			}
			xmlmessage = xmlnode_get_next_twin(xmlmessage);
		}
	}
}

void campfire_fetch_first_messages(CampfireConn *conn)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	purple_debug_info("campfire", "campfire_fetch_first_messages\n");

	GString *uri = g_string_new("/room/");
	g_string_append(uri, conn->room_id);
	g_string_append(uri, "/recent.xml?limit=20");

	xaction->campfire = conn;
	xaction->connect_cb = campfire_do_new_connection_xaction_cb;
	xaction->connect_cb_data = xaction;
	xaction->response_cb = campfire_message_callback;
	xaction->response_cb_data = conn;

	campfire_http_request(xaction, uri->str, "GET");
	g_string_free(uri, TRUE);
}

void campfire_room_join_callback(gpointer data, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	CampfireSslTransaction *xaction = (CampfireSslTransaction *)data;
	CampfireConn *conn = xaction->campfire;
	
	if (campfire_http_response(xaction, cond, NULL) == CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML)
	{		
		campfire_room_check(conn);

		purple_debug_info("campfire", "joining room: %s\n", conn->room_name);
		purple_conversation_new(PURPLE_CONV_TYPE_CHAT, purple_connection_get_account(conn->gc), conn->room_name);		

		//call this method again periodically to check for new users
		conn->message_timer = purple_timeout_add_seconds(3, (GSourceFunc)campfire_room_check, conn);		

		//campfire_fetch_first_messages(conn);
	}
}

gboolean campfire_fetch_latest_messages(gpointer data)
{
	purple_debug_info("campfire", "campfire_fetch_latest_messages\n");
	//CampfireConn *conn = data;
	return TRUE;	
}

void campfire_room_join(CampfireConn *conn)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GString *uri = g_string_new("/room/");
	g_string_append(uri, conn->room_id);
	g_string_append(uri, "/join.xml");

	xaction->campfire = conn;
	xaction->connect_cb = campfire_do_new_connection_xaction_cb;
	xaction->connect_cb_data = xaction;
	xaction->response_cb = campfire_room_join_callback;
	xaction->response_cb_data = conn;

	campfire_http_request(xaction, uri->str, "POST");
	g_string_free(uri, TRUE);
	
	//set up a refresh timer now that we're joined
	conn->message_timer = purple_timeout_add_seconds(3, (GSourceFunc)campfire_fetch_latest_messages, conn);
}

/*
gchar* campfire_get_username(gint user_id)
{
	static GHashTable *user_table = g_hash_table_new(NULL, g_int_equal);
}
*/

