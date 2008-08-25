#ifndef _MAIN_H
#define _MAIN_H

#include "v4l2.h"
#include "gui.h"
#include "config.h"
#include "model.h"

typedef struct {
	V4l2Data *v4l2;
	GuiData *gui;
	Config *config;
	Model *model;
} G3DScanner;

#endif
