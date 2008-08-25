#ifndef _SCAN_H
#define _SCAN_H

#include "model.h"

gboolean scan_update_bits(Model *model, GdkPixbuf *pixbuf);
gboolean scan_binarize_object_region(Model *model, GdkPixbuf *pixbuf);
gboolean scan_colors(Model *model, GdkPixbuf *pixbuf, guint32 angle);
gboolean scan_angle(Model *model, GdkPixbuf *pixbuf, guint32 angle);

#endif
