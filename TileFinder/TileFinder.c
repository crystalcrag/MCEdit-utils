/*
 * Tilefinder.c : Find tile position in a minecraft block texture atlas.
 *
 * this part is only some glue with every other parts of this application.
 *
 * Written by T.Pierron, Jan 2020.
 */

#include <stdio.h>
#include <SDL/SDL.h>
#include <glad.h>
#include "nanovg.h"
#include <malloc.h>
#include <math.h>
#include "SIT.h"
#include "TileFinder.h"
#include "TileFinderGL.h"

struct Prefs_t prefs;

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

static void readPrefs(STRPTR path)
{
	FILE * in = fopen("Block2.txt", "rb");

	if (in)
	{
		TEXT line[512];
		while (fgets(line, sizeof line, in))
		{
			if (line[0] == '#') continue;

			STRPTR sep = strchr(line, '=');
			if (! sep) continue; *sep ++ = 0;
			switch (FindInList("DetailMode,ShowBBox,WndWidth,WndHeight,LastTex,Block", line, 0)) {
			case 0: prefs.detail = atoi(sep); break;
			case 1: prefs.bbox   = atoi(sep); break;
			case 2: prefs.width  = atoi(sep); break;
			case 3: prefs.height = atoi(sep); break;
			case 4: CopyString(prefs.lastTex, sep, sizeof prefs.lastTex); break;
			case 5: blockParseFormat(sep);
			}
		}
		fclose(in);
	}

	if (prefs.width <= 500)
		prefs.width = 1600;
	if (prefs.height <= 300)
		prefs.height = 800;

	if (path && FileExists(path))
		CopyString(prefs.lastTex, path, sizeof prefs.lastTex);

	if (prefs.lastTex[0] == 0)
		strcpy(prefs.lastTex, FileExists("terrain.png") ? "terrain.png" : "../../MCEdit/resources/terrain.png");
}



int main(int nb, char * argv[])
{
	SDL_Surface * screen;
	SIT_Widget    app;
	int           exitProg;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		return 1;

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	readPrefs(nb > 1 ? argv[1] : NULL);
    screen = SDL_SetVideoMode(prefs.width, prefs.height, 32, SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER | SDL_OPENGL | SDL_RESIZABLE);
    if (screen == NULL) {
		SIT_Log(SIT_ERROR, "failed to set video mode, aborting.");
		return 1;
	}
	SDL_WM_SetCaption("TileFinder", "TileFinder");

	if (gladLoadGL() == 0)
	{
		SIT_Log(SIT_ERROR,
			"Fail to initialize OpenGL: minimum version required is 4.3\n\n"
			"Version installed: %s", glGetString(GL_VERSION)
		);
		return 1;
	}

	if (! renderInitStatic())
		return 1;

	app = SIT_Init(SIT_NVG_FLAGS, prefs.width, prefs.height, "resources/plastic.css", 1);

	FrameSetFPS(40);

	if (app == NULL)
	{
		SIT_Log(SIT_ERROR, "could not init SITGL: %s.", SIT_GetError());
		return 1;
	}

	exitProg = 0;
	SIT_SetValues(app,
		SIT_DefSBSize,   SITV_Em(1),
		SIT_RefreshMode, SITV_RefreshAsNeeded,
		SIT_AddFont,     "sans-serif",      "System",
		SIT_AddFont,     "sans-serif-bold", "System/Bold",
		SIT_SetAppIcon,  True,
		SIT_ExitCode,    &exitProg,
		NULL
	);

	SIT_GetValues(app, SIT_NVGcontext, &prefs.nvg, NULL);
	uiCreate(app);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);

	glViewport(0, 0, prefs.width, prefs.height);

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
		switch (SIT_RenderNodes(FrameGetTime())) {
		case SIT_RenderComposite:
			renderCube();
			SIT_RenderNodes(0);
			SDL_GL_SwapBuffers();
			break;
		case SIT_RenderDone:
			renderCube();
			SDL_GL_SwapBuffers();
		default: break;
		}

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
	/* CmdLine parameter is not unicode aware even with UNICODE macro set */
	int      nb, i;
	LPWSTR * argv = CommandLineToArgvW(GetCommandLineW(), &nb);

	/* convert strings to UTF8 */
	for (i = 0; i < nb; )
	{
		int len = wcslen(argv[i]);
		int sz  = UTF16ToUTF8(NULL, 0, (STRPTR) argv[i], len) + 1;

		CmdLine = alloca(sz);

		sz = UTF16ToUTF8(CmdLine, sz, (STRPTR) argv[i], len);

		argv[i++] = (LPWSTR) CmdLine;
	}
	return main(nb, (STRPTR *) argv);
}
#endif
