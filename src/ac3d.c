#include <stdio.h>
#include <locale.h>

#include <glib.h>
#include <g3d/types.h>
#include <g3d/matrix.h>
#include <g3d/vector.h>

gboolean ac3d_write(const gchar *filename, gfloat *angle_verts,
	guint32 n_angles, guint32 n_vert_y, guint32 height)
{
	FILE *f;
	gint32 i, j;
	gfloat a_rad, x, y, z, matrix[16], sh;

	setlocale(LC_NUMERIC, "C");

	f = fopen(filename, "w");
	if(f == NULL)
		return FALSE;

	/* write header */
	fprintf(f, "AC3Db\n");
	fprintf(f, "MATERIAL \"default\" rgb 0.7 0.7 0.7 amb 0.2 0.2 0.2 "
		"emis 0 0 0 spec 0.5 0.5 0.5 shi 5 trans 0\n");
	fprintf(f, "OBJECT world\nkids 1\n");
	fprintf(f, "OBJECT poly\nname\"scanned object\"\n");

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
			fprintf(f, "SURF 0x00\nmat 0\nrefs 4\n");
			fprintf(f, "%d 0 0\n", i * n_vert_y + j);
			fprintf(f, "%d 0 0\n",
				((i == (n_angles - 1)) ? 0 : i + 1) * n_vert_y + j);
			fprintf(f, "%d 0 0\n",
				((i == (n_angles - 1)) ? 0 : i + 1) * n_vert_y + j + 1);
			fprintf(f, "%d 0 0\n", i * n_vert_y + j + 1);
		}
	}

	fprintf(f, "kids 0\n");
	fclose(f);
	return TRUE;
}
