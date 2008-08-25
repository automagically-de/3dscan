#include "model.h"
#include "config.h"
#include "region.h"
#include "ac3d.h"

static void model_delete_regions(Model *model);
static void model_create_regions(Model *model, Config *config);
static void model_delete_storage(Model *model);
static void model_create_storage(Model *model);

Model *model_init(Config *config)
{
	Model *model;

	model = g_new0(Model, 1);
	model->n_bits = config_get_int(config, "base", "n_bits", 6);
	model->n_vert_y = config_get_int(config, "base", "n_vert_y", 64);
	model->bits = g_new0(guint8, model->n_bits);

	model_create_regions(model, config);
	model_create_storage(model);

	return model;
}

void model_cleanup(Model *model)
{
	model_delete_storage(model);
	model_delete_regions(model);
	g_free(model->bits);
	g_free(model);
}

void model_rebuild_storage(Model *model)
{
	model_delete_storage(model);
	model_create_storage(model);
}

gboolean model_save_config(Model *model, Config *config)
{
	Region *region;
	gint32 i;
	gchar *prefix;

	for(i = 0; i < NUM_REGIONS; i ++) {
		region = g_slist_nth_data(model->regions, i);
		prefix = NULL;
		switch(region->type) {
			case REGION_GRAYCODE: prefix = "regions.graycode"; break;
			case REGION_OBJECT:   prefix = "regions.object";   break;
			default: break;
		}
		if(prefix != NULL) {
			config_set_int(config, prefix, "x", region->rect.x);
			config_set_int(config, prefix, "y", region->rect.y);
			config_set_int(config, prefix, "width", region->rect.width);
			config_set_int(config, prefix, "height", region->rect.height);
		}
	}
	return TRUE;
}

gboolean model_save(Model *model, const gchar *filename)
{
	Region *region;

	region = g_slist_nth_data(model->regions, REGION_OBJECT);
	g_return_val_if_fail(region != NULL, FALSE);

	return ac3d_write(filename, model->angle_verts, model->angle_colors,
		(1 << model->n_bits), model->n_vert_y, region->rect.height);
}

/*****************************************************************************/

static void model_delete_regions(Model *model)
{
}

static void model_create_regions(Model *model, Config *config)
{
	Region *region;
	const gchar *prefix;
	gint32 i;

	for(i = 0; i < NUM_REGIONS; i ++) {
		region = g_new0(Region, 1);
		region->type = i;
		switch(region->type) {
			case REGION_GRAYCODE:
				prefix = "regions.graycode";
				break;
			case REGION_OBJECT:
				prefix = "regions.object";
				break;
			case NUM_REGIONS: /* ignore */ break;
		}
		region->rect.x = config_get_int(config, prefix, "x", 0);
		region->rect.y = config_get_int(config, prefix, "y", 0);
		region->rect.width = config_get_int(config, prefix, "width", 0);
		region->rect.height = config_get_int(config, prefix, "height", 0);
		model->regions = g_slist_append(model->regions, region);
	}
}

static void model_delete_storage(Model *model)
{
	if(model->angle_scans)
		g_free(model->angle_scans);
	if(model->angle_colors)
		g_free(model->angle_colors);
	if(model->angle_verts)
		g_free(model->angle_verts);
}

static void model_create_storage(Model *model)
{
	model->angle_scans = g_new0(guint8, (1 << model->n_bits));
	model->angle_verts = g_new0(gfloat,
		model->n_vert_y * (1 << model->n_bits));
	model->angle_colors = g_new0(guint8,
		model->n_vert_y * (1 << model->n_bits) * 3);
}


