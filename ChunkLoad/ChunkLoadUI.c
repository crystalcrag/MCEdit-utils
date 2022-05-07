/*
 * ChunkLoad.c : SITGL app to debug mulit-threaded chunk loading/meshing.
 *
 * Written by T.Pierron, apr 2022.
 */

#include <stdio.h>
#include <time.h>
#include <GL/GL.h>
#include <SDL/SDL.h>
#include "nanovg.h"
#include <malloc.h>
#include <math.h>
#include "SIT.h"
#include "ChunkLoad.h"

static struct
{
	int  width, height;
	int  mapSize;
	int  posX, posZ;
	APTR nvgCtx, mapLabel;
	APTR speedVal;
	Map  map;
}	prefs;

enum
{
	CMD_MOVE_LEFT,
	CMD_MOVE_RIGHT,
	CMD_MOVE_TOP,
	CMD_MOVE_BOTTOM,
};

int loadSpeed = 50;
extern struct Thread_t threads[];
extern struct Staging_t staging;

static uint8_t memColors[] = {
	0x22,0x61,0xa9,0xff, 0xff,0x5d,0xe6,0xff, 0x78,0xbf,0x00,0xff, 0xff,0x28,0x56,0xff, 0xff,0xca,0x00,0xff,
	0x90,0x00,0xa0,0xff, 0xff,0xa1,0xbf,0xff, 0x00,0x77,0x23,0xff, 0x4e,0x21,0x00,0xff, 0xff,0x80,0x66,0xff,
	0x6c,0x26,0xd4,0xff, 0x5e,0x81,0xff,0xff, 0x00,0xbe,0x91,0xff, 0x7c,0x00,0x18,0xff, 0xc1,0x6c,0x00,0xff,
	0x52,0xb7,0xff,0xff, 0xd8,0xbd,0xff,0xff, 0x90,0xff,0xf4,0xff, 0x1c,0x0c,0x00,0xff, 0xff,0xff,0xff,0xff
};


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

#define MARGINTL 40
#define MARGINBR 20
static int uiPaintChunks(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnPaint * paint = cd;
	NVGcontext *  vg = paint->nvg;

	int area = prefs.map->mapArea;

	TEXT  coord[16];
	float x0, y0;
	float x1, y1;
	float xc, yc;
	float textY = paint->y + MARGINTL - 3 - paint->fontSize;
	float textX = paint->x + MARGINTL - 3 - paint->fontSize;
	int   i;
	float fontSize;
	nvgStrokeColorRGBA8(vg, "\0\0\0\xff");
	nvgFontSize(vg, fontSize = roundf(paint->fontSize*0.8));
	nvgBeginPath(vg);

	/* chunk mesh location */
	Chunk chunk;
	for (x0 = paint->x + MARGINTL + 0.5f, x1 = paint->x + paint->w - MARGINBR - 0.5f, chunk = prefs.map->chunks,
	     y0 = paint->y + MARGINTL + 0.5f, y1 = paint->y + paint->h - MARGINBR - 0.5f, i = 0; i < area*area; i ++, chunk ++)
	{
		if ((chunk->cflags & CFLAG_GOTDATA) == 0)
			continue;

		int cx = i % area;
		int cy = i / area;
		xc = x0 + ((int)paint->w - (MARGINBR+MARGINTL)) * cx / area;
		yc = y0 + ((int)paint->h - (MARGINBR+MARGINTL)) * cy / area;
		float width = x0 + ((int)paint->w - (MARGINBR+MARGINTL)) * (cx+1) / area - xc;
		float height = y0 + ((int)paint->h - (MARGINBR+MARGINTL)) * (cy+1) / area - yc;

		if (chunk->cflags & CFLAG_HASMESH)
		{
			uint8_t color[4];
			memcpy(color, memColors + (chunk->color % 19) * 4, 4);
			color[3] = 0.2 * 255;
			nvgBeginPath(vg);
			nvgFillColorRGBA8(vg, color);
			nvgRect(vg, xc, yc, width, height);
			nvgFill(vg);
		}

		sprintf(coord, "%d", chunk->color);
		nvgFillColorRGBA8(vg, "\0\0\0\xff");
		nvgText(vg, xc + (width - nvgTextBounds(vg, 0, 0, coord, NULL, NULL)) * 0.5f, yc + (height - fontSize) * 0.5f, coord, NULL);
	}

	/* chunk grid */
	nvgBeginPath(vg);
	nvgFillColorRGBA8(vg, "\0\0\0\xff");
	int * horiz = alloca((area+1) * 2 * sizeof *horiz);
	int * vert  = horiz + area + 1;
	int   Xoff  = prefs.map->mapX - (area >> 1);
	int   Zoff  = prefs.map->mapZ - (area >> 1);
	int   X     = (CPOS(prefs.map->cx) << 4) - (area >> 1) * 16;
	int   Z     = (CPOS(prefs.map->cz) << 4) - (area >> 1) * 16;

	for (i = 0, area ++; i < area; i ++)
	{
		int pos = Xoff+i;
		if (pos >= area) pos -= area; else
		if (pos < 0) pos += area;
		horiz[pos] = X + i * 16;

		pos = Zoff-i;
		if (pos >= area) pos -= area; else
		while (pos < 0) pos += area;
		vert[pos] = Z + i * 16;
	}
	area --;

	for (i = 0; i <= area; i ++, X += 16, Z += 16)
	{
		xc = x0 + ((int)paint->w - (MARGINBR+MARGINTL)) * i / area;
		yc = y0 + ((int)paint->h - (MARGINBR+MARGINTL)) * i / area;
		nvgMoveTo(vg, xc, y0); nvgLineTo(vg, xc, y1);
		nvgMoveTo(vg, x0, yc); nvgLineTo(vg, x1, yc);

		sprintf(coord, "%d", horiz[i]);
		float sz = nvgTextBounds(vg, 0, 0, coord, NULL, NULL) * 0.5;
		nvgText(vg, xc - sz, textY, coord, NULL);

		sprintf(coord, "%d", vert[i]);
		sz = nvgTextBounds(vg, 0, 0, coord, NULL, NULL) * 0.5;
		nvgSave(vg);
		nvgTranslate(vg, textX, y1);
		nvgTransform(vg, 0, -1, 1, 0, 0, 0);
		nvgText(vg, yc - sz - y0, 0, coord, NULL);
		nvgRestore(vg);
	}
	nvgStroke(vg);

	/* show center and map area */
	nvgScissor(vg, x0, y0, x1 - x0, y1 - y0);

	nvgBeginPath(vg);
	nvgStrokeColorRGBA8(vg, "\xff\x20\x20\xff");
	xc = x0 + ((int)paint->w - (MARGINBR+MARGINTL)) * prefs.map->mapX / area;
	yc = y0 + ((int)paint->h - (MARGINBR+MARGINTL)) * prefs.map->mapZ / area;

	x1 = x0 + ((int)paint->w - (MARGINBR+MARGINTL)) * (prefs.map->mapX+1) / area;
	y1 = y0 + ((int)paint->h - (MARGINBR+MARGINTL)) * (prefs.map->mapZ+1) / area;
	nvgRect(vg, xc, yc, x1 - xc, y1 - yc);
	nvgStroke(vg);

	/* show map "render distance" */
	nvgBeginPath(vg);
	nvgStrokeColorRGBA8(vg, "\xff\xff\x88\xff");
	int dist = prefs.map->maxDist;
	for (i = 0; i < 4; i ++)
	{
		int mapx = prefs.map->mapX - (dist >> 1);
		int mapz = prefs.map->mapZ - (dist >> 1);

		if (i & 1)
		{
			if (mapx + dist > area) mapx -= area;
			else mapx += area;
		}

		if (i & 2)
		{
			if (mapz + dist > area) mapz -= area;
			else mapz += area;
		}

		xc = x0 + ((int)paint->w - (MARGINBR+MARGINTL)) * mapx / area;
		yc = y0 + ((int)paint->h - (MARGINBR+MARGINTL)) * mapz / area;

		x1 = x0 + ((int)paint->w - (MARGINBR+MARGINTL)) * (mapx+dist) / area;
		y1 = y0 + ((int)paint->h - (MARGINBR+MARGINTL)) * (mapz+dist) / area;

		nvgRect(vg, xc, yc, x1 - xc, y1 - yc);
	}
	nvgStroke(vg);

	nvgResetScissor(vg);

	return 1;
}

#define COLUMN          32
#define COL_MASK        (COLUMN-1)
#define COL_PIXEL(col)  (x0 + (((int)paint->w - MEM_MARGIN*2) * (col) / COLUMN))
#define ROW_STAGING     8
#define ROW_GPUMEM      32
#define MEM_MARGIN      10

static int uiPaintMemory(SIT_Widget w, APTR cd, APTR ud)
{
	static TEXT stagmem[] = "Staging memory (4kb block):";
	static TEXT gpumem[]  = "GPU memory (4Kb block):";

	SIT_OnPaint * paint = cd;
	NVGcontext *  vg = paint->nvg;

	int   fontSize = paint->fontSize * 0.8;
	float x0 = paint->x + MEM_MARGIN + 0.5f;
	float y0 = paint->y + MEM_MARGIN + 0.5f;
	float x1 = paint->x + paint->w - MEM_MARGIN - 0.5f;
	float rowSize = roundf((paint->h - MEM_MARGIN*2 - fontSize * 2) / (ROW_STAGING + ROW_GPUMEM));
	float xc, yc;

	nvgFontSize(vg, fontSize);
	nvgFillColorRGBA8(vg, "\x20\xff\x20\xff");
	nvgText(vg, x0, y0, stagmem, EOT(stagmem)-1);

	/* staging area */
	nvgStrokeColorRGBA8(vg, "\x20\xCC\x20\xff");
	y0 += fontSize;
	int i;
	for (i = 0; i < MAX_BUFFER/4096; i ++)
	{
		if (staging.usage[i>>5] & (1 << (i & 31)))
		{
			DATA32 mem = staging.mem + i * 1024;
			Chunk chunk = prefs.map->chunks + (mem[0] & 0xffff);

			nvgFillColorRGBA8(vg, memColors + (chunk->color % 19) * 4);
			nvgBeginPath(vg);
			yc = y0 + (i >> 5) * rowSize;
			xc = COL_PIXEL(i & COL_MASK);
			nvgRect(vg, xc, yc, COL_PIXEL((i & COL_MASK) + 1) - xc, rowSize);
			nvgFill(vg);
		}
	}
	nvgBeginPath(vg);
	for (i = 0; i <= ROW_STAGING; i ++)
	{
		yc = y0 + i * rowSize;
		nvgMoveTo(vg, x0, yc);
		nvgLineTo(vg, x1, yc);
	}
	for (i = 0; i <= COLUMN; i ++)
	{
		xc = COL_PIXEL(i);
		nvgMoveTo(vg, xc, y0);
		nvgLineTo(vg, xc, y0 + ROW_STAGING * rowSize);
	}
	nvgStroke(vg);


	y0 += ROW_STAGING * rowSize;
	nvgFillColorRGBA8(vg, "\x20\xff\x20\xff");
	nvgText(vg, x0, y0, gpumem, EOT(gpumem)-1);

	/* bank (GPU mem) */
	GPUBank bank = HEAD(prefs.map->gpuBanks);

	if (bank)
	{
		TEXT   coord[64];
		GPUMem mem = bank->usedList;
		GPUMem eof = mem + bank->nbItem;
		int    sz, off, length;
		float  xt, yt;
		DATA8  color;

		i = sprintf(coord, "used: %d, free: %d, max: %d", bank->nbItem, bank->freeItem, bank->maxItems);
		nvgText(vg, paint->x + paint->w - MEM_MARGIN - nvgTextBounds(vg, 0, 0, coord, coord+i, NULL), y0, coord, coord+i);
		y0 += fontSize;

		void renderChunk(void)
		{
			yc = y0 + (off >> 5) * rowSize;
			xc = COL_PIXEL(off & COL_MASK);
			xt = xc+1;
			yt = yc;

			nvgBeginPath(vg);
			nvgFillColorRGBA8(vg, color);
			for (off &= COL_MASK; sz > 0; off = 0, sz -= length, xc = x0, yc += rowSize)
			{
				length = sz;
				if (length + off > COLUMN)
					length = COLUMN - off;

				nvgRect(vg, xc, yc, COL_PIXEL(off + length) - xc, rowSize);
			}
			nvgFill(vg);

			nvgFillColorRGBA8(vg, ((color[0]*77+151*color[1]+28*color[2]) >> 8) < 140 ? "\xff\xff\xff\xff" : "\0\0\0\xff");
			nvgText(vg, xt, yt, coord, NULL);
		}

		for (; mem < eof; mem ++)
		{
			sz  = mem->size / 4096;
			off = mem->offset / 4096;
			color = memColors + (mem->id % 19) * 4;
			sprintf(coord, "%d", mem->id);

			renderChunk();
		}

		/* overlay free blocks */
		mem = bank->usedList + bank->maxItems - 1;
		eof = mem - bank->freeItem;
		for (i = 0; mem > eof; mem --, i ++)
		{
			sz = mem->size / 4096;
			off = mem->offset / 4096;
			color = memColors + 19 * 4;

			sprintf(coord, "free: %d:%d", sz, i);
			renderChunk();
		}
	}

	nvgBeginPath(vg);
	for (i = 0; i <= ROW_GPUMEM; i ++)
	{
		yc = y0 + i * rowSize;
		nvgMoveTo(vg, x0, yc);
		nvgLineTo(vg, x1, yc);
	}
	for (i = 0; i <= COLUMN; i ++)
	{
		xc = COL_PIXEL(i);
		nvgMoveTo(vg, xc, y0);
		nvgLineTo(vg, xc, y0 + ROW_GPUMEM * rowSize);
	}
	nvgStroke(vg);

	return 1;
}

static int uiThreadStatus(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnPaint * paint = cd;
	NVGcontext *  vg = paint->nvg;

	nvgFillColorRGBA8(vg, "\x20\xCC\x20\xff");
	float x0 = paint->x + MEM_MARGIN;
	float y0 = paint->y + MEM_MARGIN;
	int   i;
	for (i = 0; i < NUM_THREADS; i ++)
	{
		static STRPTR status[] = {
			"Waiting for chunk",
			"Waiting for staging",
			"Processing"
		};
		TEXT msg[128];
		sprintf(msg, "Thread %d: %s", i + 1, status[threads[i].state]);
		nvgText(vg, x0, y0, msg, NULL);
		y0 += paint->fontSize + 3;

		if (i == 1) x0 += paint->w * 0.5f, y0 = paint->y + MEM_MARGIN;
	}

	return 1;
}

static int uiSetRenderDist(SIT_Widget w, APTR cd, APTR ud)
{
	int size = prefs.mapSize + (int) ud - 1;

	if (2 <= size && size <= 16)
	{
		prefs.mapSize = size;
		mapSetRenderDist(prefs.map, size);
		size = (size<<1) + 1;
		SIT_SetValues(prefs.mapLabel, SIT_Title | XfMt, "%dx%d", size, size, NULL);
	}

	return 1;
}

static int uiShowSpeed(SIT_Widget w, APTR cd, APTR ud)
{
	loadSpeed = (int) cd;

	TEXT value[16];
	sprintf(value, "%.2gx", loadSpeed/50.);
	SIT_SetValues(prefs.speedVal, SIT_Title, value, NULL);

	return 1;
}

static void uiCreate(SIT_Widget app)
{
	SIT_CreateWidgets(app,
		"<label name=msg1 title='Chunk grid:'>"
		"<button name=inc title=+ left=WIDGET,msg1,0.5em width=2em>"
		"<button name=dec title=- left=WIDGET,inc,0.5em width=2em>"
		"<label name=size left=WIDGET,dec,0.5em top=MIDDLE,dec>"
		"<canvas name=chunks top=WIDGET,inc,0.5em left=FORM right=", SITV_AttachCenter, "bottom=FORM/>"

		"<label name=msg2 title='Threads: waiting:' left=WIDGET,chunks,1em>"
		"<slider name=speed left=WIDGET,msg2,0.5em top=MIDDLE,inc width=10em thumbThick=0.8em sliderPos=", loadSpeed, ">"
		"<label name=speedval left=WIDGET,speed,0.5em top=MIDDLE,speed title=1x>"
		"<canvas name=threads.plain top=OPPOSITE,chunks left=OPPOSITE,msg2 right=FORM height=3em/>"

		"<label name=msg3 title='Memory layout:' top=WIDGET,threads,1em left=OPPOSITE,msg2>"
		"<canvas name=gpumem.plain top=WIDGET,msg3,0.5em left=OPPOSITE,msg2 right=FORM bottom=FORM/>"
	);
	SIT_SetAttributes(app, "<msg1 top=MIDDLE,inc><msg2 top=MIDDLE,speed>");

	SIT_AddCallback(SIT_GetById(app, "chunks"),  SITE_OnPaint, uiPaintChunks, NULL);
	SIT_AddCallback(SIT_GetById(app, "gpumem"),  SITE_OnPaint, uiPaintMemory, NULL);
	SIT_AddCallback(SIT_GetById(app, "threads"), SITE_OnPaint, uiThreadStatus, NULL);

	SIT_AddCallback(SIT_GetById(app, "inc"), SITE_OnActivate, uiSetRenderDist, (APTR) 2);
	SIT_AddCallback(SIT_GetById(app, "dec"), SITE_OnActivate, uiSetRenderDist, NULL);

	SIT_AddCallback(SIT_GetById(app, "speed"), SITE_OnChange, uiShowSpeed, NULL);

	prefs.mapLabel = SIT_GetById(app, "size");
	prefs.speedVal = SIT_GetById(app, "speedval");
	int size = (prefs.mapSize<<1) + 1;
	SIT_SetValues(prefs.mapLabel, SIT_Title | XfMt, "%dx%d", size, size, NULL);

	uiShowSpeed(NULL, (APTR) loadSpeed, NULL);
}

static int uiProcessCmd(SIT_Widget w, APTR cd, APTR ud)
{
	vec4 oldpos = {prefs.posX, 0, prefs.posZ};
	switch ((int) ud) {
	case CMD_MOVE_LEFT:
		prefs.posX -= 16;
		break;
	case CMD_MOVE_RIGHT:
		prefs.posX += 16;
		break;
	case CMD_MOVE_TOP:
		prefs.posZ -= 16;
		break;
	case CMD_MOVE_BOTTOM:
		prefs.posZ += 16;
	}
	mapMoveCenter(prefs.map, oldpos, (vec4) {prefs.posX, 0, prefs.posZ});
	SIT_ForceRefresh();
	return 1;
}

static void loadPrefs(void)
{
	INIFile ini = ParseINI("ChunkLoad.ini");

	prefs.width   = GetINIValueInt(ini, "Width", 1200);
	prefs.height  = GetINIValueInt(ini, "Height", 900);
	prefs.mapSize = GetINIValueInt(ini, "MapSize", 4);
	loadSpeed     = GetINIValueInt(ini, "Speed", 50);

	if (prefs.mapSize < 1)  prefs.mapSize = 1;
	if (prefs.mapSize > 16) prefs.mapSize = 16;
	if (loadSpeed < 0)      loadSpeed = 0;
	if (loadSpeed > 100)    loadSpeed = 100;

	STRPTR pos = GetINIValue(ini, "MapPos");
	if (pos == NULL || sscanf(pos, "%dx%d", &prefs.posX, &prefs.posZ) != 2)
		prefs.posX = prefs.posZ = 8;

	FreeINI(ini);
}

static void savePrefs(void)
{
	SetINIValueInt("ChunkLoad.ini", "Width",   prefs.width);
	SetINIValueInt("ChunkLoad.ini", "Height",  prefs.height);
	SetINIValueInt("ChunkLoad.ini", "MapSize", prefs.mapSize);
	SetINIValueInt("ChunkLoad.ini", "Speed",   loadSpeed);
}

int main(int nb, char * argv[])
{
	SDL_Surface * screen;
	SIT_Widget    app;
	int           exitProg;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		return 1;

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	loadPrefs();

    screen = SDL_SetVideoMode(prefs.width, prefs.height, 32, SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER | SDL_OPENGL | SDL_RESIZABLE);
    if (screen == NULL)
    {
		SIT_Log(SIT_ERROR, "failed to set video mode, aborting.");
		return 1;
	}
	SDL_WM_SetCaption("ChunkLoad", "ChunkLoad");

	app = SIT_Init(SIT_NVG_FLAGS, prefs.width, prefs.height, "resources/default.css", 1);

	if (app == NULL)
	{
		SIT_Log(SIT_ERROR, "could not init SITGL: %s.", SIT_GetError());
		return 1;
	}

	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_FlagAlt + SITK_F4, SITE_OnClose},
		{SITK_FlagCapture + SITK_Escape,            SITE_OnClose},

		{SITK_Left,  SITE_OnActivate, CMD_MOVE_LEFT,   NULL, uiProcessCmd},
		{SITK_Right, SITE_OnActivate, CMD_MOVE_RIGHT,  NULL, uiProcessCmd},
		{SITK_Up,    SITE_OnActivate, CMD_MOVE_TOP,    NULL, uiProcessCmd},
		{SITK_Down,  SITE_OnActivate, CMD_MOVE_BOTTOM, NULL, uiProcessCmd},

		{'=',  SITE_OnActivate, 0, "inc"},
		{'-',  SITE_OnActivate, 0, "dec"},
	};

	exitProg = 0;
	SIT_SetValues(app,
		SIT_DefSBSize,   SITV_Em(1),
		SIT_RefreshMode, SITV_RefreshAsNeeded,
		SIT_AddFont,     "sans-serif",      "System",
		SIT_AddFont,     "sans-serif-bold", "System/Bold",
		SIT_SetAppIcon,  True,
		SIT_ExitCode,    &exitProg,
		SIT_AccelTable,  accels,
		NULL
	);

	SIT_GetValues(app, SIT_NVGcontext, &prefs.nvgCtx, NULL);
	uiCreate(app);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);

	glViewport(0, 0, prefs.width, prefs.height);

//	srand(time(NULL));
	FrameSetFPS(40);
	prefs.map = mapInitFromPath(prefs.mapSize, &prefs.posX);
//	renderTestAlloc(prefs.map);

	while (! exitProg)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type) {
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_F7)
					checkMem(HEAD(prefs.map->gpuBanks));
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
				prefs.width  = event.resize.w;
				prefs.height = event.resize.h;
				SIT_ProcessResize(prefs.width, prefs.height);
				glViewport(0, 0, prefs.width, prefs.height);
				break;
			case SDL_QUIT:
				exitProg = 1;
			case SDL_USEREVENT:
				break;
			default:
				continue;
			}
		}

		if (staging.total > 0)
		{
			mapGenFlush(prefs.map);
			SIT_ForceRefresh();
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
	mapFreeAll(prefs.map);

	return 0;
}

#ifdef	WIN32
#include <windows.h>
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR CmdLine,
    int CmdShow)
{
	return main(0, NULL);
}
#endif
