#include <math.h>
#include <string.h>

#include "main.h"
#include "scan.h"

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

gboolean scan_update_bits(G3DScanner *scanner, GdkPixbuf *pixbuf)
{
	guint32 sum, cx, cy;
	gint32 i;
	Region *region;
	gfloat sh;

	scanner->valid_dir = FALSE;

	region = g_slist_nth_data(scanner->regions, REGION_GRAYCODE);
	if(region == NULL)
		return FALSE;
	if((region->rect.width < 1) || (region->rect.height < scanner->n_bits * 3))
		return FALSE;

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
	return TRUE;
}

gboolean scan_binarize_object_region(G3DScanner *scanner, GdkPixbuf *pixbuf)
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

gboolean scan_angle(G3DScanner *scanner, GdkPixbuf *pixbuf, guint32 angle)
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

