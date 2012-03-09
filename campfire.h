#ifndef CAMPFIRE_H
#define CAMPFIRE_H

/*system includes*/
#include <glib/gi18n.h>

/*purple includes*/
#include <plugin.h>
#include <prpl.h>

typedef struct _CampfireConn
{
	PurpleAccount *account;
	PurpleRoomlist *roomlist;
	PurpleConnection *gc;
	PurpleSslConnection *gsc;
	gchar *hostname;
	GHashTable *rooms;
	GHashTable *users;
	guint message_timer;
	GList *queue;
} CampfireConn;

#endif /* not CAMPFIRE_H */
