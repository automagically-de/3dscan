#ifndef _CONFIG_H
#define _CONFIG_H

#include <glib.h>

typedef struct _Config Config;

Config *config_init(void);
gboolean config_save(Config *config);
void config_cleanup(Config *config);

gint32 config_get_int(Config *config, const gchar *section,
	const gchar *option, gint32 defval);
gboolean config_set_int(Config *config, const gchar *section,
	const gchar *option, gint32 value);
gchar *config_get_string(Config *config, const gchar *section,
	const gchar *option, const gchar *defval);
gboolean config_set_string(Config *config, const gchar *section,
	const gchar *option, const gchar *value);

#endif
