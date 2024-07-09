#ifndef _CONFIG_H_
#define _CONFIG_H_

extern GKeyFile  *config;

void load_config_settings(GString **confpath);
void save_config_settings(GString *confpath);

#endif
