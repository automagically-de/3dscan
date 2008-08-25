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
#include "model.h"

static gboolean idle_func(gpointer data);
static gboolean main_quit(gpointer window, GdkEvent *ev, G3DScanner *scanner);

int main(int argc, char *argv[])
{
	G3DScanner *scanner;

	gtk_init(&argc, &argv);

	scanner = g_new0(G3DScanner, 1);
	scanner->config = config_init();
	scanner->model = model_init(scanner->config);
	scanner->v4l2 = v4l2_init(scanner->config);
	if(!scanner->v4l2) {
		g_free(scanner);
		return EXIT_FAILURE;
	}
	scanner->gui = gui_init(scanner->config, scanner->model);
	if(!scanner->gui) {
		v4l2_cleanup(scanner->v4l2);
		g_free(scanner);
		return EXIT_FAILURE;
	}

	gui_set_quit_handler(scanner->gui, G_CALLBACK(main_quit), scanner);

	gui_show(scanner->gui);

	g_idle_add(idle_func, scanner);

	gtk_main();

	model_save_config(scanner->model, scanner->config);
	config_save(scanner->config);

	model_cleanup(scanner->model);
	config_cleanup(scanner->config);
	gui_cleanup(scanner->gui);
	v4l2_cleanup(scanner->v4l2);

	return EXIT_SUCCESS;
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
		scan_update_bits(scanner->model, pixbuf);
		gv = gray_decode(scanner->model->bits, scanner->model->n_bits);
		if(scanner->model->valid_dir) {
			deg = (gfloat)gv / (gfloat)(1 << scanner->model->n_bits) * 360.0;
			s = g_strdup_printf("%d:%d:%d:%d:%d:%d = %d (%.2f°)",
				scanner->model->bits[0], scanner->model->bits[1],
				scanner->model->bits[2], scanner->model->bits[3],
				scanner->model->bits[4], scanner->model->bits[5],
				gv, deg);
			if(scanner->model->angle_scans[gv] == 0) {
				/* scan colors of vertices a quarter rotation later */
				scan_colors(scanner->model, pixbuf, (gv +
					(1 << scanner->model->n_bits) * 3 / 4) %
					(1 << scanner->model->n_bits));
			}
		} else {
			s = g_strdup("invalid");
		}
		gui_set_angle(scanner->gui, s);
		g_free(s);

		gui_update(scanner->gui);

		scan_binarize_object_region(scanner->model, pixbuf);
		gui_set_image(scanner->gui, pixbuf);
		if(scanner->model->valid_dir &&
			(scanner->model->angle_scans[gv] < 10)) {
			scan_angle(scanner->model, pixbuf, gv);
			gui_set_scan_progress(scanner->gui, gv,
				scanner->model->angle_scans[gv]);
		}
		gdk_pixbuf_unref(pixbuf);
	}

	return TRUE;
}

static gboolean main_quit(gpointer window, GdkEvent *ev, G3DScanner *scanner)
{
	g_debug("quitting...");
	g_idle_remove_by_data(scanner);
	gtk_main_quit();
	return TRUE;
}
