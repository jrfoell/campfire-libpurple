//system includes
#include <xmlnode.h>
#include <string.h>

//purple includes
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
	//@TODO do some curl/xml stuff here
	//see
	//protocols/jabber/chat.c:864 roomlist_ok_cb() AND
	//protocols/jabber/chat.c:800 roomlist_disco_result_cb()
}
