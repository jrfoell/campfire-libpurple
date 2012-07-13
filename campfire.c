
/*local includes*/
#include "campfire.h"
#include "message.h"

/*purple includes*/
#include <version.h>
#include <accountopt.h>
#include <debug.h>
#include <cmds.h>


gboolean
plugin_load(G_GNUC_UNUSED PurplePlugin * plugin)
{
	return TRUE;
}

gboolean
plugin_unload(G_GNUC_UNUSED PurplePlugin * plugin)
{
	return TRUE;
}

static void
campfire_login(PurpleAccount * account)
{
	/*don't really login (it's stateless), but init the CampfireConn */
	PurpleConnection *gc = purple_account_get_connection(account);
	const char *username = purple_account_get_username(account);
	CampfireConn *conn;
	char *pos;
	PurpleCmdFlag f = PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PRPL_ONLY;
	gchar *prpl_id = "prpl-analog_g-campfire";	/* analog_g = developer.pidgin.im Trac username */

	conn = g_new0(CampfireConn, 1);
	purple_debug_info("campfire", "num_xaction_malloc:%d: num_xaction_free:%d\n",
	                  conn->num_xaction_malloc,
	                  conn->num_xaction_free);
	conn->gc = gc;
	conn->account = account;

        /* Find the last '@'; usernames can have '@' in them. */
	pos = strrchr(username, '@');
	conn->hostname = g_strdup(pos+1);
	pos[0] = 0;
	purple_connection_set_display_name(gc, username);
	pos[0] = '@';

	purple_debug_info("campfire", "username: %s\n", username);
	purple_debug_info("campfire", "hostname: %s\n", conn->hostname);

	gc->proto_data = conn;

	/*register campfire commands */
	purple_cmd_register(CAMPFIRE_CMD_ME, "s", PURPLE_CMD_P_PRPL, f, prpl_id,
			    campfire_parse_cmd,
			    "me &lt;action to perform&gt;:  Perform an action.",
			    conn);
	purple_cmd_register(CAMPFIRE_CMD_TOPIC, "s", PURPLE_CMD_P_PRPL,
			    f | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, prpl_id,
			    campfire_parse_cmd,
			    "topic &lt;new topic&gt;:  Change the room topic.",
			    conn);
	purple_cmd_register(CAMPFIRE_CMD_ROOM, "s", PURPLE_CMD_P_PRPL, f,
			    prpl_id, campfire_parse_cmd,
			    "room &lt;new room name&gt;:  Change the room name (admin only).",
			    conn);
	purple_cmd_register(CAMPFIRE_CMD_PLAY, "w", PURPLE_CMD_P_PRPL, f,
			    prpl_id, campfire_parse_cmd,
			    "play &lt;sound&gt;:  Play a sound (trombone, rimshot, crickets, live).",
			    conn);

	purple_connection_set_state(gc, PURPLE_CONNECTED);
}

static void
campfire_close(G_GNUC_UNUSED PurpleConnection * gc)
{
}

static void
campfire_buddy_free(G_GNUC_UNUSED PurpleBuddy * buddy)
{
}

static gchar *
campfire_status_text(G_GNUC_UNUSED PurpleBuddy * buddy)
{
	return NULL;
}

static void
campfire_set_status(G_GNUC_UNUSED PurpleAccount * acct,
		    G_GNUC_UNUSED PurpleStatus * status)
{
}

/*
static GHashTable * campfire_get_account_text_table(PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("API token"));
	return table;
}
*/

static GList *
campfire_statuses(G_GNUC_UNUSED PurpleAccount * acct)
{
	GList *types = NULL;
	PurpleStatusType *status;

	/*Online people have a status message and also a date when it was set */
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL,
					     _("Online"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	/*Offline people dont have messages */
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL,
					     _("Offline"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	return types;

}

GList *
campfire_chat_info(G_GNUC_UNUSED PurpleConnection * gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "room";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	return m;
}

void
campfire_chat_leave(PurpleConnection * gc, int id)
{
	purple_debug_info("campfire", "leaving CHAT room id %d\n", id);

	campfire_room_leave(gc->proto_data, id);
}

char *
campfire_get_chat_name(GHashTable * data)
{
	return g_strdup(g_hash_table_lookup(data, "room"));
}


PurpleRoomlist *
campfire_roomlist_get_list(PurpleConnection * gc)
{
	CampfireConn *campfire = gc->proto_data;
	GList *fields = NULL;
	PurpleRoomlistField *f;

	purple_debug_info("campfire", "initiating ROOMLIST GET LIST\n");

	if (campfire->roomlist) {
		purple_roomlist_unref(campfire->roomlist);
	}

	campfire->roomlist =
		purple_roomlist_new(purple_connection_get_account(gc));

	/*f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "room", TRUE); */
	/*fields = g_list_append(fields, f); */

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Topic"),
				      "topic", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "id",
				      TRUE);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(campfire->roomlist, fields);

	purple_roomlist_set_in_progress(campfire->roomlist, TRUE);

	campfire_room_query(campfire);

	/*purple_roomlist_set_in_progress(campfire->roomlist, FALSE); */
	/*purple_roomlist_unref(campfire->roomlist); */
	/*campfire->roomlist = NULL; */

	return campfire->roomlist;
}

void
campfire_roomlist_cancel(PurpleRoomlist * list)
{
	PurpleConnection *gc = purple_account_get_connection(list->account);
	CampfireConn *campfire = NULL;

	if (gc == NULL)
		return;

	campfire = gc->proto_data;

	purple_roomlist_set_in_progress(list, FALSE);

	if (campfire->roomlist == list) {
		campfire->roomlist = NULL;
		purple_roomlist_unref(list);
	}
}

void campfire_print_key(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	gchar *key = data;
	purple_debug_info("campfire", "key: %s\n", key);
}

void campfire_print_field_name(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	PurpleRoomlistField *field = data;
	purple_debug_info("campfire", "field: %s\n", field->name);
}

void
campfire_join_chat_after_room_query(CampfireConn *campfire, gchar *room_name)
{
	/*alternate when not using "Room List" */
	/* @TODO: error checking may be too simple */
	GList *fields;
	GList *rooms;
	PurpleRoomlistRoom *r;
	PurpleRoomlistField *f;
	gsize i;
	gsize list_size;
	gsize id_field_index = 0;
	gboolean found = FALSE;
	gchar *id   = NULL;
	gchar *name = NULL;
	gboolean room_name_error = FALSE;

	fields = purple_roomlist_get_fields(campfire->roomlist);
	g_list_foreach(fields, campfire_print_field_name, NULL);
	list_size = g_list_length(fields);

	/* find "id" field */
	for (i = 0; i < list_size; i++) {
		f = g_list_nth_data(fields, i);
		purple_debug_info("campfire", "field name: %s\n", f->name);
		if (strcmp("id", f->name) == 0) {
			id_field_index = i;
			found = TRUE;
			break;
		}
	}
	if (!found) {
		room_name_error = TRUE;
		purple_notify_message(campfire->gc, PURPLE_NOTIFY_MSG_ERROR,
		                      "campfire error",
		                      "couldn't find 'id' field in roomlist.",
		                      NULL,
		                      NULL,
		                      NULL);
	} else {
		found = FALSE;
	}
	rooms = campfire->roomlist->rooms;
	list_size = g_list_length(rooms);
	purple_debug_info("campfire", "join debug 6\n");

	/* find typed/chosen room name in available room names */
	for (i = 0; i < list_size; i++) {
		r = (PurpleRoomlistRoom *)g_list_nth_data(rooms, i);
		if (strcmp(r->name, room_name) == 0) {
			purple_debug_info("campfire", "room found\n");
			purple_debug_info("campfire", "room desired: %s\n", room_name);
			purple_debug_info("campfire", "room found: %s\n", r->name);
			id = g_list_nth_data(r->fields, id_field_index);
			name = r->name;
			found = TRUE;
			break;
		}
	}

	if (!found) {
		room_name_error = TRUE;
		purple_notify_message(campfire->gc, PURPLE_NOTIFY_MSG_ERROR,
		                      "campfire error",
		                      "couldn't find room name in roomlist.",
		                      room_name,
		                      NULL,
		                      NULL);
	}
	if (!room_name_error) {
		purple_debug_info("campfire", "trying to JOIN CHAT room id %s\n", id);
		campfire_room_join(campfire, id, name);
	}
}

void
campfire_join_chat(PurpleConnection * gc, GHashTable * data)
{
	GList *hash_keys = NULL;
	CampfireConn *campfire = gc->proto_data;
	gchar *desired_name = NULL;
	gchar *id   = NULL;
	gchar *name = NULL;
	gboolean room_name_error = FALSE;


	purple_debug_info("campfire", "1: %p\n", data);
	desired_name = g_hash_table_lookup(data, "room");
	purple_debug_info("campfire", "2\n");
	if (desired_name) {
		campfire->desired_room = g_strdup(desired_name);
		campfire->needs_join = TRUE;
		hash_keys = g_hash_table_get_keys(data);
		g_list_foreach(hash_keys, campfire_print_key, NULL);
		/* do this if you haven't done a room query yet */
		if (!campfire->roomlist) {
			campfire_roomlist_get_list(gc);
		} else {
			campfire_join_chat_after_room_query(campfire, campfire->desired_room);
		}
	} else {
		id   = g_hash_table_lookup(data,  "id");
		name = g_hash_table_lookup(data, "name");
		if (!id || !name) {
			room_name_error = TRUE;
			purple_notify_message(campfire->gc, PURPLE_NOTIFY_MSG_ERROR,
			                      "campfire error",
			                      "hash table error.",
			                      NULL,
			                      NULL,
			                      NULL);
		}
		if (!room_name_error) {
			purple_debug_info("campfire", "trying to JOIN CHAT room id %s\n", id);
			campfire_room_join(campfire, id, name);
		}
	}
}


const char *
campfireim_list_icon(G_GNUC_UNUSED PurpleAccount * account,
		     G_GNUC_UNUSED PurpleBuddy * buddy)
{
	return "campfire";
}

int
campfire_chat_send(PurpleConnection * gc, int id, const char *message,
		   G_GNUC_UNUSED PurpleMessageFlags flags)
{
	campfire_message_send(gc->proto_data, id, message,
			      CAMPFIRE_MESSAGE_TEXT);
	return 1;
}

static PurplePluginProtocolInfo campfire_protocol_info = {
	/* options */
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD,	/*| OPT_PROTO_SLASH_COMMANDS_NATIVE, */
	NULL,			/* user_splits */
	NULL,			/* protocol_options */
	{			/* icon_spec, a PurpleBuddyIconSpec */
	 "png,jpg,gif",		/* format */
	 0,			/* min_width */
	 0,			/* min_height */
	 128,			/* max_width */
	 128,			/* max_height */
	 10000,			/* max_filesize */
	 PURPLE_ICON_SCALE_DISPLAY,	/* scale_rules */
	 },
	campfireim_list_icon,	/* list_icon */
	NULL,			/* list_emblems */
	campfire_status_text,	/* status_text */
	NULL,
	campfire_statuses,	/* status_types */
	NULL,			/* blist_node_menu */
	campfire_chat_info,	/* chat_info */
	NULL,			/* chat_info_defaults */
	campfire_login,		/* login */
	campfire_close,		/* close */
	NULL,			/* send_im */
	NULL,			/* set_info */
	NULL,			/* send_typing */
	NULL,			/* get_info */
	campfire_set_status,	/* set_status */
	NULL,			/* set_idle */
	NULL,			/* change_passwd */
	NULL,			/* add_buddy */
	NULL,			/* add_buddies */
	NULL,			/* remove_buddy */
	NULL,			/* remove_buddies */
	NULL,			/* add_permit */
	NULL,			/* add_deny */
	NULL,			/* rem_permit */
	NULL,			/* rem_deny */
	NULL,			/* set_permit_deny */
	campfire_join_chat,	/* join_chat */
	NULL,			/* reject chat invite */
	campfire_get_chat_name,	/* get_chat_name */
	NULL,			/* chat_invite */
	campfire_chat_leave,	/* chat_leave */
	NULL,			/* chat_whisper */
	campfire_chat_send,	/* chat_send */
	NULL,			/* keepalive */
	NULL,			/* register_user */
	NULL,			/* get_cb_info */
	NULL,			/* get_cb_away */
	NULL,			/* alias_buddy */
	NULL,			/* group_buddy */
	NULL,			/* rename_group */
	campfire_buddy_free,	/* buddy_free */
	NULL,			/* convo_closed */
	purple_normalize_nocase,	/* normalize */
	NULL,			/* set_buddy_icon */
	NULL,			/* remove_group */
	NULL,			/* get_cb_real_name */
	NULL,			/* set_chat_topic */
	NULL,			/* find_blist_chat */
	campfire_roomlist_get_list,	/* roomlist_get_list */
	campfire_roomlist_cancel,	/* roomlist_cancel */
	NULL,			/* roomlist_expand_category */
	NULL,			/* can_receive_file */
	NULL,			/* send_file */
	NULL,			/* new_xfer */
	NULL,			/* offline_message */
	NULL,			/* whiteboard_prpl_ops */
	NULL,			/* send_raw */
	NULL,			/* roomlist_room_serialize */
	NULL,			/* unregister_user */
	NULL,			/* send_attention */
	NULL,			/* attention_types */
	sizeof(PurplePluginProtocolInfo),	/* struct_size */
	NULL,			/*campfire_get_account_text_table *//* get_account_text_table */
	NULL,			/* initiate_media */
	NULL,			/* get_media_caps */
#if PURPLE_MAJOR_VERSION > 1
#if PURPLE_MINOR_VERSION > 6
	NULL,			/* get_moods */
	NULL,			/* set_public_alias */
	NULL,			/* get_public_alias */
#if PURPLE_MINOR_VERSION > 7
	NULL,			/* add_buddy_with_invite */
	NULL,			/* add_buddies_with_invite */
#endif /* PURPLE_MINOR_VERSION > 7 */
#endif /* PURPLE_MINOR_VERSION > 6 */
#endif /* PURPLE_MAJOR_VERSION > 1 */
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,	/* magic */
	PURPLE_MAJOR_VERSION,	/* major_version */
	PURPLE_MINOR_VERSION,	/* minor_version */
	PURPLE_PLUGIN_PROTOCOL,	/* type */
	NULL,			/* ui_requirement */
	0,			/* flags */
	NULL,			/* dependencies */
	PURPLE_PRIORITY_DEFAULT,	/* priority */
	"prpl-analog_g-campfire",	/* id */
	"Campfire",		/* name */
	"0.1",			/* version */
	"Campfire Chat",	/* summary */
	"Campfire Chat Protocol Plugin",	/* description */
	"Jake Foell <jfoell@gmail.com>",	/* author */
	"https://github.com/jrfoell/campfire-libpurple",	/* homepage */
	plugin_load,		/* load */
	plugin_unload,		/* unload */
	NULL,			/* destroy */
	NULL,			/* ui_info */
	&campfire_protocol_info,	/* extra_info */
	NULL,			/* prefs_info */
	NULL,			/* actions */
	NULL,			/* padding... */
	NULL,
	NULL,
	NULL
};

static void
plugin_init(G_GNUC_UNUSED PurplePlugin * plugin)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option_token, *option_limit;

	split = purple_account_user_split_new(_("Hostname"), NULL, '@');
	campfire_protocol_info.user_splits =
		g_list_append(campfire_protocol_info.user_splits, split);

	option_token =
		purple_account_option_string_new(_("API token"), "api_token",
						 NULL);
	campfire_protocol_info.protocol_options =
		g_list_append(campfire_protocol_info.protocol_options,
			      option_token);

	option_limit =
		purple_account_option_int_new(_("Retrieve # msgs on join"),
					      "limit", 10);
	campfire_protocol_info.protocol_options =
		g_list_append(campfire_protocol_info.protocol_options,
			      option_limit);
}

PURPLE_INIT_PLUGIN(campfire, plugin_init, info);
