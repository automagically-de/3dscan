#ifndef _GUI_H
#define _GUI_H

#include <gtk/gtk.h>

#include "region.h"
#include "config.h"
#include "model.h"

typedef struct _GuiData GuiData;

GuiData *gui_init(Config *config, Model *model);
void gui_show(GuiData *data);
void gui_update(GuiData *data);
void gui_set_scan_progress(GuiData *data, guint32 angle, guint32 n_scans);
void gui_set_quit_handler(GuiData *data, GCallback quit, gpointer user_data);
void gui_set_image(GuiData *data, GdkPixbuf *pixbuf);
void gui_set_angle(GuiData *data, const gchar *s);
void gui_cleanup(GuiData *data);

#endif
