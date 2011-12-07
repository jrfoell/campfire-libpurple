#ifndef MESSAGE_H
#define MESSAGE_H

//purple includes
#include <plugin.h>
#include <roomlist.h>

typedef struct _CampfireMessage {
	enum {
		CAMPFIRE_MESSAGE_TEXT,
		CAMPFIRE_MESSAGE_PASTE,
		CAMPFIRE_MESSAGE_SOUND,
		CAMPFIRE_MESSAGE_TWEET
	} type;
	gchar *body;
} CampfireMessage;

enum http_response_status {
	CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK,
	CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML,
	CAMPFIRE_HTTP_RESPONSE_STATUS_TRY_AGAIN,
	CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION,
	CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED,
	CAMPFIRE_HTTP_RESPONSE_STATUS_NO_CONTENT,
	CAMPFIRE_HTTP_RESPONSE_STATUS_NO_XML,
};

typedef struct _CampfireConn {
	PurpleAccount *account;
	PurpleRoomlist *roomlist;
	PurpleConnection *gc;
	PurpleSslConnection *gsc;
	gchar *hostname;
	gchar *room_id;
	gchar *room_name;
} CampfireConn;

typedef struct _CampfireNewConnectionCrap {
	PurpleSslInputFunction connect_cb;
	gpointer connect_cb_data;
	GString *http_request;
	PurpleSslInputFunction response_cb;
	gpointer response_cb_data;
} CampfireNewConnectionCrap;

typedef struct _CampfireRawMessage {
  gchar *message;
  gsize size;
} CampfireRawMessage;


void campfire_renew_connection(CampfireConn *conn, CampfireNewConnectionCrap *crap);
void campfire_message_send(CampfireMessage *cm);
void campfire_room_query(CampfireConn *campfire);
void campfire_room_join(CampfireConn *campfire);
void campfire_curl_room_query(CampfireConn *campfire);

#endif /* not MESSAGE_H */

