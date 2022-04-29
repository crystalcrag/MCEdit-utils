/*
 * TileFinderUI.c: manage SITGL interface for TileFinder
 *
 * Written by T.Pierron, jan 2022
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "SIT.h"
#include "nanovg.h"
#include "TileFinder.h"
#include "TileFinderUI.h"
#include "TileFinderGL.h"
#include "TileFinderAnim.h"

static struct MainCtrl_t finder;
extern struct Prefs_t    prefs;

static STRPTR rot90Names[] = {
	"", "90\xC2\xB0", "180\xC2\xB0", "270\xC2\xB0"
};

static int factors[] = {100, 200, 300, 400, 800, 1100, 1600, 2300, 3200, 4000};

extern STRPTR detailTexNames[];

/*
 * view image callbacks
 */
static void viewZoomInOut(ViewImage view, double fact, int mx, int my, Bool relative)
{
	view->zoomFact *= fact;

	if (fact > 1 && view->zoomFact < 1 && view->zoomFact*1.5 > 1)
		view->zoomFact = 1;

	if (relative)
	{
		/* avoid having 100.021345% zoom factor above 100% */
		if (fact > 1 && view->zoomFact > 1.1 && view->zoomIdx < DIM(factors)-1)
			view->zoomIdx ++, view->zoomFact = factors[view->zoomIdx] / 100.;

		if (fact < 1 && view->zoomIdx > 0)
			view->zoomIdx --, view->zoomFact = factors[view->zoomIdx] / 100.;
	}
	else
	{
		view->zoomFact = fact;
		view->zoomIdx = 0;
		int diff = fact * 100, i;
		if (fact > 1)
		{
			for (i = 1; i < DIM(factors); i ++)
				if (abs(diff - factors[view->zoomIdx]) > abs(diff - factors[i]))
					view->zoomIdx = i;
		}
	}

	if (view->zoomFact <= 0) view->zoomFact = 1;
	if (view->zoomFact > 40) view->zoomFact = 40;

	/* no point in having an image smaller than 64x64px */
	fact = view->zoomFact;
	if (view->imgWidth  * fact < MIN_IMAGE_SIZE) view->zoomFact = MIN_IMAGE_SIZE / (double) view->imgWidth;
	if (view->imgHeight * fact < MIN_IMAGE_SIZE) view->zoomFact = MIN_IMAGE_SIZE / (double) view->imgHeight;

	int * rect = view->rect;
	int oldRect[4];
	memcpy(oldRect, rect, sizeof oldRect);
	fact = view->zoomFact;

	int dx = mx - oldRect[0];
	int dy = my - oldRect[1];

	rect[2] = view->imgWidth * fact;
	rect[3] = view->imgHeight * fact;
	if (mx < 0)
	{
		rect[0] = (view->dstWidth  - rect[2]) / 2;
		rect[1] = (view->dstHeight - rect[3]) / 2;
	}
	else
	{
		rect[0] = mx - dx * rect[2] / oldRect[2];
		rect[1] = my - dy * rect[3] / oldRect[3];
	}
	SIT_ForceRefresh();
}

static void viewFullScreen(ViewImage view)
{
	if (view->dstWidth * view->imgHeight > view->dstHeight * view->imgWidth)
		viewZoomInOut(view, view->dstHeight / (double) view->imgHeight, -1, -1, False);
	else
		viewZoomInOut(view, view->dstWidth / (double) view->imgWidth, -1, -1, False);
}

static int viewHandleMouse(SIT_Widget w, APTR cd, APTR ud)
{
	static int startX = -1, startY;
	static int rectX = -1, rectY;
	static int cellX = -1, cellY;
	static int lasso = 0;
	SIT_OnMouse * msg = cd;
	ViewImage view = ud;

	switch (msg->state) {
	case SITOM_ButtonPressed:
		//fprintf(stderr, "button %d pressed at %d, %d\n", msg->button, msg->x, msg->y);
		startX = -1;
		switch (msg->button) {
		case SITOM_ButtonLeft:
			if ((msg->flags & SITK_FlagShift) == 0)
			{
				startX = msg->x;
				startY = msg->y;
				rectX  = view->rect[0];
				rectY  = view->rect[1];
			}
			else goto init_lasso;
			return 2;
		case SITOM_ButtonMiddle:
			/* assign tex coord to current selected face */
			if (cellX >= 0)
			{
				if (finder.detailMode)
				{
					init_lasso:
					/* check if clicked inside old rectangle */
					lasso = 1;
					startX = floorf((msg->x - view->rect[0]) / view->zoomFact);
					startY = floorf((msg->y - view->rect[1]) / view->zoomFact);
					if (view->selection[0] <= startX && startX < view->selection[0] + view->selection[2] &&
					    view->selection[1] <= startY && startY < view->selection[1] + view->selection[3])
					{
						float dx = view->selection[2] * 0.1;
						float dy = view->selection[3] * 0.1;
						int area = 0;
						startX -= view->selection[0];
						startY -= view->selection[1];
						/* find out which corner has been clicked */
						if (startX <= dx) area |= 1; else
						if (startX >= view->selection[2] - dx - 1) area |= 2;
						if (startY <= dy) area |= 4; else
						if (startY >= view->selection[3] - dy - 1) area |= 8;
						lasso = (area + 1) << 1;
					}
					else
					{
						view->selection[0] = view->selInit[0] = startX;
						view->selection[1] = view->selInit[1] = startY;
						view->selection[2] = 0;
						view->selection[3] = 0;
					}
					return 2;
				}
				else uiSetFaceCoord(cellX, cellY);
			}
			break;
		case SITOM_ButtonWheelDown:
		case SITOM_ButtonWheelUp:
			viewZoomInOut(view, msg->button == SITOM_ButtonWheelDown ? 1/1.5 : 1.5, msg->x, msg->y, True);
			view->zoomChanged = 1;
		default:
			break;
		}
		break;
	case SITOM_Move:
		//fprintf(stderr, "mouse moved at %d, %d\n", msg->x, msg->y);
		if (startX < 0)
		{
			static int oldCellX, oldCellY;
			cellX = (msg->x - view->rect[0]) / (view->zoomFact * TILE_SIZE);
			cellY = (msg->y - view->rect[1]) / (view->zoomFact * TILE_SIZE);
			if (msg->x < view->rect[0] || cellX > prefs.defU || msg->y < view->rect[1] || cellY > prefs.defV)
				cellX = cellY = -1;

			if (oldCellX != cellX || oldCellY != cellY)
			{
				TEXT tileCoord[16];
				sprintf(tileCoord, "%d, %d\n", cellX, cellY);
				SIT_SetValues(finder.coords, SIT_Title, tileCoord, NULL);
				oldCellX = cellX;
				oldCellY = cellY;
				if (! finder.detailMode)
				{
					int * rect = view->selection;
					if (cellX >= 0)
					{
						rect[0] = cellX * TILE_SIZE;
						rect[1] = cellY * TILE_SIZE;
						rect[2] = TILE_SIZE;
						rect[3] = TILE_SIZE;
					}
					else memset(rect, 0, sizeof view->selection);
				}
			}
		}
		break;
	case SITOM_CaptureMove:
		//fprintf(stderr, "mouse moved at %d, %d: %d, %d\n", msg->x, msg->y, lasso, startX);
		if (startX >= 0)
		{
			int rect[4];
			int x = roundf((msg->x - view->rect[0]) / view->zoomFact);
			int y = roundf((msg->y - view->rect[1]) / view->zoomFact);
			if (lasso > 1)
			{
				/* move existing selection rectangle instead */
				int corners = (lasso-1) >> 1;
				memcpy(rect, view->selection, sizeof rect);
				if (corners == 0)
				{
					/* move the entire rectangle */
					rect[0] = x - startX;
					rect[1] = y - startY;
				}
				else /* constrained movement */
				{
					rect[2] += rect[0];
					rect[3] += rect[1];
					if (corners & 1) { if (x < rect[2]) rect[0] = x; } else
					if (corners & 2) { if (x > rect[0]) rect[2] = x; }
					if (corners & 4) { if (y < rect[3]) rect[1] = y; } else
					if (corners & 8) { if (y > rect[1]) rect[3] = y; }
					rect[2] -= rect[0];
					rect[3] -= rect[1];
				}
				goto adjust_selection;
			}
			else if (lasso == 1)
			{
				rect[0] = view->selInit[0];
				rect[1] = view->selInit[1],
				rect[2] = x;
				rect[3] = y;
				if (rect[2] < rect[0]) swap(rect[0], rect[2]);
				if (rect[3] < rect[1]) swap(rect[1], rect[3]);
				rect[2] -= rect[0];
				rect[3] -= rect[1];
				adjust_selection:
				if (memcmp(view->selection, rect, sizeof rect))
				{
					TEXT tileCoord[16];
					sprintf(tileCoord, "%d, %d\n", rect[2], rect[3]);
					SIT_SetValues(finder.coords, SIT_Title, tileCoord, NULL);
					memcpy(view->selection, rect, sizeof rect);
					SIT_ForceRefresh();
				}
			}
			else
			{
				rectX += msg->x - startX; startX = msg->x;
				rectY += msg->y - startY; startY = msg->y;
				view->rect[0] = abs(rectX) < 10 ? 0 : rectX;
				view->rect[1] = abs(rectY) < 10 ? 0 : rectY;
				SIT_ForceRefresh();
			}
		}
		break;
	case SITOM_ButtonReleased:
		//fprintf(stderr, "button %d pressed at %d, %d\n", msg->button, msg->x, msg->y);
		if (lasso > 0)
			uiSetFaceTexCoord(view->selection);
		startX = -1;
		lasso = 0;
	default:
		break;
	}
	return 0;
}

/* SITE_OnVanillaKey handler */
static int viewHandleKbd(SIT_Widget w, APTR cd, APTR ud)
{
	ViewImage view = ud;
	SIT_OnKey * msg = cd;
	switch (msg->utf8[0]) {
	case 'f': case 'F': viewFullScreen(view); return 1;
	case 'd': case 'D': viewZoomInOut(view, 1, -1, -1, False); return 1;
	}
	return 0;
}


/* SITE_OnPaint handler */
static int viewHandlePaint(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnPaint * paint = cd;
	ViewImage     view = ud;
	NVGcontext *  vg = finder.nvgCtx;

	if (view->imgId > 0)
	{
		int * rect = view->rect;
		int   imgx = rect[0] + paint->x;
		int   imgy = rect[1] + paint->y;
		nvgScissor(vg, paint->x, paint->y, view->dstWidth, view->dstHeight);
		nvgFillPaint(vg, nvgImagePattern(vg, paint->x + rect[0], paint->y + rect[1], rect[2], rect[3], 0, view->imgId, 1));
		nvgBeginPath(vg);
		nvgRect(vg, imgx, imgy, rect[2], rect[3]);
		nvgFill(vg);

		rect = view->selection;
		if (rect[2] > 0)
		{
			float fact = view->zoomFact;
			float width = rect[2] * fact+1;
			float height = rect[3] * fact+1;
			float x = imgx + rect[0] * fact - 0.5f;
			float y = imgy + rect[1] * fact - 0.5f;
			nvgSave(vg);
			nvgStrokeWidth(vg, 1);
			/* drawed with XOR operation */
			nvgGlobalCompositeBlendFuncSeparate(vg, NVG_ONE_MINUS_DST_COLOR, NVG_ZERO, NVG_ONE, NVG_ZERO);
			nvgStrokeColorRGBA8(vg, "\xff\xff\xff\xff");
			nvgBeginPath(vg);
			nvgRect(vg, x, y, width, height);
			nvgStroke(vg);
			if (width > 64 && height > 64 && finder.detailMode)
			{
				float dx = width * 0.1;
				float dy = height * 0.1;
				nvgBeginPath(vg);
				nvgMoveTo(vg, x + dx, y);
				nvgLineTo(vg, x + dx, y + height - 1);

				nvgMoveTo(vg, x + width - dx, y);
				nvgLineTo(vg, x + width - dx, y + height - 1);

				nvgMoveTo(vg, x, y + dy);
				nvgLineTo(vg, x + width - 1, y + dy);

				nvgMoveTo(vg, x, y + height - dy);
				nvgLineTo(vg, x + width - 1, y + height - dy);
				nvgStroke(vg);
			}
			nvgRestore(vg);
		}
	}
	return 1;
}

/* SITE_OnResize handler */
static int viewHandleResize(SIT_Widget w, APTR cd, APTR ud)
{
	ViewImage view = ud;
	float * rect = cd;
	view->dstWidth  = rect[0];
	view->dstHeight = rect[1];
	if (! view->zoomChanged)
		viewFullScreen(view);
	return 1;
}

static void viewSetImage(SIT_Widget canvas, int imgId)
{
	ViewImage view;
	SIT_GetValues(canvas, SIT_UserData, &view, NULL);
	view->imgId = imgId;
	nvgImageSize(finder.nvgCtx, imgId, &view->imgWidth, &view->imgHeight);
	view->rect[2] = view->imgWidth;
	view->rect[3] = view->imgHeight;

	prefs.defU = view->imgWidth  / TILE_SIZE - 1;
	prefs.defV = view->imgHeight / TILE_SIZE - 1;

	if (view->dstWidth > 0)
	{
		if (! view->zoomChanged)
			viewFullScreen(view);
	}
	else view->zoomFact = 1, view->zoomChanged = 0;
}

static void viewInit(SIT_Widget canvas, int textureId)
{
	ViewImage view;
	SIT_GetValues(canvas, SIT_UserData, &view, NULL);
	viewSetImage(canvas, textureId);

	SIT_AddCallback(canvas, SITE_OnClickMove,  viewHandleMouse,  view);
	SIT_AddCallback(canvas, SITE_OnPaint,      viewHandlePaint,  view);
	SIT_AddCallback(canvas, SITE_OnResize,     viewHandleResize, view);
	SIT_AddCallback(canvas, SITE_OnVanillaKey, viewHandleKbd,    view);
}

/* set selection rectangle to match the texture of the face */
static void viewShowTexCoord(Block b, int face)
{
	ViewImage view;
	SIT_GetValues(finder.tex, SIT_UserData, &view, NULL);
	if (b && finder.detailMode)
		blockGetTexRect(b, face, view->selection);
	else
		memset(view->selection, 0, sizeof view->selection);
}

/* rectangle selection can be hard to spot: zoom in around the area */
static void viewZoomSelection(int * XYWH)
{
	ViewImage view;
	SIT_GetValues(finder.tex, SIT_UserData, &view, NULL);

	int * rect = view->rect;
	float fact;
	if (view->dstWidth * XYWH[3] > view->dstHeight * XYWH[2])
	{
		/* constrained by height */
		fact = view->dstHeight / (float) (3 * XYWH[3]);
	}
	else /* constrained by width */
	{
		fact = view->dstWidth / (float) (3 * XYWH[2]);
	}

	/* adjusted factor to an integral level */
	view->zoomFact = fact;
	view->zoomIdx = 0;
	int diff = fact * 100, i;
	if (fact > 1)
	{
		for (i = 1; i < DIM(factors); i ++)
			if (abs(diff - factors[view->zoomIdx]) > abs(diff - factors[i]))
				view->zoomIdx = i;
	}

	rect[2] = view->imgWidth * fact;
	rect[3] = view->imgHeight * fact;

	rect[0] = - (XYWH[0]) * fact + (view->dstWidth  - XYWH[2] * fact) * 0.5f;
	rect[1] = - (XYWH[1]) * fact + (view->dstHeight - XYWH[3] * fact) * 0.5f;
}

/*
 * primitives list handlers
 */

static void boxAddOrUpdateList(Block box, Bool insert)
{
	TEXT boxSize[32];
	TEXT boxPos[32];

	sprintf(boxSize, "%g, %g, %g", box->size[VX],  box->size[VY],  box->size[VZ]);
	sprintf(boxPos,  "%g, %g, %g", box->trans[VX], box->trans[VY], box->trans[VZ]);

	if (! insert)
	{
		int row = SIT_ListFindByTag(finder.list, box);
		SIT_ListSetCell(finder.list, row, 1, DontChangePtr, DontChange, boxSize);
		SIT_ListSetCell(finder.list, row, 2, DontChangePtr, DontChange, boxPos);
	}
	else
	{
		int len = strlen(box->name);
		if (box->detailTex == TEX_CUBEMAP && ! box->ref)
			strcpy(box->name + len, "*"), box->ref = 1;
		else if (box->incFaceId)
			strcpy(box->name + len, "+");

		SIT_ListInsertItem(finder.list, -1, box, box->name, boxSize, boxPos);
		box->name[len] = 0;
	}
	animSyncBox(box, True);
}

/* SITE_OnActivate on addbox */
static int boxAddDefault(SIT_Widget w, APTR cd, APTR autoSelect)
{
	boxAddOrUpdateList(blockAddDefaultBox(), True);
	blockGenVertexBuffer();
	if (autoSelect)
		SIT_SetValues(finder.list, SIT_RowSel(prefs.nbBlocks-1), True, NULL);
	return 1;
}

Block boxGetCurrent(void)
{
	int index;
	SIT_GetValues(finder.list, SIT_SelectedIndex, &index, NULL);
	if (index >= 0)
	{
		Block b;
		SIT_GetValues(finder.list, SIT_RowTag(index), &b, NULL);
		return b;
	}
	return NULL;
}

static void boxResetName(Block b, int index)
{
	if (index < 0)
		index = SIT_ListFindByTag(finder.list, b);
	int len = strlen(b->name);
	if (b->ref) strcpy(b->name+len, "*"); else
	if (b->incFaceId) strcpy(b->name+len, "+");
	SIT_ListSetCell(finder.list, index, 0, DontChangePtr, DontChange, b->name);
	b->name[len] = 0;
}

/* delete one box from view */
static int boxDel(SIT_Widget w, APTR cd, APTR ud)
{
	int index;
	SIT_GetValues(finder.list, SIT_SelectedIndex, &index, NULL);
	if (index >= 0)
	{
		Block b, next, select;
		SIT_GetValues(finder.list, SIT_RowTag(index), &b, NULL);
		SIT_ListDeleteRow(finder.list, index);
		if (b->ref)
		{
			/* make the next inherited cubemap, the new cubemap */
			for (next = b, NEXT(next); next; NEXT(next))
			{
				if (next->detailTex != TEX_DETAIL)
				{
					if (next->detailTex != TEX_CUBEMAP)
					{
						next->ref = 1;
						next->detailTex = TEX_CUBEMAP;
						memcpy(next->texUV, b->texUV, sizeof next->texUV);
					}
					break;
				}
			}
		}
		next = select = (Block) b->node.ln_Next;
		if (select == NULL)
			select = (Block) b->node.ln_Prev;
		index = blockRemove(b);

		while (next)
		{
			boxResetName(next, index);
			index ++;
			NEXT(next);
		}
		if (select)
		{
			for (index = 0; select; index ++, PREV(select));
			SIT_SetValues(finder.list, SIT_SelectedIndex, index, NULL);
		}

		blockGenVertexBuffer();
	}
	return 1;
}

static int boxDelAll(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_CloseDialog(w);

	SIT_ListDeleteRow(finder.list, DeleteAllRows);
	blockDeleteAll();
	boxAddDefault(w, cd, ud);
	finder.faceEdit = 0;
	viewShowTexCoord(blockGetNth(0), 0);
	SIT_SetValues(finder.faces[0], SIT_CheckState, True, NULL);
	SIT_SetValues(finder.radio[0], SIT_CheckState, True, NULL);
	SIT_SetValues(finder.list, SIT_SelectedIndex, 0, NULL);
	return 1;
}

static int boxAskDelAll(SIT_Widget w, APTR cd, APTR ud)
{
	uiYesNo(w, "Are you sure you want to delete everything?", boxDelAll, True);
	return 1;
}

/* duplicate current box if any */
static void boxDup(void)
{
	Block b = boxGetCurrent();

	if (b)
	{
		Block dup = blockAddDefaultBox();
		memcpy(dup->name, b->name, offsetp(Block, faces) - offsetp(Block, name));
		dup->faces = b->faces;
		dup->rotateCenter = b->rotateCenter;
		dup->incFaceId = b->incFaceId;
		dup->custName = b->custName;
		boxAddOrUpdateList(dup, True);
		blockGenVertexBuffer();
		SIT_SetValues(finder.list, SIT_RowSel(prefs.nbBlocks-1), True, NULL);
		SIT_ForceRefresh();
	}
}

/* update <tex> text field */
static void boxUpdateTexCoord(void)
{
	TEXT   buffer[256];
	DATA16 coord;
	STRPTR p;
	Block  b = boxGetCurrent();
	int    i, faces;

	if (b == NULL) return;
	/* cube map: tex is only applied on first box */
	while (b->node.ln_Prev && b->detailTex == TEX_CUBEMAP_INHERIT)
		PREV(b);
	faces = b->faces;

	if (blockCanUseTileCoord(b))
	{
		/* use simplified tile coord: 90% of tex coord in blockTable.js will use this format */
		int rotate = b->rotateUV;
		for (i = 0, coord = b->texUV, p = buffer; i < 6; i ++, coord += 8, faces >>= 1, rotate >>= 2)
		{
			static uint8_t texOrigin[] = {0, 6, 4, 2};
			DATA16 tex = coord + texOrigin[rotate & 3];
			p += sprintf(p, "%2d,%2d,", tex[0] >> 4, tex[1] >> 4);
		}
		if (b->rotateUV > 0)
		{
			sprintf(p, "  %d", b->rotateUV);
		}
		else p[-1] = 0;
	}
	else /* full tex coord */
	{
		for (i = 0, coord = b->texUV, p = buffer; i < DIM(b->texUV); i += 8, faces >>= 1)
		{
			if (faces & 1)
			{
				int j;
				for (j = 0; j < 4; j ++, coord += 2)
					p += sprintf(p, "%d,", coord[0] + coord[1] * 513);
			}
			else coord += 8;
		}
		/* remove last comma */
		p[-1] = 0;
	}
	SIT_SetValues(finder.texUV, SIT_Title, buffer, NULL);
}

/* new primitive selected: update interface */
static int boxSelect(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_Widget app = finder.app;
	Block      b = boxGetCurrent();
	TEXT       sz[128];

	if (b == NULL) return 0;
	SIT_SetAttributes(app,
		"<faceS checkState=", (b->faces & 1)  > 0, ">"
		"<faceE checkState=", (b->faces & 2)  > 0, ">"
		"<faceN checkState=", (b->faces & 4)  > 0, ">"
		"<faceW checkState=", (b->faces & 8)  > 0, ">"
		"<faceT checkState=", (b->faces & 16) > 0, ">"
		"<faceB checkState=", (b->faces & 32) > 0, ">"
		"<INV   checkState=", (b->faces & 64) > 0, ">"
	);
	sprintf(sz, "<szx  title=%g><szy  title=%g><szz  title=%g>", b->size[VX],   b->size[VY],   b->size[VZ]);   SIT_SetAttributes(app, sz);
	sprintf(sz, "<trx  title=%g><try  title=%g><trz  title=%g>", b->trans[0],   b->trans[1],   b->trans[2]);   SIT_SetAttributes(app, sz);
	sprintf(sz, "<rotx title=%g><roty title=%g><rotz title=%g>", b->rotate[0],  b->rotate[1],  b->rotate[2]);  SIT_SetAttributes(app, sz);
	sprintf(sz, "<rezx title=%g><rezy title=%g><rezz title=%g>", b->cascade[0], b->cascade[1], b->cascade[2]); SIT_SetAttributes(app, sz);

	SIT_SetValues(SIT_GetById(app, "center"), SIT_CheckState, b->rotateCenter, NULL);
	int i;
	for (i = 0; i < 3; i ++)
	{
		sprintf(sz, "%g", b->rotateFrom[i]);
		SIT_SetValues(finder.center[i], SIT_Title, sz, SIT_Enabled, !b->rotateCenter, NULL);
	}

	boxUpdateTexCoord();
	viewShowTexCoord(b, 0);
	if (finder.showActive)
		renderSetFeature(finder.active, NULL, (APTR) 1);

	finder.detailMode = prefs.detail || b->detailTex == TEX_DETAIL;

	if (prefs.detail == 0)
		SIT_SetValues(finder.subdet, SIT_CheckState, b->detailTex == TEX_DETAIL, NULL);

	SIT_SetValues(finder.incface, SIT_CheckState, b->incFaceId, NULL);

	int id = 0, faces = b->faces;
	while (id < 6 && (b->faces & 1) == 0) id ++, faces >>= 1;
	if (0 <= id && id < 6 && id != finder.faceEdit)
	{
		finder.faceEdit = id;
		viewShowTexCoord(b, id);
		SIT_SetValues(finder.radio[id], SIT_CheckState, True, NULL);
	}
	for (id = 0, faces = b->faces; id <= 7; id ++, faces >>= 1)
		SIT_SetValues(finder.faces[id], SIT_CheckState, faces & 1, NULL);

	/* render location of rotation center if not set to "center" */
	blockGenAxis();

	return 1;
}

static void boxSelectRelative(int dir)
{
	int cur, count;
	SIT_GetValues(finder.list, SIT_SelectedIndex, &cur, SIT_ItemCount, &count, NULL);
	cur += dir;
	if (0 <= cur && cur < count)
		SIT_SetValues(finder.list, SIT_SelectedIndex, cur, NULL);
}

/* SITE_OnBlur */
static int boxFinishEdit(SIT_Widget w, APTR cd, APTR ud)
{
	if (! finder.cancelEdit)
	{
		Block b = ud;
		STRPTR name;
		int nth;

		for (nth = 0, b = ud; b; PREV(b), nth ++);
		b = ud;

		SIT_GetValues(w, SIT_Title, &name, NULL);
		if (name[0])
		{
			b->custName = 1;
			CopyString(b->name, name, sizeof b->name);
		}
		else
		{
			b->custName = 0;
			sprintf(b->name, "Box %d", nth);
		}
		boxResetName(b, nth-1);
		/* we will get an OnBlur once removed */
		finder.cancelEdit = 1;
	}
	SIT_RemoveWidget(w);
	return 1;
}

/* OnRawKey */
static int boxAcceptEdit(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnKey * msg = cd;
	if (msg->keycode == SITK_Return)
	{
		finder.cancelEdit = 0;
		boxFinishEdit(w, NULL, ud);
		return 1;
	}
	else if (msg->keycode == SITK_Escape)
	{
		/* remove widgets will cause an OnBlur event */
		finder.cancelEdit = 1;
		SIT_RemoveWidget(w);
		return 1;
	}
	return 0;
}

/* SITE_OnClick on list */
static int boxSetName(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_Widget parent;
	float rect[4];
	int   click;
	Block b;

	parent = NULL;
	b = NULL;
	if (ud)
	{
		/* rename currently selected item */
		SIT_GetValues(ud, SIT_SelectedIndex, &click, NULL);
		if (click >= 0)
		{
			SIT_GetValues(ud, SIT_RowTag(click), &b, NULL);
			parent = SIT_ListGetItemRect(ud, rect, click, 0);
		}
	}
	else /* edit item under mouse cursor */
	{
		SIT_OnMouse * msg = cd;
		if (msg->state == SITOM_ButtonPressed && msg->button == SITOM_ButtonRight)
		{
			parent = w;
			click = SIT_ListGetItemOver(w, rect, msg->x, msg->y, &parent);
			b = blockGetNth(click >> 8);
		}
	}

	if (parent)
	{
		finder.cancelEdit = 0;
		w = SIT_CreateWidget("editname.plain", SIT_EDITBOX, parent,
			/* cannot edit wp->name directly: we want this to be cancellable */
			SIT_Title,      b->name,
			SIT_EditLength, sizeof b->name,
			SIT_X,          (int) rect[0],
			SIT_Y,          (int) rect[1],
			SIT_Width,      (int) (rect[2] - rect[0] - 4),
			SIT_Height,     (int) (rect[3] - rect[1] - 4),
			NULL
		);
		SIT_SetFocus(w);
		SIT_AddCallback(w, SITE_OnBlur,   boxFinishEdit, b);
		SIT_AddCallback(w, SITE_OnRawKey, boxAcceptEdit, b);
		return 1;
	}
	return 0;
}

/*
 * interface state management
 */

static int uiToggleDetail(SIT_Widget w, APTR cd, APTR ud)
{
	finder.detailMode = prefs.detail = (int) ud;

	SIT_SetValues(finder.subdet, SIT_Enabled, prefs.detail == 0, NULL);

	Block b;
	if (prefs.detail)
	{
		for (b = blockGetNth(0); b; NEXT(b))
			b->detailTex = TEX_DETAIL, blockCubeMapToDetail(b);
	}
	else
	{
		b = blockGetNth(0);
		b->detailTex = TEX_CUBEMAP;
		for (NEXT(b); b; b->detailTex = TEX_CUBEMAP_INHERIT, NEXT(b));
	}

	return 1;
}

static int uiToggleUnitBBox(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_GetValues(w, SIT_CheckState, &prefs.bbox, NULL);
	SIT_ForceRefresh();
	return 1;
}

static int uiSetSubDetail(SIT_Widget w, APTR cd, APTR ud)
{
	Block b = boxGetCurrent();
	int checked = 0;
	SIT_GetValues(w, SIT_CheckState, &checked, NULL);

	if (b->detailTex != TEX_DETAIL && checked)
	{
		/* convert current cube map tex into detail tex */
		blockCubeMapToDetail(b);
	}
	b->detailTex = checked ? TEX_DETAIL : b->node.ln_Prev ? TEX_CUBEMAP_INHERIT : TEX_CUBEMAP;
	b->ref = (b->detailTex == TEX_CUBEMAP);
	boxResetName(b, -1);
	blockGenVertexBuffer();

	finder.detailMode = prefs.detail || checked;
	SIT_ForceRefresh();

	return 1;
}

static int uiSetIncFaceId(SIT_Widget w, APTR cd, APTR ud)
{
	Block b = boxGetCurrent();
	int checked = 0;
	SIT_GetValues(w, SIT_CheckState, &checked, NULL);
	if (b)
	{
		b->incFaceId = checked;
		boxResetName(b, -1);
		for (NEXT(b); b; NEXT(b))
		{
			/* will cancel cascading rotation */
			if (b->cascade[VX] != 0 ||
			    b->cascade[VY] != 0 ||
			    b->cascade[VZ] != 0)
			{
				blockGenVertexBuffer();
				break;
			}
		}
	}
	return 1;
}

/* SITE_OnChange on text box: set current box size, translation and rotation */
static int uiSetBlockValue(SIT_Widget w, APTR cd, APTR ud)
{
	Block b = boxGetCurrent();
	if (b)
	{
		int   param = (int) ud;
		float value = strtof(cd, NULL);
		if (isnan(value) || (param < 3 && value < 0)) return 0;

		switch (param) {
		case 0:  case 1:  case 2:  b->size[param]      = value; break;
		case 3:  case 4:  case 5:  b->trans[param-3]   = value; break;
		case 6:  case 7:  case 8:  b->rotate[param-6]  = value; break;
		case 9:  case 10: case 11: b->cascade[param-9] = value; break;
		case 12: case 13: case 14: b->rotateFrom[param-12] = value; break;
		}
		if (param < 6)
			boxAddOrUpdateList(b, False);
		blockGenVertexBuffer();
	}
	return 1;
}

/* rotate by 90deg step */
static int uiRotate90(SIT_Widget w, APTR cd, APTR ud)
{
	int i = prefs.rot90 + (ud ? -1 : 1);
	if (i < 0) i = 3;
	if (i > 3) i = 0;
	prefs.rot90 = i;

	blockGenVertexBuffer();
	SIT_SetValues(finder.lab90, SIT_Title, rot90Names[prefs.rot90], NULL);

	return 1;
}

/* face visibility (S, E, N, W, T, B buttons) */
static int uiToggleFaceVisibility(SIT_Widget w, APTR cd, APTR ud)
{
	Block b = boxGetCurrent();
	if (b)
	{
		int checked;
		int faceId = 1 << (int) ud;
		SIT_GetValues(w, SIT_CheckState, &checked, NULL);
		if (checked)
			b->faces |= faceId;
		else
			b->faces &= ~ faceId;
		blockGenVertexBuffer();

		faceId = finder.faceEdit;
		if (checked == 0 && faceId == (int) ud)
		{
			while (faceId < 6 && (b->faces & (1<<faceId)) == 0) faceId ++;
			if (faceId == 6 && finder.faceEdit > 0)
			{
				for (faceId = finder.faceEdit-1; faceId >= 0 && (b->faces & (1<<faceId)) == 0; faceId --);
			}
			if (0 <= faceId && faceId < 6)
			{
				finder.faceEdit = faceId;
				viewShowTexCoord(b, faceId);
				SIT_SetValues(finder.faces[faceId], SIT_CheckState, True, NULL);
			}
		}

		SIT_ForceRefresh();
	}
	return 1;
}

/* right click in 3d view: select face/primitive under mouse */
void uiSelectFace(int faceId)
{
	/* faceId comes from vertex data (see blockGenVertexBuffer()) */
	if (faceId > 0)
	{
		/* primitive id */
		SIT_SetValues(finder.list, SIT_RowSel(faceId>>3), True, NULL);
		/* face of primitive */
		finder.faceEdit = finder.lastFaceSet = (faceId & 7) - 1;
		SIT_SetValues(finder.radio[finder.faceEdit], SIT_CheckState, True, NULL);
		Block b = boxGetCurrent();
		TEXT tileCoord[16];
		int XYWH[4];
		blockGetTexRect(b, finder.faceEdit, XYWH);
		sprintf(tileCoord, "%d, %d\n", XYWH[2], XYWH[3]);
		SIT_SetValues(finder.coords, SIT_Title, tileCoord, NULL);
		viewShowTexCoord(boxGetCurrent(), finder.faceEdit);
	}
}

static void uiEditNextFace(int dir)
{
	Block b = boxGetCurrent();
	int id = finder.faceEdit + dir;

	if (b->detailTex == TEX_DETAIL)
	{
		while (0 <= id && id < 6 && (b->faces & (1<<id)) == 0) id += dir;
	}
	if (0 <= id && id < 6)
	{
		finder.faceEdit = finder.lastFaceSet = id;
		viewShowTexCoord(b, id);
		SIT_SetValues(finder.radio[id], SIT_CheckState, True, NULL);
	}
}

/* viewImage callback: select a texture from a tile coord */
void uiSetFaceCoord(int cellX, int cellY)
{
	Block b = boxGetCurrent();
	if (b)
	{
		/* cube map: apply to first box */
		if (b->detailTex == TEX_CUBEMAP_INHERIT)
		{
			b->detailTex = TEX_CUBEMAP;
			if (b->ref == 0) b->ref = 1, boxResetName(b, -1);
		}

		finder.lastFaceSet = finder.faceEdit;
		blockSetFaceCoord(b, finder.faceEdit, cellX, cellY);
		blockGenVertexBuffer();
		if (finder.faceEdit < 6)
			uiEditNextFace(1);
		boxUpdateTexCoord();
		SIT_ForceRefresh();
	}
}

/* viewImage callback: select a texture from a precise rectangle */
void uiSetFaceTexCoord(int * texCoords)
{
	Block b = boxGetCurrent();
	if (b)
	{
		blockSetFaceTexCoord(b, finder.lastFaceSet, texCoords);
		blockGenVertexBuffer();
		boxUpdateTexCoord();
		SIT_ForceRefresh();
	}
}

static int uiSetFaceEdit(SIT_Widget w, APTR cd, APTR ud)
{
	Block b = boxGetCurrent();
	finder.faceEdit = finder.lastFaceSet = (int) ud;
	viewShowTexCoord(b, finder.faceEdit);
	return 1;
}

static void uiToggleAnim(SIT_Widget w)
{
	SIT_GetValues(w, SIT_CheckState, &prefs.anim, NULL);

	SIT_SetValues(finder.tex,   SIT_Visible, 1-prefs.anim, NULL);
	SIT_SetValues(finder.anim,  SIT_Visible, prefs.anim, NULL);
	SIT_SetValues(finder.model, SIT_LeftObject, prefs.anim ? finder.anim : finder.tex, NULL);

	if (prefs.anim)
		animShow();
}

/* SITE_OnFinalize */
static int uiSaveChanges(SIT_Widget w, APTR cd, APTR ud)
{
	FILE *  out = fopen("Block2.txt", "wb");
	Block   b = blockGetNth(0);
	TexBank banks;
	int     i, j;
	fprintf(out, "# Settings\n");
	fprintf(out, "WndWidth=%d\n", prefs.width);
	fprintf(out, "WndHeight=%d\n", prefs.height);
	fprintf(out, "DetailMode=%d\n", prefs.detail);
	fprintf(out, "ShowBBox=%d\n", prefs.bbox);

	for (banks = HEAD(prefs.banks); banks; NEXT(banks))
		fprintf(out, "TexBank=%s\n", banks->path);

	fprintf(out, "LastTex=%s\n", prefs.lastTex);
	for (i = 0; i < prefs.nbBlocks; NEXT(b))
	{
		DATA16 tex;
		int    faces;
		i ++;
		// if (! detail && b->detailTex) faces |= b->detailFaces<<11;
		fprintf(out, "Block=FACES,%d,%s", b->faces & 63, detailTexNames[b->detailTex]);
		if (b->faces & BHDR_INVERTNORM) fprintf(out, ",INVERT"); else
		if (b->faces & BHDR_DUALSIDE)   fprintf(out, ",DUALSIDE");
		if (b->node.ln_Prev == NULL && prefs.rot90 > 0) fprintf(out, ",ROT90,%d", prefs.rot90 * 90);
		fprintf(out, ",SIZE,%g,%g,%g", b->size[0], b->size[1], b->size[2]);
		if (b->trans[0] != 0 || b->trans[1] != 0 || b->trans[2] != 0)
			fprintf(out, ",TR,%g,%g,%g", b->trans[0], b->trans[1], b->trans[2]);
		if (b->rotate[0] != 0 || b->rotate[1] != 0 || b->rotate[2] != 0)
			fprintf(out, ",ROT,%g,%g,%g", b->rotate[0], b->rotate[1], b->rotate[2]);
		if (b->cascade[0] != 0 || b->cascade[1] != 0 || b->cascade[2] != 0)
			fprintf(out, ",ROTCAS,%g,%g,%g", b->cascade[0], b->cascade[1], b->cascade[2]);
		if (b->rotateCenter == 0)
			fprintf(out, ",REF,%g,%g,%g", b->rotateFrom[0], b->rotateFrom[1], b->rotateFrom[2]);
		if (b->incFaceId)
			fprintf(out, ",INC_FACEID");
		if (b->custName)
			fprintf(out, ",NAME,%s", toJS(b->name));

		switch (b->detailTex) {
		case TEX_DETAIL:
			fprintf(out, ",TEX");
			for (tex = b->texUV, faces = b->faces & 63; faces; faces >>= 1)
			{
				if (faces & 1)
				{
					for (j = 0; j < 8; j += 2, tex += 2)
						fprintf(out, ",%d", tex[0] + tex[1] * 513);
				}
				else tex += 8;
			}
			break;
		case TEX_CUBEMAP:
			fprintf(out, ",TEX");
			for (tex = b->texUV, faces = 0; faces < 6; faces ++)
				for (j = 0; j < 8; j += 2, tex += 2)
					fprintf(out, ",%d", tex[0] + tex[1] * 513);
		}
		fputc('\n', out);
	}
	fclose(out);

	return 1;
}

static int uiHandleCommands(SIT_Widget w, APTR cd, APTR ud)
{
	Block b;
	switch ((int) ud) {
	case MENU_COPY: /* Ctrl+C */
		if (SIT_GetFocus() == finder.texUV)
			return 0;
		else
			blockCopy();
		break;
	case MENU_PASTE: /* Ctrl+V */
		if (blockPaste())
		{
			SIT_SetValues(finder.lab90, SIT_Title, rot90Names[prefs.rot90], NULL);
			SIT_SetValues((&finder.full)[prefs.detail], SIT_CheckState, True, NULL);
			SIT_SetValues(finder.subdet, SIT_Enabled, prefs.detail == 0, NULL);
			SIT_ListDeleteRow(finder.list, DeleteAllRows);
			for (b = blockGetNth(0); b; NEXT(b))
				boxAddOrUpdateList(b, True);
			SIT_SetValues(finder.list, SIT_SelectedIndex, 0, NULL);
			SIT_SetValues(finder.radio[0], SIT_CheckState, True, NULL);
			finder.faceEdit = finder.lastFaceSet = 0;
			blockGenVertexBuffer();
		}
		break;
	case MENU_RESETVIEW: /* F1: reset view matrix */
		renderResetView(NULL, NULL, NULL);
		break;
	case MENU_RESETTEX: /* Del: clear texture for a single primitive */
		b = boxGetCurrent();
		if (b)
		{
			if (prefs.detail == 0 && b->detailTex == TEX_DETAIL)
			{
				/* switch back to cubemap */
				Block prev;
				for (prev = (Block) b->node.ln_Prev; prev && prev->detailTex != TEX_CUBEMAP; PREV(prev));
				b->detailTex = prev ? TEX_CUBEMAP_INHERIT : TEX_CUBEMAP;
				if (b->ref != (b->detailTex == TEX_CUBEMAP))
				{
					b->ref = b->detailTex == TEX_CUBEMAP;
					boxResetName(b, -1);
				}
			}
			blockResetTex(b);
			blockGenVertexBuffer();
			finder.faceEdit = finder.lastFaceSet = 0;
			viewShowTexCoord(b, 0);
			SIT_SetValues(finder.radio[0], SIT_CheckState, True, NULL);
			boxUpdateTexCoord();
			SIT_ForceRefresh();
		}
		break;
	case MENU_ROT90TEX: /* R: rotate last texture */
		b = boxGetCurrent();
		if (b)
		{
			blockRotateTex(b, finder.lastFaceSet);
			blockGenVertexBuffer();
			boxUpdateTexCoord();
			SIT_ForceRefresh();
		}
		break;
	case MENU_MIRRORTEX: /* M: mirror last tex */
		b = boxGetCurrent();
		if (b)
		{
			blockMirrorTex(b, finder.lastFaceSet);
			blockGenVertexBuffer();
			boxUpdateTexCoord();
			SIT_ForceRefresh();
		}
		break;
	case MENU_CLEARTEX: /* Shift + del : clear all tex */
		blockDelAllTex();
		finder.faceEdit = finder.lastFaceSet = 0;
		SIT_SetValues(finder.radio[0], SIT_CheckState, True, NULL);
		blockGenVertexBuffer();
		boxUpdateTexCoord();
		SIT_ForceRefresh();
		b = boxGetCurrent();
		if (b) viewShowTexCoord(b, 0);
		break;
	case MENU_LOCATETEX: /* F2: zoom in the texture rectangle from current face */
		b = boxGetCurrent();
		if (b)
		{
			int rect[4];
			blockGetTexRect(b, finder.faceEdit, rect);
			viewZoomSelection(rect);
			SIT_ForceRefresh();
		}
		break;
	case MENU_COPYTEX: /* C: copy last tex coord to the face being edited */
		b = boxGetCurrent();
		if (b)
		{
			int face = finder.faceEdit - 1;
			int faces = b->faces;
			while (face >= 0 && (faces & (1<<face)) == 0) face --;
			if (face >= 0)
			{
				DATA16 tex = b->texUV + face * 8;
				memcpy(tex + 8, tex, 8 * sizeof *tex);
				uiEditNextFace(1);
				blockGenVertexBuffer();
				boxUpdateTexCoord();
				SIT_ForceRefresh();
			}
		}
		break;
	case MENU_PREVBOX:
		boxSelectRelative(-1);
		break;
	case MENU_NEXTBOX:
		boxSelectRelative(1);
		break;
	case MENU_PREVSIDE:
		uiEditNextFace(-1);
		break;
	case MENU_NEXTSIDE:
		uiEditNextFace(1);
		break;
	case MENU_DUPBOX:
		boxDup();
		break;
	case MENU_ANIMATE:
		uiToggleAnim(w);
		break;
	case MENU_RENAME:
		boxSetName(finder.list, NULL, finder.list);
		break;
	case MENU_ABOUT:
		uiYesNo(w,
			"TileFinder v1.2 for " PLATFORM "<br>"
			"Written by T.Pierron<br>"
			"<br>"
			"Under terms of BSD 2-clause license.<br>"
			"No warranty, use at your own risk.<br>"
			"<br>"
			"Compiled on "__DATE__" with " COMPILER,
			NULL, False
		);
	}
	return 1;
}

static int uiActiveCenter(SIT_Widget w, APTR cd, APTR ud)
{
	Block b = boxGetCurrent();

	if (b)
	{
		int checked, i;
		SIT_GetValues(w, SIT_CheckState, &checked, NULL);
		b->rotateCenter = checked;
		for (i = 0; i < 3; i ++)
			SIT_SetValues(finder.center[i], SIT_Enabled, ! checked, NULL);

		blockGenVertexBuffer();
		SIT_ForceRefresh();
	}

	return 1;
}

/* will shift all texture coordinates of everything block */
static int uiShiftTex(SIT_Widget w, APTR cd, APTR ud)
{
	int dx = finder.shiftDX;
	int dy = finder.shiftDY;

	Block b;
	for (b = blockGetNth(0); b; NEXT(b))
	{
		DATA16 tex, end;
		for (tex = b->texUV, end = EOT(b->texUV); tex < end; tex += 2)
			tex[0] += dx, tex[1] += dy;
	}

	boxUpdateTexCoord();
	blockGenVertexBuffer();
	viewShowTexCoord(boxGetCurrent(), finder.faceEdit);
	SIT_CloseDialog(w);

	return 1;
}

static int uiShowShift(SIT_Widget w, APTR cd, APTR ud)
{
	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_Escape, SITE_OnClose},
		{0}
	};

	SIT_Widget ask = SIT_CreateWidget("ask.bg", SIT_DIALOG, w,
		SIT_DialogStyles, SITV_Plain | SITV_Modal | SITV_Movable,
		SIT_Style,        "padding: 1em",
		SIT_AccelTable,   accels,
		NULL
	);

	finder.shiftDX = 0;
	finder.shiftDY = 0;
	SIT_Widget max = NULL;
	SIT_CreateWidgets(ask,
		"<label name=msg.alt title=", "Shift all texture coordinates by:", ">"
		"<editbox name=dx buddyLabel=", "Horizontal:", &max, "top=WIDGET,msg,0.5em editType=", SITV_Integer, "curValue=", &finder.shiftDX, ">"
		"<label name=unit1.alt title=px right=FORM top=MIDDLE,dx>"
		"<editbox name=dy buddyLabel=", "Vertical:", &max, "top=WIDGET,dx,0.5em editType=", SITV_Integer, "curValue=", &finder.shiftDY, ">"
		"<label name=unit2.alt title=px right=FORM top=MIDDLE,dy>"
		"<button name=ko.danger title=Cancel right=FORM top=WIDGET,dy,0.5em buttonType=", SITV_CancelButton, ">"
		"<button name=ok title=Ok right=WIDGET,ko,0.5em top=OPPOSITE,ko nextCtrl=ko buttonType=", SITV_DefaultButton, ">"
	);
	SIT_SetAttributes(ask,
		"<dx right=WIDGET,unit1,0.5em><dy right=WIDGET,unit2,0.5em>"
	);
	SIT_SetFocus(SIT_GetById(ask, "dx"));
	SIT_AddCallback(SIT_GetById(ask, "ok"), SITE_OnActivate, uiShiftTex, NULL);
	SIT_ManageWidget(ask);
	return 1;
}

/*
 * switch between texture files
 */
static int uiSetTexture(SIT_Widget w, APTR cd, APTR ud)
{
	TexBank bank = cd;
	if (strcasecmp(bank->path, prefs.lastTex))
	{
		CopyString(prefs.lastTex, bank->path, sizeof prefs.lastTex);
		renderSetTexture(bank->path, finder.texUVMapId);
		nvgDeleteImage(finder.nvgCtx, finder.nvgImgId);
		finder.nvgImgId = nvgCreateImage(finder.nvgCtx, (APTR) finder.texUVMapId, NVG_IMAGE_NEAREST | NVG_IMAGE_GLTEX);
		viewSetImage(finder.tex, finder.nvgImgId);
	}
	SIT_CloseDialog(w);
	return 1;
}

static int uiUseTexture(SIT_Widget w, APTR cd, APTR ud)
{
	int nth;
	SIT_GetValues(ud, SIT_SelectedIndex, &nth, NULL);
	if (nth >= 0)
	{
		TexBank bank;
		SIT_GetValues(ud, SIT_RowTag(nth), &bank, NULL);
		uiSetTexture(ud, bank, NULL);
	}
	return 1;
}

static int uiShowBanks(SIT_Widget w, APTR cd, APTR ud)
{
	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_Escape, SITE_OnClose},
		{0}
	};

	SIT_Widget ask = SIT_CreateWidget("ask.bg", SIT_DIALOG, w,
		SIT_DialogStyles, SITV_Plain | SITV_Modal | SITV_Movable,
		SIT_Style,        "padding: 1em",
		SIT_AccelTable,   accels,
		NULL
	);

	SIT_CreateWidgets(ask,
		"<label name=msg.alt title=", "Select the file to use for texturing:", ">"
		"<listbox name=texbank left=FORM right=FORM height=7em top=WIDGET,msg,0.5em listBoxFlags=", SITV_SelectAlways, ">"
		"<button name=ko.danger title=Cancel right=FORM top=WIDGET,texbank,0.5em buttonType=", SITV_CancelButton, ">"
		"<button name=ok title=Select right=WIDGET,ko,0.5em top=OPPOSITE,ko nextCtrl=ko buttonType=", SITV_DefaultButton, ">"
	);

	SIT_Widget list = SIT_GetById(ask, "texbank");
	TexBank bank;
	for (bank = HEAD(prefs.banks); bank; NEXT(bank))
	{
		int item = SIT_ListInsertItem(list, -1, bank, bank->path);
		if (strcasecmp(bank->path, prefs.lastTex) == 0)
			SIT_SetValues(list, SIT_SelectedIndex, item, NULL);
	}
	SIT_AddCallback(list, SITE_OnActivate, uiSetTexture, NULL);
	SIT_AddCallback(SIT_GetById(ask, "ok"), SITE_OnActivate, uiUseTexture, list);
	SIT_ManageWidget(ask);
	return 1;
}

static int uiShowHelp(SIT_Widget w, APTR cd, APTR ud)
{
	uiYesNo(w,
		"Global shortcuts:<br>"
		" &#x25cf; <key>F1</key>: reset 3d view.<br>"
		" &#x25cf; <key>F2</key>: locate texture of current face.<br>"
		" &#x25cf; <key>F3</key>: toggle 'Show active'.<br>"
		" &#x25cf; <key>F4</key>: rename current box.<br>"
		" &#x25cf; <key>Del</key>: clear texture of current box.<br>"
		" &#x25cf; <key>Shift+Del</key>: clear all textures.<br>"
		" &#x25cf; <key>Ctrl+C</key>: copy model into clipboard.<br>"
		" &#x25cf; <key>Ctrl+V</key>: parse model from clipboard.<br>"
		" &#x25cf; <key>R</key>: rotate texture of current face.<br>"
		" &#x25cf; <key>M</key>: mirror texture.<br>"
		" &#x25cf; <key>C</key>: copy texture onto next face.<br>"
		" &#x25cf; <key>Up, Down</key>: select previous/next box.<br>"
		" &#x25cf; <key>Left, Right</key>: select previous/next face.<br>"
		"<br>"
		"Texture view:<br>"
		" &#x25cf; <key>LMB</key>: drag the view.<br>"
		" &#x25cf; <key>MMB</key>: select tile or modify texture rect.<br>"
		" &#x25cf; <key>Wheel</key>: zoom in/out.<br>"
		" &#x25cf; <key>F</key>: show entire texture.<br>"
		"<br>"
		"3D view:<br>"
		" &#x25cf; <key>LMB</key>: rotate model.<br>"
		" &#x25cf; <key>RMB</key>: select face and box under the mouse.<br>"
		" &#x25cf; <key>Wheel</key>: zoom model in/out.<br>",
		NULL, False
	);
	return 1;
}


void uiCreate(SIT_Widget app)
{
	SIT_Widget max = NULL;
	SIT_Widget max2 = NULL;

	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_FlagAlt + SITK_F4, SITE_OnClose},
		{SITK_FlagShift + SITK_Delete, SITE_OnActivate, MENU_CLEARTEX, NULL, uiHandleCommands},
		{SITK_F3,             SITE_OnActivate, 0, "active"},
		{SITK_FlagCtrl + 'a', SITE_OnActivate, MENU_ABOUT,     NULL, uiHandleCommands},
		{SITK_FlagCtrl + 'c', SITE_OnActivate, MENU_COPY,      NULL, uiHandleCommands},
		{SITK_FlagCtrl + 'v', SITE_OnActivate, MENU_PASTE,     NULL, uiHandleCommands},
		{SITK_Delete,         SITE_OnActivate, MENU_RESETTEX,  NULL, uiHandleCommands},
		{SITK_F1,             SITE_OnActivate, MENU_RESETVIEW, NULL, uiHandleCommands},
		{SITK_F2,             SITE_OnActivate, MENU_LOCATETEX, NULL, uiHandleCommands},
		{SITK_F4,             SITE_OnActivate, MENU_RENAME,    NULL, uiHandleCommands},
		{SITK_Up,             SITE_OnActivate, MENU_PREVBOX,   NULL, uiHandleCommands},
		{SITK_Down,           SITE_OnActivate, MENU_NEXTBOX,   NULL, uiHandleCommands},
		{SITK_Left,           SITE_OnActivate, MENU_PREVSIDE,  NULL, uiHandleCommands},
		{SITK_Right,          SITE_OnActivate, MENU_NEXTSIDE,  NULL, uiHandleCommands},
		{'r',                 SITE_OnActivate, MENU_ROT90TEX,  NULL, uiHandleCommands},
		{'m',                 SITE_OnActivate, MENU_MIRRORTEX, NULL, uiHandleCommands},
		{'c',                 SITE_OnActivate, MENU_COPYTEX,   NULL, uiHandleCommands},
		{SITK_Escape,         SITE_OnClose},
		{0}
	};

	SIT_SetValues(app, SIT_AccelTable, accels, NULL);

	SIT_CreateWidgets(app,
		"<canvas name=bevel#div left=FORM,,NOPAD right=FORM,,NOPAD top=FORM,,NOPAD height=3.2em>"
			"<label name=msg1 title='Tile :' top=", SITV_AttachCenter, ">"
			"<label name=coords#msg left=WIDGET,msg1,0.5em title='-1, -1' top=", SITV_AttachCenter, ">"
			"<label name=msg2 title='Tex selection :' left=FORM,,7em top=", SITV_AttachCenter, ">"
			"<button name=full   title='Full block' radioGroup=1 radioID=0 checkState=", prefs.detail == 0,
			" buttonType=", SITV_ToggleButton, "left=WIDGET,msg2,0.5em top=", SITV_AttachCenter, ">"
			"<button name=detail title=Detail radioGroup=1 radioID=1 checkState=", prefs.detail != 0,
			" buttonType=", SITV_ToggleButton, "left=WIDGET,full,0.5em top=", SITV_AttachCenter, ">"
			"<label name=help title='<a href=#>Quick help</a>' right=FORM"
			" top=MIDDLE,full style='font-size: 0.9em'>"

			/* button for preview */
			"<button name=unit title='Show unit bbox' left=WIDGET,detail,2em buttonType=", SITV_ToggleButton,
			" checkState=", prefs.bbox, "top=", SITV_AttachCenter, ">"
			"<button name=reset title='Reset view' left=WIDGET,unit,0.5em top=OPPOSITE,unit>"
			"<button name=active title='Show active' curValue=", &finder.showActive, "buttonType=", SITV_ToggleButton,
			" left=WIDGET,reset,0.5em top=", SITV_AttachCenter, ">"
			"<button name=anim title=Animate buttonType=", SITV_ToggleButton, "left=WIDGET,active,0.5em top=", SITV_AttachCenter, ">"
			"<button name=bank.danger title='Tex bank' left=WIDGET,anim,0.5em top=", SITV_AttachCenter, "visible=", prefs.banks.lh_Head != NULL, ">"

		"</canvas>"

		/* primitives list */
		"<canvas name=toolbox#div right=FORM top=WIDGET,bevel,0.2em bottom=FORM>"
			"<button name=addbox title='Add box'>"
			"<button name=delbox.danger title=Del left=WIDGET,addbox,0.5em top=OPPOSITE,addbox>"
			"<button name=delall.danger title='Del all' left=WIDGET,delbox,0.5em top=OPPOSITE,addbox>"
			"<button name=dupbox title='Dup' left=WIDGET,delall,0.5em top=OPPOSITE,addbox>"

			"<listbox name=objects height=10em width=23em columnNames='Primitive\tSize\tPos' columnWidths='*\t*\t*'"
			" listBoxFlags=", SITV_SelectAlways, "top=WIDGET,addbox,0.5em>"

			/* primitive detail */
			"<label name=X.alt title=X><label name=Y.alt title=Y><label name=Z.alt title=Z>"
			"<editbox name=szx width=5em title=16 editType=", SITV_Float, "minValue=0 buddyLabel=", "SIZE:", &max, "top=WIDGET,objects,1.5em>"
			"<editbox name=szy width=5em title=16 editType=", SITV_Float, "minValue=0 top=OPPOSITE,szx left=WIDGET,szx,0.2em>"
			"<editbox name=szz width=5em title=16 editType=", SITV_Float, "minValue=0 top=OPPOSITE,szx left=WIDGET,szy,0.2em>"
			"<label name=info.alt title=px left=WIDGET,szz,0.2em top=MIDDLE,szz>"

			"<editbox name=trx width=5em title=0 editType=", SITV_Float, "buddyLabel=", "TR:", &max, "top=WIDGET,szx,0.5em>"
			"<editbox name=try width=5em title=0 editType=", SITV_Float, "top=OPPOSITE,trx left=WIDGET,trx,0.2em>"
			"<editbox name=trz width=5em title=0 editType=", SITV_Float, "top=OPPOSITE,trx left=WIDGET,try,0.2em>"
			"<label name=info.alt title=px left=WIDGET,trz,0.2em top=MIDDLE,trz>"

			"<editbox name=rotx width=5em title=0 editType=", SITV_Float, "minValue=-180 maxValue=180 buddyLabel=", "ROT:", &max, "top=WIDGET,trx,0.5em>"
			"<editbox name=roty width=5em title=0 editType=", SITV_Float, "minValue=-180 maxValue=180 top=OPPOSITE,rotx left=WIDGET,rotx,0.2em>"
			"<editbox name=rotz width=5em title=0 editType=", SITV_Float, "minValue=-180 maxValue=180 top=OPPOSITE,rotx left=WIDGET,roty,0.2em>"
			"<label name=info.alt title=deg. left=WIDGET,rotz,0.2em top=MIDDLE,rotz>"

			"<editbox name=rotcx width=5em title=0 enabled=0 editType=", SITV_Float, "buddyLabel=", "REF:", &max, "top=WIDGET,rotx,0.5em>"
			"<editbox name=rotcy width=5em title=0 enabled=0 editType=", SITV_Float, "top=OPPOSITE,rotcx left=WIDGET,rotcx,0.2em>"
			"<editbox name=rotcz width=5em title=0 enabled=0 editType=", SITV_Float, "top=OPPOSITE,rotcx left=WIDGET,rotcy,0.2em>"
			"<label name=info.alt title=px left=WIDGET,rotcz,0.2em top=MIDDLE,rotcx>"

			"<button name=center buttonType=", SITV_CheckBox, "title='Use box center' left=OPPOSITE,rotcx,0.2em top=WIDGET,rotcx,0.2em checkState=1>"
			"<button name=incface buttonType=", SITV_CheckBox, "title='Inc. face Id' right=OPPOSITE,rotcz top=OPPOSITE,center style='text-align: right'>"

			"<editbox name=tex readonly=1 title='' buddyLabel=", "TEX:", &max, "top=WIDGET,center,0.5em right=OPPOSITE,szz>"

			"<button name=faceS title='S' buddyLabel=", "FACES:", &max, "buttonType=", SITV_ToggleButton, "checkState=1 top=WIDGET,tex,0.5em>"
			"<button name=faceE title='E' buttonType=", SITV_ToggleButton, "checkState=1 top=OPPOSITE,faceS left=WIDGET,faceS,0.4em>"
			"<button name=faceN title='N' buttonType=", SITV_ToggleButton, "checkState=1 top=OPPOSITE,faceS left=WIDGET,faceE,0.4em>"
			"<button name=faceW title='W' buttonType=", SITV_ToggleButton, "checkState=1 top=OPPOSITE,faceS left=WIDGET,faceN,0.4em>"
			"<button name=faceT title='T' buttonType=", SITV_ToggleButton, "checkState=1 top=OPPOSITE,faceS left=WIDGET,faceW,0.4em>"
			"<button name=faceB title='B' buttonType=", SITV_ToggleButton, "checkState=1 top=OPPOSITE,faceS left=WIDGET,faceT,0.4em>"
			"<button name=faceI title='I' buttonType=", SITV_ToggleButton, "checkState=0 top=OPPOSITE,faceS left=WIDGET,faceB,0.4em tooltip='Invert normals'>"
			"<button name=faceD.thin title='D' buttonType=", SITV_ToggleButton, "checkState=0 top=OPPOSITE,faceS left=WIDGET,faceI,0.4em tooltip='Double-sided faces'>"

			"<label  name=beditS title='EDIT:' left=OPPOSITE,bszx maxWidth=bfaceS>"
			"<button name=editS buttonType=", SITV_RadioButton, "checkState=1 top=WIDGET,faceS,0.5em left=MIDDLE,faceS>"
			"<button name=editE buttonType=", SITV_RadioButton, "top=OPPOSITE,editS left=MIDDLE,faceE>"
			"<button name=editN buttonType=", SITV_RadioButton, "top=OPPOSITE,editS left=MIDDLE,faceN>"
			"<button name=editW buttonType=", SITV_RadioButton, "top=OPPOSITE,editS left=MIDDLE,faceW>"
			"<button name=editT buttonType=", SITV_RadioButton, "top=OPPOSITE,editS left=MIDDLE,faceT>"
			"<button name=editB buttonType=", SITV_RadioButton, "top=OPPOSITE,editS left=MIDDLE,faceB>"
			"<button name=editI buttonType=", SITV_RadioButton, "visible=0>"

			"<button name=subdetail enabled=", prefs.detail == 0, "buttonType=", SITV_CheckBox,
			" title='Force detail selection' top=WIDGET,editS,0.2em left=OPPOSITE,editS>"

			"<frame name=sep left=OPPOSITE,objects top=WIDGET,subdetail,0.8em title='Global :' right=FORM/>"

			"<button name=rotm90 title='-90' buddyLabel=", "ORIENT:", &max, "top=WIDGET,sep,0.5em>"
			"<button name=rot90 title='+90' top=OPPOSITE,rotm90 left=WIDGET,rotm90,0.5em>"
			"<button name=shift title='Shift tex' top=OPPOSITE,rotm90 left=WIDGET,rot90,0.5em>"
			"<label name=brot90 title=", rot90Names[prefs.rot90], " left=WIDGET,shift,0.5em top=MIDDLE,rot90>"

			"<editbox name=rezx width=5em title=0 editType=", SITV_Integer, "minValue=-180 maxValue=180 buddyLabel=", "ROT:", &max, "top=WIDGET,rotm90,0.5em>"
			"<editbox name=rezy width=5em title=0 editType=", SITV_Integer, "minValue=-180 maxValue=180 top=OPPOSITE,rezx left=WIDGET,rezx,0.2em>"
			"<editbox name=rezz width=5em title=0 editType=", SITV_Integer, "minValue=-180 maxValue=180 top=OPPOSITE,rezx left=WIDGET,rezy,0.2em>"
			"<label name=info.alt title=deg. left=WIDGET,rezz,0.2em top=MIDDLE,rezz>"

			"<button name=copy title=Copy top=WIDGET,rezx,0.5em left=OPPOSITE,objects>"
			"<button name=paste.danger title=Paste top=OPPOSITE,copy left=WIDGET,copy,1em>"
			"<button name=biome title='Apply biome color' left=WIDGET,paste,1em top=MIDDLE,paste buttonType=", SITV_CheckBox, ">"
		"</canvas>"

		/* preview/texture area */
		"<canvas name=cont#div left=FORM bottom=FORM top=WIDGET,bevel,0.2em right=WIDGET,toolbox,0.5em>"

			"<canvas name=texture extra=", sizeof (struct ViewImage_t), "left=FORM top=FORM bottom=FORM"
			" right=", SITV_AttachPosition, SITV_AttachPos(49), 0, "/>"
			"<canvas name=preview composited=1 left=WIDGET,texture,0.5em top=FORM bottom=FORM right=FORM/>"

			"<canvas name=animate#div visible=0 left=FORM top=FORM bottom=FORM right=", SITV_AttachPosition, SITV_AttachPos(49), 0, ">"
				"<label name=msg title='Applies to:'>"
				"<listbox name=applyto left=FORM right=", SITV_AttachPosition, SITV_AttachPos(30), 0, "top=WIDGET,msg,0.5em"
				" height=10em listBoxFlags=", SITV_SelectAlways, ">"
				"<label name=msg2 title=Interpolation: left=WIDGET,applyto,0.5em>"
				"<canvas name=graph#div.lcd left=WIDGET,applyto,0.5em top=WIDGET,msg2,0.5em right=FORM bottom=OPPOSITE,applyto/>"
				"<button name=linear radioID=0 radioGroup=2 buttonType=", SITV_ToggleButton, "checkState=1"
				" title=Linear top=WIDGET,graph,0.5em left=OPPOSITE,graph>"
				"<button name=curve radioID=1 radioGroup=2 buttonType=", SITV_ToggleButton, "title=Curve top=OPPOSITE,linear left=WIDGET,linear,0.5em>"
				"<button name=addpt title='Add point' top=OPPOSITE,linear left=WIDGET,curve,0.5em>"

				"<listbox columnNames='Parameter\tStart\tEnd' name=params listBoxFlags=", SITV_SelectAlways, "left=FORM"
				" right=FORM height=14.5em top=WIDGET,linear,1em>"

				"<editbox name=time minValue=1 width=7em buddyLabel=", "Time:", &max2, "editType=", SITV_Integer, "top=WIDGET,params,0.5em>"
				"<label name=info.alt title=msec. left=WIDGET,time,0.2em top=MIDDLE,time>"

				"<button name=repeat buttonType=", SITV_CheckBox, "title='Repeat animation' top=MIDDLE,time left=WIDGET,info,0.5em>"
				"<button name=play title=Play top=WIDGET,time,0.5em>"
				"<button name=stop.danger title=Stop top=OPPOSITE,play left=WIDGET,play,0.5em>"
				"<button name=cpanim title=Copy top=OPPOSITE,play left=WIDGET,stop,1em>"
				"<button name=psanim.danger title=Paste top=OPPOSITE,play left=WIDGET,cpanim,0.5em>"
			"</canvas>"

		"</canvas>"

	);
	SIT_SetAttributes(app,
		"<X left=MIDDLE,szx bottom=WIDGET,szx,0.1em>"
		"<Y left=MIDDLE,szy bottom=WIDGET,szy,0.1em>"
		"<Z left=MIDDLE,szz bottom=WIDGET,szz,0.1em>"
		"<beditS top=MIDDLE,editS><bdetailS top=MIDDLE,detailS>"
	);

	int i;
	finder.app     = app;
	finder.list    = SIT_GetById(app, "objects");
	finder.full    = SIT_GetById(app, "full");
	finder.manual  = SIT_GetById(app, "detail");
	finder.tex     = SIT_GetById(app, "texture");
	finder.texUV   = SIT_GetById(app, "tex");
	finder.model   = SIT_GetById(app, "preview");
	finder.lab90   = SIT_GetById(app, "brot90");
	finder.coords  = SIT_GetById(app, "coords");
	finder.active  = SIT_GetById(app, "active");
	finder.subdet  = SIT_GetById(app, "subdetail");
	finder.anim    = SIT_GetById(app, "animate");
	finder.incface = SIT_GetById(app, "incface");
	SIT_GetValues(app, SIT_NVGcontext, &finder.nvgCtx, NULL);
	for (i = 0; i <= 7; i ++)
	{
		static TEXT sides[] = "SENWTBID";
		TEXT name[8];
		sprintf(name, "face%c", sides[i]); finder.faces[i] = SIT_GetById(app, name);
		sprintf(name, "edit%c", sides[i]); finder.radio[i] = SIT_GetById(app, name);

		SIT_AddCallback(finder.faces[i], SITE_OnActivate, uiToggleFaceVisibility, (APTR) i);
		SIT_AddCallback(finder.radio[i], SITE_OnActivate, uiSetFaceEdit, (APTR) i);
	}

	for (i = 0; i < 3; i ++)
	{
		TEXT name[8];
		TEXT axis = "xyz"[i];
		sprintf(name, "sz%c",  axis); SIT_AddCallback(SIT_GetById(app, name), SITE_OnChange, uiSetBlockValue, (APTR) i);
		sprintf(name, "tr%c",  axis); SIT_AddCallback(SIT_GetById(app, name), SITE_OnChange, uiSetBlockValue, (APTR) (i+3));
		sprintf(name, "rot%c", axis); SIT_AddCallback(SIT_GetById(app, name), SITE_OnChange, uiSetBlockValue, (APTR) (i+6));
		sprintf(name, "rez%c", axis); SIT_AddCallback(SIT_GetById(app, name), SITE_OnChange, uiSetBlockValue, (APTR) (i+9));

		sprintf(name, "rotc%c", axis);
		finder.center[i] = SIT_GetById(app, name);
		SIT_AddCallback(finder.center[i], SITE_OnChange, uiSetBlockValue, (APTR) (i+12));
	}

	finder.texUVMapId = renderSetTexture(prefs.lastTex, 0);
	finder.nvgImgId = nvgCreateImage(finder.nvgCtx, (APTR) finder.texUVMapId, NVG_IMAGE_NEAREST | NVG_IMAGE_GLTEX);
	viewInit(finder.tex, finder.nvgImgId);

	SIT_AddCallback(app,            SITE_OnFinalize,  uiSaveChanges, NULL);
	SIT_AddCallback(finder.manual,  SITE_OnActivate,  uiToggleDetail, (APTR) 1);
	SIT_AddCallback(finder.full,    SITE_OnActivate,  uiToggleDetail, NULL);
	SIT_AddCallback(finder.list,    SITE_OnChange,    boxSelect, NULL);
	SIT_AddCallback(finder.list,    SITE_OnClick,     boxSetName, NULL);
	SIT_AddCallback(finder.model,   SITE_OnResize,    renderGetVPSize, NULL);
	SIT_AddCallback(finder.model,   SITE_OnClickMove, renderRotateView, NULL);
	SIT_AddCallback(finder.active,  SITE_OnActivate,  renderSetFeature, (APTR) 1);
	SIT_AddCallback(finder.subdet,  SITE_OnActivate,  uiSetSubDetail, NULL);
	SIT_AddCallback(finder.incface, SITE_OnActivate,  uiSetIncFaceId, NULL);
	SIT_AddCallback(SIT_GetById(app, "unit"),    SITE_OnActivate, uiToggleUnitBBox, NULL);
	SIT_AddCallback(SIT_GetById(app, "reset"),   SITE_OnActivate, renderResetView, NULL);
	SIT_AddCallback(SIT_GetById(app, "addbox"),  SITE_OnActivate, boxAddDefault, (APTR) 1);
	SIT_AddCallback(SIT_GetById(app, "delbox"),  SITE_OnActivate, boxDel, NULL);
	SIT_AddCallback(SIT_GetById(app, "delall"),  SITE_OnActivate, boxAskDelAll, NULL);
	SIT_AddCallback(SIT_GetById(app, "rot90"),   SITE_OnActivate, uiRotate90, NULL);
	SIT_AddCallback(SIT_GetById(app, "rotm90"),  SITE_OnActivate, uiRotate90, (APTR) 1);
	SIT_AddCallback(SIT_GetById(app, "biome"),   SITE_OnActivate, renderSetFeature, NULL);
	SIT_AddCallback(SIT_GetById(app, "dupbox"),  SITE_OnActivate, uiHandleCommands, (APTR) MENU_DUPBOX);
	SIT_AddCallback(SIT_GetById(app, "copy"),    SITE_OnActivate, uiHandleCommands, (APTR) MENU_COPY);
	SIT_AddCallback(SIT_GetById(app, "paste"),   SITE_OnActivate, uiHandleCommands, (APTR) MENU_PASTE);
	SIT_AddCallback(SIT_GetById(app, "anim"),    SITE_OnActivate, uiHandleCommands, (APTR) MENU_ANIMATE);
	SIT_AddCallback(SIT_GetById(app, "center"),  SITE_OnActivate, uiActiveCenter, NULL);
	SIT_AddCallback(SIT_GetById(app, "shift"),   SITE_OnActivate, uiShowShift, NULL);
	SIT_AddCallback(SIT_GetById(app, "bank"),    SITE_OnActivate, uiShowBanks, NULL);
	SIT_AddCallback(SIT_GetById(app, "help"),    SITE_OnActivate, uiShowHelp, NULL);

	animInit(finder.anim);

	/* initial box list */
	Block box = blockGetNth(0);
	if (box == NULL)
	{
		/* add a dummy block if there are none */
		box = blockAddDefaultBox();
	}
	while (box)
	{
		boxAddOrUpdateList(box, True);
		if (box->node.ln_Prev == NULL)
			SIT_SetValues(finder.list, SIT_RowSel(0), True, NULL);
		NEXT(box);
	}
	blockGenVertexBuffer();
}

/*
 * simple yes/no dialog
 */
static int uiCloseDialog(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_CloseDialog(w);
	return 1;
}

/* ask a question to the user with Yes/No as possible answer */
void uiYesNo(SIT_Widget parent, STRPTR msg, SIT_CallProc cb, Bool yesNo)
{
	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_Escape, SITE_OnClose},
		{0}
	};

	SIT_Widget ask = SIT_CreateWidget("ask.bg", SIT_DIALOG, parent,
		SIT_DialogStyles, SITV_Plain | SITV_Modal | SITV_Movable,
		SIT_Style,        "padding: 1em",
		SIT_AccelTable,   accels,
		NULL
	);

	SIT_CreateWidgets(ask, "<label name=msg.alt title=", msg, ">");

	if (yesNo)
	{
		SIT_CreateWidgets(ask,
			"<button name=ok.danger title=Yes top=WIDGET,msg,0.8em buttonType=", SITV_DefaultButton, ">"
			"<button name=ko title=No top=OPPOSITE,ok right=FORM buttonType=", SITV_CancelButton, ">"
		);
		SIT_SetAttributes(ask, "<ok right=WIDGET,ko,1em>");
	}
	else /* only a "no" button */
	{
		SIT_CreateWidgets(ask, "<button name=ok right=FORM title=Ok top=WIDGET,msg,0.8em buttonType=", SITV_DefaultButton, ">");
		cb = uiCloseDialog;
	}
	SIT_AddCallback(SIT_GetById(ask, "ok"), SITE_OnActivate, cb, NULL);
	SIT_ManageWidget(ask);
}
