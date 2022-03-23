/*
 * SkyLight.c : 2d dynamic sky light test
 *
 * Written by T.Pierron, aug 2020.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "SIT.h"
#include "graphics.h"

#define CELLW         64
#define CELLH         64
#define CELLSZ        12
#define MAXSKY        8
#define WALL          0x7f
#define SAVEFILE      "skylight.map"

static DATA8 skyLight;
static uint8_t heightMap[CELLW];
static Image back;

static void refreshBack(void);

static void skyInit(void)
{
	skyLight = calloc(CELLW, CELLH);
	back     = GFX_CreateImage(CELLW*CELLSZ, CELLH*CELLSZ, 8);

	memset(back->bitmap, 0, back->stride * back->height);

	/* restore previous state */
	FILE * in = fopen(SAVEFILE, "rb");
	if (in)
	{
		fread(skyLight, 1, CELLW * CELLH, in);
		fclose(in);

		/* recompute heightmap */
		DATA8 p;
		int   i, j;
		for (j = 0; j < CELLW; j ++)
		{
			for (i = 0, p = skyLight + j; i < CELLH && p[0] != WALL; i ++, p += CELLW);
			heightMap[j] = i - 1;
		}
		refreshBack();
	}
	else
	{
		int i;
		for (i = 0; i < CELLW; heightMap[i] = CELLH-1, i ++);
		memset(skyLight, MAXSKY, CELLW * CELLH);
	}
}

static int8_t xoff[] = {1, -1,  0, 0};
static int8_t yoff[] = {0,  0, -1, 1};
static int8_t dirs[] = {15, 15, 11, 7, 15};
static int8_t opp[]  = {13, 14, 7, 11, 15};
int8_t track[255], xstart, ystart;
int    pos, last;

static int skyIsIncorrect(int x, int y, int dir)
{
//	static uint8_t opposite[] = {0, 2, 1, 4, 3};
	y *= CELLW;
	uint8_t sky = skyLight[x+y];
	uint8_t max, i, flags;
	uint8_t values[4];
	if (sky == WALL) return 0;

	for (i = max = 0, flags = dirs[dir]; i < 4; i ++, flags >>= 1)
	{
		uint8_t val = skyLight[x+xoff[i]+y+yoff[i]*CELLW];
		if (val == WALL) val = 0;
		values[i] = val;
		if (i == 2 && val == MAXSKY) val = MAXSKY+1;
		if ((flags & 1) && max < val)
			max = val;
	}

	i = max;

	if (dir == 2 && values[2] == sky+1) dir = 3, i = MAXSKY+1; else
	if (dir == 3 && values[3] == sky+1) return 6;

	if (max > 0) max --;
	if (sky == max) return 0; /* correct actually */
	if (sky >  i)   return 5; /* local maximum */
	return dir+1;
}

static uint8_t skyValue(int x, int y)
{
	return skyLight[x+y*CELLW];
}

static void addTrack(int x, int y, int dir)
{
	int i;
	for (i = pos; i != last; )
	{
		if (track[i] == x && track[i+1] == y) return;
		i += 3;
		if (i == DIM(track)) i = 0;
	}
	track[last++] = x;
	track[last++] = y;
	track[last++] = dir;
	if (last == DIM(track)) last = 0;
}

static void skyUpdateInit(int x, int y, int w, int h)
{
	int i, x2;
	pos = last = 0;
	xstart = x;
	ystart = y;
	i = skyIsIncorrect(x-1, y, 1);
	if (i)
		addTrack(-1, 0, i-1);

	for (i = 0, x2 = x; i < w; i ++, x2 ++)
	{
		/* need to recompute heightmap first */
		if (heightMap[x2] > y)
		{
			int y2;
			for (y2 = y, x2 = x+i; y2 < CELLH && skyValue(x2, y2) != WALL; y2 ++);
			heightMap[x2] = y2-1;
		}
		if (skyValue(x2, y) == WALL)
			addTrack(i, 1, 3);
		else
			addTrack(i, 0, 4);
	}
	i = skyIsIncorrect(x+w, y, 0);
	if (i)
		addTrack(w, 0, i-1);
}

static void skyUpdate(void)
{
	int i, out = 0;

	if (pos != last)
	{
		int xsky = xstart + track[pos++];
		int ysky = ystart + track[pos++];
		int dir  = track[pos++];
		DATA8 cell = skyLight + xsky + ysky * CELLW;
		uint8_t sky = *cell;

		if (pos == DIM(track)) pos = 0;

		if (sky != WALL)
		{
			uint8_t val, max, maxSide, flags = dirs[dir];
			for (i = 0, maxSide = max = 0; i < 4; i ++, flags >>= 1)
			{
				val = skyValue(xsky + xoff[i], ysky + yoff[i]);
				if (val == WALL || (flags&1) == 0) continue;
				if (i == 2 && val == MAXSKY) { max = MAXSKY+1; break; }
				if (i < 2 && maxSide < val) maxSide = val;
				if (max < val) max = val;
			}
			if (maxSide < sky && ysky > heightMap[xsky])
			{
				/* are we on a local maximum */
				max = skyValue(xsky, dir == 2 ? ysky + 1 : ysky - 1);
				if (max == WALL) max = 0;
				if (dir >= 2 && max == MAXSKY) max = MAXSKY+1;
			}
			if (max > 0) max--;
			if (sky != max)
			{
				out = fprintf(stderr, "setting %d,%d to %d (old: %d)\n", xsky-xstart, ysky-ystart, max, sky);
				*cell = max;
				/* check if surrounding cell depended on this value */
				for (i = 0, flags = opp[dir]; i < 4; i ++, flags >>= 1)
				{
					if ((flags & 1) == 0) continue;
					int x2 = xsky + xoff[i], y2 = ysky + yoff[i];
					val = skyIsIncorrect(x2, y2, i);
					if (val == 6) addTrack(x2 - xstart, y2 - ystart - 1, 2); else
//					if (i >= 2 && val) fprintf(stderr, "changing direction to %d\n", val-1);
					if (val) addTrack(x2 - xstart, y2 - ystart, i >= 2 ? val - 1 : i);
				}
			}
		}
	}
	fputc(out ? ' ' : '\n', stderr);
	fprintf(stderr, "need to check:");
	for (i = pos; i != last; )
	{
		fprintf(stderr, " %d,%d%s", track[i], track[i+1], "\x10\0\x11\0\x1e\0\x1f\0\x04" + track[i+2] * 2);
		i += 3;
		if (i == DIM(track)) i = 0;
	}
	fputc('\n', stderr);
}


/* redraw entire bitmap */
static void refreshBack(void)
{
	DATA8 light;
	DATA8 p, d, s;
	int   i, j, k;
	for (j = 0, p = back->bitmap, light = skyLight; j < CELLH; j ++, p += back->stride * CELLSZ)
	{
		for (i = 0, d = p; i < CELLW; i ++, d += CELLSZ, light++)
		{
			if (*light == WALL)
			{
				for (s = d, k = 0; k < CELLSZ; k ++, s += back->stride)
				{
					int l;
					for (l = 0; l < CELLSZ; l ++)
					{
						static uint8_t pattern[] = {0, 0, 255, 0, 0, 255, 0, 0, 255};
						s[l] = pattern[(i*CELLSZ+l+j*CELLSZ*CELLW+k) % 9];
					}
				}
				int sides = 0;
				if (j == 0       || light[-CELLW] != WALL) memset(d, 255, CELLSZ);
				if (i == 0       || light[-1] != WALL) sides |= 1;
				if (i == CELLW-1 || light[ 1] != WALL) sides |= 2;
				if (sides & 1) d[0] = 255;
				if (sides & 2) d[CELLSZ-1] = 255;

				for (s = d+back->stride, k = 1; k < CELLSZ; k ++, s += back->stride)
				{
					if (sides & 1) s[0] = 255;
					if (sides & 2) s[CELLSZ-1] = 255;
				}
				s -= back->stride;
				if (j == CELLH-1 || light[CELLW] != WALL) memset(s, 255, CELLSZ);
				if (sides & 1) s[0] = 255;
				if (sides & 2) s[CELLSZ-1] = 255;
				continue;
			}
			for (s = d, k = 0; k < CELLSZ; k ++, s += back->stride)
			{
				int val = (*light & 0xf) * 255 / MAXSKY;
				if (val > 255) val = 255;
				memset(s, val, CELLSZ);
			}
		}
	}
}

static int paint(SIT_Widget w, APTR gc, APTR ud)
{
	GFX_SetPixels(back, 0, 0, back->width, back->height, gc, 0, 0, back->width, back->height);
	return 1;
}

static int drag(SIT_Widget w, APTR cd, APTR ud)
{
	static int mx, my;
	SIT_OnMouse * msg = cd;
	TEXT coord[32];

	switch (msg->state) {
	case SITOM_Move:
		msg->x /= CELLSZ;
		msg->y /= CELLSZ;
		if (msg->x != mx || msg->y != my)
		{
			int bl = -1;
			mx = msg->x;
			my = msg->y;
			if (0 <= mx && mx < CELLW && 0 <= my && my < CELLH)
				bl = skyLight[mx+my*CELLW];
			sprintf(coord, "%d, %d: ", mx, my);
			if (bl == WALL)
				strcat(coord, "WALL");
			else
				sprintf(strchr(coord, 0), " light %d", bl);
			SIT_SetValues(ud, SIT_Title, coord, NULL);
		}
		break;
	case SITOM_CaptureMove:
	case SITOM_ButtonPressed:
		msg->x /= CELLSZ;
		msg->y /= CELLSZ;
		if (msg->x >= 0 && msg->x < CELLW && msg->y >= 0 && msg->y < CELLH)
		{
			DATA8 cell = skyLight + msg->y * CELLW + msg->x;
			/* right click: clear tile, left click: add wall */
			if (msg->button == 0 && *cell != WALL)
			{
				*cell = WALL;
				goto update;
			}
			if (msg->button == 1 && *cell == WALL)
			{
				*cell = 0;
				update:
				skyUpdateInit(msg->x, msg->y, 1, 1);
				skyUpdate();
				refreshBack();
				SIT_Refresh(w, 0, 0, 0, 0, False);
				return 1;
			}
		}
		return 1;
	}
	return 0;
}

static int save(SIT_Widget w, APTR cd, APTR ud)
{
	if (skyLight)
	{
		FILE * out = fopen(SAVEFILE, "wb");
		fwrite(skyLight, 1, CELLW * CELLH, out);
		fclose(out);
	}
	return 1;
}

static int fullStep(SIT_Widget w, APTR cd, APTR ud)
{
	while (pos != last)
		skyUpdate();
	refreshBack();
	SIT_Refresh(ud, 0, 0, 0, 0, False);
	return 1;
}

static int reset(SIT_Widget w, APTR cd, APTR ud)
{
	#if 0
	FILE * in = fopen(SAVEFILE, "rb");
	if (in)
	{
		fread(skyLight, 1, CELLW * CELLH, in);
		fclose(in);
		refreshBack();
		SIT_Refresh(ud, 0, 0, 0, 0, False);
	}
	#else
	if (pos != last)
	{
		skyUpdate();
		refreshBack();
		SIT_Refresh(ud, 0, 0, 0, 0, False);
	}
	#endif
	return 1;
}

static int clear(SIT_Widget w, APTR cd, APTR ud)
{
	memset(skyLight, 0, CELLW*CELLH);
	refreshBack();
	SIT_Refresh(ud, 0, 0, 0, 0, False);
	return 1;
}

static int menuHandler(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_Exit(0);
	return 1;
}

int my_main(int nb, char * argv[])
{
	skyInit();
	SIT_MenuStruct menu[] = {
		{1, "&SkyLight"},
			{2, "Quit", "Esc", 0, 118},
		{0}
	};
	SIT_Widget w = SIT_CreateWidget("SkyLight", SIT_APP, NULL, NULL);

	SIT_Widget dialog = SIT_CreateWidget("MainWnd", SIT_DIALOG, w,
		SIT_Title,        "SkyLight",
		SIT_Styles,       SITV_NoResize,
		SIT_Margins,      8, 8, 8, 8,
		SIT_FocusOnClick, True,
		SIT_Menu,         menu,
		SIT_MenuVisible,  False,
		NULL
	);

	SIT_CreateWidgets(dialog,
		"<button name=clear title='Clear all'>"
		"<button name=save title='Save' left=WIDGET,clear,0.5em>"
		"<button name=reset title='Step' left=WIDGET,save,0.5em>"
		"<button name=full title='Full' left=WIDGET,reset,0.5em>"
		"<label name=coord title='' resizePolicy=", SITV_Fixed, "width=10em left=WIDGET,full,0.5em, top=MIDDLE,reset>"
		"<label name=help title='LMB: add wall, RMB: delete' right=FORM top=MIDDLE,reset>"
		"<canvas name=light background=0 top=WIDGET,clear,0.5em width=", CELLW*CELLSZ, "height=", CELLH*CELLSZ, "autofocus=1>"
	);

	w = SIT_GetById(dialog, "light");

	SIT_AddCallback(w, SITE_OnPaint,     paint, NULL);
	SIT_AddCallback(w, SITE_OnClickMove, drag,  SIT_GetById(dialog, "coord"));
	SIT_AddCallback(SIT_GetById(dialog, "clear"), SITE_OnActivate, clear, w);
	SIT_AddCallback(SIT_GetById(dialog, "save"),  SITE_OnActivate, save, NULL);
	SIT_AddCallback(SIT_GetById(dialog, "reset"),  SITE_OnActivate, reset, w);
	SIT_AddCallback(SIT_GetById(dialog, "full"),  SITE_OnActivate, fullStep, w);
	SIT_AddCallback(dialog, SITE_OnMenu, menuHandler, w);

	SIT_ManageWidget(dialog);

	return SIT_Main();
}
