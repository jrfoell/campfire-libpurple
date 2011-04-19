#include <prpl.h>
#include <version.h>
#include <glib/gi18n.h>
#include "campfire.h"

static void plugin_init(PurplePlugin *plugin)
{
}

gboolean plugin_load(PurplePlugin *plugin)
{
	campfire_plugin_init(plugin);

	return TRUE;
}

gboolean plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static void campfire_login(PurpleAccount *acct)
{
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

static PurplePluginProtocolInfo campfire_protocol_info = {
	/* options */
	OPT_PROTO_UNIQUE_CHATNAME,
	NULL,                   /* user_splits */
	NULL,                   /* protocol_options */
	//NO_BUDDY_ICONS          /* icon_spec */
	{   /* icon_spec, a PurpleBuddyIconSpec */
		"png,jpg,gif",                   /* format */
		0,                               /* min_width */
		0,                               /* min_height */
		50,                             /* max_width */
		50,                             /* max_height */
		10000,                           /* max_filesize */
		PURPLE_ICON_SCALE_DISPLAY,       /* scale_rules */
	},
	NULL,   /* list_icon */
	NULL,                   /* list_emblems */
	campfire_status_text, /* status_text */
	NULL,
	campfire_statuses,    /* status_types */
	NULL,                   /* blist_node_menu */
	NULL,                   /* chat_info */
	NULL,                   /* chat_info_defaults */
	campfire_login,       		/* login */
	campfire_close,       		/* close */
	NULL,     		/* send_im */
	NULL,                   /* set_info */
	NULL,
	NULL,
	campfire_set_status,/* set_status */
	NULL,                   /* set_idle */
	NULL,                   /* change_passwd */
	NULL,
	NULL,                   /* add_buddies */
	NULL,
	NULL,                   /* remove_buddies */
	NULL,                   /* add_permit */
	NULL,                   /* add_deny */
	NULL,                   /* rem_permit */
	NULL,                   /* rem_deny */
	NULL,                   /* set_permit_deny */
	NULL,                   /* join_chat */
	NULL,                   /* reject chat invite */
	NULL,                   /* get_chat_name */
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
	NULL,                   /* roomlist_get_list */
	NULL,                   /* roomlist_cancel */
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
	sizeof(PurplePluginProtocolInfo) /* struct_size */
};

static GList * campfire_actions(PurplePlugin *plugin, gpointer context)
{
	return NULL;
}

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD, /* type */
	NULL, /* ui_requirement */
	0, /* flags */
	NULL, /* dependencies */
	PURPLE_PRIORITY_DEFAULT, /* priority */
	"prpl-campfire", /* id */
	"Campfire", /* name */
	"0.1", /* version */
	"Campfire Chat", /* summary */
	"Campfire Chat Protocol Plugin", /* description */
	"Jake Foell <jfoell@gmail.com>", /* author */
	"https://github.com/jrfoell/campfire-libpurple", /* homepage */
	plugin_load, /* load */
	plugin_unload, /* unload */
	NULL, /* destroy */
	NULL, /* ui_info */
	&campfire_protocol_info, /* extra_info */
	NULL, /* prefs_info */
	campfire_actions, /* actions */
	NULL, /* padding */
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(campfire, plugin_init, info);
