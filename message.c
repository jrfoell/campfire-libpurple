//system includes
#include <string.h>
#include <glib/gi18n.h>
#include <errno.h>
#include <time.h>

//purple includes
#include <xmlnode.h>
#include <notify.h>
#include <debug.h>

//local includes
#include "message.h"

//@TODO replace PurpleInputCondition params with PURPLE_INPUT_READ or PURPLE_INPUT_WRITE

void campfire_ssl_failure(PurpleSslConnection *gsc, PurpleSslErrorType error, gpointer data)
{
	purple_debug_info("campfire", "ssl connect failure\n");
}

void campfire_http_request(CampfireSslTransaction *xaction, gchar *uri, gchar *method, xmlnode *postxml)
{
	CampfireConn *conn = xaction->campfire;
	const char *api_token = purple_account_get_string(conn->account, "api_token", "");
	gchar *xmlstr;

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

	if(postxml)
	{
		xmlstr = xmlnode_to_str(postxml, NULL);
		g_string_append(xaction->http_request, "Content-Type: text/xml\r\n");
		g_string_append(xaction->http_request, "Content-Length: 32\r\n");
		g_string_append(xaction->http_request, xmlstr);		
		g_string_append(xaction->http_request, "\r\n");
	}
	g_string_append(xaction->http_request, "\r\n");			
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

	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);
	if (size_response == 0) {
		if (xaction->http_response) {
			g_string_free(xaction->http_response, TRUE);
		}
		xaction->http_response = g_string_new("");
	}

	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);
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
	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);


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
			if (node) {
				*node = NULL;
			}
			return CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION;
		}
	} else if (len == 0) {
		purple_debug_info("campfire", "SERVER CLOSED CONNECTION\n");
		if (size_response == 0) {
			return CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED;
		}
	}
	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);


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
	purple_debug_info("campfire","status_and_after:%p\n", status_and_after);
	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);
	gchar *status = g_malloc0(4); //status is 3-digits plus NULL
	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);
	g_strlcpy (status, &status_and_after[strlen(status_header)], 4);
	purple_debug_info("campfire", "HTTP status: %s\n", status);
	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);
	
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

	if(rawxml == NULL)
	{
		if(node)
		{
			*node = NULL;
		}
		if(g_strcmp0(status, "200") == 0)
		{
			purple_debug_info("campfire", "no xml found, status OK\n");
			return CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML;
		}
		purple_debug_info("campfire", "no xml found\n");
		return CAMPFIRE_HTTP_RESPONSE_STATUS_NO_XML;
	}

	//purple_debug_info("campfire", "raw xml: %s\n", rawxml);

	tmpnode = xmlnode_from_str(rawxml, -1);
	node_str = xmlnode_to_str(tmpnode, NULL);
	//purple_debug_info("campfire", "xml: %s\n", node_str);
	g_free(node_str);
	g_string_free(xaction->http_response, TRUE);
	xaction->http_response = NULL;
	if (node) {
		*node = tmpnode;
	}
	return CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK;
}

void campfire_ssl_handler(GList **queue, PurpleSslConnection *gsc, PurpleInputCondition cond)
{	
	GList *first = g_list_first(*queue);
	CampfireSslTransaction *xaction;
	static CampfireConn *campfire = NULL;
	gint status;
	gboolean close_ssl = FALSE;
	gboolean cleanup = TRUE;

	purple_debug_info("campfire", "campfire_ssl_handler(): first: %p\n", first);
	if (!first)
	{
		CampfireSslTransaction *tmpxaction = g_new0(CampfireSslTransaction, 1);
		tmpxaction->campfire = campfire;
		/* this situation will occur when the server closes the
		 * connection after the last transaction.  Possibly others?
		 */
		purple_debug_info("campfire", "checking response\n");		
		status = campfire_http_response(tmpxaction, cond, &(tmpxaction->xml_response));
		purple_debug_info("campfire", "Nothing left in HTTP queue\n");		
		if (status == CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED)
		{
			purple_ssl_close(gsc);
			if (campfire)
			{
				campfire->gsc = NULL;
			}
		}
		g_free(tmpxaction);
	}
	else
	{
		xaction = first->data;
		campfire = xaction->campfire;
		status = campfire_http_response(xaction, cond, &(xaction->xml_response));
		if(status == CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK
		   || status == CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML)
		{
			xaction->response_cb(xaction, gsc, cond);
			cleanup = TRUE;
		}
		else if(status == CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION)
		{
			close_ssl = TRUE;
		}
		else if (status ==  CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED)
		{
			close_ssl = TRUE;
		}
		else
		{
			cleanup = FALSE;
		}

		if (close_ssl)
		{
			purple_ssl_close(xaction->campfire->gsc);
			xaction->campfire->gsc = NULL;
			cleanup = TRUE;
		}
			

		if (cleanup)
		{
			if (xaction->room_id)
			{
				g_free(xaction->room_id);
			}
			if (xaction->http_request)
			{
				g_string_free(xaction->http_request, TRUE);
			}
			if (xaction->http_response)
			{
				g_string_free(xaction->http_response, TRUE);
			}
			if (xaction->xml_response)
			{
				xmlnode_free(xaction->xml_response);
			}
			g_free(xaction);
			purple_debug_info("campfire", "removing from queue: length: %d\n",
			                  g_list_length(*queue));
			*queue = g_list_remove(*queue, xaction);
			purple_debug_info("campfire", "removed from queue: length: %d\n",
			                  g_list_length(*queue));
			first = g_list_first(*queue);
			if (first)
			{
				xaction = first->data;
				purple_debug_info("campfire", "writing subsequent request on ssl connection\n");
				purple_ssl_write(gsc, xaction->http_request->str, xaction->http_request->len);
			}
		}			
	}
}

void campfire_queue_xaction(CampfireSslTransaction *xaction, PurpleSslConnection *gsc, PurpleInputCondition cond)
{
	static GList *queue = NULL;
	purple_debug_info("campfire", "%s\n", __FUNCTION__);

	if (!xaction->campfire->gsc) {
		purple_debug_info("campfire", "new ssl connection\n");
		xaction->campfire->gsc = purple_ssl_connect(xaction->campfire->account,
		                                            xaction->campfire->hostname,
		                                            443,
		                                            (PurpleSslInputFunction)(campfire_queue_xaction),
		                                            campfire_ssl_failure,
		                                            xaction);
	} else {
		purple_debug_info("campfire", "previous ssl connection\n");
		queue = g_list_append(queue, xaction);
		purple_debug_info("campfire", "queue length %d\n", g_list_length(queue));
		if (g_list_length(queue) == 1) {
			purple_debug_info("campfire", "adding input\n");
			purple_ssl_input_add(gsc, (PurpleSslInputFunction)(campfire_ssl_handler), &queue);
			purple_debug_info("campfire", "writing first request on ssl connection\n");
			purple_debug_info("campfire", "writing %s\n", xaction->http_request->str);			
			purple_ssl_write(gsc, xaction->http_request->str, xaction->http_request->len);
		}
	}
			
}

void campfire_message_send_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
									PurpleInputCondition cond)
{
	/*
	int id = g_ascii_strtoll(xaction->room_id, NULL, 10);
	PurpleConversation *convo = purple_find_chat(xaction->campfire->gc, id);
	purple_conv_chat_write(PURPLE_CONV_CHAT(convo), "", message, flags, time(NULL));
	*/
}

void campfire_message_send(CampfireConn *campfire, int id, const char *message)
{	
	xmlnode *xmlmessage, *xmlchild;
	gchar *room_id = g_strdup_printf("%i", id);
	
	xmlmessage = xmlnode_new("message");
	xmlnode_set_attrib(xmlmessage, "type", CAMPFIRE_MESSAGE_TEXT);	
	xmlchild = xmlnode_new_child(xmlmessage, "body");
	xmlnode_insert_data(xmlchild, message, -1);
	
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	GString *uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, "/speak.xml");

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_message_send_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);

	purple_debug_info("campfire", "Sending message %s\n", xmlnode_to_str(xmlmessage, NULL));
		
	campfire_http_request(xaction, uri->str, "POST", xmlmessage);
	g_string_free(uri, TRUE);
	g_free(room_id);
	xmlnode_free(xmlmessage);
	campfire_queue_xaction(xaction, campfire->gsc, 0);
}

//see
//protocols/jabber/chat.c:864 roomlist_ok_cb() AND
//protocols/jabber/chat.c:800 roomlist_disco_result_cb()
void campfire_room_query_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
                                  PurpleInputCondition cond)
{
	xmlnode *xmlroom = NULL;

	purple_debug_info("campfire", "processing xml...\n");
	xmlroom = xmlnode_get_child(xaction->xml_response, "room");
	while (xmlroom != NULL)
	{
		xmlnode *xmlname = xmlnode_get_child(xmlroom, "name");
		gchar *name = xmlnode_get_data(xmlname);
		xmlnode *xmltopic = xmlnode_get_child(xmlroom, "topic");
		gchar *topic = xmlnode_get_data(xmltopic);
		xmlnode *xmlid = xmlnode_get_child(xmlroom, "id");
		gchar *id = xmlnode_get_data(xmlid);

	
		PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, NULL);
		purple_roomlist_room_add_field(xaction->campfire->roomlist, room, topic);
		purple_roomlist_room_add_field(xaction->campfire->roomlist, room, id);
		purple_roomlist_room_add(xaction->campfire->roomlist, room);
		xmlroom = xmlnode_get_next_twin(xmlroom);
	}
	purple_roomlist_set_in_progress(xaction->campfire->roomlist, FALSE);
}

void campfire_room_query(CampfireConn *campfire)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)(campfire_room_query_callback);
	xaction->response_cb_data = xaction;
	xaction->http_response = NULL;
	campfire_http_request(xaction, "/rooms.xml", "GET", NULL);
	campfire_queue_xaction(xaction, campfire->gsc, 0);
}


void campfire_userlist_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
                                PurpleInputCondition cond)
{
	PurpleConversation *convo;
	xmlnode *xmlroomname = NULL;
	xmlnode *xmltopic = NULL;
	xmlnode *xmlusers = NULL;
	xmlnode *xmluser = NULL;
	
	xmlroomname = xmlnode_get_child(xaction->xml_response, "name");
	gchar *room_name = xmlnode_get_data(xmlroomname);
	purple_debug_info("campfire", "locating room: %s\n", room_name);
	convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_name, purple_connection_get_account(xaction->campfire->gc));

	xmltopic = xmlnode_get_child(xaction->xml_response, "topic");
	gchar *topic = xmlnode_get_data(xmltopic);
	purple_debug_info("campfire", "setting topic to %s\n", topic);
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(convo), NULL, topic);
	xmlusers = xmlnode_get_child(xaction->xml_response, "users");
	xmluser = xmlnode_get_child(xmlusers, "user");
	GList *users = NULL;

	while (xmluser != NULL)
	{
		xmlnode *xmlname = xmlnode_get_child(xmluser, "name");
		gchar *name = xmlnode_get_data(xmlname);
		purple_debug_info("campfire", "user in room: %s\n", name);

		if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(convo), name))
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
		purple_debug_info("campfire", "removing all users from room\n");
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
				if (g_strcmp0(users->data, buddy->name) == 0)
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
	//xmlnode *xmlroomid = xmlnode_get_child(xaction->xml_response, "id");
	//gchar *room_id = xmlnode_get_data(xmlroomid);
	//purple_debug_info("campfire", "about to fetch message for room %s\n", room_id);		
	//campfire_fetch_first_messages(xaction->campfire, room_id);
	
}

gboolean campfire_room_check(gpointer data)	
{
	CampfireConn *campfire = (CampfireConn *)data;
	
	GList *rooms = g_hash_table_get_values(campfire->rooms);

	// cancel the timer if we've left all rooms
	if(!rooms)
	{
		campfire->message_timer = 0;
		return FALSE;
	}
	
	for (; rooms != NULL; rooms = rooms->next)
	{
		CampfireRoom *room = rooms->data;

		//first check the room users
		CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

		xaction->campfire = campfire;
		xaction->response_cb = (PurpleSslInputFunction)campfire_userlist_callback;
		xaction->response_cb_data = xaction;
	
		purple_debug_info("campfire", "checking for users in room: %s\n", room->id);

		GString *uri = g_string_new("/room/");
		g_string_append(uri, room->id);
		g_string_append(uri, ".xml");
		
		campfire_http_request(xaction, uri->str, "GET", NULL);
		campfire_queue_xaction(xaction, campfire->gsc, 0);
		g_string_free(uri, TRUE);

		//then get recent messages
		if(room->last_message_id)
		{
			CampfireSslTransaction *xaction2 = g_new0(CampfireSslTransaction, 1);

			xaction2->campfire = campfire;
			xaction2->response_cb = (PurpleSslInputFunction)campfire_message_callback;
			xaction2->response_cb_data = xaction2;
			xaction2->room_id = g_strdup(room->id);

			uri = g_string_new("/room/");
			g_string_append(uri, room->id);
			g_string_append(uri, "/recent.xml?since_message_id=");
			g_string_append(uri, room->last_message_id);

			purple_debug_info("campfire", "getting latest messages: %s\n", uri->str);

			campfire_http_request(xaction2, uri->str, "GET", NULL);
			g_string_free(uri, TRUE);
			campfire_queue_xaction(xaction2, campfire->gsc, 0);
		}
	}

	//@TODO set this to zero once all rooms have been left
	if(!campfire->message_timer)
	{
		//call this function again periodically to check for new users
		//campfire->message_timer = purple_timeout_add_seconds(3, (GSourceFunc)campfire_room_check, campfire);
	}

	return TRUE;
}

void campfire_message_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
                               PurpleInputCondition cond)
{
	purple_debug_info("campfire", "%s\n", __FUNCTION__);
	xmlnode *xmlmessage = NULL;
	
	GList *msgs = NULL;
	xmlmessage = xmlnode_get_child(xaction->xml_response, "message");
	gchar *room_id = xaction->room_id;

	while (xmlmessage != NULL)
	{
		xmlnode *xmlbody = xmlnode_get_child(xmlmessage, "body");
		gchar *body = xmlnode_get_data(xmlbody);
		
		xmlnode *xmluser_id = xmlnode_get_child(xmlmessage, "user-id");
		gchar *user_id = xmlnode_get_data(xmluser_id);

		xmlnode *xmltime = xmlnode_get_child(xmlmessage, "created-at");
		GTimeVal timeval;
		time_t mtime;
		if(g_time_val_from_iso8601(xmlnode_get_data(xmltime), &timeval))
		{
			mtime = timeval.tv_sec;
		}
								
		xmlnode *xmltype = xmlnode_get_child(xmlmessage, "type");
		gchar *msgtype = xmlnode_get_data(xmltype);

		xmlnode *xmlid = xmlnode_get_child(xmlmessage, "id");
		gchar *msg_id = xmlnode_get_data(xmlid);

		if (g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TIME) == 0)
		{
			purple_debug_info("campfire", "Skipping message of type: %s\n", msgtype);
			xmlmessage = xmlnode_get_next_twin(xmlmessage);
			continue;
		}
				
		purple_debug_info("campfire", "got message of type: %s\n", msgtype);
		CampfireMessage *msg = g_new0(CampfireMessage, 1);
		msg->id = msg_id;
		msg->user_id = user_id;
		msg->time = mtime;
		msg->type = msgtype;
		
		if (g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TEXT) == 0)
		{
			msg->message = body;
		}

		msgs = g_list_append(msgs, msg);
		
		xmlmessage = xmlnode_get_next_twin(xmlmessage);
	}

	//print the messages out
	purple_debug_info("campfire", "calling print messages\n");
				
	//create a new one
	CampfireSslTransaction *xaction2 = g_new0(CampfireSslTransaction, 1);

	xaction2->campfire = xaction->campfire;
	xaction2->response_cb = (PurpleSslInputFunction)campfire_print_messages;
	xaction2->response_cb_data = xaction2;
	xaction2->messages = msgs;
	xaction2->room_id = g_strdup(room_id);
	xaction2->first_check = xaction->first_check;

	campfire_print_messages(xaction2, xaction->campfire->gsc, 0);
}

void campfire_fetch_first_messages(CampfireConn *campfire, gchar *room_id)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	purple_debug_info("campfire", "%s\n", __FUNCTION__);

	GString *uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, "/recent.xml?limit=40");

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_message_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);
	xaction->first_check = TRUE;

	campfire_http_request(xaction, uri->str, "GET", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(xaction, campfire->gsc, 0);
}

void campfire_room_join_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	CampfireRoom *room = g_hash_table_lookup(xaction->campfire->rooms, xaction->room_id);
	purple_debug_info("campfire", "joining room: %s with id: %s\n", room->name, room->id);
	serv_got_joined_chat(xaction->campfire->gc, g_ascii_strtoll(xaction->room_id, NULL, 10), room->name);
	
	//campfire_fetch_first_messages(xaction->campfire, xaction->room_id);
}

void campfire_room_join(CampfireConn *campfire, gchar *id, gchar *name)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GString *uri = g_string_new("/room/");
	CampfireRoom *room = NULL;

	g_string_append(uri, id);
	g_string_append(uri, "/join.xml");

	if(!campfire->rooms)
	{
		campfire->rooms = g_hash_table_new(g_str_hash, g_str_equal);
	}
	purple_debug_info("campfire", "add room to list %s ID: %s\n", name, id);
	room = g_new0(CampfireRoom, 1);
	room->name = g_strdup(name);
	room->id = g_strdup(id);
	g_hash_table_replace(campfire->rooms, id, room);

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_room_join_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(id);
	purple_debug_info("campfire", "ID: %s\n", xaction->room_id);

	campfire_http_request(xaction, uri->str, "POST", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(xaction, campfire->gsc, 0);
}

void campfire_room_leave_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	CampfireRoom *room = g_hash_table_lookup(xaction->campfire->rooms, xaction->room_id);
	purple_debug_info("campfire", "leaving room: %s\n", room->name);
	serv_got_chat_left(xaction->campfire->gc, g_ascii_strtoll(xaction->room_id, NULL, 10));
}

void campfire_room_leave(CampfireConn *campfire, gint id)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GString *uri = NULL;

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_room_leave_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup_printf("%d", id);

	uri = g_string_new("/room/");
	g_string_append(uri, xaction->room_id);
	g_string_append(uri, "/leave.xml");

	campfire_http_request(xaction, uri->str, "POST", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(xaction, campfire->gsc, 0);
}



/** also functions as a callback (to itself) */
void campfire_print_messages(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	purple_debug_info("campfire", "%s with first_check %s\n", __FUNCTION__, xaction->first_check?"true":"false");
	CampfireConn *campfire = xaction->campfire;

	//initialize user list
	if(!campfire->users)
	{
		campfire->users = g_hash_table_new(g_str_hash, g_str_equal);
	}

	//handle possible user response (if used as a callback)
	if(xaction->xml_response)
	{
		xmlnode *xmlusername = xmlnode_get_child(xaction->xml_response, "name");
		xmlnode *xmluserid = xmlnode_get_child(xaction->xml_response, "id");

		gchar *user_id = xmlnode_get_data(xmluserid);
		gchar *username = xmlnode_get_data(xmlusername);
		purple_debug_info("campfire", "adding username %s ID %s\n", username, user_id);
		g_hash_table_replace(campfire->users, user_id, username);
	}
	
	GList *first = g_list_first(xaction->messages);

	if (!first) {
		//print the user as "in the room" after the previous messages have been printed
		//(only if this is the first message check [cond == 1 hint])
		if(xaction->first_check)
			campfire_room_check(campfire);
		
		//maybe cleanup here?
		purple_debug_info("campfire", "no more messages to process\n");
	} else {
		CampfireMessage *msg = first->data;

		gchar *user_name = g_hash_table_lookup(campfire->users, msg->user_id);
		
		purple_debug_info("campfire", "Looked up user_id: %s, got %s\n", msg->user_id, user_name);

		if(!user_name)
		{
			GString *uri = g_string_new("/users/");
			g_string_append(uri, msg->user_id);
			g_string_append(uri, ".xml");
			
			//create a new one
			CampfireSslTransaction *xaction2 = g_new0(CampfireSslTransaction, 1);

			xaction2->campfire = xaction->campfire;
			xaction2->response_cb = (PurpleSslInputFunction)campfire_print_messages;
			xaction2->response_cb_data = xaction2;
			xaction2->messages = xaction->messages;
			xaction2->room_id = g_strdup(xaction->room_id);
			xaction2->first_check = xaction->first_check;
			
			campfire_http_request(xaction2, uri->str, "GET", NULL);
			campfire_queue_xaction(xaction2, campfire->gsc, cond);
		} else {
			//print
			purple_debug_info("campfire", "not crashed yet\n");
			CampfireRoom *room = g_hash_table_lookup(campfire->rooms, xaction->room_id);
			purple_debug_info("campfire", "not crashed yet 1.5 %s\n", xaction->room_id);
			PurpleConversation *convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room->name, purple_connection_get_account(xaction->campfire->gc));
			purple_debug_info("campfire", "not crashed yet 2\n");

			purple_debug_info("campfire", "Writing chat message ID \"%s\" to %p from name %s\n", msg->id, convo, user_name);
			
			if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_TEXT) == 0)
			{
				purple_debug_info("campfire", "Writing chat message \"%s\" to %p from name %s\n", msg->message, convo, user_name);
				purple_conversation_write(convo, user_name, msg->message,
										  PURPLE_MESSAGE_RECV,
										  msg->time);
			}
			else
			{
				GString *message = g_string_new(user_name);
				if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_ENTER) == 0)
				{
					g_string_append(message, " entered the room.");
				}
				else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_LEAVE) == 0)
				{
					g_string_append(message, " left the room.");
				}
				else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_KICK) == 0)
				{
					g_string_append(message, " kicked.");
				}
				purple_conversation_write(convo, "", message->str,
										  PURPLE_MESSAGE_SYSTEM,
										  msg->time);
			}
			
			//remember the last message we've written
			room->last_message_id = msg->id;
			
			xaction->messages = g_list_remove(xaction->messages, msg);
			xaction->xml_response = NULL;
			//recurse
			campfire_print_messages(xaction, gsc, cond);
		}
	}
}
