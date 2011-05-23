//system includes
#include <string.h>
#include <curl/curl.h>

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

void campfire_room_query(CampfireConn *campfire)
{
	
	PurpleRoomlistRoom *room;
	gchar room_request[] =
		"GET /rooms.xml HTTP/1.1\r\n"
#if 0
		"User-Agent: curl/7.21.6 (x86_64-pc-linux-gnu) libcurl/7.21.6 OpenSSL/1.0.0d zlib/1.2.3.4 libidn/1.20 libssh2/1.2.8 librtmp/2.3\r\n"
#endif
		"Authorization: Basic MzU4NDUxN2U3ZTFiYTk3MzEzNzcxYjVmYWM4NmY3YTRkMzQ4NDFiYzpY\r\n"
		"Host: ingroup.campfirenow.com\r\n"
		"Accept: */*\r\n"
		"\r\n"
	;

	purple_ssl_write(campfire->gsc,room_request, strlen(room_request));
	purple_debug_info("campfire", "HTTP request:\n%s\n", room_request);
	room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, "test1", NULL);
	purple_roomlist_room_add_field(campfire->roomlist, room, "hahaha");
	purple_roomlist_room_add(campfire->roomlist, room);
	//@TODO do some curl/xml stuff here
	//see
	//protocols/jabber/chat.c:864 roomlist_ok_cb() AND
	//protocols/jabber/chat.c:800 roomlist_disco_result_cb()
}

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

gchar *campfire_get_url(CampfireConn *conn)
{
	GString *url = g_string_new("https://");
												
	g_string_append(url, conn->hostname);
	purple_debug_info("campfire", "Connect base url: %s\n", url->str);
	return g_string_free(url, FALSE);
}

gsize campfire_curl_write_callback(void *ptr, gsize size, gsize nmemb, void *data)
{
  gsize realsize = size * nmemb;
  CampfireRawMessage *raw = (CampfireRawMessage *)data;

  raw->message = g_try_realloc(raw->message, raw->size + realsize + 1);
  if (raw->message == NULL) {
    /* out of memory! */
	purple_debug_info("campfire", "not enough memory for curl response (g_try_realloc returned NULL)\n");
	/* @TODO make sure this a good return value */
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

		raw.message = g_malloc(1);  /* will be grown as needed by the realloc above */
		raw.size = 0;    /* no data at this point */
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

