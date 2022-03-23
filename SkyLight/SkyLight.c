/*
 * SkyLight.c : 2d dynamic sky light test: this is the part that updates numeric values.
 *
 * this is the reference algorithms that is used by MCEdit to update skylight value.
 *
 * Written by T.Pierron, aug 2019.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "SkyLight.h"

static struct TrackUpdate_t track;
extern struct SkyLight_t    prefs;

/* enmerate neighbor in the order: Right, Left, Bottom, Top */
static int8_t xoff[] = {1, -1,  0, 0};
static int8_t yoff[] = {0,  0, -1, 1};
static int8_t opp[]  = {1<<5, 0, 3<<5, 2<<5};


/* use for debugging step by step */
void skyGetNextCell(int XY[2])
{
	if (track.usage > 0)
	{
		int x = track.coord[track.pos];
		int y = track.coord[track.pos+1];

		if (track.unique)
			x = (x & 31) - MAXSKY;

		XY[0] = track.startX + x;
		XY[1] = track.startY + y;
	}
	else XY[0] = XY[1] = -1;
}


#define STEP     256   /* need to be multiple of 2 */

#define mapUpdateInitTrack(track)    \
	memset(&track.max, 0, sizeof track - offsetof(struct TrackUpdate_t, max));

/* coordinates that will need further investigation for skylight/blocklight */
static void trackAdd(int x, int y)
{
	int8_t * buffer;
	/* this is an expanding ring buffer */
	if (track.usage == track.max)
	{
		/* not enough space left */
		track.max += STEP;
		buffer = realloc(track.coord, track.max);
		if (! buffer) return;
		track.coord = buffer;
		if (track.last > 0)
		{
			int nb = track.last;
			if (nb > STEP) nb = STEP;
			memcpy(track.coord + track.usage, track.coord, nb);
			if (nb < track.last) memmove(track.coord, track.coord + nb, track.last - nb);
		}
		track.last -= STEP;
		if (track.last < 0)
			track.last += track.max;
	}
	if (track.unique)
	{
		int i;
		for (i = track.usage, buffer = track.coord + track.last; i > 0; i -= 2)
		{
			if (buffer == track.coord) buffer += track.max;
			buffer -= 2;
			if ((buffer[0]&31) == (x&31) && buffer[1] == y)
				return;
		}
	}
	buffer = track.coord + track.last;
	buffer[0] = x;
	buffer[1] = y;
	track.last += 2;
	track.usage += 2;
	if (track.last == track.max)
		track.last = 0;
	if (track.maxUsage < track.usage)
		track.maxUsage = track.usage;
}

static int skyGetOpacity(int blockId, int min)
{
	switch (blockId) {
	case BLOCK_AIR:   return min;
	case BLOCK_LEAVE: return 1;
	case BLOCK_WATER: return 2;
	default:          return MAXSKY;
	}
}

/* recalc everything (note: heightmap needs to be correct) */
static void skyRecalcLight(void)
{
	DATA8 sky, block;
	int i, j;

	mapUpdateInitTrack(track);
	memset(prefs.skyLight, 0, CELLW*CELLH);

	for (i = 0, block = prefs.blockIds, sky = prefs.skyLight; i < CELLW; i ++, sky ++, block ++)
	{
		uint8_t height[2];
		DATA8 skyCol, blockCol;
		height[0] = i > 0 ? prefs.heightMap[i-1] : 255;
		height[1] = i < CELLW-1 ? prefs.heightMap[i+1] : 255;
		for (j = 0, skyCol = sky, blockCol = block; j < CELLH; j ++, skyCol += CELLW, blockCol += CELLW)
		{
			switch (*blockCol) {
			case BLOCK_AIR:
				*skyCol = MAXSKY;
				if ((j > height[0] && skyGetOpacity(blockCol[-1], 0) < MAXSKY) ||
				    (j > height[1] && skyGetOpacity(blockCol[ 1], 0) < MAXSKY))
					trackAdd(i, j);
				break;
			case BLOCK_OPAQUE: j = CELLH; break;
			case BLOCK_LEAVE:
			case BLOCK_WATER:
				*skyCol = MAXSKY - skyGetOpacity(*blockCol, 1);
				trackAdd(i, j);
				j = CELLH;
			}
		}
	}

	while (track.usage > 0)
	{
		int x = track.coord[track.pos];
		int y = track.coord[track.pos + 1];
		track.pos += 2;
		track.usage -= 2;
		if (track.pos == track.max) track.pos = 0;

		uint8_t skyVal = prefs.skyLight[x+y*CELLW];
		for (i = 0; i < 4; i ++)
		{
			int x2 = x + xoff[i];
			int y2 = y + yoff[i];

			if (x2 < 0 || x2 >= CELLW || y2 >= CELLH) continue;
			int    off = x2+y2*CELLW;
			int8_t col = skyVal - skyGetOpacity(prefs.blockIds[off], i < 2 || col < MAXSKY);
			if (col < 0) col = 0;
			if (col > MAXSKY) col = MAXSKY;
			if (prefs.skyLight[off] < col)
			{
				prefs.skyLight[off] = col;
				trackAdd(x2, y2);
			}
		}
	}
}

/* adjust sky light level around block */
void skySetBlockInit(int x, int y)
{
	int i = x+y*CELLW;

	if (y == CELLH-1) return;
	mapUpdateInitTrack(track);
	track.startX = x;
	track.startY = y;
	track.unique = 1;

	DATA8  block = &prefs.blockIds[i];
	int8_t sky   = prefs.skyLight[i] - skyGetOpacity(*block, 0);
	if (sky < 0) sky = 0;

	if (prefs.skyLight[i] != sky)
		trackAdd(MAXSKY | (4<<5), 0);

	if (prefs.heightMap[x] > y-1)
	{
		/* block is set at a higher position */
		for (i = y+1; i <= prefs.heightMap[x]; i ++)
			trackAdd(MAXSKY | (2<<5), i - y);

		prefs.heightMap[x] = y-1;
	}
	else
	{
		if (skyGetOpacity(block[CELLW], 0) < MAXSKY)
			trackAdd(MAXSKY | (2<<5), 1);

		if (skyGetOpacity(block[CELLW], 0) < MAXSKY)
			trackAdd(MAXSKY | (3<<5), -1);
	}
	if (x > 0 && prefs.heightMap[x-1] < y && skyGetOpacity(block[-1], 0) < MAXSKY)
		trackAdd(MAXSKY-1, 0);

	if (x < CELLW-1 && prefs.heightMap[x+1] < y && skyGetOpacity(block[1], 0) < MAXSKY)
		trackAdd((MAXSKY+1) | (1<<5), 0);
}

/* skylight is blocked */
int skySetBlock(void)
{
	if (track.usage > 0)
	{
		int8_t sky, max, level, newsky;
		int8_t dx   = (track.coord[track.pos] & 31) - MAXSKY;
		int8_t dy   = track.coord[track.pos+1];
		int8_t dir  = (track.coord[track.pos] >> 5) & 7;
		int    xsky = track.startX + dx;
		int    ysky = track.startY + dy;
		int    off  = xsky + ysky * CELLW;
		int    i;

		sky = prefs.skyLight[off];

		/* is it a local maximum? */
		for (i = max = 0; i < 4; i ++)
		{
			int xs = xsky + xoff[i];
			int ys = ysky + yoff[i];
			int off2 = xs + ys * CELLW;
			if (! (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH)) continue;
			level = prefs.skyLight[off2];
			if (level-(i>>1) >= sky && max < level)
			{
				max = level;
				if (max == MAXSKY) break;
			}
		}
		if (max > 0)
		{
			/* not a local maximum (there is a cell with higher value nearby) */
			newsky = prefs.skyLight[off] = max - skyGetOpacity(prefs.blockIds[off], 1);
			//fprintf(stderr, "setting %d,%d to %d (old: %d)\n", dx, dy, newsky, sky);

			/* check if surrounding values need increase in sky level (note: direction is reversed compared to next loop) */
			if (newsky <= 0) newsky = 0;
			for (i = 0; i < 4; i ++)
			{
				int   off2 = xsky + xoff[i] + (ysky + yoff[i]) * CELLW;
				DATA8 prev = &prefs.skyLight[off2];
				int8_t min = newsky - skyGetOpacity(prefs.blockIds[off2], 1);
				if (*prev < min)
				{
					*prev = min;
					//fprintf(stderr, "setting %d,%d to %d (old: %d)\n", xsky+xoff[i]-startx, ysky+yoff[i]-starty, *prev, max-2);
					trackAdd((MAXSKY + dx + xoff[i]) | opp[i], dy + yoff[i]);
				}
			}
			if (sky == newsky)
				goto skip;
		}
		else /* it is a local maximum */
		{
			int    off2 = xsky + xoff[dir] + (ysky + yoff[dir]) * CELLW;
			int8_t prev = prefs.skyLight[off2] - skyGetOpacity(prefs.blockIds[off], 1);
			if (dx == 1 || dx == -1)
			{
				/* painful but required: local max can be somewhere other than <dir> */
				for (i = 0; i < 4; i ++)
				{
					off2 = xsky + xoff[i] + (ysky + yoff[i]) * CELLW;
					if (prefs.skyLight[off2] == sky)
					{
						/* that could be another local max */
						uint8_t j;
						for (j = 0; j < 4; j ++)
						{
							int off3 = off2 + xoff[j] + yoff[j] * CELLW;
							if (prefs.skyLight[off3] > sky) { prev = sky - skyGetOpacity(prefs.blockIds[off], 1); goto break_all; }
						}
					}
				}
			}
			break_all:
			prefs.skyLight[off] = prev < 0 ? 0 : prev;
			//fprintf(stderr, "setting %d,%d to %d (old: %d)\n", dx, dy, skyLight[off], sky);
		}
		//fprintf(stderr, "setting %d, %d to %d\n", xsky, ysky, newsky);
		/* check if neighbors depended on light level of cell we just changed */
		for (i = 0; i < 4; i ++)
		{
			int8_t dx2 = dx + xoff[i];
			int8_t dy2 = dy + yoff[i];
			int    xs  = track.startX + dx2;
			int    ys  = track.startY + dy2;
			if (! (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH)) continue;
			if (i == dir) continue;
			off = xs + ys * CELLW;
			if (prefs.blockIds[off] == BLOCK_OPAQUE) continue;
			level = prefs.skyLight[off];
			newsky = sky - skyGetOpacity(prefs.blockIds[off], 1);
			if (level > 0 && level == newsky)
			{
				/* incorrect light level here */
				trackAdd((dx2 + MAXSKY) | opp[i], dy2);
			}
		}
		skip:
		track.pos += 2;
		track.usage -= 2;
		if (track.pos == track.max) track.pos = 0;
		return 1;
	}
	return 0;
}

/*
 * block removed
 */
void skyUnsetBlockInit(int x, int y)
{
	int i, max;
	mapUpdateInitTrack(track);
	track.startX = x;
	track.startY = y;

	if (y-1 == prefs.heightMap[x])
	{
		DATA8 p;
		/* highest block removed: compute new height */
		for (i = y, p = &prefs.blockIds[x+i*CELLW]; i < CELLH && *p == BLOCK_AIR; i ++, p += CELLW)
		{
			prefs.skyLight[x+i*CELLW] = MAXSKY;
			trackAdd(0, i - y);
		}
		prefs.heightMap[x] = i-1;
	}
	else
	{
		/* look around for a skylight value */
		for (i = max = 0; i < 4; i ++)
		{
			int xs = x + xoff[i];
			int ys = y + yoff[i];

			uint8_t sky = prefs.skyLight[xs+ys*CELLW];
			if (0 <= xs && xs < CELLW && 0 <= ys && ys < CELLH && sky > max)
				max = sky;
		}
		if (max > 0)
		{
			trackAdd(0, 0);
			prefs.skyLight[x+y*CELLW] = max-1;
		}
	}
}

/* adjust light level after block has been removed */
int skyUnsetBlock(void)
{
	/* quite a lot simpler, since we don't have to backtrack */
	if (track.usage > 0)
	{
		int xsky = track.startX + track.coord[track.pos];
		int ysky = track.startY + track.coord[track.pos+1];
		int i;

		uint8_t sky = prefs.skyLight[xsky+ysky*CELLW];
		for (i = 0; i < 4; i ++)
		{
			int x2 = xsky + xoff[i];
			int y2 = ysky + yoff[i];

			if (x2 < 0 || x2 >= CELLW || y2 >= CELLH) continue;
			int    off = x2+y2*CELLW;
			int8_t col = sky - skyGetOpacity(prefs.blockIds[off], i < 2 || col < MAXSKY);
			if (col < 0) col = 0;
			if (col > MAXSKY) col = MAXSKY;
			if (prefs.skyLight[off] < col)
			{
				prefs.skyLight[off] = col;
				trackAdd(x2 - track.startX, y2 - track.startY);
			}
		}
		track.pos += 2;
		track.usage -= 2;
		if (track.pos == track.max) track.pos = 0;
		return 1;
	}
	return 0;
}

/* quick and dirty 2D terrain generator */
void skyGenTerrain(void)
{
	memset(prefs.skyLight, 0, CELLW*CELLH);
	memset(prefs.blockIds, 0, CELLW*CELLH);

	/* rough terrain */
	DATA8 p;
	int   i, j, height = RandRange(50, 60);
	int   minX = 0, minY = 0;
	for (i = 0; i < CELLW; i ++)
	{
		p = prefs.blockIds + i;
		for (j = height, p += j * CELLW; j < CELLH; p[0] = BLOCK_OPAQUE, j ++, p += CELLW);
		if (minY < height)
			minY = height, minX = i;


		height += RandRange(-3, 3);
		if (height < 20) height = 20;
		if (height >= CELLH) height = CELLH-1;
	}

	height = RandRange(5, 15);

	/* flood the lowest point (minX, minY) with water */
	mapUpdateInitTrack(track);
	trackAdd(minX, minY);
	while (track.usage > 0)
	{
		int x = track.coord[track.pos];
		int y = track.coord[track.pos + 1];
		track.pos += 2;
		track.usage -= 2;
		if (track.pos == track.max) track.pos = 0;

		/* check 4 surrounding blocks for air */
		for (i = 0; i < 4; i ++)
		{
			int x2 = x + xoff[i];
			int y2 = y + yoff[i];
			if (x2 < 0 || x2 >= CELLW || y2 < 0 || y2 >= CELLH) continue;
			DATA8 block = &prefs.blockIds[x2+y2*CELLW];
			if (block[0] == BLOCK_AIR && minY - y2 < height)
			{
				block[0] = BLOCK_WATER;
				trackAdd(x2, y2);
			}
		}
	}

	/* try to plant a tree somewhere */
	for (i = 0; i < 10; i ++)
	{
		i = RandRange(10, CELLW-10);
		for (height = 0, p = prefs.blockIds + i; height < CELLH && p[0] == 0; height ++, p += CELLW);
		if (p[0] == BLOCK_OPAQUE)
		{
			int x = i, y, x2, y2;
			/* plant here */
			p = prefs.blockIds + x;
			/* trunk */
			j = RandRange(4, 8);
			p += height * CELLW;
			if (j >= height) j = height - 1;
			while (j >= 0) p[0] = BLOCK_OPAQUE, p -= CELLW, j --, height --;
			/* leaves */
			y = height;
			i = RandRange(5, 10);
			if ((i & 1) == 0) i ++;
			for (y2 = y - (i >> 1), j = i; j > 0; j --, y2 ++)
			{
				int k;
				for (x2 = x - (i >> 1), k = i; k > 0; k --, x2 ++)
				{
					if (x2 < 0 || x2 >= CELLW || y2 >= CELLH || y2 < 0) continue;
					p = &prefs.blockIds[x2 + y2 * CELLW];
					if (p[0] == BLOCK_AIR && (x2 - x) * (x2 - x) + (y2 - y) * (y2 - y) < (i * i >> 2))
						p[0] = BLOCK_LEAVE;
				}
			}
			break;
		}
	}

	/* add some caves */
	for (i = 0, height = 127; i < 3; i ++)
	{
		j = RandRange(0, CELLW-1);
		p = &prefs.blockIds[j + (CELLH-1)*CELLW];

		if (p[0] != BLOCK_OPAQUE) continue;

		/* remove some blocks */
		mapUpdateInitTrack(track);
		trackAdd(j, CELLH-1);
		while (track.usage > 0)
		{
			int x = track.coord[track.pos];
			int y = track.coord[track.pos + 1];
			track.pos += 2;
			track.usage -= 2;
			if (track.pos == track.max) track.pos = 0;

			for (j = 0; j < 4; j ++)
			{
				p = &prefs.blockIds[x + y * CELLW];

				int x2 = x, k;
				int y2 = y;
				for (k = 0; k < 5; k ++)
				{
					x2 += xoff[j];
					y2 += yoff[j];

					if (x2 < 0 || x2 >= CELLW || y2 >= CELLH || y2 < 0) continue;
					if (prefs.blockIds[x2 + y2 * CELLW] != BLOCK_OPAQUE) break;
				}
				if (k == 5 && (height > 0 || RandRange(0, 1) < 0.5f))
				{
					x2 = x + xoff[j];
					y2 = y + yoff[j];
					if (0 <= x2 && x2 < CELLW && 0 <= y2 && y2 < CELLH)
					{
						/* we can "dig" that way */
						p[0] = BLOCK_AIR;
						trackAdd(x2, y2);
						height --;
					}
				}
			}
		}
	}

	/* recalc heightmap */
	for (j = 0; j < CELLW; j ++)
	{
		for (i = 0, p = prefs.blockIds + j; i < CELLH && p[0] == 0; i ++, p += CELLW);
		prefs.heightMap[j] = i - 1;
	}

	skyRecalcLight();
}
