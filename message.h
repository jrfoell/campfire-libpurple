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


typedef struct _CampfireConn {
	PurpleAccount *account;
	PurpleRoomlist *roomlist;	
	PurpleConnection *gc;
	PurpleSslConnection *gsc;
	gchar *hostname;
} CampfireConn;


typedef struct _CampfireRawMessage {
  gchar *message;
  gsize size;
} CampfireRawMessage;


void campfire_renew_connection(CampfireConn *conn, void *callback);
void campfire_message_send(CampfireMessage *cm);
void campfire_room_query(CampfireConn *campfire);
void campfire_room_join(CampfireConn *campfire, char *id, char *room);
void campfire_curl_room_query(CampfireConn *campfire);

#endif /* not MESSAGE_H */

