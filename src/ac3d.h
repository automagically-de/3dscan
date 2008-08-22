#ifndef _AC3D_H
#define _AC3D_H

#include <glib.h>

gboolean ac3d_write(const gchar *filename, gfloat *angle_verts,
	guint32 n_angles, guint32 n_vert_y, guint32 height);

#endif
