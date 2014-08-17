#ifndef MESSAGE_H
#define MESSAGE_H

/* local includes */
#include "http.h"
#include "campfire.h"

/* system includes */
#include <glib/gi18n.h>
#include <time.h>

/* purple includes */
#include <xmlnode.h>
#include <plugin.h>
#include <cmds.h>

#define CAMPFIRE_MESSAGE_TEXT "TextMessage"
#define	CAMPFIRE_MESSAGE_PASTE "PasteMessage"
#define	CAMPFIRE_MESSAGE_SOUND "SoundMessage"
#define	CAMPFIRE_MESSAGE_TWEET "TweetMessage"
#define	CAMPFIRE_MESSAGE_ENTER "EnterMessage"
#define	CAMPFIRE_MESSAGE_LEAVE "LeaveMessage"
#define	CAMPFIRE_MESSAGE_TIME "TimestampMessage"
#define	CAMPFIRE_MESSAGE_KICK "KickMessage"
#define	CAMPFIRE_MESSAGE_UPLOAD "UploadMessage"
#define CAMPFIRE_MESSAGE_TOPIC "TopicChangeMessage"
#define CAMPFIRE_MESSAGE_GUESTALLOW "AllowGuestsMessage"
#define CAMPFIRE_MESSAGE_GUESTDENY "DisallowGuestsMessage"

#define CAMPFIRE_CMD_ME "me"
#define CAMPFIRE_CMD_PLAY "play"
/* not really commands but we'll implement them to emulate web interface */
#define CAMPFIRE_CMD_ROOM "room"
#define CAMPFIRE_CMD_TOPIC "topic"

typedef struct _CampfireMessage
{
	gchar *id;
	gchar *type;
	gchar *message;
	time_t time;
	gchar *user_id;
} CampfireMessage;

typedef struct _CampfireRoom
{
	gchar *id;
	gchar *name;
	gchar *last_message_id;
	GList *message_id_buffer;
} CampfireRoom;

void
campfire_message_send(CampfireConn *campfire, int id, const char *message, char *msg_type);

void
campfire_room_query(CampfireConn *campfire);

void
campfire_room_join(CampfireConn *campfire, gchar *room_id, gchar *room_name);

void
campfire_room_leave(CampfireConn *campfire, gint id);

PurpleCmdRet
campfire_parse_cmd(PurpleConversation *conv, const gchar *cmd,
				   gchar **args, gchar **error, void *data);

char *replace(char *orig, char *rep, char *with);

#endif /* not MESSAGE_H */

