/*
 * chunks.h: public function and datatypes to manage map chunks
 *
 * written by T.Pierron, jan 2020.
 */

#ifndef MCCHUNKS_H
#define	MCCHUNKS_H

#include "utils.h"

#define BUILD_HEIGHT                   256
#define CHUNK_LIMIT                    (BUILD_HEIGHT/16)
#define CHUNK_BLOCK_POS(x,z,y)         ((x) + ((z) << 4) + ((y) << 8))
#define CHUNK_POS2OFFSET(chunk,pos)    (((int) floorf(pos[VX]) - chunk->X) + (((int) floorf(pos[VZ]) - chunk->Z) << 4) + (((int) floorf(pos[VY]) & 15) << 8))
#define CHUNK_EMIT_SIZE                4

typedef struct Chunk_t *               Chunk;
typedef struct ChunkData_t             ChunkData_t;
typedef struct ChunkData_t *           ChunkData;

struct ChunkData_t                     /* one sub-chunk of 16x16x16 blocks */
{
	ChunkData visible;                 /* frustum culling list */
	Chunk     chunk;                   /* bidirectional link */
	uint16_t  Y;                       /* vertical pos in blocks */
	uint16_t  cnxGraph;                /* face graph connection (cave culling) */

	uint16_t  cdFlags;                 /* CDFLAG_* */
	uint8_t   slot;                    /* used by ChunkFake (0 ~ 31) */
	int8_t    comingFrom;              /* cave culling (face id 0 ~ 5) */
	int       frame;

	DATA8     blockIds;                /* 16*16*16 = XZY ordered, note: point directly to NBT struct (4096 bytes) */
};

struct Chunk_t                         /* an entire column of 16x16 blocks */
{
	ListNode  next;                    /* chunk loading */
	Chunk     save;                    /* next chunk that needs saving */
	ChunkData layer[CHUNK_LIMIT];      /* sub-chunk array */
	uint8_t   outflags[CHUNK_LIMIT+1];
	uint8_t   neighbor;                /* offset for chunkNeighbor[] table */
	uint8_t   cflags;                  /* CLFAG_* */
	uint8_t   maxy;                    /* number of sub-chunks in layer[], starting at 0 */

	uint8_t   noChunks;                /* S,E,N,W bitfield: no chunks in this direction */
	uint16_t  entityList;              /* linked list of all entities in this chunk */

	uint16_t  cdIndex;                 /* iterate over ChunkData/Entities/TileEnt when saving */
	int16_t   signList;                /* linked list of all the signs in this chunk */

	int       X, Z;                    /* coord in blocks unit (not chunk, ie: map coord) */
	DATA8     biomeMap;                /* XZ map of biome id */
	DATA32    heightMap;               /* XZ map of lowest Y coordinate where skylight value == 15 */
	APTR      tileEntities;            /* hashmap of tile entities (direct NBT records) *(TileEntityHash)c->tileEntities */
	int       secOffset;               /* offset within <nbt> where "Sections" TAG_List_Compound starts */
	int       teOffset;                /* same with "TileEntities" */
	int       stride;                  /* "Entities" offset */
	int       chunkFrame;
};

extern int16_t chunkNeighbor[];        /* where neighbors of a chunk based on Chunk->neighbor+direction value */

enum /* flags for Chunk.cflags */
{
	CFLAG_GOTDATA    = 0x01,           /* data has been retrieved */
	CFLAG_HASMESH    = 0x02,           /* mesh generated and pushed to GPU */
	CFLAG_NEEDSAVE   = 0x04,           /* modifications need to be saved on disk */
	CFLAG_HASENTITY  = 0x08,           /* entity transfered in active list */
	CFLAG_REBUILDTE  = 0x10,           /* mark TileEntity list as needing to be rebuilt (the NBT part) */
	CFLAG_ETTLIGHT   = 0x20,           /* update entity light for this chunk */
	CFLAG_REBUILDETT = 0x40,           /* mark Entity list for rebuilt when saved */
	CFLAG_PRIORITIZE = 0x80,           /* already move in front of map->genList */
};

enum /* flags for ChunkData.cdFalgs */
{
	CDFLAG_SOUTHPATH  = 0x01,
	CDFLAG_NORTHPATH  = 0x02,
	CDFLAG_EASTPATH   = 0x04,
	CDFLAG_WESTPATH   = 0x08,
	CDFLAG_TOPPATH    = 0x10,
	CDFLAG_BOTPATH    = 0x20
};

#endif
