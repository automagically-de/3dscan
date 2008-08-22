#ifndef _V4L2_H
#define _V4L2_H

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "config.h"

typedef struct _V4l2Data V4l2Data;

V4l2Data *v4l2_init(Config *config);
void v4l2_cleanup(V4l2Data *data);
GdkPixbuf *v4l2_get_pixbuf(V4l2Data *data);

#endif /* _V4L2_H */
