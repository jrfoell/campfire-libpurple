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
#define	CAMPFIRE_MESSAGE_UPLOAD "UploadMessage"


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
	GHashTable *rooms;
	GHashTable *users;
	guint message_timer;
} CampfireConn;

typedef struct _CampfireSslTransaction {
	CampfireConn *campfire;
	GString *http_request;
	GString *http_response;
	PurpleSslInputFunction response_cb;
	gpointer response_cb_data;
	xmlnode *xml_response;
	gint content_len;
	//optional
	gchar *room_id;
	GList *messages;
	gboolean first_check;
	gboolean my_message;
} CampfireSslTransaction;

typedef struct _CampfireMessage {
	gchar *id;
	gchar *type;
	gchar *message;
	time_t time;
	gchar *user_id;
} CampfireMessage;

typedef struct _CampfireRoom {
	gchar *id;
	gchar *name;
	gchar *last_message_id;
	GList *my_message_ids;
} CampfireRoom;

void campfire_message_send(CampfireConn *campfire, int id, const char *message);
void campfire_room_query(CampfireConn *campfire);
void campfire_room_join(CampfireConn *campfire, gchar *room_id, gchar *room_name);
void campfire_room_leave(CampfireConn *campfire, gint id);

//internal functions
void campfire_fetch_first_messages(CampfireConn *campfire, gchar *room_id);
void campfire_print_messages(CampfireSslTransaction *xaction, PurpleSslConnection *gsc, PurpleInputCondition cond);
void campfire_message_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc, PurpleInputCondition cond);

#endif /* not MESSAGE_H */

