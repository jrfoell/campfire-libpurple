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
	guint num_xaction_malloc; /* valgrind investigation */
	guint num_xaction_free;   /* valgrind investigation */
	gboolean needs_join;
	GHashTable *desired_room;
} CampfireConn;

void campfire_join_chat(PurpleConnection * gc, GHashTable * data);

#endif /* not CAMPFIRE_H */
