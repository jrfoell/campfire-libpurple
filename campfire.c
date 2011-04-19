#include "campfire.h"
#include "notify.h"

void campfire_plugin_init(PurplePlugin *plugin)
{
	purple_notify_message(plugin, PURPLE_NOTIFY_MSG_INFO, "Hello World!",
                        "This is the Hello World! plugin :)", NULL, NULL, NULL);
}

