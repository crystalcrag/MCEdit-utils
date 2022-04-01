/*
 * maps.h: public functions/datatypes to handle maps using anvil file format.
 *
 * written by T.Pierron, jan 2020.
 */

#ifndef MCMAPS_H
#define MCMAPS_H

#include <stdint.h>
#include "chunks.h"

typedef struct Map_t *             Map;
typedef struct MapExtraData_t *    MapExtraData;
typedef struct BlockIter_t *       BlockIter;
typedef struct ChunkFake_t *       ChunkFake;

Map  mapInit(int renderDist, DATA8 chunkData, int chunkX, int chunkY);
void mapViewFrustum(Map map, vec4 camera);
void mapUpdateCnxGraph(Map, DATA8 bitmap, int x, int y, int w, int h);


#define MAP_MIN       7            /* nb chunks active around player (need odd numer) */
#define MAP_HALF      (MAP_MIN/2)
#define MAP_AREA      (MAP_MIN+4)  /* 1 for lazy chunks, 1 for purgatory */
#define MAX_PATHLEN   256
#define MAX_PICKUP    24           /* max reach in blocks */

#define MAP_SIZE      (MAP_AREA * MAP_AREA)
#define CPOS(pos)     ((int) floorf(pos) >> 4)
#define CREM(pos)     ((int) floorf(pos) & 15)

struct Map_t
{
	float     cx, cy, cz;          /* player pos (init) */
	int       mapX, mapZ;          /* map center (coords in Map_t.chunks) */
	int       maxDist;             /* max render distance in chunks */
	int       mapArea;             /* size of entire area of map (including lazy chunks) */
	int       frame;               /* needed by frustum culling */
	int       GPUchunk;            /* stat for debug: chunks with mesh on GPU */
	int       GPUMaxChunk;         /* bytes to allocate for a single VBO */
	int       totalChunks;         /* debug: ChunkData allocated (excl. fake chunks) */
	int       frustumChunks;       /* debug: ChunkData in frustum */
	uint16_t  chunkCulled;         /* stat for debug: chunk culled from cave culling (not from frustum) */
	uint16_t  curOffset;           /* reduce sorting for alpha transparency of current chunk */
	uint16_t  size[3];             /* brush only: size in blocks of brush (incl. 1 block margin around) */
	Chunk     center;              /* chunks + mapX + mapZ * mapArea */
	ListHead  gpuBanks;            /* VBO for chunk mesh (GPUBank) */
	ListHead  genList;             /* chunks to process (Chunk) */
	ListHead  players;             /* list of player on this map (Player) */
	Chunk     genLast;
	DATAS16   chunkOffsets;        /* array 16*9: similar to chunkNeighbor[] */
	char      path[MAX_PATHLEN];   /* path to level.dat */
	ChunkData dirty;               /* sub-chunks that needs mesh regeneration */
	ChunkData firstVisible;        /* list of visible chunks according to the MVP matrix */
	Chunk     needSave;            /* linked list of chunk that have been modified */
	Chunk     chunks;              /* 2d array of chunk containing the entire area around player */
	ChunkFake cdPool;              /* pool of partial ChunkData for frustum culling */
};

struct MapFrustum_t                /* frustum culling static tables (see doc/internals.html for detail) */
{
	uint32_t  neighbors[8];        /* 8 corners having 8 neighbors: bitfield encode 27 neighbors */
	uint8_t   chunkOffsets[27];    /* bitfield of where each chunks are (S, E, N, W, T, B) */
	int8_t *  spiral;
	int8_t *  lazy;
	uint16_t  lazyCount;
};

struct ChunkFake_t
{
	ChunkFake next;
	uint32_t  usage;
	uint8_t   total;
	uint8_t   buffer[0]; /* more bytes will follow */
};

#endif
