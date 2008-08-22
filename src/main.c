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

	scanner->n_bits = config_get_int(scanner->config, "base", "n_bits", 6);
	scanner->n_vert_y = config_get_int(scanner->config, "base", "n_vert_y",
		64);

	scanner->bits = g_new0(guint8, scanner->n_bits);
	scanner->angle_scans = g_new0(guint8, (1 << scanner->n_bits));
	scanner->angle_verts = g_new0(gfloat,
		scanner->n_vert_y * (1 << scanner->n_bits));
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

static inline guint8 *get_pixel(GdkPixbuf *pixbuf, guint32 x, guint32 y)
{
	return gdk_pixbuf_get_pixels(pixbuf) +
		y * gdk_pixbuf_get_rowstride(pixbuf) +
		x * gdk_pixbuf_get_n_channels(pixbuf);
}

#define GRAY_VALUE_U8(r, g, b) \
	(guint8)(0.299 * (gfloat)(r) + 0.587 * (gfloat)(g) + 0.114 * (gfloat)(b))

static inline guint8 black_pixel(guint8 *pixel)
{
	guint8 g;

	g = GRAY_VALUE_U8(pixel[0], pixel[1], pixel[2]);
	return (g < 96) ? 1 : 0;
}

static void update_bits(G3DScanner *scanner, GdkPixbuf *pixbuf)
{
	guint32 sum, cx, cy;
	gint32 i;
	Region *region;
	gfloat sh;

	scanner->valid_dir = FALSE;

	region = g_slist_nth_data(scanner->regions, REGION_GRAYCODE);
	if(region == NULL)
		return;
	if((region->rect.width < 1) || (region->rect.height < scanner->n_bits * 3))
		return;

	sh = (gfloat)region->rect.height / scanner->n_bits;
	cx = region->rect.x + region->rect.width / 2;

	for(i = 0; i < scanner->n_bits; i ++) {
		cy = region->rect.y + i * sh + sh / 2;
		sum = black_pixel(get_pixel(pixbuf, cx, cy));
		sum += black_pixel(get_pixel(pixbuf, cx + 1, cy));
		sum += black_pixel(get_pixel(pixbuf, cx - 1, cy));
		sum += black_pixel(get_pixel(pixbuf, cx, cy + 1));
		sum += black_pixel(get_pixel(pixbuf, cx, cy - 1));
		scanner->bits[scanner->n_bits - i - 1] = (sum > 2) ? 1 : 0;
	}

	scanner->valid_dir = TRUE;
}

static inline void avg_pixel_9(GdkPixbuf *pixbuf, guint32 x, guint32 y,
	guint8 *newpixel)
{
	guint32 sum_r = 0, sum_g = 0, sum_b = 0, n_pix = 0;
	gint32 i, j;
	guint8 *pixel;

	for(j = y - 1; j <= (y + 1); j ++) {
		if((j < 0) || (j >= gdk_pixbuf_get_height(pixbuf)))
			continue;
		for(i = x - 1; i <= (x + 1); i ++) {
			if((i < 0) || ( i >= gdk_pixbuf_get_width(pixbuf)))
				continue;
			n_pix ++;
			pixel = get_pixel(pixbuf, i, j);
			sum_r += pixel[0];
			sum_g += pixel[1];
			sum_b += pixel[2];
		}
	}
	newpixel[0] = sum_r / n_pix;
	newpixel[1] = sum_g / n_pix;
	newpixel[2] = sum_b / n_pix;
}

static gboolean binarize_object_region(G3DScanner *scanner, GdkPixbuf *pixbuf)
{
	Region *region = g_slist_nth_data(scanner->regions, REGION_OBJECT);
	guint8 *new, *pixels, *pix, bg_gv, gv;
	gint32 x, y, i;
	guint32 nc, rs, sum_r = 0, sum_g = 0, sum_b = 0, n_pix = 0, w;
	gfloat bg_div[3], div[3];
	gboolean is_bg;

	if(!region || (region->rect.width < 10) || (region->rect.height < 10))
		return FALSE;

	pixels = gdk_pixbuf_get_pixels(pixbuf);
	rs = gdk_pixbuf_get_rowstride(pixbuf);
	nc = gdk_pixbuf_get_n_channels(pixbuf);

	/* get background color */
	w = MIN(10, region->rect.width / 10);
	for(y = 0; y < region->rect.height; y ++)
		for(x = 0; x < w; x ++) {
			pix = pixels + (y + region->rect.y) * rs +
				(x + region->rect.x) * nc;
			sum_r += pix[0];
			sum_g += pix[1];
			sum_b += pix[2];
		}
	n_pix = region->rect.height * w;
	bg_div[0] = (gfloat)(sum_r / n_pix) / (gfloat)(sum_g / n_pix + 0.01);
	bg_div[1] = (gfloat)(sum_r / n_pix) / (gfloat)(sum_b / n_pix + 0.01);
	bg_div[2] = (gfloat)(sum_g / n_pix) / (gfloat)(sum_b / n_pix + 0.01);
	bg_gv = GRAY_VALUE_U8(sum_r / n_pix, sum_g / n_pix, sum_b / n_pix);

	new = g_new0(guint8, region->rect.width * region->rect.height * 3);

	for(y = 0; y < region->rect.height; y ++) {
		for(x = 0; x < region->rect.width; x ++) {
			pix = new + y * region->rect.width * 3 + x * 3;
			avg_pixel_9(pixbuf, x + region->rect.x, y + region->rect.y, pix);
			div[0] = (gfloat)pix[0] / (gfloat)(pix[1] + 0.01);
			div[1] = (gfloat)pix[0] / (gfloat)(pix[2] + 0.01);
			div[2] = (gfloat)pix[1] / (gfloat)(pix[2] + 0.01);
			gv = GRAY_VALUE_U8(pix[0], pix[1], pix[2]);
			is_bg = TRUE;
			for(i = 0; i < 3; i ++)
				if(fabs(div[i] - bg_div[i]) > 0.8)
					is_bg = FALSE;
			if((gv - bg_gv) > 32)
				is_bg = FALSE;
			memset(pix, is_bg ? 0xFF : 0x00, 3);
		}
	}
	for(y = 0; y < region->rect.height; y ++)
		for(x = 0; x < region->rect.width; x ++)
			memcpy(
				pixels + (y + region->rect.y) * rs + (x + region->rect.x) * nc,
				new + y * region->rect.width * 3 + x * 3, 3);

	g_free(new);

	return TRUE;
}

static gboolean scan_angle(G3DScanner *scanner, GdkPixbuf *pixbuf,
	guint32 angle)
{
	Region *region;
	gint32 i, x;
	guint8 *pix;
	guint32 y;
	gfloat *v, sh;

	region = g_slist_nth_data(scanner->regions, REGION_OBJECT);
	g_return_val_if_fail(region != NULL, FALSE);
	g_return_val_if_fail(angle < (1 << scanner->n_bits), FALSE);

	sh = (gfloat)region->rect.height / scanner->n_vert_y;
	for(i = 0; i < scanner->n_vert_y; i ++) {
		y = (region->rect.y + region->rect.height - 1) - i * sh - sh / 2;
		v = scanner->angle_verts + angle * scanner->n_vert_y + i;
		for(x = 0; x < (region->rect.width * 0.75); x ++) {
			pix = get_pixel(pixbuf, x + region->rect.x, y);
			if(pix[0] == 0x00) {
				if((*v == 0) || (*v > (region->rect.width / 2 - x)))
					*v = region->rect.width / 2 - x;
				break;
			}
		}
#if 0
		g_debug("0x%02X: %02d: %.2f", angle, i,
			scanner->angle_verts[angle * scanner->n_vert_y + i]);
#endif
	}
	scanner->angle_scans[angle] ++;
	return TRUE;
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
		update_bits(scanner, pixbuf);
		gv = gray_decode(scanner->bits, scanner->n_bits);
		if(scanner->valid_dir) {
			deg = (gfloat)gv / (gfloat)(1 << scanner->n_bits) * 360.0;
			s = g_strdup_printf("%d:%d:%d:%d:%d:%d = %d (%.2f°)",
				scanner->bits[0], scanner->bits[1], scanner->bits[2],
				scanner->bits[3], scanner->bits[4], scanner->bits[5],
				gv, deg);
		} else {
			s = g_strdup("invalid");
		}
		gui_set_angle(scanner->gui, s);
		g_free(s);
		binarize_object_region(scanner, pixbuf);
		gui_set_image(scanner->gui, pixbuf);
		if(scanner->valid_dir && (scanner->angle_scans[gv] < 10))
			scan_angle(scanner, pixbuf, gv);
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
}

static gboolean main_quit(gpointer window, GdkEvent *ev, G3DScanner *scanner)
{
	Region *region;

	g_debug("quitting...");
	region = g_slist_nth_data(scanner->regions, REGION_OBJECT);
	if(region != NULL)
		ac3d_write("test.ac", scanner->angle_verts, (1 << scanner->n_bits),
			scanner->n_vert_y, region->rect.height);
	g_idle_remove_by_data(scanner);
	gtk_main_quit();
	return TRUE;
}
