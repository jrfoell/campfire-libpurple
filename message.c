//system includes
#include <string.h>
#include <glib/gi18n.h>

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

void campfire_http_request(CampfireConn *conn, gchar *uri, gchar *post)
{
	const char *api_token = purple_account_get_string(conn->account,
			"api_token", "");

	GString *request = g_string_new("GET ");
	g_string_append(request, uri);
	g_string_append(request, " HTTP/1.1\r\n");

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

xmlnode *campfire_http_response(gpointer data, PurpleSslConnection *gsc,
							PurpleInputCondition cond)
{
	PurpleConnection *gc = data;
	static gchar buf[2048];
	int len;

	while ((len = purple_ssl_read(gsc, buf, sizeof(buf) - 1)) > 0)
	{
		buf[len] = '\0';
		purple_debug_info("campfire",
		                  "HTTP input: %d bytes:\n%s\n",
		                  len, buf);
	}

	if (len == 0)
	{
		purple_debug_info("campfire", "DISCONNECTED YO\n");
		purple_ssl_close(gsc);
		//don't show this to the user, it's stateless
		/*
		purple_connection_error_reason(gc, 
		                               PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                               "Server closed the connection");
		*/
		return NULL;
	}

	//look for the content
	gchar *blank_line = "\r\n\r\n";
	gchar *content = g_strstr_len(buf, len, blank_line);

	purple_debug_info("campfire", "content: %s", content);

	if(content == NULL)
	{
		purple_debug_info("campfire", "no content found");
		return NULL;
	}

	gchar *xml_header = "<?xml";
	gchar *rawxml = g_strstr_len(content, strlen(content), xml_header);

	if(rawxml == NULL)
	{
		purple_debug_info("campfire", "no xml found");
		return NULL;
	}

	purple_debug_info("campfire", "raw xml: %s", rawxml);
	
	xmlnode *node = xmlnode_from_str(rawxml, -1);
	gint xmllen;
	gchar *str = xmlnode_to_str(node, &xmllen);
	purple_debug_info("campfire", "xml: %s", str);
	g_free(str);
	return node;
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

	if((xmlrooms = campfire_http_response(gc, gsc, cond)) != NULL)
	{
		if((xmlroom = xmlnode_get_child(xmlrooms, "room")) != NULL)
		{
			xmlnode *xmlname = xmlnode_get_child(xmlroom, "name");
			gchar *name = xmlnode_get_data(xmlname);
			xmlnode *xmltopic = xmlnode_get_child(xmlroom, "topic");
			gchar *topic = xmlnode_get_data(xmltopic);

	
			PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, NULL);
			purple_roomlist_room_add_field(conn->roomlist, room, topic);
			purple_roomlist_room_add(conn->roomlist, room);
	
			while((xmlroom = xmlnode_get_next_twin(xmlroom)) != NULL)
			{
				xmlname = xmlnode_get_child(xmlroom, "name");
				name = xmlnode_get_data(xmlname);
				xmltopic = xmlnode_get_child(xmlroom, "topic");
				topic = xmlnode_get_data(xmltopic);

	
				PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, NULL);
				purple_roomlist_room_add_field(conn->roomlist, room, topic);
				purple_roomlist_room_add(conn->roomlist, room);
			}
		}
	}		   
}

void campfire_room_query(CampfireConn *conn)
{
	campfire_http_request(conn, "/rooms.xml", NULL);

	purple_ssl_input_add(conn->gsc, campfire_room_query_callback, conn->gc);
}
