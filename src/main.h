#ifndef _MAIN_H
#define _MAIN_H

#include "v4l2.h"
#include "gui.h"
#include "config.h"

typedef struct {
	V4l2Data *v4l2;
	GuiData *gui;
	Config *config;

	GSList *regions;
	gboolean valid_dir;

	guint32 n_vert_y;
	guint32 n_bits;
	guint8 *bits;

	guint8 *angle_scans;
	gfloat *angle_verts;
} G3DScanner;

#endif
