#ifndef _GUI_H
#define _GUI_H

#include <gtk/gtk.h>

#include "region.h"
#include "config.h"

typedef struct {
	Config *config;
	GtkWindow *window;
	GtkWidget *image;
	GtkWidget *l_angle;
	GSList *regions;
	RegionType region_selector;
} GuiData;

GuiData *gui_init(Config *config);
void gui_show(GuiData *data);
void gui_set_quit_handler(GuiData *data, GCallback quit, gpointer user_data);
void gui_set_regions(GuiData *data, GSList *regions);
void gui_set_image(GuiData *data, GdkPixbuf *pixbuf);
void gui_set_angle(GuiData *data, const gchar *s);
void gui_cleanup(GuiData *data);

#endif
