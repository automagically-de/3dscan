#include <glib.h>
#include <glib/gstdio.h>

#include "config.h"

struct _Config {
	GKeyFile *keyfile;
};

Config *config_init(void)
{
	Config *config;
	gchar *path;

	config = g_new0(Config, 1);
	config->keyfile = g_key_file_new();

	path = g_strdup_printf("%s/.3dscan/config.ini", g_getenv("HOME"));
	g_key_file_load_from_file(config->keyfile, path, G_KEY_FILE_NONE, NULL);
	g_free(path);

	return config;
}

gboolean config_save(Config *config)
{
	gchar *path, *dirname, *fdata;
	gboolean retval;

	path = g_strdup_printf("%s/.3dscan/config.ini", g_getenv("HOME"));
	fdata = g_key_file_to_data(config->keyfile, NULL, NULL);
	dirname = g_path_get_dirname(path);
	g_mkdir(dirname, 0755);
	retval = g_file_set_contents(path, fdata, -1, NULL);
	if(!retval)
		g_warning("failed to save config");
	g_free(dirname);
	g_free(path);
	return retval;
}

void config_cleanup(Config *config)
{
	g_key_file_free(config->keyfile);
	g_free(config);
}

gint32 config_get_int(Config *config, const gchar *section,
	const gchar *option, gint32 defval)
{
	if(g_key_file_has_key(config->keyfile, section, option, NULL))
		return g_key_file_get_integer(config->keyfile, section, option, NULL);
	else {
		g_key_file_set_integer(config->keyfile, section, option, defval);
		return defval;
	}
}

gboolean config_set_int(Config *config, const gchar *section,
	const gchar *option, gint32 value)
{
	g_key_file_set_integer(config->keyfile, section, option, value);
	return TRUE;
}

gchar *config_get_string(Config *config, const gchar *section,
	const gchar *option, const gchar *defval)
{
	if(g_key_file_has_key(config->keyfile, section, option, NULL))
		return g_key_file_get_string(config->keyfile, section, option, NULL);
	else {
		g_key_file_set_string(config->keyfile, section, option, defval);
		return g_strdup(defval);
	}
}

gboolean config_set_string(Config *config, const gchar *section,
	const gchar *option, const gchar *value)
{
	g_key_file_set_string(config->keyfile, section, option, value);
	return TRUE;
}

