/*
 * maps.c : trimmed down maps.c from MCEdit v2 (mostly contain frustum culling functions).
 *
 * Written by T.Pierron, feb 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "SIT.h"
#include "maps.h"
#include "Frustum.h"

extern struct Frustum_t globals;

static struct MapFrustum_t frustum = {
	.neighbors    = {0x0202a02c, 0x00809426, 0x002202a9, 0x00081263, 0x0101601c, 0x00404c16, 0x00110199, 0x00040953},
	.chunkOffsets = {0,1,2,4,8,16,32,3,9,17,33,6,18,34,12,20,36,24,40,19,35,25,41,22,38,28,44}
};

static uint8_t edgeCheck[] = {
	/* index 6 (1step) + 12 (2steps) */
	19+0,   19+8, 19+16, 19+24, 19+32, 19+40,
	19+48, 19+50, 19+52, 19+54, 19+56, 19+58,
	19+60, 19+62, 19+64, 19+66, 19+68, 19+70,
	19+72,

	/* 1 step from center: S, E, N, W, T, B (4 lines to check) */
	2, 3, 3, 7, 6, 7, 6, 2,
	3, 1, 1, 5, 5, 7, 7, 3,
	1, 7, 7, 4, 4, 5, 5, 1,
	0, 2, 2, 6, 6, 4, 4, 0,
	4, 5, 5, 7, 7, 6, 6, 4,
	0, 1, 1, 3, 3, 2, 2, 0,

	/* 2 steps from center (1 line to check) */
	7, 3, 6, 2, 6, 7, 2, 3,
	5, 1, 7, 5, 3, 1, 0, 4,
	4, 5, 0, 1, 4, 6, 0, 2,

};

/* given a direction encoded as bitfield (S, E, N, W), return offset of where that chunk is */
int16_t chunkNeighbor[16*9];

uint8_t multiplyDeBruijnBitPosition[] = {
	0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
	31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

uint8_t mask8bit[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

static uint8_t opp[] = {2, 3, 0, 1, 5, 4};

/* given a S,E,N,W,T,B bitfield, will give what face connections we can reach */
uint16_t faceCnx[] = {
0, 0, 0, 1, 0, 2, 32, 35, 0, 4, 64, 69, 512, 518, 608, 615, 0, 8, 128, 137, 1024,
1034, 1184, 1195, 4096, 4108, 4288, 4301, 5632, 5646, 5856, 5871, 0, 16, 256, 273,
2048, 2066, 2336, 2355, 8192, 8212, 8512, 8533, 10752, 10774, 11104, 11127, 16384,
16408, 16768, 16793, 19456, 19482, 19872, 19899, 28672, 28700, 29120, 29149, 32256,
32286, 32736, 32767
};

/* given two faces (encoded as bitfield S,E,N,W,T,B), return connection bitfield */
uint16_t hasCnx[] = {
0, 0, 0, 1, 0, 2, 32, 0, 0, 4, 64, 0, 512, 0, 0, 0, 0, 8, 128, 0, 1024, 0, 0, 0,
4096, 0, 0, 0, 0, 0, 0, 0, 0, 16, 256, 0, 2048, 0, 0, 0, 8192, 0, 0, 0, 0, 0, 0,
0, 16384, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

enum
{
	SIDE_SOUTH,
	SIDE_EAST,
	SIDE_NORTH,
	SIDE_WEST,
	SIDE_TOP,
	SIDE_BOTTOM
};

uint8_t slotsY[] = {1<<SIDE_TOP,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1<<SIDE_BOTTOM};
uint8_t slotsX[] = {1<<SIDE_WEST,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1<<SIDE_EAST};

#define popcount     __builtin_popcount

int mapFirstFree(DATA32 usage, int count)
{
	int base, i;
	for (i = count, base = 0; i > 0; i --, usage ++, base += 32)
	{
		/* from https://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightMultLookup */
		uint32_t bits = *usage ^ 0xffffffff;
		if (bits == 0) continue;
		/* count leading 0 */
		bits = multiplyDeBruijnBitPosition[((uint32_t)((bits & -(signed)bits) * 0x077CB531U)) >> 27];
		*usage |= 1 << bits;
		return base + bits;
	}
	return -1;
}

static int mapGetCnxFromPos(ChunkData cd, int start, DATA8 visited)
{
	uint8_t track[CELLSZ*3/2];
	int init = slotsX[start&15] | slotsY[start>>4];
	int pos, last, stride;
	DATA8 blockIds = cd->blockIds;

	last = pos = 0;
	track[last++] = start;
	stride = cd->chunk->stride;

	while (pos != last)
	{
		uint8_t XY = track[pos];
		uint8_t i, x, y;

		pos ++;
		if (pos == DIM(track)) pos = 0;

		for (i = 0; i < 4; i ++)
		{
			static int8_t relx[] = {-1, 1, 0,  0};
			static int8_t rely[] = { 0, 0, 1, -1};
			x = (XY & 15) + relx[i];
			y = (XY >> 4) + rely[i];

			/* clipping (not 100% portable, but who cares?) */
			if (x >= 16 || y >= 16) continue;
			/* only fully opaque blocks will stop flood: we could be more precise, but not worth the time spent */
			int vispos = x + y * CELLSZ;
			if (blockIds[x + y * stride] == 0 &&
				(visited[vispos>>3] & mask8bit[vispos&7]) == 0)
			{
				track[last++] = x | (y << 4);
				if (last == DIM(track)) last = 0;
				visited[vispos>>3] |= mask8bit[vispos&7];
				init |= slotsX[x] | slotsY[y];
			}
		}
	}
	// fprintf(stderr, "cnx for %d, %d: %d (%x)\n", cd->chunk->X, cd->Y, init, faceCnx[init]);

	return faceCnx[init];
}

int mapGetCnxGraph(ChunkData cd)
{
	uint8_t visited[CELLSZ*CELLSZ/8];
	DATA8   blocks = cd->blockIds;
	int     stride = cd->chunk->stride;
	int     i, j, k, cnx, cdFlags = 0;

	memset(visited, 0, sizeof visited);

	for (j = k = cnx = 0; j < CELLSZ; j ++, blocks += stride)
	{
		for (i = 0; i < CELLSZ; i ++, k ++)
		{
			if (blocks[i] == 0)
			{
				cdFlags |= slotsX[i] | slotsY[j];
				if ((visited[k>>3] & mask8bit[k&7]) == 0)
					cnx |= mapGetCnxFromPos(cd, k, visited);
			}
		}
	}
	cd->cdFlags = cdFlags;

	return cnx;
}

void mapUpdateCnxGraph(Map map, DATA8 bitmap, int x, int y, int w, int h)
{
//	fprintf(stderr, "rect = %d, %d - %d, %d - ", x, w, y, h);

	w  = (w + x + CELLSZ - 1) / CELLSZ;
	x /= CELLSZ;

	if (x < 0) x = 0;
	if (w < 0 || x >= CELLMAPX) return;
	if (w > CELLMAPX) w = CELLMAPX;

	h = (IMAGESIZEY + h - y + (CELLSZ - 1)) / CELLSZ;
	y = (IMAGESIZEY - y) / CELLSZ;

	if (y < 0) y = 0;
	if (h < 0 || y >= CELLMAPY) return;
	if (h > CELLMAPY) h = CELLMAPY;

//	fprintf(stderr, "%d, %d - %d, %d\n", x, w, y, h);

	Chunk chunk;
	for (chunk = map->center + x; x < w; x ++, chunk ++)
	{
		int i, j;
		for (j = y ; j < h; j ++)
		{
			static uint8_t isAir[CELLSZ];
			ChunkData cd = chunk->layer[j];
			DATA8 start = bitmap + x * CELLSZ + (CELLMAPY-1-j) * CELLSZ * chunk->stride;
			DATA8 check;
			for (check = start, i = 0; i < CELLSZ && memcmp(check, isAir, CELLSZ) == 0; i ++, check += chunk->stride);
			if (! cd && i < CELLSZ)
			{
				/* new chunk data added */
				cd = calloc(sizeof *cd, 1);
				chunk->layer[j] = cd;
				cd->chunk = chunk;
				cd->blockIds = start;
				cd->Y = j * CELLSZ;
				cd->cnxGraph = mapGetCnxGraph(cd);
				map->totalChunks ++;
				if (chunk->maxy <= j)
					chunk->maxy  = j+1;
			}
			else if (cd && i == CELLSZ)
			{
				/* empty chunk: remove it */
				free(cd);
				map->totalChunks --;
				chunk->layer[j] = NULL;
				if (chunk->maxy == j + 1)
				{
					while (chunk->maxy > 0 && chunk->layer[chunk->maxy-1] == NULL)
						chunk->maxy --;
				}
			}
			else if (cd)
			{
				cd->cnxGraph = mapGetCnxGraph(cd);
			}
		}
	}
}

/* before world is loaded, check that the map has a few chunks in it */
Map mapInit(int renderDist, DATA8 chunkData, int chunkX, int chunkY)
{
	Map map = calloc(sizeof *map + (renderDist + 2) * 3 * sizeof *map->chunks, 1);

	if (map)
	{
		Chunk chunks;
		DATA8 blockIds;
		int   i, j, k;

		map->maxDist = renderDist;
		map->chunks  = (Chunk) (map + 1);
		map->center = map->chunks + renderDist + 3;
		map->chunkOffsets = chunkNeighbor;
		map->totalChunks = 0;

		/*
		 * the visible chunks are composed of a 1 x <renderDist> grid (ie: single row of chunks), but has
		 * to be surrounded by 1 chunks, in order for frustum culling to do its work. therefore the total
		 * grid is 3 x (<renderDist> + 2).
		 */
		for (i = 0, renderDist += 2; i < 16; i ++)
		{
			int offset = 0;
			/* S, E, N, W bitfield */
			if (i & 1) offset += renderDist;
			if (i & 2) offset ++;
			if (i & 4) offset -= renderDist;
			if (i & 8) offset --;
			chunkNeighbor[i] = offset;
		}

		for (chunks = map->chunks, i = 0; i < renderDist; i ++, chunks ++)
		{
			chunks->X = chunks[renderDist].X = chunks[renderDist*2].X = (i * CELLSZ) - CELLSZ;
			chunks->Z = -16;
			chunks[renderDist*2].Z = 16;
		}

		blockIds = chunkData + (chunkY - 1) * chunkX * CELLSZ * CELLSZ;

		/* add ChunkData */
		for (i = 0, chunks = map->center; i < chunkX; i ++, blockIds += CELLSZ, chunks ++)
		{
			chunks->X = i * CELLSZ;
			chunks->cflags = CFLAG_GOTDATA | CFLAG_HASMESH;
			chunks->stride = chunkX * CELLSZ;
			DATA8 mem = blockIds, row;
			for (j = 0; j < chunkY; j ++, mem -= chunks->stride * CELLSZ)
			{
				for (k = 0, row = mem; k < CELLSZ; k ++, row += chunks->stride)
				{
					static uint8_t isAir[CELLSZ];
					if (memcmp(row, isAir, sizeof isAir))
					{
						/* chunk has data in it: add a ChunkData */
						ChunkData cd = calloc(sizeof *cd, 1);
						chunks->layer[j] = cd;
						cd->chunk = chunks;
						cd->blockIds = mem;
						cd->Y = j * CELLSZ;
						cd->cnxGraph = mapGetCnxGraph(cd);
						map->totalChunks ++;
						//fprintf(stderr, "cd: %d, %d = %x\n", (mem - chunkData) % chunks->stride, (mem - chunkData) / chunks->stride, cd->cnxGraph);
						if (chunks->maxy <= j)
							chunks->maxy  = j+1;
						break;
					}
				}
			}
		}

		return map;
	}
	else return NULL;
}

/*
 * Frustum culling: the goal of these functions is to create a linked list of chunks
 * representing all the ones that are visible in the current view matrix (MVP)
 * check doc/internals.html for explanation on how this part works.
 */

#define FAKE_CHUNK_SIZE     (offsetof(struct ChunkData_t, blockIds)) /* fields after blockIds are completely useless for frustum */
#define UNVISITED           0x40
#define VISIBLE             0x80
//#define FRUSTUM_DEBUG

#if 0
static void mapPrintUsage(ChunkFake cf, int dir)
{
	uint32_t flags, i;
	cf->total += dir;
	fprintf(stderr, "%c%d: ", dir < 0 ? '-' : '+', cf->total);
	for (flags = cf->usage, i = 0; i < 16; i ++, flags >>= 1)
		fputc(flags & 1 ? '1' : '0', stderr);
	fputc('\n', stderr);
}
#else
#define mapPrintUsage(x, y)
#endif

static ChunkData mapAllocFakeChunk(Map map)
{
	ChunkFake * prev;
	ChunkFake   cf;
	ChunkData   cd;
	int         slot;

	for (prev = &map->cdPool, cf = *prev; cf && cf->usage == 0xffffffff; prev = &cf->next, cf = cf->next);

	if (cf == NULL)
	{
		cf = calloc(sizeof *cf + FAKE_CHUNK_SIZE * 32, 1);
		if (cf == NULL) return NULL;
		*prev = cf;
	}

	slot = mapFirstFree(&cf->usage, 1);
	cd = (ChunkData) (cf->buffer + FAKE_CHUNK_SIZE * slot);
	memset(cd, 0, FAKE_CHUNK_SIZE);
	cd->slot   = slot+1;
	cf->usage |= 1<<slot;
	cd->cnxGraph = 0xffff;

	mapPrintUsage(cf, 1);

	return cd;
}

static void mapFreeFakeChunk(ChunkData cd)
{
	/* as ugly as it looks, it is portable however... */
	int       slot = cd->slot-1;
	ChunkFake cf = (ChunkFake) ((DATA8) cd - FAKE_CHUNK_SIZE * slot - offsetof(struct ChunkFake_t, buffer));
	Chunk     c  = cd->chunk;
	cf->usage &= ~(1 << slot);
	c->layer[cd->Y>>4] = NULL;
	mapPrintUsage(cf, -1);
}

static int mapGetOutFlags(Map map, ChunkData cur, DATA8 outflags)
{
	uint8_t out, i, sector;
	Chunk   chunk = cur->chunk;
	int     layer = cur->Y >> 4;
	int     neighbors = 0;
	for (i = out = neighbors = 0; i < 8; i ++)
	{
		static uint8_t dir[] = {0, 2, 1, 3, 16, 16+2, 16+1, 16+3};
		Chunk neighbor = chunk + chunkNeighbor[chunk->neighbor + (dir[i] & 15)];
		if (neighbor->chunkFrame != map->frame)
		{
			memset(neighbor->outflags, UNVISITED, sizeof neighbor->outflags);
			neighbor->chunkFrame = map->frame;
		}
		int Y = layer + (dir[i]>>4);
		if ((sector = neighbor->outflags[Y]) & UNVISITED)
		{
			vec4 point = {neighbor->X, Y << 4, neighbor->Z, 1};

			sector &= ~0x7f;
			/* XXX global var for MVP, should be a function param */
			//fprintf(stderr, "point %g,%g,%g = ", point[VX], point[VY], point[VZ]);
			matMultByVec(point, globals.matMVP, point);

			if (point[0] <= -point[3]) sector |= 1;  /* to the left of left plane */
			if (point[0] >=  point[3]) sector |= 2;  /* to the right of right plane */
			if (point[1] <= -point[3]) sector |= 4;  /* below the bottom plane */
			if (point[1] >=  point[3]) sector |= 8;  /* above top plane */
			if (point[2] <= -point[3]) sector |= 16; /* behind near plane */
			if (point[2] >=  point[3]) sector |= 32; /* after far plane */
			sector = (neighbor->outflags[Y] = sector) & 63;
			//fprintf(stderr, "%d\n", sector);
		}
		else sector &= 63;
		if (sector == 0)
			/* point of the chunk is entirely included in frustum: add all connected chunks to the list */
			neighbors |= frustum.neighbors[i];
		else
			out ++;
		outflags[i] = sector;
	}
	outflags[i] = out;
	return neighbors;
}

static ChunkData mapAddToVisibleList(Map map, Chunk from, int direction, int layer, DATA8 outFlagsFrom, int frame)
{
	static int8_t dir[] = {0, 1, -1};
	uint8_t dirFlags = frustum.chunkOffsets[direction];
	Chunk c = from + chunkNeighbor[from->neighbor + (dirFlags & 15)];
	Chunk center = map->center;
	ChunkData cd;

	// HACK: ONLY SCAN CHUNKS IN XY plane - IGNORE SOUTH/NORTH direction
	if (dirFlags & 5) return NULL;
	// END HACK

	int X = c->X - center->X;
	int Y = layer + dir[dirFlags >> 4];

	/* outside of render distance */
	#if 0
	int Z = c->Z - center->Z;
	int half = (map->maxDist >> 1) << 4;
	if (X < -half || X > half || Z < -half || Z > half || Y < 0)
		return NULL;
	#else
	/* grid is fixed in this example */
	if (X < 0 || X > CELLMAPX * CELLSZ || Y < 0 || Y >= CHUNK_LIMIT)
		return NULL;
	#endif

	if (c->chunkFrame != frame)
	{
		memset(c->outflags, UNVISITED, sizeof c->outflags);
		c->chunkFrame = frame;
	}
	if ((c->cflags & CFLAG_HASMESH) == 0)
	{
		/* move to front of list of chunks needed to be generated */
		if (c->next.ln_Prev && (c->cflags & CFLAG_PRIORITIZE) == 0)
		{
			c->cflags |= CFLAG_PRIORITIZE;
			ListRemove(&map->genList, &c->next);
			ListInsert(&map->genList, &c->next, &map->genLast->next);
			map->genLast = c;
		}
		return NULL;
	}

	/*
	 * special case: there might be chunks below but they are currently outside of the frustum.
	 * As we get farther from the camera, we might eventually intersect with these, that's why we have
	 * to add those fake chunks along the bottom plane of the frustum.
	 */
	cd = c->layer[Y];
	if (cd == NULL)
	{
		/* direction >= 19 means chunk is 3 steps away from center (share a single vertex from center): cannot cross bottom plane */
		if (Y >= CHUNK_LIMIT || direction >= 19)
			return NULL;

		/* check if faces/edges cross bottom plane */
		DATA8 edges = edgeCheck + edgeCheck[direction - 1];
		DATA8 end   = edgeCheck + edgeCheck[direction];
		while (edges < end)
		{
			if ((outFlagsFrom[edges[0]] ^ outFlagsFrom[edges[1]]) & 4)
			{
				/* fake chunks are alloced above ground, make sure we are following bottom plane if we alloc some */
				cd = mapAllocFakeChunk(map);
				cd->Y = Y << 4;
				cd->chunk = c;
				cd->frame = frame;
				c->layer[Y] = cd;
				goto break_all;
			}
			edges += 2;
		}
		return NULL;
	}

	break_all:
	if (cd && c->outflags[Y] < VISIBLE)
	{
		/* not visited yet */
		c->outflags[Y] |= VISIBLE;
		cd->comingFrom = 0;
		cd->visible = NULL;
		return cd;
	}
	return NULL;
}

/* cave culling based on visibility graph traversal (returns True if <cur> should be culled) */
static void mapCullCave(ChunkData cur, vec4 camera)
{
	uint8_t side, i, oppSide;
	Chunk   chunk = cur->chunk;
	int     X = chunk->X;
	int     Z = chunk->Z;
	int     frame = chunk->chunkFrame;

	/* try to get back to a known location from <cur> */
	for (i = 0; i < 3; i ++)
	{
		static int8_t TB[] = {0, 0, 0, 0, -1, 1};
		ChunkData neighbor;

		/* check which face is visible based on dot product between face normal and camera */
		switch (i) {
		case 0: /* N/S */
			/* side is a flag, oppSide is enumeration */
			if (Z + 16 - camera[VZ] < 0) side = 1, oppSide = 2;
			else if (camera[VZ] - Z < 0) side = 4, oppSide = 0;
			else continue;
			break;
		case 1: /* E/W */
			if (X + 16 - camera[VX] < 0) side = 2, oppSide = 3;
			else if (camera[VX] - X < 0) side = 8, oppSide = 1;
			else continue;
			break;
		case 2: /* T/B */
			if (cur->Y + 16 - camera[VY] < 0) side = 0, oppSide = 5;
			else if (camera[VY] - cur->Y < 0) side = 0, oppSide = 4;
			else continue;
		}

		chunk    = cur->chunk + chunkNeighbor[cur->chunk->neighbor + side];
		neighbor = chunk->layer[(cur->Y >> 4) + TB[oppSide]];
		side     = 1 << opp[oppSide];

		if (neighbor && chunk->chunkFrame != frame) continue;
		if (neighbor == NULL || neighbor->slot > 0)
		{
			/* high column without neighbor: consider this chunk visible */
			cur->comingFrom = side;
			return;
		}
		if (neighbor->comingFrom > 0 /* can be visited */)
		{
			if (neighbor->comingFrom == 127)
			{
				/* starting pos: multiple paths possible */
				static int canGoTo[] = { /* S, E, N, W, T, B */
					1+2+4+8+16+(1<<15), 1+32+64+128+256+(1<<16), 2+32+512+1024+2048+(1<<17), 4+64+512+4096+8192+(1<<18),
					8+128+1024+4096+16384+(1<<19), 16+256+2048+8192+16384+(1<<20)
				};
				if ((neighbor->cnxGraph | (neighbor->cdFlags << 15)) & canGoTo[oppSide])
				{
					cur->comingFrom = side;
					return;
				}
			}
			else if (neighbor->cnxGraph & hasCnx[(1 << oppSide) | neighbor->comingFrom])
			{
				cur->comingFrom = side;
				return;
			}
		}
	}
	/* chunk is culled because of cave visibility */
	cur->comingFrom = -1;
}

static void mapFreeFakeChunks(Map map)
{
	ChunkFake pool;
	for (pool = map->cdPool; pool; pool = pool->next)
	{
		uint32_t slot, usage;
		for (usage = pool->usage, slot = 0; usage > 0; usage >>= 1, slot ++)
		{
			if (usage & 1)
				mapFreeFakeChunk((ChunkData) (pool->buffer + FAKE_CHUNK_SIZE * slot));
		}
		pool->usage = 0;
	}
}

static void renderClearBank(Map map);
static void renderAddToBank(ChunkData cur);

void mapViewFrustum(Map map, vec4 camera)
{
	ChunkData cur, last;
	Chunk     chunk;
	int       frame;
	int       center[3];

	#if 0
	chunk = map->center;
	#else
	/* chunk grid is fixed in this tool */
	chunk = map->center + CPOS(camera[VX]);
	/* fake chunk are not deallocated at the end for debugging purpose */
	mapFreeFakeChunks(map);
	#endif

	center[VY] = CPOS(camera[1]);
	center[VX] = chunk->X;
	center[VZ] = chunk->Z;

	map->firstVisible = NULL;
	map->frustumChunks = 0;
	renderClearBank(map);

	frame = 255;
	if (center[1] < 0)
	{
		/* don't care: you are not supposed to be here anyway */
		return;
	}
	else if (center[1] >= chunk->maxy)
	{
		if (center[1] >= CHUNK_LIMIT)
		{
			/* higher than build limit: we need to get below build limit using geometry */
			vec4 dir = {0, -1, 0, 1};

			center[1] = CHUNK_LIMIT-1;

			matMultByVec(dir, globals.matInvMVP, dir);

			dir[VX] = dir[VX] / dir[VT] - camera[VX];
			dir[VY] = dir[VY] / dir[VT] - camera[VY];
			dir[VZ] = dir[VZ] / dir[VT] - camera[VZ];

			/* dir is now the vector coplanar with bottom plane (anglev - FOV/2 doesn't seem to work: slightly off :-/) */
			if (dir[1] >= 0)
			{
				/* camera is pointing up above build limit: no chunks can be visible in this configuration */
				return;
			}

			float DYSlope = (CHUNK_LIMIT*16 - camera[VY]) / dir[VY];
			int   chunkX  = CPOS(camera[VX] + dir[VX] * DYSlope) - (chunk->X>>4);
			int   chunkZ  = CPOS(camera[VZ] + dir[VZ] * DYSlope) - (chunk->Z>>4);
			int   area    = map->mapArea;
			int   half    = map->maxDist >> 1;

			if (chunkX < -half || chunkX > half ||
			    chunkZ < -half || chunkZ > half)
			    return;

			chunkX += map->mapX;
			chunkZ += map->mapZ;

			if (chunkX < 0)     chunkX += area; else
			if (chunkX >= area) chunkX -= area;
			if (chunkZ < 0)     chunkZ += area; else
			if (chunkZ >= area) chunkZ -= area;

			chunk = map->chunks + chunkX + chunkZ * area;
			center[0] = chunk->X;
			center[2] = chunk->Z;

			cur = chunk->layer[center[1]];
			if (cur) goto found_start;
		}
		cur = mapAllocFakeChunk(map);
		cur->Y = center[1] * 16;
		cur->chunk = chunk;
		chunk->layer[center[1]] = cur;
		frame = center[1];
		// fprintf(stderr, "init fake chunk at %d, %d: %d\n", chunk->X, chunk->Z, center[1] << 4);
	}
	else cur = chunk->layer[center[1]];

	if (! cur) return;
	found_start:
	map->firstVisible = cur;
	map->chunkCulled = 0;
	cur->visible = NULL;
	cur->comingFrom = 127;
	memset(chunk->outflags, UNVISITED, sizeof chunk->outflags);
	chunk->outflags[cur->Y>>4] |= VISIBLE;
	frame = ++ map->frame;
	chunk->chunkFrame = frame;

	for (last = cur; cur; cur = cur->visible)
	{
		uint8_t outflags[9];
		int     i, neighbors;

		/* 1st pass: check if chunk corners are in frustum */
		chunk     = cur->chunk;
		center[1] = cur->Y >> 4;
		neighbors = mapGetOutFlags(map, cur, outflags);

		//fprintf(stderr, "chunk %d, %d: slot: %d\n", cur->chunk->X >> 4, cur->Y >> 4, cur->slot);

		/* up to 26 neighbor chunks can be added for the 8 corners of <cur> */
		for (i = 1; neighbors; i ++, neighbors >>= 1)
		{
			if ((neighbors & 1) == 0) continue;
			ChunkData cd = mapAddToVisibleList(map, chunk, i, center[1], outflags, frame);
			if (cd)
			{
				/* by culling chunk early, it will prune huge branch of chunks from frustum */
				mapCullCave(cd, camera);
				/* fake chunks (slot > 0) must not be culled (yet) */
				if (cd->comingFrom > 0 || cd->slot > 0)
				{
					last->visible = cd;
					last = cd;
					cd->frame = frame;
					map->frustumChunks ++;
				}
				else /* ignore this chunk */
				{
					map->chunkCulled ++;
				}
			}
		}

		/* 2nd pass: try harder for chunks that have at least 2 corners out of frustum */
		if (outflags[8] >= 2)
		{
			static uint8_t faces[] = {
				/* numbers reference boxPts, order is S, E, N, W, T, B */
				3, 2, 7, 6,
				1, 3, 5, 7,
				0, 1, 4, 5,
				2, 0, 6, 4,
				4, 5, 6, 7,
				0, 1, 2, 3,
			};
			DATA8 p;
			for (i = 0, p = faces; i < sizeof faces/4; i ++, p += 4)
			{
				/* check if an entire face crosses a plane */
				uint8_t sector1 = outflags[p[0]];
				uint8_t sector2 = outflags[p[1]];
				uint8_t sector3 = outflags[p[2]];
				uint8_t sector4 = outflags[p[3]];

				if ((sector1*sector2*sector3*sector4) != 0 && /* all points of face must be outside frustum */
				    (sector1&sector2&sector3&sector4) == 0 && /* but not all outside the same plane */
				    (popcount(sector1 ^ sector2) >= 2 ||      /* segment is crossing 2 or more planes */
				     popcount(sector2 ^ sector4) >= 2 ||
				     popcount(sector3 ^ sector4) >= 2 ||
				     popcount(sector1 ^ sector3) >= 2))
				{
					/* face crosses a plane: add chunk connected to it to the visible list */
					ChunkData cd = mapAddToVisibleList(map, chunk, i+1, center[1], outflags, frame);
					if (cd)
					{
						#ifdef FRUSTUM_DEBUG
						fprintf(stderr, "extra chunk added: %d, %d [%d]\n", cd->chunk->X, cd->chunk->Z, cd->Y);
						#endif
						last->visible = cd;
						last = cd;
					}
				}
			}
		}

		if (cur->slot > 0)
		{
			/* fake or empty chunk: remove from list */
			#if 0
			if (cur->slot > 0)
				mapFreeFakeChunk(cur);
			else /* still need to mark from direction we went */
				mapCullCave(cur, camera);
			*prev = cur->visible;
			#else
			/* in this utility, keep fake chunks for debugging purpose */
			cur->frame = frame;
			#endif
		}
		else /* chunk is visible */
			renderAddToBank(cur);
	}
}

/* obviously not needed by this utility, but needed for 3d engine */
static void renderClearBank(Map map)
{
	/* clear the list of rendered chunks on the GPU */
}

static void renderAddToBank(ChunkData cur)
{
	/* mark <cur> as needing to be rendered by the GPU */
}
