
static void plugin_init(PurplePlugin *plugin)
{
}

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL, /* type */
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
	&dummy_prpl_info, /* extra_info */
	NULL, /* prefs_info */
	dummy_actions, /* actions */
	NULL, /* padding */
	NULL,
	NULL,
	NULL
};

PURPLE_INIT_PLUGIN(campfire, plugin_init, info);
