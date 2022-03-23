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
#define LIGHTRADIUS   (MAXSKY*2-1)
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
static int8_t opp[]  = {1<<5, 0, 3<<5, 2<<5};
int8_t track[256];

/* adjust sky light level around block */
static void setBlock(int x, int y)
{
	int pos, last, i, maxUsage;

	if (y == CELLH-1) return;
	pos = last = 0;
	DATA8 sky = &skyLight[x+y*CELLW];
	if (heightMap[x] > y-1)
	{
		/* block is set at a higher position */
		for (i = y + 1; i <= heightMap[x]; i ++)
		{
			track[last ++] = MAXSKY | (2<<5);
			track[last ++] = i - y;
		}
		heightMap[x] = y-1;
	}
	else if (sky[CELLW] != WALL)
	{
		track[last ++] = MAXSKY | (2<<5);
		track[last ++] = 1;
	}
	if (x > 0 && heightMap[x-1] < y && sky[-1] != WALL)
	{
		track[last ++] = MAXSKY-1;
		track[last ++] = 0;
	}
	if (x < CELLW-1 && heightMap[x+1] < y && sky[1] != WALL)
	{
		track[last ++] = (MAXSKY+1) | (1<<5);
		track[last ++] = 0;
	}

	maxUsage = last - pos;
	while (pos != last)
	{
		int stop = last;

		while (pos != stop)
		{
			DATA8   p;
			uint8_t val, max, level, old;
			int8_t  dx   = (track[pos] & 31) - MAXSKY;
			int8_t  dy   = track[pos+1];
			int     xsky = x + dx;
			int     ysky = y + dy;
			p = &skyLight[xsky + ysky*CELLW];
			val = *p;

			/* is it a local maximum? */
			for (i = max = 0; i < 4; i ++)
			{
				int xs = xsky + xoff[i];
				int ys = ysky + yoff[i];
				sky = &skyLight[xs+ys*CELLW];
				if (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH && *sky != WALL && (level = *sky)-(i>=2) >= val)
				{
					if (max < level) max = level;
				}
			}
			if (max > 0)
			{
				old = max - 1;
				/* not a local maximum */
				if (val != old)
					*p = old;

				/* same values, check if surrounding need adjusment */
				for (i = 0; i < 4; i ++)
				{
					DATA8 prev = &skyLight[xsky + xoff[i] + (ysky + yoff[i]) * CELLW];
					if (*prev < max-2)
					{
						*prev = max-2;
						track[last++] = (MAXSKY + dx + xoff[i]) | opp[i];
						track[last++] = dy + yoff[i];
						if (last == DIM(track)) last = 0;

						int curmax = pos < last ? last - pos : last + DIM(track) - pos;
						if (maxUsage < curmax) maxUsage = curmax;
					}
				}
				if (val == old)
					goto skip;
			}
			else /* it is a local maximum */
			{
				i = track[pos] >> 5;
				uint8_t prev = skyLight[xsky + xoff[i] + (ysky + yoff[i]) * CELLW];
				old = val-1;
				*p = prev == WALL || prev == 0 ? 0 : prev - 1;
			}
//			fprintf(stderr, "setting %d, %d to %d\n", xsky, ysky, *p);
			/* check if neighbors depend on light level of cell we just changed */
			for (i = 0; i < 4; i ++)
			{
				int8_t dx2 = dx + xoff[i];
				int8_t dy2 = dy + yoff[i];
				int    xs  = x + dx2;
				int    ys  = y + dy2;
				sky = &skyLight[xs+ys*CELLW];
				if (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH && (level = *sky) != WALL && (level == old || (dy == 0 && i >= 2 && level == old+1)))
				{
					/* incorrect light level here */
					track[last ++] = (dx2 + MAXSKY) | opp[i];
					track[last ++] = dy2;
					if (last == DIM(track)) last = 0;

					int curmax = pos < last ? last - pos : last + DIM(track) - pos;
					if (maxUsage < curmax) maxUsage = curmax;
				}
			}
			skip:
			pos += 2;
			if (pos == DIM(track)) pos = 0;
		}
	}
	fprintf(stderr, "max usage = %d/%d\n", maxUsage, DIM(track));
}

#if 0
int j;
for (j = pos+3; j != last; j += 3)
{
	if (track[j] == dx2 && track[j+1] == dy2) { puts("already added"); break; }
	if (j == DIM(track)) j = 0;
}
if (j == last)
#endif

/* adjust light level after block has been removed */
static void unsetBlock(int x, int y)
{
	DATA8 p;
	int   pos, last, i;
	pos = last = 0;
	if (y-1 == heightMap[x])
	{
		/* highest block removed: compute new height */
		for (i = y, p = &skyLight[x+i*CELLW]; i < CELLH && *p != WALL; i ++, p += CELLW)
		{
			*p = MAXSKY;
			track[last ++] = 0;
			track[last ++] = i - y;
		}
		heightMap[x] = i-1;
	}
	else
	{
		int max;
		/* look around for a skylight value */
		for (i = max = 0; i < 4; i ++)
		{
			int xs = x + xoff[i];
			int ys = y + yoff[i];

			p = &skyLight[xs+ys*CELLW];
			if (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH && *p != WALL && *p > max)
				max = *p;
		}
		if (max > 0)
		{
			track[last ++] = 0;
			track[last ++] = 0;
			skyLight[x+y*CELLW] = max-1;
		}
	}

	while (pos != last)
	{
		int posold = pos;
		int stop = last;
		/* horizontal spread first */
		while (pos != stop)
		{
			int xsky = x + track[pos];
			int ysky = y + track[pos+1];
			uint8_t val, lv;
			p = &skyLight[xsky + ysky*CELLW];
			val = *p-1;

			if (xsky < CELLW-1 && (lv = p[1]) != WALL && lv < val)
			{
				/* incorrect light level here */
				p[1] = val;
				track[last ++] = track[pos]+1;
				track[last ++] = track[pos+1];
				if (last == DIM(track)) last = 0;
			}
			if (xsky > 0 && (lv = p[-1]) != WALL && lv < val)
			{
				p[-1] = val;
				track[last ++] = track[pos]-1;
				track[last ++] = track[pos+1];
				if (last == DIM(track)) last = 0;
			}
			pos += 2;
			if (pos == DIM(track)) pos = 0;
		}
		/* then vertical */
		pos = posold;
		while (pos != stop)
		{
			int xsky = x + track[pos];
			int ysky = y + track[pos+1];
			uint8_t val, lv;
			p = &skyLight[xsky + ysky*CELLW];
			val = *p-1;

			if (ysky < CELLH-1 && (lv = p[CELLW]) != WALL && lv < val)
			{
				p[CELLW] = val;
				track[last ++] = track[pos];
				track[last ++] = track[pos+1]+1;
				if (last == DIM(track)) last = 0;
			}
			if (ysky > 0 && (lv = p[-CELLW]) != WALL && lv < val)
			{
				p[-CELLW] = val;
				track[last ++] = track[pos];
				track[last ++] = track[pos+1]-1;
				if (last == DIM(track)) last = 0;
			}
			pos += 2;
			if (pos == DIM(track)) pos = 0;
		}
	}
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
			DATA8 pos = skyLight + msg->y * CELLW + msg->x;
			/* right click: clear tile, left click: add wall */
			if (msg->button == 0 && *pos != WALL)
			{
				*pos = WALL;
				setBlock(msg->x, msg->y);
				refreshBack();
				SIT_Refresh(w, 0, 0, 0, 0, False);
				return 1;
			}
			if (msg->button == 1 && *pos == WALL)
			{
				*pos = 0;
				unsetBlock(msg->x, msg->y);
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

static int reset(SIT_Widget w, APTR cd, APTR ud)
{
	FILE * in = fopen(SAVEFILE, "rb");
	if (in)
	{
		fread(skyLight, 1, CELLW * CELLH, in);
		fclose(in);
		refreshBack();
		SIT_Refresh(ud, 0, 0, 0, 0, False);
	}
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
		"<button name=reset title='Reset' left=WIDGET,save,0.5em>"
		"<label name=coord title='' resizePolicy=", SITV_Fixed, "width=10em left=WIDGET,reset,0.5em, top=MIDDLE,reset>"
		"<label name=help title='LMB: add wall, RMB: delete' right=FORM top=MIDDLE,reset>"
		"<canvas name=light background=0 top=WIDGET,clear,0.5em width=", CELLW*CELLSZ, "height=", CELLH*CELLSZ, "autofocus=1>"
	);

	w = SIT_GetById(dialog, "light");

	SIT_AddCallback(w, SITE_OnPaint,     paint, NULL);
	SIT_AddCallback(w, SITE_OnClickMove, drag,  SIT_GetById(dialog, "coord"));
	SIT_AddCallback(SIT_GetById(dialog, "clear"), SITE_OnActivate, clear, w);
	SIT_AddCallback(SIT_GetById(dialog, "save"),  SITE_OnActivate, save, NULL);
	SIT_AddCallback(SIT_GetById(dialog, "reset"),  SITE_OnActivate, reset, w);
	SIT_AddCallback(dialog, SITE_OnMenu, menuHandler, w);

	SIT_ManageWidget(dialog);

	return SIT_Main();
}
