#ifndef MESSAGE_H
#define MESSAGE_H

//local includes
#include "http.h"
#include "campfire.h"

//system includes
#include <glib/gi18n.h>
#include <time.h>

//purple includes
#include <xmlnode.h>
#include <plugin.h>

#define CAMPFIRE_MESSAGE_TEXT "TextMessage"
#define	CAMPFIRE_MESSAGE_PASTE "PasteMessage"
#define	CAMPFIRE_MESSAGE_SOUND "SoundMessage"
#define	CAMPFIRE_MESSAGE_TWEET "TweetMessage"
#define	CAMPFIRE_MESSAGE_ENTER "EnterMessage"
#define	CAMPFIRE_MESSAGE_LEAVE "LeaveMessage"
#define	CAMPFIRE_MESSAGE_TIME "TimestampMessage"
#define	CAMPFIRE_MESSAGE_KICK "KickMessage"
#define	CAMPFIRE_MESSAGE_UPLOAD "UploadMessage"

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

