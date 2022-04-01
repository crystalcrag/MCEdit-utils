/*
 * animate.c : handle animation interface of TileFinder.
 *
 * Written by T.Pierron, Mar 2022.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "SIT.h"
#include "nanovg.h"
#include "TileFinder.h"
#include "TileFinderAnim.h"

static struct TileFinderAnim_t animate;

static STRPTR params[] = {
	"Size X", "Size Y", "Size Z",
	"Rotation X", "Rotation Y", "Rotation Z",
	"Translation X", "Translation Y", "Translation Z"
};

#define MARGIN 10
static int animPaintGraph(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnPaint * paint = cd;
	NVGcontext *  vg = paint->nvg;
	float         x = paint->x + MARGIN;
	float         y = paint->y + MARGIN;

	nvgStrokeColorRGBA8(vg, "\x44\x44\x44\xff");
	nvgStrokeWidth(vg, SIT_EmToReal(w, SITV_Em(0.2)));
	nvgBeginPath(vg);
	nvgMoveTo(vg, x, y);
	nvgLineTo(vg, x, y + paint->h - 2*MARGIN);
	nvgLineTo(vg, x + paint->w - 2*MARGIN, y + paint->h - 2*MARGIN);
	nvgStroke(vg);

	return 1;
}
#undef MARGIN

static int animAddPoint(SIT_Widget w, APTR cd, APTR ud)
{
	//
	return 1;
}

void animInit(SIT_Widget parent)
{
	animate.graph   = SIT_GetById(parent, "graph");
	animate.applyTo = SIT_GetById(parent, "applyto");
	animate.params  = SIT_GetById(parent, "params");
	animate.repeat  = SIT_GetById(parent, "repeat");
	animate.time    = SIT_GetById(parent, "time");

	SIT_AddCallback(animate.graph, SITE_OnPaint, animPaintGraph, NULL);
	SIT_AddCallback(SIT_GetById(parent, "addpt"), SITE_OnActivate, animAddPoint, NULL);

	int i;
	for (i = 0; i < DIM(params); i ++)
		SIT_ListInsertItem(animate.params, -1, NULL, params[i], "", "");

	SIT_SetValues(animate.params, SIT_SelectedIndex, 0, NULL);
}

void animShow(void)
{
	int selected, i;
	SIT_GetValues(animate.applyTo, SIT_SelectedIndex, &selected, NULL);
	if (selected < 0)
		SIT_SetValues(animate.applyTo, SIT_SelectedIndex, 0, NULL), selected = 0;

	Block box = animate.current = blockGetNth(selected);

	for (i = 0; i < DIM(params); i ++)
	{
		TEXT param[32];
		sprintf(param, "%g", (&box->size[0])[i]);
		SIT_ListSetCell(animate.params, i, 1, DontChangePtr, DontChange, param);
	}
}

void animSyncBox(Block box, int add)
{
	SIT_ListInsertItem(animate.applyTo, -1, box, box->name);
}

