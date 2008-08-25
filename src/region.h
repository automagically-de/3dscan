#ifndef _REGION_H
#define _REGION_H

#include <gtk/gtk.h>

typedef enum {
	REGION_GRAYCODE,
	REGION_OBJECT,
	NUM_REGIONS
} RegionType;

typedef struct {
	RegionType type;
	GdkRectangle rect;
} Region;

#endif
