/*
 * ChunkLoad.h : more or less a direct dump from MCEdit maps.h and chunk.h
 */

#ifndef CHUNKLOAD_H
#define CHUNKLOAD_H

#define NUM_THREADS       2
#define MEMPOOL           4 * 1024 * 1024    /* allocated on the GPU (in bytes) */
#define MEMITEM           32
#define BUILD_HEIGHT      256
#define CHUNK_LIMIT       (BUILD_HEIGHT/16)
#define CPOS(pos)         ((int) floor(pos) >> 4)

/* private definition */
typedef struct ChunkData_t *       ChunkData;
typedef struct GPUBank_t *         GPUBank;
typedef struct GPUMem_t *          GPUMem;
typedef struct Map_t *             Map;
typedef struct Chunk_t *           Chunk;
typedef struct Chunk_t             Chunk_t;
typedef struct ChunkData_t         ChunkData_t;
typedef struct ChunkData_t *       ChunkData;
typedef float                      vec4[4];
typedef float                      mat4[16];
typedef uint32_t *                 DATA32;
typedef uint16_t *                 DATA16;
typedef int16_t *                  DATAS16;

Map  mapInitFromPath(int renderDist, int * XZ);
Bool mapMoveCenter(Map, vec4 old, vec4 pos);
int  checkMem(GPUBank bank);
void mapGenFlush(Map map);
void mapFreeAll(Map map);
Bool mapSetRenderDist(Map, int maxDist);


struct ChunkData_t
{
	Chunk     chunk;
	int       Y;
	int       cdFlags;

	/* VERTEX_ARRAY_BUFFER location */
	void *    glBank;              /* note: this field must be first after tables (needed in chunkFill()) */
	int       glSlot;
	int       glSize;              /* size in bytes */
};

struct Chunk_t
{
	ListNode  next;                /* processing */
	ChunkData layer[CHUNK_LIMIT];  /* sub-chunk array */
	int       X, Z;                /* map coord (not chunk) */
	uint8_t   cflags;              /* CLFAG_* */
	uint8_t   neighbor;
	uint8_t   maxy;
	uint8_t   processing;
	int       color;
};

enum
{
	VX, VY, VZ, VW
};

enum /* flags for Chunk_t.cflags */
{
	CFLAG_GOTDATA    = 0x01,       /* data has been retrieved */
	CFLAG_HASMESH    = 0x02,       /* mesh generated and pushed to GPU */
	CFLAG_NEEDSAVE   = 0x04,       /* modifications need to be saved on disk */
	CFLAG_PENDINGDEL = 0x10,       /* chunk not needed anymore */
};

struct GPUMem_t                    /* one allocation */
{
	ChunkData cd;                  /* chunk at this location (NULL == free) */
	int       size;                /* in bytes */
	int       offset;              /* avoid scanning the whole list */
	int       id;                  /* easier to debug */
};

struct GPUBank_t                   /* one chunk of memory */
{
	ListNode  node;
	int       memAvail;            /* in bytes */
	int       memUsed;             /* in bytes */
	GPUMem    usedList;            /* Array of memory range in use */
	int       maxItems;            /* max items available in usedList */
	int       nbItem;              /* number of items in usedList */
	int       freeItem;
};

struct Map_t
{
	ListHead  gpuBanks;
	ListHead  genList;
	Semaphore genCount;
	Mutex     genLock;
	DATAS16   chunkOffsets;
	int       mapArea;
	int       maxDist;
	float     cx, cy, cz;          /* player pos (init) */
	int       mapX, mapZ;          /* map center */
	Chunk     center;              /* chunks + mapX + mapZ * MAP_AREA */
	ChunkData firstVisible;        /* frustum chain to render */
	Chunk     chunks;
	int       GPUchunk;
};

struct Thread_t
{
	Mutex wait;
	Map   map;
	int   state;
};

enum {
	THREAD_WAIT_GENLIST,
	THREAD_WAIT_BUFFER,
	THREAD_RUNNING
};

/* cannot be more than 1Mb becase of start[], need to change to uint16_t if more than that */
#define MAX_BUFFER     (1024*1024)

struct Staging_t
{
	Semaphore capa;
	Mutex     alloc;
	DATA32    mem;
	int       total;
	int       chunkData;
	uint32_t  usage[MAX_BUFFER/4096/32];
	uint8_t   start[MAX_BUFFER/4096];
};

struct Frustum_t                   /* frustum culling static tables (see doc/internals.html for detail) */
{
	int8_t *  spiral;
	int8_t *  lazy;
	uint16_t  lazyCount;
};

enum
{
	SIDE_SOUTH,
	SIDE_EAST,
	SIDE_NORTH,
	SIDE_WEST,
	SIDE_TOP,
	SIDE_BOTTOM,
};

#endif
