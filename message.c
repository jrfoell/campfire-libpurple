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

	purple_debug_info("campfire", "DISCONNECTED YO\n");
	purple_connection_ssl_error (gc, error);
}

void campfire_renew_connection(CampfireConn *conn, void *callback)
{
	if(!conn->gsc) {
		conn->gsc = purple_ssl_connect(conn->account,
									   conn->hostname,
									   443,
									   callback,
									   campfire_ssl_failure,
									   conn->gc);

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

void campfire_http_response(gpointer data, PurpleSslConnection *gsc,
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
		purple_connection_error_reason(gc, 
		                               PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                               "Server closed the connection");
	}
}

void campfire_room_query_callback(gpointer data, PurpleSslConnection *gsc,
                                    PurpleInputCondition cond)
{
	campfire_http_response(data, gsc, cond);
}

void campfire_room_query(CampfireConn *conn)
{
	PurpleRoomlistRoom *room;

	campfire_http_request(conn, "/rooms.xml", NULL);

	purple_ssl_input_add(conn->gsc, campfire_room_query_callback, conn->gc);

	room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, "test1", NULL);
	purple_roomlist_room_add_field(conn->roomlist, room, "hahaha");
	purple_roomlist_room_add(conn->roomlist, room);
	//@TODO do some curl/xml stuff here
	//see
	//protocols/jabber/chat.c:864 roomlist_ok_cb() AND
	//protocols/jabber/chat.c:800 roomlist_disco_result_cb()
}


/*
CURL *get_curl(CampfireConn *conn)
{
	const char *api_token = purple_account_get_string(conn->account,
			"api_token", "");
	
	static CURL *curl;

	if(!curl)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERPWD, api_token);
	}
	
	return curl;	
}

gsize campfire_curl_write_callback(void *ptr, gsize size, gsize nmemb, void *data)
{
  gsize realsize = size * nmemb;
  CampfireRawMessage *raw = (CampfireRawMessage *)data;

  raw->message = g_try_realloc(raw->message, raw->size + realsize + 1);
  if (raw->message == NULL) {
    // out of memory!
	purple_debug_info("campfire", "not enough memory for curl response (g_try_realloc returned NULL)\n");
	// @TODO make sure this a good return value
	return realsize;
  }

  memcpy(&(raw->message[raw->size]), ptr, realsize);
  raw->size += realsize;
  raw->message[raw->size] = 0;

  return realsize;
}

xmlnode *campfire_curl_request(CampfireConn *conn, gchar *uri, gchar *post)
{
	CURLcode res;
	CURL *curl = get_curl(conn);
	GString *url = g_string_new(campfire_get_url(conn));
	g_string_append(url, uri);
		
	purple_debug_info("campfire", "Connect full url: %s\n", url->str);
	curl_easy_setopt(curl, CURLOPT_URL, url->str);
	g_string_free(url, TRUE);
	
	if(post)
	{
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, "Content-Type: application/xml");
	    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	}
	else //get
	{		
		CampfireRawMessage raw;

		raw.message = g_malloc(1);  // will be grown as needed by the realloc above
		raw.size = 0;    // no data at this point
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, campfire_curl_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&raw);
		
		res = curl_easy_perform(curl);

		purple_debug_info("campfire", "%lu bytes retrieved\n", (long)raw.size);

		if(raw.message)
		{
			purple_debug_info("campfire", "room list raw: %s", raw.message);
			xmlnode *node = xmlnode_from_str(raw.message, -1);
			gint len;
			gchar *str = xmlnode_to_str(node, &len);
			purple_debug_info("campfire", "room list: %s", str);
			g_free(str);
			g_free(raw.message);
			return node;
		}
	}

	return NULL;
}

void campfire_curl_room_query(CampfireConn *conn)
{
	campfire_curl_request(conn, "/rooms.xml", NULL);
}
*/
