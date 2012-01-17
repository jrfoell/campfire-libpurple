#ifndef MESSAGE_H
#define MESSAGE_H

//purple includes
#include <plugin.h>
#include <roomlist.h>

#define CAMPFIRE_MESSAGE_TEXT "TextMessage"
#define	CAMPFIRE_MESSAGE_PASTE "PasteMessage"
#define	CAMPFIRE_MESSAGE_SOUND "SoundMessage"
#define	CAMPFIRE_MESSAGE_TWEET "TweetMessage"
#define	CAMPFIRE_MESSAGE_ENTER "EnterMessage"
#define	CAMPFIRE_MESSAGE_LEAVE "LeaveMessage"
#define	CAMPFIRE_MESSAGE_TIME "TimestampMessage"
#define	CAMPFIRE_MESSAGE_KICK "KickMessage"

typedef struct _CampfireMessage {
	gchar *type;
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
	guint message_timer;	
} CampfireConn;

typedef struct _CampfireSslTransaction {
	CampfireConn *campfire;
	PurpleSslInputFunction connect_cb;
	gpointer connect_cb_data;
	GString *http_request;
	GString *http_response;
	PurpleSslInputFunction response_cb;
	gpointer response_cb_data;
} CampfireSslTransaction;

typedef struct _CampfireRawMessage {
  gchar *message;
  gsize size;
} CampfireRawMessage;


void campfire_renew_connection(CampfireSslTransaction *xaction);
void campfire_message_send(CampfireMessage *cm);
void campfire_room_query(CampfireConn *campfire);
void campfire_room_join(CampfireConn *campfire);
void campfire_curl_room_query(CampfireConn *campfire);

//internal functions
void campfire_fetch_first_messages(CampfireConn *conn);

#endif /* not MESSAGE_H */

