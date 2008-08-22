#ifndef _SCAN_H
#define _SCAN_H

#include "main.h"

gboolean scan_update_bits(G3DScanner *scanner, GdkPixbuf *pixbuf);
gboolean scan_binarize_object_region(G3DScanner *scanner, GdkPixbuf *pixbuf);
gboolean scan_colors(G3DScanner *scanner, GdkPixbuf *pixbuf, guint32 angle);
gboolean scan_angle(G3DScanner *scanner, GdkPixbuf *pixbuf, guint32 angle);

#endif
