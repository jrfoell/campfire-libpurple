#ifndef MESSAGE_H
#define MESSAGE_H
#include "plugin.h"
#include "roomlist.h"

typedef struct _CampfireMessage {
	enum {
		CAMPFIRE_MESSAGE_TEXT,
		CAMPFIRE_MESSAGE_PASTE,
		CAMPFIRE_MESSAGE_SOUND,
		CAMPFIRE_MESSAGE_TWEET
	} type;
	char *body;
} CampfireMessage;


typedef struct _CampfireConn {
	PurpleRoomlist *roomlist;
} CampfireConn;

void campfire_plugin_init(PurplePlugin *plugin);
GList *campfire_chat_info(PurpleConnection *gc);
PurpleRoomlist *campfire_roomlist_get_list(PurpleConnection *gc);
void campfire_roomlist_cancel(PurpleRoomlist *list);

#endif /* not MESSAGE_H */

