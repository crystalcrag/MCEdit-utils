/*
 * SkyLight.c : SItGL app to manage skyLight information in 2d space
 *
 * Written by T.Pierron, feb 2022.
 */

#include <stdio.h>
#include <time.h>
#include <GL/GL.h>
#include <SDL/SDL.h>
#include "nanovg.h"
#include <malloc.h>
#include <math.h>
#include "SIT.h"
#include "SkyLight.h"

struct SkyLight_t prefs;

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



static int uiSave(SIT_Widget w, APTR cd, APTR ud)
{
	FILE * out = fopen(SAVEFILE, "wb");
	int    i;

	if (out)
	{
		fprintf(stderr, "saving to " SAVEFILE "...\n");
		DATA8 combine = malloc(CELLW*CELLH);
		for (i = 0; i < CELLW * CELLH; i ++)
			combine[i] = prefs.blockIds[i] | prefs.skyLight[i];

		fwrite(combine, 1, CELLW * CELLH, out);
		fclose(out);
		free(combine);
	}
	else SIT_Log(SIT_INFO, "Failed to write to " SAVEFILE ": %s", GetError());
	return 1;
}

/* create a random map */
static int uiNewMap(SIT_Widget w, APTR cd, APTR ud)
{
	skyGenTerrain();
	SIT_ForceRefresh();
	return 1;
}

static uint8_t colors[] = {
	0xa6, 0xeb, 0xff, 0xff,   /* sky color */
	0x33, 0x33, 0x33, 0xff,   /* cave color */
	0xbb, 0xaa, 0xaa, 0xff,   /* wall color */
	0x10, 0x10, 0xff, 0xff,   /* water color */
	0x10, 0x88, 0x10, 0xff,   /* leaf color */
	0xff, 0xff, 0x00, 0xff,   /* heightmap lines */
};

/* render world using crude 2d tile */
static int uiPaint(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnPaint * paint = cd;
	NVGcontext * vg = paint->nvg;
	float x = prefs.rect[0] + paint->x;
	float y = prefs.rect[1] + paint->y;

	nvgBeginPath(vg);
	nvgRect(vg, x, y, prefs.rect[2], prefs.rect[3]);
	nvgStrokeColorRGBA8(vg, "\0\0\0\xff");
	nvgStroke(vg);

	DATA8 sky, block;
	float x2;
	int   i, j;
	for (j = 0, sky = prefs.skyLight, block = prefs.blockIds; j < CELLH; j ++, y += prefs.cellSz)
	{
		for (i = 0, x2 = x; i < CELLW; i ++, sky ++, block ++, x2 += prefs.cellSz)
		{
			static uint8_t alpha[] = {255, 127, 168};
			uint8_t blend[4], pattern;
			DATA8 color;
			switch (block[0]) {
			case BLOCK_OPAQUE: color = colors + 8;  pattern = 0; break;
			case BLOCK_WATER:  color = colors + 12; pattern = 1; break;
			case BLOCK_LEAVE:  color = colors + 16; pattern = 2; break;
			default:           color = blend; pattern = 1;
			}
			if (pattern > 0)
			{
				int k = sky[0];
				/* blend == skyLight color */
				blend[0] = (colors[0] * k + colors[4] * (MAXSKY-k)) / MAXSKY;
				blend[1] = (colors[1] * k + colors[5] * (MAXSKY-k)) / MAXSKY;
				blend[2] = (colors[2] * k + colors[6] * (MAXSKY-k)) / MAXSKY;
				blend[3] = 255;

				nvgBeginPath(vg);
				nvgRect(vg, x2, y, prefs.cellSz, prefs.cellSz);
				nvgFillColorRGBA8(vg, blend);
				nvgFill(vg);
			}
			memcpy(blend, color, 4);
			blend[3] = alpha[pattern];
			nvgBeginPath(vg);
			nvgRect(vg, x2, y, prefs.cellSz, prefs.cellSz);
			nvgFillColorRGBA8(vg, blend);
			nvgFill(vg);
		}
	}
	/* draw heightmap */
	nvgBeginPath(vg);
	nvgStrokeWidth(vg, 2);
	nvgStrokeColorRGBA8(vg, colors + 20);
	for (i = 0, y = prefs.rect[1] + paint->y; i < CELLW; i ++)
	{
		j  = y + prefs.cellSz * (prefs.heightMap[i] + 1) - 0.5f;
		x2 = x + prefs.cellSz * i;
		nvgMoveTo(vg, x2, j);
		nvgLineTo(vg, x2 + prefs.cellSz, j);
	}
	nvgStroke(vg);

	/* next-step cell highlight */
	if (prefs.cellXY[0] >= 0)
	{
		nvgBeginPath(vg);
		nvgRect(vg, x + prefs.cellXY[0] * prefs.cellSz - 1, y + prefs.cellXY[1] * prefs.cellSz - 1, prefs.cellSz + 1, prefs.cellSz + 1);
		nvgStroke(vg);
	}
	return 1;
}

static void uiActivateStep(int type)
{
	if (prefs.stepping)
	{
		/* finish previous stepping */
		switch (prefs.stepping) {
		case STEP_UNSETBLOCK: while (skyUnsetBlock()); break;
		case STEP_SETBLOCK:   while (skySetBlock()); break;
		}
	}
	prefs.stepping = type;

	/* already do one iteration */
	if (type == STEP_SETBLOCK) skySetBlock();
	else skyUnsetBlock();
	skyGetNextCell(prefs.cellXY);

	SIT_SetValues(prefs.step, SIT_Enabled, True, NULL);
}

/* move/click mouse over drawing area */
static int uiMouse(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnMouse * msg = cd;
	static int cellX, cellY, button;

	switch (msg->state) {
	case SITOM_Move:
		msg->x = (msg->x - prefs.rect[0]) / prefs.cellSz;
		msg->y = (msg->y - prefs.rect[1]) / prefs.cellSz;
		if (msg->x != cellX || msg->y != cellY)
		{
			TEXT coord[32];
			int sky = -1;
			cellX = msg->x;
			cellY = msg->y;
			if (0 <= cellX && cellX < CELLW && 0 <= cellY && cellY < CELLH)
				sky = prefs.skyLight[cellX + cellY * CELLW];
			sprintf(coord, "%d, %d: light %d", cellX, cellY, sky);
			SIT_SetValues(ud, SIT_Title, coord, NULL);
		}
		break;
	case SITOM_ButtonPressed:
		button = msg->button;
		// no break;
	case SITOM_CaptureMove:
		cellX = (msg->x - prefs.rect[0]) / prefs.cellSz;
		cellY = (msg->y - prefs.rect[1]) / prefs.cellSz;
		if (0 <= cellX && cellX < CELLW && 0 <= cellY && cellY < CELLH)
		{
			DATA8 block = prefs.blockIds + cellY * CELLW + cellX;

			/* right click: clear tile, left click: add wall */
			switch (button) {
			case SITOM_ButtonLeft:
				if (*block == BLOCK_AIR)
				{
					*block = prefs.blockType;
					skySetBlockInit(cellX, cellY);
					if (prefs.stepByStep)
						uiActivateStep(STEP_SETBLOCK);
					else
						while (skySetBlock());
					SIT_ForceRefresh();
				}
				break;
			case SITOM_ButtonRight:
				if (*block != BLOCK_AIR)
				{
					*block = BLOCK_AIR;
					skyUnsetBlockInit(cellX, cellY);
					if (prefs.stepByStep)
					{
						skyUnsetBlock();
						prefs.stepping = STEP_UNSETBLOCK;
					}
					else while (skyUnsetBlock());
					SIT_ForceRefresh();
				}
				break;
			default: return 0;
			}
			return 2;
		}
		return 1;
	default: break;
	}
	return 0;
}

static int uiResize(SIT_Widget w, APTR cd, APTR ud)
{
	float * rect = cd;
	int     zoom;

	if (rect[0] * CELLH > rect[1] * CELLW)
		/* constrained by height */
		zoom = floorf(rect[1] / (float) CELLH);
	else /* constrained by width */
		zoom = floorf(rect[0] / (float) CELLW);

	prefs.rect[2] = CELLW * zoom;
	prefs.rect[3] = CELLH * zoom;
	prefs.rect[0] = (rect[0] - prefs.rect[2]) * 0.5f;
	prefs.rect[1] = (rect[1] - prefs.rect[3]) * 0.5f;
	prefs.cellSz  = zoom;

	return 1;
}

/* step by step checkbox */
static int uiSetStepping(SIT_Widget w, APTR cd, APTR ud)
{
	if (prefs.stepByStep == 0)
	{
		/* disabled step by step, but stepping in progress: finish the job now */
		switch (prefs.stepping) {
		case STEP_UNSETBLOCK: while (skyUnsetBlock()); break;
		case STEP_SETBLOCK:   while (skySetBlock()); break;
		}
		prefs.stepping = STEP_DONE;
		SIT_SetValues(prefs.step, SIT_Enabled, False, NULL);
	}
	return 1;
}

static int uiStepByStep(SIT_Widget w, APTR cd, APTR ud)
{
	int more = 0;
	switch (prefs.stepping) {
	case STEP_UNSETBLOCK: more = skyUnsetBlock(); break;
	case STEP_SETBLOCK:   more = skySetBlock(); break;
	}
	if (! more)
	{
		SIT_SetValues(prefs.step, SIT_Enabled, False, NULL);
		prefs.stepping = STEP_DONE;
		prefs.cellXY[0] = -1;
	}
	else skyGetNextCell(prefs.cellXY);
	SIT_ForceRefresh();
	return 1;
}

/* parse skylight.map */
static void readSkyMap(void)
{
	FILE * in = fopen(SAVEFILE, "rb");
	if (in)
	{
		DATA8 p;
		int   i, j;

		fread(prefs.skyLight, 1, CELLW * CELLH, in);
		fclose(in);

		/* separate skylight and blockId */
		for (i = 0; i < CELLW*CELLH; i ++)
			prefs.blockIds[i] = prefs.skyLight[i] & 0xf0, prefs.skyLight[i] &= 15;

		/* recompute heightmap */
		for (j = 0; j < CELLW; j ++)
		{
			for (i = 0, p = prefs.blockIds + j; i < CELLH && p[0] == 0; i ++, p += CELLW);
			prefs.heightMap[j] = i - 1;
		}
	}
}

static void uiCreate(SIT_Widget app)
{
	SIT_CreateWidgets(app,
		"<button name=newmap title='New map'>"
		"<button name=save title=Save left=WIDGET,newmap,0.5em>"
		"<button name=step title=Step left=WIDGET,save,0.5em enabled=0>"
		"<button name=debug buttonType=", SITV_CheckBox, "title='Step by step' left=WIDGET,step,0.5em top=MIDDLE,newmap curValue=", &prefs.stepByStep, ">"
		"<label name=coord.big width=10em left=WIDGET,debug,0.5em, top=MIDDLE,newmap>"

		"<button name=opaque title=Opaque radioGroup=1 radioID=", BLOCK_OPAQUE, "buttonType=", SITV_ToggleButton,
		" left=WIDGET,coord  curValue=", &prefs.blockType, "checkState=1>"
		"<button name=leaves title=Leave  radioGroup=1 radioID=", BLOCK_LEAVE, "buttonType=", SITV_ToggleButton,
		" left=WIDGET,opaque curValue=", &prefs.blockType, ">"
		"<button name=water  title=Water  radioGroup=1 radioID=", BLOCK_WATER, "buttonType=", SITV_ToggleButton,
		" left=WIDGET,leaves curValue=", &prefs.blockType, ">"

		"<label name=help.dim title='LMB: add block, RMB: delete' right=FORM top=MIDDLE,newmap>"
		"<canvas name=light top=WIDGET,newmap,0.5em left=FORM right=FORM bottom=FORM>"
	);
	prefs.blocks[0] = SIT_GetById(app, "opaque");
	prefs.blocks[1] = SIT_GetById(app, "leaves");
	prefs.blocks[2] = SIT_GetById(app, "water");
	prefs.step      = SIT_GetById(app, "step");

	SIT_Widget canvas = SIT_GetById(app, "light");
	SIT_AddCallback(canvas, SITE_OnPaint,     uiPaint,  NULL);
	SIT_AddCallback(canvas, SITE_OnResize,    uiResize, NULL);
	SIT_AddCallback(canvas, SITE_OnClickMove, uiMouse,  SIT_GetById(app, "coord"));

	SIT_AddCallback(prefs.step, SITE_OnActivate, uiStepByStep, NULL);

	SIT_AddCallback(SIT_GetById(app, "save"),   SITE_OnActivate, uiSave, NULL);
	SIT_AddCallback(SIT_GetById(app, "newmap"), SITE_OnActivate, uiNewMap, NULL);
	SIT_AddCallback(SIT_GetById(app, "debug"),  SITE_OnActivate, uiSetStepping, NULL);

	prefs.skyLight = calloc(CELLW, CELLH * 2 + 1);
	prefs.blockIds = prefs.skyLight + CELLW * CELLH;
	prefs.heightMap = prefs.blockIds + CELLW * CELLH;
	memset(prefs.skyLight, 0, CELLW * CELLH * 2);
	memset(prefs.heightMap, CELLH-1, CELLW);
	prefs.blockType = BLOCK_OPAQUE;
	prefs.cellXY[0] = -1;

	readSkyMap();
}

/* SIT_Accel callback */
static int uiProcessCommand(SIT_Widget w, APTR cd, APTR ud)
{
	int cmd = (int) ud;
	if (prefs.blockType != cmd)
	{
		prefs.blockType = cmd;
		switch (cmd) {
		case BLOCK_OPAQUE: cmd = 0; break;
		case BLOCK_LEAVE:  cmd = 1; break;
		case BLOCK_WATER:  cmd = 2; break;
		default: return 1;
		}
		SIT_SetValues(prefs.blocks[cmd], SIT_CheckState, True, NULL);
	}
	return 1;
}

int main(int nb, char * argv[])
{
	SDL_Surface * screen;
	SIT_Widget    app;
	int           exitProg;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		return 1;

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	prefs.width = 1200;
	prefs.height = 900;

    screen = SDL_SetVideoMode(prefs.width, prefs.height, 32, SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER | SDL_OPENGL | SDL_RESIZABLE);
    if (screen == NULL)
    {
		SIT_Log(SIT_ERROR, "failed to set video mode, aborting.");
		return 1;
	}
	SDL_WM_SetCaption("SkyLight", "SkyLight");

	app = SIT_Init(SIT_NVG_FLAGS, prefs.width, prefs.height, "resources/default.css", 1);

	if (app == NULL)
	{
		SIT_Log(SIT_ERROR, "could not init SITGL: %s.", SIT_GetError());
		return 1;
	}

	FrameSetFPS(40);

	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_FlagAlt + SITK_F4, SITE_OnClose},
		{SITK_FlagCapture + SITK_Escape,            SITE_OnClose},

		{SITK_FlagCtrl + 'S', SITE_OnActivate, 0,            NULL, uiSave},
		{'1',                 SITE_OnActivate, BLOCK_OPAQUE, NULL, uiProcessCommand},
		{'2',                 SITE_OnActivate, BLOCK_LEAVE,  NULL, uiProcessCommand},
		{'3',                 SITE_OnActivate, BLOCK_WATER,  NULL, uiProcessCommand},
		{SITK_Space,          SITE_OnActivate, 0,            NULL, uiStepByStep},
		{'s',                 SITE_OnActivate, 0,            "debug"},
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

	SIT_GetValues(app, SIT_NVGcontext, &prefs.nvg, NULL);
	uiCreate(app);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);

	glViewport(0, 0, prefs.width, prefs.height);

	srand(time(NULL));

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

		/* update and render */
		if (SIT_RenderNodes(FrameGetTime()))
			SDL_GL_SwapBuffers();

		FrameWaitNext();
	}

	SIT_ApplyCallback(app, NULL, SITE_OnFinalize);
	SDL_FreeSurface(screen);
	SDL_Quit();

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
