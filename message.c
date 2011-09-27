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
	const char *type = NULL;

	message = xmlnode_new("message");

	switch(cm->type) {
		case CAMPFIRE_MESSAGE_PASTE:
			type = "PasteMessage";
			break;
		case CAMPFIRE_MESSAGE_SOUND:
			type = "SoundMessage";
			break;
		case CAMPFIRE_MESSAGE_TWEET:
			type = "TweetMessage";
			break;
		case CAMPFIRE_MESSAGE_TEXT:
		default:
			type = "TextMessage";
			break;
	}

	if(type)
		xmlnode_set_attrib(message, "type", type);
	
	if(cm->body) {
		child = xmlnode_new_child(message, "body");
		xmlnode_insert_data(child, cm->body, -1);
	}
	
	//jabber_send(message);

	xmlnode_free(message);
}

void campfire_ssl_failure(PurpleSslConnection *gsc, PurpleSslErrorType error, gpointer data)
{
	PurpleConnection *gc = data;
	CampfireConn *campfire = gc->proto_data;

	campfire->gsc = NULL;

	purple_connection_ssl_error (gc, error);
}

void campfire_renew_connection(CampfireConn *conn, void *callback)
{
	if(!conn->gsc) {
		purple_debug_info("campfire", "Renewing connnection\n");
		conn->gsc = purple_ssl_connect(conn->account,
									   conn->hostname,
									   443,
									   callback,
									   campfire_ssl_failure,
									   conn->gc);

	} else {
		purple_debug_info("campfire", "connnection is still open\n");
	}

	if(!conn->gsc) {
		purple_connection_error_reason(conn->gc,
		                               PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                               _("Unable to connect"));
	}	
}

void campfire_http_request(CampfireConn *conn, gchar *uri, gchar *method)
{
	const char *api_token = purple_account_get_string(conn->account,
			"api_token", "");

	GString *request = g_string_new(method);
	g_string_append(request, " ");
	g_string_append(request, uri);
	g_string_append(request, " HTTP/1.1\r\n");
	//g_string_append(request, "\r\n");

	g_string_append(request, "Content-Type: application/xml\r\n");
	
	g_string_append(request, "Authorization: Basic ");
	gsize auth_len = strlen(api_token);
	gchar *encoded = purple_base64_encode((const guchar *)api_token, auth_len);
	g_string_append(request, encoded);
	g_string_append(request, "\r\n");	
	g_free(encoded);
	
	g_string_append(request, "Host: ");
	g_string_append(request, conn->hostname);
	g_string_append(request, "\r\n");

	g_string_append(request, "Accept: */*\r\n\r\n");

	campfire_renew_connection(conn, NULL);
	purple_ssl_write(conn->gsc,request->str, strlen(request->str));
	purple_debug_info("campfire", "HTTP request:\n%s\n", request->str);
	g_string_free(request, TRUE);
}

gint campfire_http_response(gpointer data, PurpleSslConnection *gsc,
                            PurpleInputCondition cond, xmlnode **node)
{
	/*PurpleConnection *gc = data;*/
	GString *response = g_string_new("");
	static gchar buf[1024];
	gchar *blank_line = "\r\n\r\n";
	gchar *xml_header = "<?xml";
	gchar *content, *rawxml, *node_str;
	xmlnode *tmpnode;
	gint len = 0;
	gint done_reading = 0;

	/* We need a while loop here if/when the response is larger
	 * than our 'static gchar buf'
	 * NOTE: jabber is using a while loop here and parsing chunks of
	 *       xml each loop with libxml call xmlParseChunk()
	 */
	while (!done_reading) {
		gint num_bytes;

		errno = 0;
		num_bytes = purple_ssl_read(gsc, buf, sizeof(buf));
		if (num_bytes < 0 && len == 0 && errno == EAGAIN) {
			purple_debug_info("campfire", "TRY AGAIN...\n");
			if (node) {
				*node = NULL;
			}
			return CAMPFIRE_HTTP_RESPONSE_STATUS_TRY_AGAIN;
		} else if (num_bytes < 0 && errno != EAGAIN) {
			purple_debug_info("campfire", "LOST CONNECTION\n");
			purple_debug_info("campfire", "errno: %d\n", errno);
			purple_ssl_close(gsc);
			if (node) {
				*node = NULL;
			}
			return CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION;
		} else if (num_bytes == 0 && len == 0) {
			purple_debug_info("campfire", "DISCONNECTED YO\n");
			purple_ssl_close(gsc);
			if (node) {
				*node = NULL;
			}
			return CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED;
		} else if (num_bytes < 0) {
			purple_debug_info("campfire", "GOT RESPONSE\n");
			response = g_string_append_c(response, '\0');
			done_reading = 1;
		} else {
			purple_debug_info("campfire",
			                  "read %d bytes from HTTP Response\n",
			                  num_bytes);
			len += num_bytes;
			response = g_string_append_len(response, buf, num_bytes);
		}
	}

	/* only continue here when len > 0
	 */
	purple_debug_info("campfire", "HTTP input: %d bytes:\n", len);
	purple_debug_info("campfire", "HTTP response: %s\n", response->str);

	/*look for the content
	 */
	content = g_strstr_len(response->str, len, blank_line);

	purple_debug_info("campfire", "HTTP response: %s\n", response->str);
	purple_debug_info("campfire", "content: %s\n", content);

	if(content == NULL) {
		purple_debug_info("campfire", "no content found\n");
		if (node) {
			*node = NULL;
		}
		return CAMPFIRE_HTTP_RESPONSE_STATUS_NO_CONTENT;
	}

	rawxml = g_strstr_len(content, strlen(content), xml_header);

	if(rawxml == NULL) {
		purple_debug_info("campfire", "no xml found\n");
		if (node) {
			*node = NULL;
		}
		return CAMPFIRE_HTTP_RESPONSE_STATUS_NO_XML;
	}

	purple_debug_info("campfire", "raw xml: %s\n", rawxml);

	tmpnode = xmlnode_from_str(rawxml, -1);
	node_str = xmlnode_to_str(tmpnode, NULL);
	purple_debug_info("campfire", "xml: %s\n", node_str);
	g_free(node_str);
	g_string_free(response, TRUE);
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
	PurpleConnection *gc = data;
	CampfireConn *conn = gc->proto_data;

	xmlnode *xmlrooms = NULL;
	xmlnode *xmlroom = NULL;

	//if (conn->roomlist)
	//	purple_roomlist_unref(conn->roomlist);

	if(campfire_http_response(gc, gsc, cond, &xmlrooms) == CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK)
	{
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
	}		   
	purple_roomlist_set_in_progress(conn->roomlist, FALSE);
}

void campfire_room_query(CampfireConn *conn)
{
	campfire_http_request(conn, "/rooms.xml", "GET");
	purple_ssl_input_add(conn->gsc, campfire_room_query_callback, conn->gc);
}

void campfire_room_join_callback(gpointer data, PurpleSslConnection *gsc,
                                    PurpleInputCondition cond)
{
	PurpleConnection *gc = (PurpleConnection *)data;
	if (campfire_http_response(gc, gsc, cond, NULL) == CAMPFIRE_HTTP_RESPONSE_STATUS_NO_CONTENT) {
		purple_conversation_new(PURPLE_CONV_TYPE_CHAT, purple_connection_get_account(gc), "bob");
	}
}

void campfire_room_join(CampfireConn *conn, char *room_id, char *room_name)
{
	GString *uri = g_string_new("/room/#");
	g_string_append(uri, room_id);
	g_string_append(uri, "/join.xml");


	campfire_http_request(conn, uri->str, "POST");
	purple_ssl_input_add(conn->gsc, campfire_room_join_callback, conn->gc);	
	g_string_free(uri, TRUE);
}

