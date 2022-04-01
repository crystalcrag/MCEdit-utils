/*
 * Frustum.c : SITGL application to test frustum culling of MCEdit v2 in 2d (XY plane).
 *
 * this part is dedicated to user interface management.
 *
 * Written by T.Pierron, jan 2022.
 */

#include <stdio.h>
#include <SDL/SDL.h>
#include <GL/GL.h>
#include "nanovg.h"
#include <malloc.h>
#include <math.h>
#include "SIT.h"
#include "frustum.h"
#include "maps.h"

struct Frustum_t globals;
static Map map;

static int SDLKtoSIT[] = {
	SDLK_HOME,      SITK_Home,
	SDLK_END,       SITK_End,
	SDLK_PAGEUP,    SITK_PrevPage,
	SDLK_PAGEDOWN,  SITK_NextPage,
	SDLK_UP,        SITK_Up,
	SDLK_DOWN,      SITK_Down,
	SDLK_LEFT,      SITK_Left,
	SDLK_RIGHT,     SITK_Right,
	SDLK_LSHIFT,    SITK_LShift,
	SDLK_RSHIFT,    SITK_RShift,
	SDLK_LAST,      SITK_LAlt,
	SDLK_RALT,      SITK_RAlt,
	SDLK_LCTRL,     SITK_LCtrl,
	SDLK_RCTRL,     SITK_RCtrl,
	SDLK_LSUPER,    SITK_LCommand,
	SDLK_RSUPER,    SITK_RCommand,
	SDLK_MENU,      SITK_AppCommand,
	SDLK_RETURN,    SITK_Return,
	SDLK_CAPSLOCK,  SITK_Caps,
	SDLK_INSERT,    SITK_Insert,
	SDLK_DELETE,    SITK_Delete,
	SDLK_NUMLOCK,   SITK_NumLock,
	SDLK_PRINT,     SITK_Impr,
	SDLK_F1,        SITK_F1,
	SDLK_F2,        SITK_F2,
	SDLK_F3,        SITK_F3,
	SDLK_F4,        SITK_F4,
	SDLK_F5,        SITK_F5,
	SDLK_F6,        SITK_F6,
	SDLK_F7,        SITK_F7,
	SDLK_F8,        SITK_F8,
	SDLK_F9,        SITK_F9,
	SDLK_F10,       SITK_F10,
	SDLK_F11,       SITK_F11,
	SDLK_F12,       SITK_F12,
	SDLK_F13,       SITK_F13,
	SDLK_F14,       SITK_F14,
	SDLK_F15,       SITK_F15,
	SDLK_BACKSPACE, SITK_BackSpace,
	SDLK_ESCAPE,    SITK_Escape,
	SDLK_SPACE,     SITK_Space,
	SDLK_HELP,      SITK_Help,
};

static int SDLMtoSIT(int mod)
{
	int ret = 0;
	if (mod & KMOD_CTRL)  ret |= SITK_FlagCtrl;
	if (mod & KMOD_SHIFT) ret |= SITK_FlagShift;
	if (mod & KMOD_ALT)   ret |= SITK_FlagAlt;
	return ret;
}


/* midpoint circle algorithm in action here */
static void setBrushRange(int radius)
{
	int x = (radius+1) >> 1, y = 0, P = 1 - x, pos;

	memset(globals.brushRange, 0, sizeof globals.brushRange);

	while (x > y)
	{
		y ++;
		if (P <= 0)
		{
			/* mid-point is inside or on the perimeter */
			P += 2*y + 1;
		}
		else /* mid-point is outside perimeter */
		{
			x --;
			P += 2*y - 2*x + 1;
		}
		if (x < y) break;
		pos = y - 1; if (globals.brushRange[pos] < x) globals.brushRange[pos] = x;
		pos = x - 1; if (globals.brushRange[pos] < y) globals.brushRange[pos] = y;
	}

	/* we only have a quarter of a cicle at this point: make it a complete one */
	for (y = (radius-1) >> 1, x = radius-1; y >= 0; y --, x --)
	{
		pos = (globals.brushRange[y] << 1) - (radius&1);
		globals.brushRange[x] = ((radius - pos) >> 1) | (pos << 8);
	}
	for (y = x + 1 + (radius&1); x >= 0; x --, y ++)
		globals.brushRange[x] = globals.brushRange[y];

	/* convert to bitmap, then to vector */
	pos = radius * radius;
	DATA8 bitmap = alloca(pos);
	memset(bitmap, 0, pos);

	for (y = 0; y < radius; y ++)
	{
		x = globals.brushRange[y];
		memset(bitmap + radius * y + (x & 0xff), 128, x >> 8);
		#if 0
		fwrite("                              ", x & 0xff, 1, stdout);
		fwrite("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", x >> 8,   1, stdout);
		fputc('\n', stdout);
		#endif
	}

	vectorize(&globals.brushVector, bitmap, radius, radius);
}

static void setViewMat(void);

#define TOVPX(X)      (paint->x + MARGIN + (X) * scalex)
#define TOVPY(Y)      (paint->y + MARGIN + h - (Y) * scaley)

#define TOCELLX(X)    (((X) - MARGIN) / globals.vpWidth * IMAGESIZEX)
#define TOCELLY(Y)    ((globals.vpHeight - ((Y) - MARGIN)) / globals.vpHeight * IMAGESIZEY)

#define FROMCELLX(X)  ((X) * globals.vpWidth / IMAGESIZEX + MARGIN)
#define FROMCELLY(Y)  (globals.vpHeight + MARGIN - (Y) * globals.vpHeight / IMAGESIZEY)

#define ARROW         10

static int paintFrustum(SIT_Widget view, APTR call_data, APTR ud)
{
	SIT_OnPaint * paint = call_data;
	NVGcontext *  vg = paint->nvg;
	int i;

	/* chunk grid */
	nvgStrokeColorRGBA8(vg, "\x20\xCC\x20\xff");
	nvgBeginPath(vg);
	float w = globals.vpWidth  = paint->w - 2*MARGIN;
	float h = globals.vpHeight = paint->h - 2*MARGIN;
	float x, y;
	for (i = 0; i <= CELLMAPX; i ++)
	{
		x = paint->x + w * i / CELLMAPX + MARGIN;
		nvgMoveTo(vg, x, paint->y + MARGIN);
		nvgLineTo(vg, x, paint->y + h + MARGIN);
	}

	for (i = 0; i <= CELLMAPY; i ++)
	{
		y = paint->y + h * i / CELLMAPY + MARGIN;
		nvgMoveTo(vg, paint->x + MARGIN, y);
		nvgLineTo(vg, paint->x + w + MARGIN, y);
	}
	nvgStroke(vg);

	float scalex = w / IMAGESIZEX;
	float scaley = h / IMAGESIZEY;

	/* chunk bitmap */
	x = paint->x + MARGIN;
	y = paint->y + MARGIN;
	nvgFillColorRGBA8(vg, "\x14\xfd\xce\xff");
	nvgBeginPath(vg);
	nvgRect(vg, x, y, w, h);
	NVGpaint fill = nvgImagePattern(vg, x, y, w, h, 0, globals.nvgChunkImg, 1);
	nvgGetCurTextColor(vg, &fill.innerColor);
	nvgFillPaint(vg, fill);
	nvgFill(vg);

	Chunk chunk;
	for (i = 0, chunk = map->center; i < CELLMAPX; i ++, chunk ++)
	{
		int j;
		for (j = 0; j < CELLMAPY; j ++)
		{
			static uint8_t coords[] = { /* 15 cnx, but only 6 used in 2d */
			/* SE */ 0,0,0,0,
			/* SN */ 0,0,0,0,
			/* SW */ 0,0,0,0,
			/* ST */ 0,0,0,0,
			/* SB */ 0,0,0,0,
			/* EN */ 0,0,0,0,
			/* EW */ 0, CELLSZ/2,        CELLSZ, CELLSZ/2,
			/* ET */ CELLSZ, CELLSZ/2,   CELLSZ/2, CELLSZ,
			/* EB */ CELLSZ, CELLSZ/2,   CELLSZ/2, 0,
			/* NW */ 0,0,0,0,
			/* NT */ 0,0,0,0,
			/* NB */ 0,0,0,0,
			/* WT */ 0, CELLSZ/2,        CELLSZ/2, CELLSZ,
			/* WB */ 0, CELLSZ/2,        CELLSZ/2, 0,
			/* TB */ CELLSZ/2, 0,        CELLSZ/2, CELLSZ,
			};
			ChunkData cd = chunk->layer[j];
			if (! cd) continue;
			/*
			 * highlight chunks whose outflags[] have to be computed: most expensive operation:
			 * worse case being 8 mat4 / vec4 product per chunk
			 */
			if (cd->frame == map->frame)
			{
				/* chunk is in frustum */
				nvgStrokeColorRGBA8(vg, cd->slot > 0 ? "\xff\x55\x55\xff" : "\xff\xff\xff\xff");
				nvgBeginPath(vg);
				x = TOVPX(chunk->X);
				y = TOVPY(cd->Y + CELLSZ);
				float szw = TOVPX(chunk->X + CELLSZ) - x;
				float szh = TOVPY(cd->Y) - y;
				nvgRect(vg, x, y, szw, szh);
				nvgStroke(vg);
				if (cd->slot > 0)
				{
					TEXT number[10];
					sprintf(number, "%d", cd->slot);
					nvgText(vg, x + (szw - nvgTextBounds(vg, 0, 0, number, NULL, NULL)) * 0.5f, y + szh * 0.5f, number, NULL);
				}
			}
			/* chunk cnx graph (cave culling) */
			if (globals.showCnx && cd->cnxGraph > 0 && cd->slot == 0 /* not a fake chunk */)
			{
				int cnx;
				DATA8 coord;
				for (cnx = cd->cnxGraph, coord = coords; cnx > 0; cnx >>= 1, coord += 4)
				{
					if (coord[2] == 0 || (cnx & 1) == 0) continue;
					nvgStrokeColorRGBA8(vg, "\xff\x20\x20\xff");
					nvgBeginPath(vg);
					nvgMoveTo(vg, x = TOVPX(chunk->X + coord[0]), y = TOVPY(cd->Y + coord[1]));
					nvgLineTo(vg, x = TOVPX(chunk->X + coord[2]), y = TOVPY(cd->Y + coord[3]));
					nvgStroke(vg);
				}
			}
		}
	}

	w /= CELLMAPX;

	nvgScissor(vg, paint->x, paint->y, paint->w, paint->h);
	nvgStrokeColorRGBA8(vg, "\x14\xfd\xce\xff");
	nvgTextAlign(vg, NVG_ALIGN_TOP);

	/* show axis */
	x = TOVPX(CELLSZ*0.3f);
	y = TOVPY(CELLSZ*1.3f);
	nvgBeginPath(vg);
	nvgMoveTo(vg, x, y - w);
	nvgLineTo(vg, x, y);
	nvgLineTo(vg, x + w, y);
	y -= w;
	nvgMoveTo(vg, x - ARROW, y + ARROW);
	nvgLineTo(vg, x, y);
	nvgLineTo(vg, x + ARROW, y + ARROW);
	y += w; x += w;
	nvgMoveTo(vg, x - ARROW, y - ARROW);
	nvgLineTo(vg, x, y);
	nvgLineTo(vg, x - ARROW, y + ARROW);
	nvgStroke(vg);

	float sz = nvgTextBounds(vg, 0, 0, "Y", NULL, NULL) * 0.5f;
	nvgText(vg, x + ARROW, y - paint->fontSize * 0.5f, "X", NULL);
	nvgText(vg, x - w - sz, y - w - paint->fontSize, "Y", NULL);

	/* show frustum (XY plane only) */
	vec pt = globals.frustum;

	nvgStrokeColorRGBA8(vg, "\x20\xCC\x20\xff");
	nvgBeginPath(vg);
	nvgMoveTo(vg, TOVPX(globals.camera[VX]), TOVPY(globals.camera[VY]));
	nvgLineTo(vg, TOVPX(pt[4]), TOVPY(pt[5]));
	nvgStroke(vg);

	nvgStrokeColorRGBA8(vg, "\x14\xfd\xce\xff");
	nvgBeginPath(vg);
	nvgMoveTo(vg, TOVPX(globals.camera[VX]), TOVPY(globals.camera[VY]));
	nvgLineTo(vg, TOVPX(pt[0]), TOVPY(pt[1]));
	nvgLineTo(vg, TOVPX(pt[2]), TOVPY(pt[3]));
	nvgClosePath(vg);
	nvgStroke(vg);

	TEXT buffer[64];
	if (globals.drawMode == 1)
	{
		/* show outline of brush */
		DATA16 coords = globals.brushVector.coords;
		DATA16 eof    = coords + globals.brushVector.count;
		while (coords < eof)
		{
			x = TOVPX(globals.brushX - (globals.brushSize>>1) + coords[1]);
			y = TOVPY(globals.brushY + (globals.brushSize>>1) + coords[2]);

			nvgBeginPath(vg);
			nvgMoveTo(vg, roundf(x)+0.5f, roundf(y)+0.5f);
			for (i = coords[0] & 0x7fff, coords += 3; i > 0; i--, coords ++)
			{
				int size = (coords[0] >> 4) + 1;
				switch (coords[0] & 15) {
				case 1: y += size * scaley; break;
				case 2: x -= size * scalex; break;
				case 4: y -= size * scaley; break;
				case 8: x += size * scalex; break;
				}
				nvgLineTo(vg, roundf(x)+0.5f, roundf(y)+0.5f);
			}
			nvgStroke(vg);
		}
		/* mouse coords in "world" space */
		i = sprintf(buffer, "X: %.1f Y: %.1f", globals.posX, globals.posY);
	}
	else
	{
		/* show chunk coord */
		i = sprintf(buffer, "Chunk: %d, %d", (int) globals.posX >> 4, (int) globals.posY >> 4);
	}
	nvgText(vg, paint->x + MARGIN, paint->y + MARGIN, buffer, buffer+i);

	return 1;
}

static void adjustBrushSize(int dir)
{
	int size = globals.brushSize + dir;
	if (size < 5) size = 5;
	if (size > 30) size = 30;
	if (size != globals.brushSize)
	{
		globals.brushSize = size;
		setBrushRange(size);
		SIT_SetValues(globals.brush, SIT_Title | XfMt, "%d", size, NULL);
	}
}

static void adjustFOV(int step)
{
	/* adjust FOV */
	int fov = globals.FOV + step;
	if (fov < 20) fov = 20;
	if (fov > 90) fov = 90;
	if (fov != globals.FOV)
	{
		globals.FOV = fov;
		setViewMat();
		SIT_SetValues(globals.brush, SIT_Title | XfMt, "%d", fov, NULL);
	}
}


/* paint a single dot */
static void paintBrush(int x, int y, int color)
{
	y = IMAGESIZEY - y;
	x -= globals.brushSize >> 1;
	y -= globals.brushSize >> 1;

	DATA16 points;
	int i;
	for (i = globals.brushSize, points = globals.brushRange; i > 0; i --, points ++, y ++)
	{
		if (y < 0) continue;
		if (y >= IMAGESIZEY) break;
		int x1 = x + (points[0] & 0xff);
		int x2 = x1 + (points[0] >> 8);
		if (x1 < 0) x1 = 0;
		if (x2 > IMAGESIZEX) x2 = IMAGESIZEX;
		if (x1 >= x2) continue;
		memset(globals.chunkBitmap + x1 + y * IMAGESIZEX, color, x2 - x1);
	}
	if (globals.nvgCtx && (color & 0x100) == 0)
	{
		mapUpdateCnxGraph(map, globals.chunkBitmap, x, y, globals.brushSize, globals.brushSize);
		nvgUpdateImage(globals.nvgCtx, globals.nvgChunkImg, globals.chunkBitmap);
	}
}

/* paint a line with dots using bresenham line drawing */
static void paintLineBrush(int x0, int y0, int x1, int y1, int color)
{
	int rect[] = {1e6, 1e6, -1e6, -1e6};
	int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
	int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
	int err = dx+dy;
	int sz = globals.brushSize >> 1;

	/* bresenham line drawing */
	for(;;)
	{
		paintBrush(x0, y0, color | 0x100);
		int xs = x0 - sz;
		int ys = (IMAGESIZEY - y0) + sz;
		if (xs < rect[0]) rect[0] = xs;
		if (ys < rect[1]) rect[1] = ys;
		if (xs + globals.brushSize > rect[2]) rect[2] = xs + globals.brushSize;
		if (ys + globals.brushSize > rect[3]) rect[3] = ys + globals.brushSize;

		if (x0 == x1 && y0 == y1) break;
		int e2 = err << 1;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
	if (globals.nvgCtx)
	{
		mapUpdateCnxGraph(map, globals.chunkBitmap, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1]);
		nvgUpdateImage(globals.nvgCtx, globals.nvgChunkImg, globals.chunkBitmap);
	}
}

static int handleMouse(SIT_Widget w, APTR cd, APTR ud)
{
	static int startX, startY, color;
	SIT_OnMouse * msg = cd;
	switch (msg->state) {
	case SITOM_ButtonPressed:
		switch (msg->button) {
		case SITOM_ButtonLeft:
			if (globals.drawMode == 0)
			{
				/* move camera position and angle */
				float Y = TOCELLY(msg->y);
				if (Y < 16) return 0;
				globals.camera[VX] = TOCELLX(msg->x);
				globals.camera[VY] = Y;
				setViewMat();
				startX = msg->x;
				startY = msg->y;
			}
			else paintBrush(globals.brushX, globals.brushY, color = 128); /* draw on chunk bitmap */
			SIT_ForceRefresh();
			return 2;
			break;
		case SITOM_ButtonRight:
			if (globals.drawMode == 0)
			{
				/* change camera angle only */
				startX = FROMCELLX(globals.camera[VX]);
				startY = FROMCELLY(globals.camera[VY]);
				goto set_camera;
			}
			/* clear chunk bitmap */
			paintBrush(globals.brushX, globals.brushY, color = 0);
			SIT_ForceRefresh();
			return 2;
		case SITOM_ButtonWheelDown:
			if (globals.drawMode == 0)
				adjustFOV(-5);
			else
				adjustBrushSize(-1);
			break;
		case SITOM_ButtonWheelUp:
			if (globals.drawMode == 0)
				adjustFOV(5);
			else
				adjustBrushSize(1);
		default:
			break;
		}
		break;
	case SITOM_Move:
		{
			/* track which "pixel" of the chunks bitmap the mouse is over */
			int cellX = TOCELLX(msg->x);
			int cellY = TOCELLY(msg->y);
			/* chunk coords */
			globals.posX = TOCELLX((float)msg->x);
			globals.posY = TOCELLY((float)msg->y);

			if (globals.drawMode == 1)
			{
				if (cellX != globals.brushX || cellY != globals.brushY)
				{
					globals.brushX = cellX;
					globals.brushY = cellY;
					SIT_ForceRefresh();
				}
			}
			else if ((cellX >> 4) != (globals.brushX >> 4) ||
			         (cellY >> 4) != (globals.brushY >> 4))
			{
				globals.brushX = cellX;
				globals.brushY = cellY;
				SIT_ForceRefresh();
			}
		}
		break;
	case SITOM_CaptureMove: /* move mouse while button is held */
		if (globals.drawMode == 1)
		{
			/* draw/clear with current brush */
			int cellX = TOCELLX(msg->x);
			int cellY = TOCELLY(msg->y);
			if (cellX != globals.brushX || cellY != globals.brushY)
			{
				paintLineBrush(globals.brushX, globals.brushY, cellX, cellY, color);
				globals.brushX = cellX;
				globals.brushY = cellY;
				SIT_ForceRefresh();
			}
		}
		else if (startX > 0)
		{
			set_camera:
			/* change camera angle */
			if (abs(startX - msg->x) > 10 ||
			    abs(startY - msg->y) > 10)
			{
				float x = TOCELLX(msg->x);
				float y = TOCELLY(msg->y);
				/* +/- 90 pitch does not work with matLookAt */
				if (x != globals.camera[VX])
				{
					globals.lookAt[VX] = x - globals.camera[VX];
					globals.lookAt[VY] = y - globals.camera[VY];
				}
				setViewMat();
				SIT_ForceRefresh();
			}
			return 2;
		}
		break;
	case SITOM_ButtonReleased:
		startX = 0;
	default: break;
	}
	return 1;
}


/* check if current mouse position is inside frustum matrix */
static int handleCommands(SIT_Widget w, APTR cd, APTR ud)
{
	vec4 pos = {globals.posX, globals.posY, globals.camera[VZ], 1};

	matMultByVec(pos, globals.matMVP, pos);

	int sector = 0;
	if (pos[0] <= -pos[3]) sector |= 1,  fprintf(stderr, "left of left plane\n");
	if (pos[0] >=  pos[3]) sector |= 2,  fprintf(stderr, "right of right plane\n");
	if (pos[1] <= -pos[3]) sector |= 4,  fprintf(stderr, "below bottom plane\n");
	if (pos[1] >=  pos[3]) sector |= 8,  fprintf(stderr, "above top plane\n");
	if (pos[2] <= -pos[3]) sector |= 16, fprintf(stderr, "behind near plane\n");
	if (pos[2] >=  pos[3]) sector |= 32, fprintf(stderr, "after far plane\n");

	if (sector == 0) fprintf(stderr, "in frustum\n");

	return 1;
}

/* OnActivate handler for save button */
static int saveChunks(SIT_Widget w, APTR cd, APTR ud)
{
	FILE * pbm = fopen("chunks.pbm", "wb");
	fprintf(pbm, "P5\n%d %d 255\n", IMAGESIZEX, IMAGESIZEY);
	fwrite(globals.chunkBitmap, 1, IMAGESIZEX * IMAGESIZEY, pbm);
	fclose(pbm);
	return 1;
}

static int refresh(SIT_Widget w, APTR cd, APTR ud)
{
	/* hmm, OnActivate is only triggered after mouse is released :-/ */
	SIT_ForceRefresh();
	return 1;
}

/* change mode: draw or move */
static int showsize(SIT_Widget w, APTR cd, APTR ud)
{
	/* draw mode: show brush size, move mode: show FOV */
	SIT_SetValues(globals.label, SIT_Title, globals.drawMode == 0 ? "FOV:" : "Brush:", NULL);
	SIT_SetValues(globals.brush, SIT_Title | XfMt, "%d", globals.drawMode == 0 ? globals.FOV : globals.brushSize, NULL);
	return 1;
}

static void createUI(SIT_Widget app)
{
	TEXT size[16];
	sprintf(size, "%d", globals.drawMode == 0 ? globals.FOV : globals.brushSize);
	SIT_CreateWidgets(app,
		"<canvas name=frame left=FORM top=FORM right=FORM bottom=FORM>"
		"  <label name=msg title=Show:>"
		"  <button name=cnx curValue=", &globals.showCnx, "buttonType=", SITV_ToggleButton, "title='[Cnx graph]' left=WIDGET,msg,0.5em bottom=FORM>"
		"  <label name=msg2 title=Action: top=MIDDLE,cnx left=WIDGET,cnx,0.5em>"
		"  <button name=move radioID=0 radioGroup=1 curValue=", &globals.drawMode, "buttonType=", SITV_ToggleButton,
			"title='[Move]' left=WIDGET,msg2,0.5em bottom=FORM>"
		"  <button name=draw radioID=1 radioGroup=1 curValue=", &globals.drawMode, "buttonType=", SITV_ToggleButton,
			"title='[Draw]' left=WIDGET,move,0.5em bottom=FORM>"
		"  <label name=msg3 title=", globals.drawMode == 0 ? "FOV:" : "Brush:", "top=MIDDLE,draw left=WIDGET,draw,1em>"
		"  <label name=size title=", size, "top=MIDDLE,cnx left=WIDGET,msg3,0.5em>"
		"  <label name=msg4 title='Chunk:' top=MIDDLE,draw left=WIDGET,size,1em>"
		"  <label name=total top=MIDDLE,cnx left=WIDGET,msg4,0.5em>"
		"  <button name=save title=[SAVE] bottom=FORM right=FORM>"
		"</canvas>"

		/* must be last */
		"<canvas name=scanlines ptrEvents=0 left=FORM top=FORM right=FORM bottom=FORM/>"
	);
	SIT_SetAttributes(app, "<msg top=MIDDLE,cnx>");
	SIT_Widget w = SIT_GetById(app, "frame");
	SIT_AddCallback(w, SITE_OnPaint,     paintFrustum, NULL);
	SIT_AddCallback(w, SITE_OnClickMove, handleMouse, NULL);

	globals.brush  = SIT_GetById(app, "size");
	globals.chunks = SIT_GetById(app, "total");
	globals.label  = SIT_GetById(app, "msg3");
	setBrushRange(globals.brushSize);

	SIT_AddCallback(SIT_GetById(app, "move"), SITE_OnActivate, showsize, NULL);
	SIT_AddCallback(SIT_GetById(app, "draw"), SITE_OnActivate, showsize, NULL);
	SIT_AddCallback(SIT_GetById(app, "cnx"),  SITE_OnActivate, refresh, NULL);
	SIT_AddCallback(SIT_GetById(app, "save"), SITE_OnActivate, saveChunks, NULL);

	SIT_SetValues(globals.chunks, SIT_Title|XfMt, "%d/%d", map->frustumChunks, map->totalChunks, NULL);

	SIT_GetValues(app, SIT_NVGcontext, &globals.nvgCtx, NULL);
	globals.nvgChunkImg = nvgCreateImageMask(globals.nvgCtx, IMAGESIZEX, IMAGESIZEY, NVG_IMAGE_MASK | NVG_IMAGE_NEAREST, globals.chunkBitmap);
}

static void setViewMat(void)
{
	mat4 P, MV;
	vec4 lookAt;
	memcpy(lookAt, globals.camera, sizeof globals.camera);
	vec3Add(lookAt, globals.lookAt);

	matPerspective(P, globals.FOV, 1, ZNEAR, ZFAR);
	matLookAt(MV, globals.camera, lookAt, (vec4) {0, 1, 0});
	matMult(globals.matMVP, P, MV);
	matInverse(globals.matInvMVP, globals.matMVP);

	mapViewFrustum(map, globals.camera);

	SIT_SetValues(globals.chunks, SIT_Title|XfMt, "%d/%d", map->frustumChunks, map->totalChunks, NULL);

//	fprintf(stderr, "camera = %g,%g,%g lookAt = %g,%g,%g\n", globals.camera[VX], globals.camera[VY], globals.camera[VZ],
//		lookAt[VX], lookAt[VY], lookAt[VZ]);

	float fov   = globals.FOV * (M_PI / 360);
	vec   pt1   = globals.camera;
	vec   pts   = globals.frustum;
	float angle = atan2(globals.lookAt[VY], globals.lookAt[VX]);
	float cosh  = cosf(fov);

	pts[0] = pt1[VX] + cosf(angle - fov) * (ZFAR / cosh);
	pts[1] = pt1[VY] + sinf(angle - fov) * (ZFAR / cosh);

	pts[2] = pt1[VX] + cosf(angle + fov) * (ZFAR / cosh);
	pts[3] = pt1[VY] + sinf(angle + fov) * (ZFAR / cosh);

	pts[4] = pt1[VX] + cosf(angle) * ZFAR;
	pts[5] = pt1[VY] + sinf(angle) * ZFAR;
}

static void savePrefs(void)
{
	TEXT buffer[128];
	sprintf(buffer, "%dx%d", globals.width, globals.height);
	SetINIValueInt(PREFS_FILE, "BrushSize", globals.brushSize);
	SetINIValueInt(PREFS_FILE, "DrawMode", globals.drawMode);
	SetINIValueInt(PREFS_FILE, "CnxGraph", globals.showCnx);
	SetINIValueInt(PREFS_FILE, "FOV", globals.FOV);
	SetINIValue(PREFS_FILE, "WinSize", buffer);

	sprintf(buffer, "%g,%g", globals.camera[VX], globals.camera[VY]);
	SetINIValue(PREFS_FILE, "Camera", buffer);

	sprintf(buffer, "%g,%g", globals.lookAt[VX], globals.lookAt[VY]);
	SetINIValue(PREFS_FILE, "LookAt", buffer);
}

static void loadPrefs(void)
{
	INIFile ini = ParseINI(PREFS_FILE);

	globals.brushSize = GetINIValueInt(ini, "BrushSize", 10);
	globals.drawMode  = GetINIValueInt(ini, "DrawMode", 0);
	globals.showCnx   = GetINIValueInt(ini, "CnxGraph", 1);
	globals.FOV       = GetINIValueInt(ini, "FOV", 60);

	STRPTR value = GetINIValue(ini, "WinSize");

	if (value == NULL || sscanf(value, "%dx%d", &globals.width, &globals.height) != 2)
		globals.width = 1000, globals.height = 1000;

	if (globals.width  < 480) globals.width  = 480;
	if (globals.height < 480) globals.height = 480;

	value = GetINIValue(ini, "Camera");

	globals.camera[VT] = 1;
	globals.camera[VZ] = CELLSZ/2;
	if (value == NULL || sscanf(value, "%f,%f", &globals.camera[VX], &globals.camera[VY]) != 2)
	{
		globals.camera[VX] = CELLSZ * 1.5;
		globals.camera[VY] = CELLSZ * 11.5;
	}

	value = GetINIValue(ini, "LookAt");

	globals.lookAt[VT] = 1;
	globals.lookAt[VZ] = 0;
	if (value == NULL || sscanf(value, "%f,%f", &globals.lookAt[VX], &globals.lookAt[VY]) != 2)
	{
		globals.lookAt[VX] = CELLSZ;
		globals.lookAt[VY] = - CELLSZ / 2;
	}

	FreeINI(ini);

	/* load chunk bitmap too */
	globals.chunkBitmap = calloc(IMAGESIZEX, IMAGESIZEY);

	FILE * in = fopen("chunks.pbm", "rb");

	if (in)
	{
		STRPTR line = globals.chunkBitmap;
		/* Q'n'D PBM reader */
		fgets(line, IMAGESIZEX * IMAGESIZEY, in);
		if (line[0] == 'P' && line[1] == '5')
		{
			int numId = 0, lineNum = 1;
			int width = 0, height = 0, max = 0;
			while (lineNum < 10 && numId < 3 && fgets(line, IMAGESIZEX * IMAGESIZEY, in))
			{
				lineNum ++;
				if (line[0] == '#') continue;
				StripCRLF(line);
				STRPTR p;
				for (p = line; *p; )
				{
					int num = strtoul(p, &p, 10);
					if (num == 0) break;
					switch (numId) {
					case 0: width  = num; break;
					case 1: height = num; break;
					case 2: max    = num; break;
					}
					numId ++;
				}
			}
			if (width == IMAGESIZEX && height >= IMAGESIZEY && max == 255)
			{
				fread(line, 1, IMAGESIZEX * IMAGESIZEY, in);
			}
		}
		fclose(in);
	}
	map = mapInit(CELLMAPX, globals.chunkBitmap, CELLMAPX, CELLMAPY);
}

int main(int nb, char * argv[])
{
	SDL_Surface * screen;
	SIT_Widget    app;
	int           width, height, exitProg;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		return 1;

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	loadPrefs();
	setViewMat();

	width  = globals.width;
	height = globals.height;
    screen = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER | SDL_OPENGL | SDL_RESIZABLE);
    if (screen == NULL) {
		fprintf(stderr, "failed to set video mode, aborting.\n");
		return 1;
	}
	SDL_WM_SetCaption("Frustum culling", "Frustum culling");

	app = SIT_Init(SIT_NVG_FLAGS, width, height, "resources/analogtv.css", 1);

	if (app == NULL)
	{
		SIT_Log(SIT_ERROR, "could not init SITGL: %s.\n", SIT_GetError());
		return -1;
	}

	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_FlagAlt + SITK_F4, SITE_OnClose},
		{SITK_FlagCapture + SITK_Escape,            SITE_OnClose},
		{'1', SITE_OnActivate, 0, "move"},
		{'2', SITE_OnActivate, 1, "draw"},
		{'3', SITE_OnActivate, 0, "cnx"},
		{SITK_FlagCtrl + 'S', SITE_OnActivate, 0, "save"},
		{SITK_F3, SITE_OnActivate, 0, NULL, handleCommands},
		{0}
	};

	exitProg = 0;
	SIT_SetValues(app,
		SIT_DefSBSize,   SITV_Em(0.5),
		SIT_RefreshMode, SITV_RefreshAsNeeded,
		SIT_AddFont,     "sans-serif", "Courier New/Bold",
		SIT_AccelTable,  accels,
		SIT_ExitCode,    &exitProg,
		NULL
	);

	createUI(app);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);

	glViewport(0, 0, width, height);
	FrameSetFPS(40);

	while (! exitProg)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				{
					int * sdlk;
					for (sdlk = SDLKtoSIT; sdlk < EOT(SDLKtoSIT); sdlk += 2)
					{
						if (sdlk[0] == event.key.keysym.sym) {
							SIT_ProcessKey(sdlk[1], SDLMtoSIT(event.key.keysym.mod), event.type == SDL_KEYDOWN);
							goto break_loop;
						}
					}
				}
				if (event.key.keysym.unicode > 0)
					SIT_ProcessChar(event.key.keysym.unicode, SDLMtoSIT(event.key.keysym.mod));
			break_loop:
				break;
			case SDL_VIDEOEXPOSE:
				SIT_ForceRefresh();
				break;
			case SDL_MOUSEBUTTONDOWN:
				SIT_ProcessClick(event.button.x, event.button.y, event.button.button-1, 1);
				break;
			case SDL_MOUSEBUTTONUP:
				SIT_ProcessClick(event.button.x, event.button.y, event.button.button-1, 0);
				break;
			case SDL_MOUSEMOTION:
				SIT_ProcessMouseMove(event.motion.x, event.motion.y);
				break;
			case SDL_VIDEORESIZE:
				width  = globals.width  = event.resize.w;
				height = globals.height = event.resize.h;
				SIT_ProcessResize(width, height);
				glViewport(0, 0, width, height);
				break;
			case SDL_QUIT:
				exitProg = 1;
			default:
				continue;
			}
		}

		/* update and render */
		if (SIT_RenderNodes(FrameGetTime()))
			SDL_GL_SwapBuffers();

		FrameWaitNext();
	}

	savePrefs();
	SIT_Nuke(SITV_NukeAll);
	SDL_FreeSurface(screen);
	SDL_Quit();

	return 0;
}

#ifdef	WIN32
#include <windows.h>
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
	return main(0, NULL);
}
#endif
