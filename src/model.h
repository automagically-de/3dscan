#ifndef _MODEL_H
#define _MODEL_H

#include "config.h"

typedef struct {
	GSList *regions;
	gboolean valid_dir;

	guint32 n_vert_y;
	guint32 n_bits;
	guint8 *bits;

	guint8 *angle_scans;
	gfloat *angle_verts;
	guint8 *angle_colors;
} Model;

Model *model_init(Config *config);
void model_cleanup(Model *model);
void model_rebuild_storage(Model *model);
gboolean model_save_config(Model *model, Config *config);
gboolean model_save(Model *model, const gchar *filename);

#endif
