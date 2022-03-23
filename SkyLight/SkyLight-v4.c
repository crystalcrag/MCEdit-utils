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
#define CELLSZ        11
#define MAXSKY        8
#define WALL          0x70
#define WATER         0x80
#define LEAVES        0xa0
#define SAVEFILE      "skylight.map"
#define STEPBYSTEP

static uint8_t colors[] = {
	0xa6, 0xeb, 0xff, 0xff,   /* sky color */
	0x33, 0x33, 0x33, 0xff,   /* cave color */
	0xbb, 0xaa, 0xaa, 0xff,   /* wall color */
	0x10, 0x10, 0xff, 0xff,   /* water color */
	0x10, 0x88, 0x10, 0xff,   /* leaf color */
	0xff, 0xff, 0x00, 0xff,   /* heightmap lines */
};
static DATA8   skyLight;
static DATA8   blockId;
static uint8_t heightMap[CELLW];
static Image   back;
static int     blockType;

static void refreshBack(void);

static void skyInit(void)
{
	skyLight = calloc(CELLW * 2, CELLH);
	blockId  = skyLight + CELLH*CELLW;
	back     = GFX_CreateImage(CELLW*CELLSZ, CELLH*CELLSZ, 24);

	memset(back->bitmap, 0, back->stride * back->height);

	/* restore previous state */
	FILE * in = fopen(SAVEFILE, "rb");
	if (in)
	{
		DATA8 p;
		int   i, j;

		fread(skyLight, 1, CELLW * CELLH, in);
		fclose(in);

		/* separate skylight and blockId */
		for (i = 0; i < CELLW*CELLH; i ++)
			blockId[i] = skyLight[i] & 0xf0, skyLight[i] &= 15;

		/* recompute heightmap */
		for (j = 0; j < CELLW; j ++)
		{
			for (i = 0, p = blockId + j; i < CELLH && p[0] == 0; i ++, p += CELLW);
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
int    pos, last, startx, starty;

static int getSkyOpacity(int blockId)
{
	switch (blockId) {
	case 0:      return 0;
	case LEAVES: return 1;
	case WATER:  return 2;
	default:     return MAXSKY;
	}
}

/* recalc everything (note: heightmap need to be correct) */
static void recalcSkyLight(void)
{
	DATA8 sky, block;
	int i, j;

	pos = last = 0;

	memset(skyLight, 0, CELLW*CELLH);

	for (i = 0, block = blockId, sky = skyLight; i < CELLW; i ++, sky ++, block ++)
	{
		uint8_t height[2];
		DATA8 skyCol, blockCol;
		height[0] = i > 0 ? heightMap[i-1] : 255;
		height[1] = i < CELLW-1 ? heightMap[i+1] : 255;
		for (j = 0, skyCol = sky, blockCol = block; j < CELLH; j ++, skyCol += CELLW, blockCol += CELLW)
		{
			switch (*blockCol) {
			case 0:
				*skyCol = MAXSKY;
				if ((j > height[0] && blockCol[-1] != WALL) || (j > height[1] && blockCol[1] != WALL))
					track[last++] = i, track[last++] = j;
				break;
			case WALL: j = CELLH; break;
			case LEAVES:
				*skyCol = MAXSKY-1;
				track[last++] = i, track[last++] = j;
				j = CELLH;
				break;
			case WATER:
				*skyCol = MAXSKY-3;
				track[last++] = i, track[last++] = j;
				j = CELLH;
			}
		}
	}

	while (pos != last)
	{
		int x = track[pos];
		int y = track[pos+1];
		pos += 2;
		if (pos == DIM(track)) pos = 0;

		uint8_t sky = skyLight[x+y*CELLW];
		for (i = 0; i < 4; i ++)
		{
			int x2 = x + xoff[i];
			int y2 = y + yoff[i];

			if (x2 < 0 || x2 >= CELLW || y2 >= CELLH) continue;
			int    off = x2+y2*CELLW;
			int8_t col = getSkyOpacity(blockId[off]);
			if (col == 0 && (i < 2 || col < MAXSKY)) col = 1;
			col = sky - col;
			if (col < 0) col = 0;
			if (col > MAXSKY) col = MAXSKY;
			if (skyLight[off] < col)
			{
				skyLight[off] = col;
				track[last ++] = x2;
				track[last ++] = y2;
				if (last == DIM(track)) last = 0;
				if (last == pos) {
					puts("overflow?");
				}
			}
		}
	}
}

/* adjust sky light level around block */
static void setBlockInit(int x, int y)
{
	int i;

	if (y == CELLH-1) return;
	pos = last = 0;
	startx = x;
	starty = y;
	DATA8 block = &blockId[x+y*CELLW];
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
	else
	{
		if (block[CELLW] != WALL)
		{
			track[last ++] = MAXSKY | (2<<5);
			track[last ++] = 1;
		}
		if (block[-CELLW] != WALL)
		{
			track[last ++] = MAXSKY | (3<<5);
			track[last ++] = -1;
		}
	}
	if (x > 0 && heightMap[x-1] < y && block[-1] != WALL)
	{
		track[last ++] = MAXSKY-1;
		track[last ++] = 0;
	}
	if (x < CELLW-1 && heightMap[x+1] < y && block[1] != WALL)
	{
		track[last ++] = (MAXSKY+1) | (1<<5);
		track[last ++] = 0;
	}
}

void setBlock(void)
{
	int i;
	while (pos != last)
	{
		DATA8   p, sky;
		uint8_t val, max, level, old;
		int8_t  dx   = (track[pos] & 31) - MAXSKY;
		int8_t  dy   = track[pos+1];
		int     xsky = startx + dx;
		int     ysky = starty + dy;
		p = &skyLight[xsky + ysky*CELLW];
		val = *p;

		/* is it a local maximum? */
		for (i = max = 0; i < 4; i ++)
		{
			int xs = xsky + xoff[i];
			int ys = ysky + yoff[i];
			sky = &skyLight[xs+ys*CELLW];
			if (! (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH)) continue;
			level = *sky;
			if (blockId[xs+ys*CELLW] != WALL && level-(i>=2) >= val && max < level)
				max = level;
		}
		if (max > 0)
		{
			old = max - 1;
			/* not a local maximum (there is a cell with higher value nearby) */
			if (val != old)
			{
				//fprintf(stderr, "setting %d,%d to %d (old: %d)\n", xsky-startx, ysky-starty, *p, val);
				*p = old;
			}

			/* same values, check if surrounding need adjusment */
			for (i = 0; i < 4; i ++)
			{
				DATA8 prev = &skyLight[xsky + xoff[i] + (ysky + yoff[i]) * CELLW];
				if (*prev < max-2)
				{
					*prev = max-2;
					//fprintf(stderr, "setting %d,%d to %d (old: %d)\n", xsky+xoff[i]-startx, ysky+yoff[i]-starty, *prev, max-2);
					track[last++] = (MAXSKY + dx + xoff[i]) | opp[i];
					track[last++] = dy + yoff[i];
					if (last == DIM(track)) last = 0;
				}
			}
			if (val == old)
				goto skip;
		}
		else /* it is a local maximum */
		{
			i = track[pos] >> 5;
			int     off  = xsky + xoff[i] + (ysky + yoff[i]) * CELLW;
			uint8_t prev = skyLight[off];
			old = val-1;
			*p = blockId[off] == WALL || prev == 0 ? 0 : prev - 1;
			//fprintf(stderr, "setting %d,%d to %d (old: %d)\n", xsky-startx, ysky-starty, *p, val);
		}
//		fprintf(stderr, "setting %d, %d to %d\n", xsky, ysky, *p);
		/* check if neighbors depended on light level of cell we just changed */
		for (i = 0; i < 4; i ++)
		{
			int8_t dx2 = dx + xoff[i];
			int8_t dy2 = dy + yoff[i];
			int    xs  = startx + dx2;
			int    ys  = starty + dy2;
			if (! (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH)) continue;
			level = skyLight[xs+ys*CELLW];
			if (level > 0 && blockId[xs+ys*CELLW] != WALL && (level == old || (dy == 0 && i >= 2 && level == old+1)))
			{
				/* incorrect light level here */
				track[last ++] = (dx2 + MAXSKY) | opp[i];
				track[last ++] = dy2;
				if (last == DIM(track)) last = 0;
			}
		}
		skip:
		pos += 2;
		if (pos == DIM(track)) pos = 0;
	}
	#if 0
	fprintf(stderr, " need to check:");
	for (i = pos; i != last; )
	{
		int x = track[i];
		int dir = x >> 5;
		x = (x&31) - MAXSKY;
		fprintf(stderr, " %d,%d%s", x, track[i+1], "\x10\0\x11\0\x1e\0\x1f\0\x04" + dir * 2);
		i += 2;
		if (i == DIM(track)) i = 0;
	}
	fputc('\n', stderr);
	#endif
}

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
			if (val == 255) val = 0;

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
			if (val == 255) val = 0;

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

static void setTile(DATA8 dest, DATA8 color, DATA8 pattern, int size)
{
	DATA8 d;
	int   i, j;

	if (pattern)
	{
		int off = dest - back->bitmap;
		off = off / back->stride + (off % back->stride) / 3;

		for (j = 0; j < CELLSZ; j ++, dest += back->stride)
			for (i = 0, d = dest; i < CELLSZ; i ++, d += 3)
				if (pattern[(i+j+off) % size] > 0)
					d[2] = color[0], d[1] = color[1], d[0] = color[2];
	}
	else
	{
		for (j = 0; j < CELLSZ; j ++, dest += back->stride)
			for (i = 0, d = dest; i < CELLSZ; d[2] = color[0], d[1] = color[1], d[0] = color[2], i ++, d += 3);
	}
}

/* redraw entire bitmap */
static void refreshBack(void)
{
	DATA8 light, block;
	DATA8 p, d;
	int   i, j, k;
	for (j = 0, p = back->bitmap, light = skyLight, block = blockId; j < CELLH; j ++, p += back->stride * CELLSZ)
	{
		for (i = 0, d = p; i < CELLW; i ++, d += CELLSZ*3, light++, block++)
		{
			static uint8_t pattern1[] = {0, 255};
			static uint8_t pattern2[] = {0, 0, 255};

			uint8_t blend[3], pattern;
			DATA8 color;
			switch (block[0]) {
			case WALL:   color = colors + 8;  pattern = 0; break;
			case WATER:  color = colors + 12; pattern = 1; break;
			case LEAVES: color = colors + 16; pattern = 2; break;
			case 0:      color = blend;
			}
			k = light[0];
			blend[0] = (colors[0] * k + colors[4] * (MAXSKY-k)) / MAXSKY;
			blend[1] = (colors[1] * k + colors[5] * (MAXSKY-k)) / MAXSKY;
			blend[2] = (colors[2] * k + colors[6] * (MAXSKY-k)) / MAXSKY;

			if (pattern > 0)
			{
				/* show sky below */
				setTile(d, blend, NULL, 0);
			}

			switch (pattern) {
			case 0: setTile(d, color, NULL,     0); break; /* solid color */
			case 1: setTile(d, color, pattern1, 2); break; /* 50% dither */
			case 2: setTile(d, color, pattern2, 3); break; /* 66% dither */
			}
		}
	}
	/* show heightmap */
	for (i = 0; i < CELLW; i ++)
	{
		j = heightMap[i];

		DATA8 dst = back->bitmap + (j * CELLSZ + CELLSZ-1) * back->stride + i * CELLSZ * 3;

		for (j = 0; j < CELLSZ; dst[2] = colors[20], dst[1] = colors[21], dst[0] = colors[22], dst += 3, j ++);
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
			DATA8 pos = blockId + msg->y * CELLW + msg->x;
			/* right click: clear tile, left click: add wall */
			if (msg->button == 0 && *pos != WALL)
			{
				static uint8_t blocks[] = {WALL, LEAVES, WATER};
				*pos = blocks[blockType];
				setBlockInit(msg->x, msg->y);
				setBlock();
				refresh:
				refreshBack();
				SIT_Refresh(w, 0, 0, 0, 0, False);
				return 1;
			}
			if (msg->button == 1 && *pos == WALL)
			{
				*pos = 0;
				unsetBlock(msg->x, msg->y);
				goto refresh;
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
		int    i;

		DATA8 combine = malloc(CELLW*CELLH);
		for (i = 0; i < CELLW * CELLH; i ++)
			combine[i] = blockId[i] | skyLight[i];

		fwrite(combine, 1, CELLW * CELLH, out);
		fclose(out);
		free(combine);
	}
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
	memset(skyLight, 0, CELLW*CELLH);
	refreshBack();
	SIT_Refresh(ud, 0, 0, 0, 0, False);
	#endif
	return 1;
}

static int full(SIT_Widget w, APTR cd, APTR ud)
{
	recalcSkyLight();
	refreshBack();
	SIT_Refresh(ud, 0, 0, 0, 0, False);
	return 1;
}

static int clear(SIT_Widget w, APTR cd, APTR ud)
{
	memset(skyLight, 0, CELLW*CELLH);
	refreshBack();
	SIT_Refresh(ud, 0, 0, 0, 0, False);
	return 1;
}

static int toggleBlock(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_Widget * list = ud;
	int button = 0;
	if (w == list[1]) button = 1; else
	if (w == list[2]) button = 2;

	if (button != blockType)
	{
		SIT_SetValues(list[blockType], SIT_CheckState, 0, NULL);
		SIT_SetValues(list[button],    SIT_CheckState, 1, NULL);
		blockType = button;
	}

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
		"<button name=full title='Full' left=WIDGET,reset,0.5em>"
		"<label name=coord title='' resizePolicy=", SITV_Fixed, "width=10em left=WIDGET,full,0.5em, top=MIDDLE,reset>"

		"<button name=opaque checkState=1 buttonType=", SITV_ToggleButton, "left=WIDGET,coord>"
		"<button name=leaves buttonType=", SITV_ToggleButton, "left=WIDGET,opaque>"
		"<button name=water  buttonType=", SITV_ToggleButton, "left=WIDGET,leaves>"

		"<label name=help title='LMB: add wall, RMB: delete' right=FORM top=MIDDLE,reset>"
		"<canvas name=light background=0 top=WIDGET,clear,0.5em width=", CELLW*CELLSZ, "height=", CELLH*CELLSZ, "autofocus=1>"
	);

	w = SIT_GetById(dialog, "light");

	SIT_Widget toggle[3];
	toggle[0] = SIT_GetById(dialog, "opaque");
	toggle[1] = SIT_GetById(dialog, "leaves");
	toggle[2] = SIT_GetById(dialog, "water");

	SIT_AddCallback(w, SITE_OnPaint,     paint, NULL);
	SIT_AddCallback(w, SITE_OnClickMove, drag,  SIT_GetById(dialog, "coord"));
	SIT_AddCallback(SIT_GetById(dialog, "clear"), SITE_OnActivate, clear, w);
	SIT_AddCallback(SIT_GetById(dialog, "save"),  SITE_OnActivate, save, NULL);
	SIT_AddCallback(SIT_GetById(dialog, "reset"),  SITE_OnActivate, reset, w);
	SIT_AddCallback(SIT_GetById(dialog, "full"),  SITE_OnActivate, full, w);
	SIT_AddCallback(toggle[0], SITE_OnActivate, toggleBlock, toggle);
	SIT_AddCallback(toggle[1], SITE_OnActivate, toggleBlock, toggle);
	SIT_AddCallback(toggle[2], SITE_OnActivate, toggleBlock, toggle);
	SIT_AddCallback(dialog, SITE_OnMenu, menuHandler, w);

	SIT_ManageWidget(dialog);

	return SIT_Main();
}
