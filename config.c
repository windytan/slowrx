#include <stdio.h>
#include <glib.h>
#include "config.h"

GKeyFile    *config          = NULL;

void load_config_settings(GString **confpath) {
  const gchar *confdir;

  // Load config
  confdir  = g_get_user_config_dir();
  *confpath = g_string_new(confdir);
  g_string_append(*confpath, "/slowrx.ini");

  config = g_key_file_new();
  if (g_key_file_load_from_file(config, (*confpath)->str, G_KEY_FILE_KEEP_COMMENTS, NULL)) {

  } else {
    printf("No valid config file found\n");
    g_key_file_load_from_data(config, "[slowrx]\ndevice=default", -1, G_KEY_FILE_NONE, NULL);
  }
}

void save_config_settings(GString *confpath) {
  FILE        *ConfFile;
  gchar       *confdata;
  gsize       *keylen=NULL;

  // Save config on exit
  ConfFile = fopen(confpath->str,"w");
  if (ConfFile == NULL) {
    perror("Unable to open config file for writing");
  } else {
    confdata = g_key_file_to_data(config,keylen,NULL);
    fprintf(ConfFile,"%s",confdata);
    fwrite(confdata,1,(size_t)keylen,ConfFile);
    fclose(ConfFile);
  }

}
