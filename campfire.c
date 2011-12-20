//system includes
#include <glib/gi18n.h>

//purple includes
#include <prpl.h>
#include <notify.h>
#include <version.h>
#include <accountopt.h>
#include <xmlnode.h>
#include <debug.h>
//for connections et al.
#include <sslconn.h>
#include <proxy.h> 

//local includes
#include "message.h"

gboolean plugin_load(PurplePlugin *plugin)
{
	return TRUE;
}

gboolean plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static void campfire_login_callback(gpointer data, PurpleSslConnection *gsc, PurpleInputCondition cond)
{
	PurpleConnection *gc = (PurpleConnection *)data;	
	purple_connection_set_state(gc, PURPLE_CONNECTED);
	/*purple_ssl_close(gsc);*/
}

static void campfire_login(PurpleAccount *account)
{
	//don't really login (it's stateless), but init the CampfireConn
	PurpleConnection *gc = purple_account_get_connection(account);
	const char *username = purple_account_get_username(account);
	CampfireConn *conn;
	CampfireSslTransaction *xaction;
	char **userparts;

	conn = gc->proto_data = g_new0(CampfireConn, 1);
	conn->gc = gc;
	conn->account = account;
	
	xaction = g_new0(CampfireSslTransaction, 1);
	xaction->connect_cb = campfire_login_callback;
	xaction->connect_cb_data = gc;

	userparts = g_strsplit(username, "@", 2);
	purple_connection_set_display_name(gc, userparts[0]);
	conn->hostname = g_strdup(userparts[1]);
	g_strfreev(userparts);	

	campfire_renew_connection(conn, xaction);
}

static void campfire_close(PurpleConnection *gc)
{
}

static void campfire_buddy_free(PurpleBuddy * buddy)
{
}

static gchar * campfire_status_text(PurpleBuddy *buddy)
{
	return NULL;
}

static void campfire_set_status(PurpleAccount *acct, PurpleStatus *status)
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

static GList * campfire_statuses(PurpleAccount *acct)
{
	GList *types = NULL;
	PurpleStatusType *status;
	
	//Online people have a status message and also a date when it was set	
	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, NULL, _("Online"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	//Offline people dont have messages
	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, NULL, _("Offline"), TRUE, TRUE, FALSE);
	types = g_list_append(types, status);
	
	return types;

}

GList *campfire_chat_info(PurpleConnection *gc)
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

void campfire_join_chat(PurpleConnection *gc, GHashTable *data)
{
	CampfireConn *campfire = gc->proto_data;
	
	campfire->room_id = g_hash_table_lookup(data, "id");
	campfire->room_name = g_hash_table_lookup(data, "name");

	purple_debug_info("campfire", "trying to JOIN CHAT room id %s\n", campfire->room_id);
	
	campfire_room_join(campfire);
}

char *campfire_get_chat_name(GHashTable *data) {
	return g_strdup(g_hash_table_lookup(data, "room"));
}


PurpleRoomlist *campfire_roomlist_get_list(PurpleConnection *gc)
{	
	CampfireConn *campfire = gc->proto_data;
	GList *fields = NULL;
	PurpleRoomlistField *f;

	purple_debug_info("campfire", "initiating ROOMLIST GET LIST\n");

	/*if (campfire->roomlist)*/
	/*{*/
		/*purple_roomlist_unref(campfire->roomlist);*/
		/*if (campfire->roomlist->ref == 0) {*/
			/*campfire->roomlist = NULL;*/
		/*}*/
	/*}*/
	
	campfire->roomlist = purple_roomlist_new(purple_connection_get_account(gc));

	/*f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "room", TRUE);*/
	/*fields = g_list_append(fields, f);*/

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "id", TRUE);
	fields = g_list_append(fields, f);
	
	purple_roomlist_set_fields(campfire->roomlist, fields);

	purple_roomlist_set_in_progress(campfire->roomlist, TRUE);

	campfire_room_query(campfire);
	
	/*purple_roomlist_set_in_progress(campfire->roomlist, FALSE);*/
	//purple_roomlist_unref(campfire->roomlist);
	//campfire->roomlist = NULL;

	return campfire->roomlist;
}

void campfire_roomlist_cancel(PurpleRoomlist *list)
{
	PurpleConnection *gc = purple_account_get_connection(list->account);

	if (gc == NULL)
		return;

	CampfireConn *campfire = gc->proto_data;

	purple_roomlist_set_in_progress(list, FALSE);

	if (campfire->roomlist == list) {
		campfire->roomlist = NULL;
		purple_roomlist_unref(list);
	}
}


const char *campfireim_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "campfire";
}

static PurplePluginProtocolInfo campfire_protocol_info = {
	/* options */
	OPT_PROTO_NO_PASSWORD | OPT_PROTO_SLASH_COMMANDS_NATIVE,
	NULL,                   /* user_splits */
	NULL,                   /* protocol_options */
	{   /* icon_spec, a PurpleBuddyIconSpec */
		"png,jpg,gif",                   /* format */
		0,                               /* min_width */
		0,                               /* min_height */
		128,                             /* max_width */
		128,                             /* max_height */
		10000,                           /* max_filesize */
		PURPLE_ICON_SCALE_DISPLAY,       /* scale_rules */
	},
	campfireim_list_icon,   /* list_icon */
	NULL,                   /* list_emblems */
	campfire_status_text,   /* status_text */
	NULL,
	campfire_statuses,      /* status_types */
	NULL,                   /* blist_node_menu */
	campfire_chat_info,     /* chat_info */
	NULL,                   /* chat_info_defaults */
	campfire_login,       	/* login */
	campfire_close,       	/* close */
	NULL,     		        /* send_im */
	NULL,                   /* set_info */
	NULL,                   /* send_typing */
	NULL,                   /* get_info */
	campfire_set_status,    /* set_status */
	NULL,                   /* set_idle */
	NULL,                   /* change_passwd */
	NULL,                   /* add_buddy */
	NULL,                   /* add_buddies */
	NULL,                   /* remove_buddy */
	NULL,                   /* remove_buddies */
	NULL,                   /* add_permit */
	NULL,                   /* add_deny */
	NULL,                   /* rem_permit */
	NULL,                   /* rem_deny */
	NULL,                   /* set_permit_deny */
	campfire_join_chat,     /* join_chat */
	NULL,                   /* reject chat invite */
	campfire_get_chat_name, /* get_chat_name */
	NULL,                   /* chat_invite */
	NULL,                   /* chat_leave */
	NULL,                   /* chat_whisper */
	NULL,                   /* chat_send */
	NULL,                   /* keepalive */
	NULL,                   /* register_user */
	NULL,                   /* get_cb_info */
	NULL,                   /* get_cb_away */
	NULL,                   /* alias_buddy */
	NULL,                   /* group_buddy */
	NULL,                   /* rename_group */
	campfire_buddy_free,	/* buddy_free */
	NULL,                   /* convo_closed */
	purple_normalize_nocase,/* normalize */
	NULL,                   /* set_buddy_icon */
	NULL,                   /* remove_group */
	NULL,                   /* get_cb_real_name */
	NULL,                   /* set_chat_topic */
	NULL,                   /* find_blist_chat */
	campfire_roomlist_get_list,/* roomlist_get_list */
	campfire_roomlist_cancel,/* roomlist_cancel */
	NULL,                   /* roomlist_expand_category */
	NULL,                   /* can_receive_file */
	NULL,                   /* send_file */
	NULL,                   /* new_xfer */
	NULL,                   /* offline_message */
	NULL,                   /* whiteboard_prpl_ops */
	NULL,                   /* send_raw */
	NULL,                   /* roomlist_room_serialize */
	NULL,                   /* unregister_user */
	NULL,                   /* send_attention */
	NULL,                   /* attention_types */
	sizeof(PurplePluginProtocolInfo), /* struct_size */
	NULL, //campfire_get_account_text_table /* get_account_text_table */	
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,                             /* magic */
	PURPLE_MAJOR_VERSION,                            /* major_version */
	PURPLE_MINOR_VERSION,                            /* minor_version */
	PURPLE_PLUGIN_PROTOCOL,                          /* type */
	NULL,                                            /* ui_requirement */
	0,                                               /* flags */
	NULL,                                            /* dependencies */
	PURPLE_PRIORITY_DEFAULT,                         /* priority */
	"prpl-campfire",                                 /* id */
	"Campfire",                                      /* name */
	"0.1",                                           /* version */
	"Campfire Chat",                                 /* summary */
	"Campfire Chat Protocol Plugin",                 /* description */
	"Jake Foell <jfoell@gmail.com>",                 /* author */
	"https://github.com/jrfoell/campfire-libpurple", /* homepage */
	plugin_load,                                     /* load */
	plugin_unload,                                   /* unload */
	NULL,                                            /* destroy */
	NULL,                                            /* ui_info */
	&campfire_protocol_info,                         /* extra_info */
	NULL,                                            /* prefs_info */
	NULL,                                            /* actions */
	NULL,                                            /* padding... */
	NULL,
	NULL,
	NULL
};

static void plugin_init(PurplePlugin *plugin)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;

	split = purple_account_user_split_new(_("Hostname"), NULL, '@');
	campfire_protocol_info.user_splits = g_list_append(campfire_protocol_info.user_splits, split);

	option = purple_account_option_string_new(_("API token"), "api_token", NULL);
	campfire_protocol_info.protocol_options = g_list_append(campfire_protocol_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(campfire, plugin_init, info);

