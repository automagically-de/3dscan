#ifndef _REGION_H
#define _REGION_H

#include <gtk/gtk.h>

typedef enum {
	REGION_GRAYCODE,
	REGION_OBJECT,
	NUM_REGIONS
} RegionType;

typedef void (* RegionChangedCb)(RegionType type, GdkRectangle *region,
	gpointer user_data);

typedef struct {
	RegionType type;
	GdkRectangle rect;
	RegionChangedCb changed_cb;
	gpointer changed_data;
} Region;

#endif
