#include <stdio.h>
#include <locale.h>

#include <glib.h>
#include <g3d/types.h>
#include <g3d/matrix.h>
#include <g3d/vector.h>

static inline guint32 ac3d_col16(guint8 r, guint8 g, guint8 b)
{
	return ((r / 8) << 11) + ((g / 4) << 5) + (b / 8);
}

static guint32 ac3d_median_color_4(guint8 *angle_colors, guint32 n_angles,
	guint32 n_vert_y, guint32 *indices, GHashTable *colors)
{
	guint32 sum[3] = { 0, 0, 0 }, c[3], col16, colid;
	gint32 i, j;

	for(j = 0; j < 3; j ++) {
		for(i = 0; i < 4; i ++)
			sum[j] += angle_colors[indices[i] * 3 + j];
		c[j] = sum[j] / 4;
	}
	col16 = ac3d_col16(c[0], c[1], c[2]);
	if(col16 == 0)
		return 0;
	colid = GPOINTER_TO_UINT(g_hash_table_lookup(colors,
		GUINT_TO_POINTER(col16)));
	if(colid != 0)
		return colid;

	/* unknown mixed color, just take 1st vertex */
	col16 = ac3d_col16(
		angle_colors[indices[0] * 3 + 0],
		angle_colors[indices[0] * 3 + 1],
		angle_colors[indices[0] * 3 + 2]);
	return GPOINTER_TO_UINT(g_hash_table_lookup(colors,
		GUINT_TO_POINTER(col16)));
}

static GHashTable *ac3d_init_colors(guint8 *angle_colors, guint32 n_angles,
	guint32 n_vert_y, FILE *f)
{
	GHashTable *hash;
	guint8 *c;
	gint32 i, j;
	guint32 colid = 1, col16;

	hash = g_hash_table_new(g_direct_hash, g_direct_equal);

	fprintf(f, "MATERIAL \"default\" rgb 0 0 0 amb 0.2 0.2 0.2 "
		"emis 0 0 0 spec 0.5 0.5 0.5 shi 5 trans 0\n");

	for(i = 0; i < n_angles; i ++) {
		for(j = 0; j < n_vert_y; j ++) {
			c = angle_colors + (i * n_vert_y + j) * 3;
			col16 = ac3d_col16(c[0], c[1], c[2]);
			if(col16 == 0) /* black already done */
				continue;
			if(g_hash_table_lookup(hash, GUINT_TO_POINTER(col16)) == NULL) {
				/* color not seen yet */
				fprintf(f, "MATERIAL \"color%d\" rgb %.2f %.2f %.2f "
					"amb 0.2 0.2 0.2 "
					"emis 0 0 0 spec 0.5 0.5 0.5 shi 5 trans 0\n",
					colid,
					(gfloat)(c[0] / 8) / 32.0, /* r */
					(gfloat)(c[1] / 4) / 64.0, /* g */
					(gfloat)(c[2] / 8) / 32.0);
				g_hash_table_insert(hash, GUINT_TO_POINTER(col16),
					GUINT_TO_POINTER(colid));
				colid ++;
			}
		}
	}
	return hash;
}

gboolean ac3d_write(const gchar *filename, gfloat *angle_verts,
	guint8 *angle_colors, guint32 n_angles, guint32 n_vert_y, guint32 height)
{
	FILE *f;
	gint32 i, j, k;
	guint32 ind[4], colid;
	gfloat a_rad, x, y, z, matrix[16], sh;
	GHashTable *colors;


	setlocale(LC_NUMERIC, "C");

	f = fopen(filename, "w");
	if(f == NULL)
		return FALSE;

	/* write header */
	fprintf(f, "AC3Db\n");

	colors = ac3d_init_colors(angle_colors, n_angles, n_vert_y, f);
	fprintf(f, "OBJECT world\nkids 1\n");
	fprintf(f, "OBJECT poly\nname\"scanned_object\"\n");

	sh = (gfloat)height / n_vert_y;

	/* vertices */
	fprintf(f, "numvert %d\n", n_angles * n_vert_y);
	for(i = 0; i < n_angles; i ++) {
		a_rad = G_PI * 2.0 * i / (gfloat)n_angles;
		for(j = 0; j < n_vert_y; j ++) {
			x = -(angle_verts[i * n_vert_y + j]);
			y = height - sh * j - sh / 2;
			z = 0;
			g3d_matrix_identity(matrix);
			g3d_matrix_rotate(a_rad, 0.0, 1.0, 0.0, matrix);
			g3d_vector_transform(&x, &y, &z, matrix);
			fprintf(f, "%f %f %f\n", x, y, z);
		}
	}

	fprintf(f, "numsurf %d\n", (n_vert_y - 1) * n_angles);
	for(i = 0; i < n_angles; i ++) {
		for(j = 0; j < (n_vert_y - 1); j ++) {
			ind[0] = i * n_vert_y + j;
			ind[1] = ((i + 1) % n_angles) * n_vert_y + j;
			ind[2] = ((i + 1) % n_angles) * n_vert_y + j + 1;
			ind[3] = i * n_vert_y + j + 1;
			colid = ac3d_median_color_4(angle_colors, n_angles, n_vert_y,
				ind, colors);
			fprintf(f, "SURF 0x00\nmat %u\nrefs 4\n", colid);
			for(k = 0; k < 4; k ++)
				fprintf(f, "%d 0 0\n", ind[k]);
		}
	}

	fprintf(f, "kids 0\n");
	fclose(f);
	return TRUE;
}
