#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "main.h"
#include "v4l2.h"
#include "gui.h"
#include "region.h"
#include "gray.h"
#include "ac3d.h"
#include "config.h"
#include "scan.h"

static void resize_storage(G3DScanner *scanner);
static GSList *init_regions(G3DScanner *scanner);
static gboolean idle_func(gpointer data);
static void graycode_region_changed(RegionType type, GdkRectangle *r,
	gpointer user_data);
static gboolean main_quit(gpointer window, GdkEvent *ev, G3DScanner *scanner);

int main(int argc, char *argv[])
{
	G3DScanner *scanner;

	gtk_init(&argc, &argv);

	scanner = g_new0(G3DScanner, 1);
	scanner->config = config_init();
	/* FIXME: init n_bits in config before GUI init */
	scanner->n_bits = config_get_int(scanner->config, "base", "n_bits", 6);

	scanner->regions = init_regions(scanner);
	scanner->v4l2 = v4l2_init(scanner->config);
	if(!scanner->v4l2) {
		g_free(scanner);
		return EXIT_FAILURE;
	}
	scanner->gui = gui_init(scanner->config);
	if(!scanner->gui) {
		v4l2_cleanup(scanner->v4l2);
		g_free(scanner);
		return EXIT_FAILURE;
	}

	gui_set_quit_handler(scanner->gui, G_CALLBACK(main_quit), scanner);

	scanner->n_vert_y = config_get_int(scanner->config, "base", "n_vert_y",
		64);
	resize_storage(scanner);
	scanner->bits = g_new0(guint8, scanner->n_bits);
	gui_set_regions(scanner->gui, scanner->regions);

	gui_show(scanner->gui);

	g_idle_add(idle_func, scanner);

	gtk_main();

	config_save(scanner->config);
	config_cleanup(scanner->config);

	gui_cleanup(scanner->gui);
	v4l2_cleanup(scanner->v4l2);

	g_free(scanner->angle_scans);
	g_free(scanner->angle_verts);
	g_free(scanner->bits);
	g_free(scanner);

	return EXIT_SUCCESS;
}

static void resize_storage(G3DScanner *scanner)
{
	if(scanner->angle_scans)
		g_free(scanner->angle_scans);
	scanner->angle_scans = g_new0(guint8, (1 << scanner->n_bits));

	if(scanner->angle_verts)
		g_free(scanner->angle_verts);
	scanner->angle_verts = g_new0(gfloat,
		scanner->n_vert_y * (1 << scanner->n_bits));

	if(scanner->angle_colors)
		g_free(scanner->angle_colors);
	scanner->angle_colors = g_new0(guint8,
		scanner->n_vert_y * (1 << scanner->n_bits) * 3);
}

static GSList *init_regions(G3DScanner *scanner)
{
	GSList *regions = NULL;
	Region *region;
	const gchar *prefix;
	gint32 i;

	for(i = 0; i < NUM_REGIONS; i ++) {
		region = g_new0(Region, 1);
		region->type = i;
		switch(region->type) {
			case REGION_GRAYCODE:
				region->changed_cb = graycode_region_changed;
				region->changed_data = scanner;
				prefix = "regions.graycode";
				break;
			case REGION_OBJECT:
				region->changed_cb = graycode_region_changed;
				region->changed_data = scanner;
				prefix = "regions.object";
				break;
			case NUM_REGIONS: /* ignore */ break;
		}
		region->rect.x = config_get_int(scanner->config, prefix, "x", 0);
		region->rect.y = config_get_int(scanner->config, prefix, "y", 0);
		region->rect.width = config_get_int(scanner->config, prefix,
			"width", 0);
		region->rect.height = config_get_int(scanner->config, prefix,
			"height", 0);
		regions = g_slist_append(regions, region);
	}
	return regions;
}

static gboolean idle_func(gpointer data)
{
	G3DScanner *scanner = data;
	GdkPixbuf *pixbuf;
	gchar *s;
	guint32 gv;
	gfloat deg;

	g_return_val_if_fail(scanner != NULL, FALSE);

	pixbuf = v4l2_get_pixbuf(scanner->v4l2);
	if(pixbuf) {
		scan_update_bits(scanner, pixbuf);
		gv = gray_decode(scanner->bits, scanner->n_bits);
		if(scanner->valid_dir) {
			deg = (gfloat)gv / (gfloat)(1 << scanner->n_bits) * 360.0;
			s = g_strdup_printf("%d:%d:%d:%d:%d:%d = %d (%.2f°)",
				scanner->bits[0], scanner->bits[1], scanner->bits[2],
				scanner->bits[3], scanner->bits[4], scanner->bits[5],
				gv, deg);
			if(scanner->angle_scans[gv] == 0) {
				/* scan colors of vertices a quarter rotation later */
				scan_colors(scanner, pixbuf, (gv +
					(1 << scanner->n_bits) * 3 / 4) % (1 << scanner->n_bits));
			}
		} else {
			s = g_strdup("invalid");
		}
		gui_set_angle(scanner->gui, s);
		g_free(s);
		scan_binarize_object_region(scanner, pixbuf);
		gui_set_image(scanner->gui, pixbuf);
		if(scanner->valid_dir && (scanner->angle_scans[gv] < 10)) {
			scan_angle(scanner, pixbuf, gv);
			gui_set_scan_progress(scanner->gui, gv, scanner->angle_scans[gv]);
		}
		gdk_pixbuf_unref(pixbuf);
	}

	return TRUE;
}

static void graycode_region_changed(RegionType type, GdkRectangle *r,
	gpointer user_data)
{
	G3DScanner *scanner = user_data;
	const gchar *prefix;

	g_return_if_fail(scanner != NULL);
	g_debug("main: region changed to %dx%d+%d+%d",
		r->width, r->height, r->x, r->y);

	switch(type) {
		case REGION_GRAYCODE: prefix = "regions.graycode"; break;
		case REGION_OBJECT:   prefix = "regions.object";   break;
		default: break;
	}

	config_set_int(scanner->config, prefix, "x", r->x);
	config_set_int(scanner->config, prefix, "y", r->y);
	config_set_int(scanner->config, prefix, "width", r->width);
	config_set_int(scanner->config, prefix, "height", r->height);

	resize_storage(scanner);
}

static gboolean main_quit(gpointer window, GdkEvent *ev, G3DScanner *scanner)
{
	Region *region;

	g_debug("quitting...");
	region = g_slist_nth_data(scanner->regions, REGION_OBJECT);
	if(region != NULL)
		ac3d_write("test.ac", scanner->angle_verts, scanner->angle_colors,
			(1 << scanner->n_bits),
			scanner->n_vert_y, region->rect.height);
	g_idle_remove_by_data(scanner);
	gtk_main_quit();
	return TRUE;
}
