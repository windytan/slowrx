#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <glib.h>
#include <glib/gtypes.h>

extern GKeyFile  *config;

void load_config_settings(GString **confpath);
void save_config_settings(GString *confpath);

#endif
