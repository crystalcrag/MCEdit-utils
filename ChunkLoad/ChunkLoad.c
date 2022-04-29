/*
 * ChunkLoad.c : load chunk into grid according to map center and show memory
 *               block allocated on GPU.
 *
 * Written by T.Pierron, aug 2020
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include "SIT.h"
#include "ChunkLoad.h"

struct Thread_t  threads[NUM_THREADS];
struct Frustum_t frustum;
struct Staging_t staging;

int16_t chunkNeighbor[16*9];
extern int loadSpeed;
static volatile int threadStop;

static void renderFreeArray(ChunkData cd);
static void chunkFree(Chunk);

#define END_OF_LIST        0xffffffff
#define THREAD_EXIT_LOOP   1
#define THREAD_EXIT        2

/* thoroughly checks that all data structure are coherent */
int checkMem(GPUBank bank)
{
	GPUMem free = bank->usedList + bank->maxItems - 1;
	GPUMem eof  = free - bank->freeItem + 1;

	while (free > eof)
	{
		GPUMem next = free - 1;

		if (next->offset <= free->offset + free->size)
			/* must be ordered by increasing <offset>, without range overlapping */
			return 1;

		free --;
	}

	if (free->offset + free->size == bank->memUsed)
		/* last block is freed: it should reduce the range of memory used */
		return 2;

	return 0;
}

/* malloc()-like function */
static int renderStoreArrays(Map map, ChunkData cd, int size)
{
	GPUBank bank;

	if (size == 0)
	{
		if (cd->glBank)
		{
			renderFreeArray(cd);
			cd->glBank = NULL;
		}
		return -1;
	}

	for (bank = HEAD(map->gpuBanks); bank && bank->memAvail <= bank->memUsed + size /* bank is full */; NEXT(bank));

	if (bank == NULL)
	{
		bank = calloc(sizeof *bank, 1);
		bank->memAvail = MEMPOOL;
		bank->maxItems = MEMITEM;
		bank->usedList = calloc(sizeof *bank->usedList, MEMITEM);

		#if 0
		glGenVertexArrays(1, &bank->vaoTerrain);
		/* will also init vboLocation and vboMDAI */
		glGenBuffers(3, &bank->vboTerrain);

		/* pre-configure terrain VAO: 5 bytes per vertex */
		glBindVertexArray(bank->vaoTerrain);
		glBindBuffer(GL_ARRAY_BUFFER, bank->vboTerrain);
		/* this will allocate memory on the GPU: mem chunks of 20Mb */
		glBufferData(GL_ARRAY_BUFFER, MEMPOOL, NULL, GL_STATIC_DRAW);
		glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, VERTEX_DATA_SIZE, 0);
		glEnableVertexAttribArray(0);
		glVertexAttribIPointer(1, 3, GL_UNSIGNED_INT, VERTEX_DATA_SIZE, (void *) 16);
		glEnableVertexAttribArray(1);
		/* 16 bytes of per-instance data (3 float for loc and 1 uint for flags) */
		glBindBuffer(GL_ARRAY_BUFFER, bank->vboLocation);
		glVertexAttribPointer(2, 4, GL_FLOAT, 0, 0, 0);
		glEnableVertexAttribArray(2);
		glVertexAttribDivisor(2, 1);

		checkOpenGLError("renderStoreArrays");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		#endif

		ListAddTail(&map->gpuBanks, &bank->node);
	}

	/* check for free space in the bank */
	GPUMem store = bank->usedList + bank->nbItem;
	GPUMem free  = bank->usedList + bank->maxItems - 1;
	GPUMem eof   = free - bank->freeItem;
	while (free > eof)
	{
		/* first place available */
		if (size <= free->size)
		{
			/* no need to keep track of such a small quantity (typical chunk mesh is around 10Kb) */
			if (size + 2*4096 >= free->size)
				size = free->size;
			store->offset = free->offset;
			store->size = size;
			if (free->size == size)
			{
				/* freed slot entirely reused */
				eof -= bank->freeItem;
				bank->freeItem --;
				/* free list must be contiguous */
				memmove(eof + 1, eof, (DATA8) free - (DATA8) eof);
			}
			else /* still some capacity left */
			{
				free->size -= size;
				free->offset += size;
			}
			goto found;
		}
		free --;
	}
	/* no free block big enough: alloc at the end */
	if (bank->nbItem + bank->freeItem == bank->maxItems)
	{
		/* not enough items */
		store = realloc(bank->usedList, (bank->maxItems + MEMITEM) * sizeof *store);
		if (store)
		{
			/* keep free list at the end */
			memmove(store + bank->maxItems - bank->freeItem, store + bank->maxItems + MEMITEM - bank->freeItem, bank->freeItem * sizeof *store);
			memset(store + bank->maxItems - bank->freeItem, 0, MEMITEM * sizeof *store);
			bank->maxItems += MEMITEM;
			bank->usedList = store;
		}
		else { fprintf(stderr, "alloc failed: aborting\n"); return -1; }
	}
	store = bank->usedList + bank->nbItem;
	store->size = size;
	store->offset = bank->memUsed;
	bank->memUsed += size;

	found:
	bank->nbItem ++;
	store->cd = cd;
	store->id = cd->chunk->color;
	cd->glSlot = bank->nbItem - 1;
	cd->glSize = size;
	cd->glBank = bank;

//	fprintf(stderr, "alloc chunk %d at %d\n", cd->chunk->color, bank->nbItem - 1);

	int ret = checkMem(bank);
	if (ret > 0)
		fprintf(stderr, "error alloc code = %d for chunk %d\n", ret, cd->chunk->color);

	return store->offset;
}

/* mark memory occupied by the array as free */
static void renderFreeArray(ChunkData cd)
{
	GPUMem  free;
	GPUBank bank  = cd->glBank;
	GPUMem  mem   = bank->usedList + cd->glSlot;
	GPUMem  eof   = bank->usedList + bank->nbItem - 1;
	int     start = mem->offset;
	int     size  = mem->size;
	int     end   = start + size;

	cd->glBank = NULL;
//	fprintf(stderr, "freeing chunk %d at %d\n", cd->chunk->color, cd->glSlot);

	if (mem < eof)
	{
		/* keep block list contiguous, but not necessarily ordered */
		mem[0] = eof[0];
		eof->cd->glSlot = cd->glSlot;
	}
	bank->nbItem --;

	/* add block <off> - <size> to free list */
	mem = free = bank->usedList + bank->maxItems - 1;
	eof = mem - bank->freeItem + 1;

	/* keep free list ordered in increasing offset (from end of array toward beginning) */
	while (mem >= eof)
	{
		if (end < mem->offset)
		{
			/* insert before mem */
			memmove(eof - 1, eof, (DATA8) (mem + 1) - (DATA8) eof);
			mem->offset = start;
			mem->size   = size;
			bank->freeItem ++;
			return;
		}
		else if (end == mem->offset)
		{
			/* can be merged at beginning of <mem> */
			mem->offset = start;
			mem->size += size;
			/* can we merge with previous item? */
			if (mem < free && mem[1].offset + mem[1].size == start)
			{
				mem[1].size += mem->size;
				memmove(eof + 1, eof, (DATA8) mem - (DATA8) free);
				bank->freeItem --;
				eof ++;
				mem ++;
			}
			check_free:
			if (mem->size + mem->offset == bank->memUsed)
			{
				/* discard last free block */
				bank->memUsed -= mem->size;
				bank->freeItem --;
			}
			return;
		}
		else if (start == mem->offset + mem->size)
		{
			/* can be merged at end of <mem> */
			mem->size += size;
			/* can we merge with next item? */
			if (mem > eof && mem[-1].offset == end)
			{
				mem->size += mem[-1].size;
				memmove(eof + 1, eof, (DATA8) (mem - 1) - (DATA8) eof);
				bank->freeItem --;
			}
			goto check_free;
		}
		else mem --;
	}

	/* cannot merge with existing free list: add it at the beginning */
	if (end < bank->memUsed)
	{
		/* we just removed an item, therefore it is safe to add one back */
		eof[-1].offset = start;
		eof[-1].size = size;
		bank->freeItem ++;
	}
	else bank->memUsed -= size;
	/* else last item being removed: simply discard everything */

	end = checkMem(bank);
	if (end > 0)
		fprintf(stderr, "error free code = %d for chunk %d\n", end, cd->chunk->color);

}

void renderFinishMesh(Map map, ChunkData cd)
{
	GPUBank bank;
	int     total, offset;

	total = cd->glSize;
	bank = cd->glBank;

	if (bank)
	{
		GPUMem mem = bank->usedList + cd->glSlot;
		if (total > mem->size)
		{
			/* not enough space: need to "free" previous mesh before */
			renderFreeArray(cd);
			offset = renderStoreArrays(map, cd, total);
		}
		else offset = mem->offset, cd->glSize = total; /* reuse mem segment */
	}
	else offset = renderStoreArrays(map, cd, total);

	(void) offset;

//	fprintf(stderr, "allocating %d bytes at %d for chunk %d, %d / %d\n", total, offset, cd->chunk->X, cd->chunk->Z, cd->Y);
}

static void chunkFree(Chunk c)
{
	int i;
	for (i = 0; i < DIM(c->layer); i ++)
	{
		ChunkData cd = c->layer[i];
		if (cd)
		{
			if (cd->glBank) renderFreeArray(cd);
			free(cd);
		}
	}
	memset(c->layer, 0, (c->maxy+1) * sizeof c->layer[0]);
	c->cflags = 0;
	c->maxy = 0;
}

Bool chunkLoad(Chunk chunk, int x, int z, int id)
{
	static int color = 0;
	if (chunk->X != x || chunk->Z != z)
		chunkFree(chunk);

	if ((chunk->cflags & CFLAG_GOTDATA) == 0)
	{
		fprintf(stderr, "thread %d: loaded chunk %d, %d: %d\n", id, x, z, chunk->processing);
		chunk->X = x;
		chunk->Z = z;
		chunk->maxy = 1;
		chunk->color = color ++;

		if (loadSpeed > 0)
			ThreadPause(rand() % loadSpeed);

		if (chunk->layer[0])
			puts("not good");

		ChunkData cd = calloc(sizeof *cd, 1);
		chunk->layer[0] = cd;
		cd->chunk = chunk;
		/* should be filled in chunkUpdate(), but that function cannot be included in this test setup */
		cd->glSize = (4 + rand() % 8) * 4096;
		cd->Y = 0;
		return True;
	}
	return False;
}

/* ask thread to stop what they are doing and wait for them */
void mapGenStopThread(Map map, int exit)
{
	threadStop = exit;
	int i;

	if (exit == THREAD_EXIT)
	{
		/* map is being deleted: empty semaphore and exit threads */
		while (SemWaitTimeout(map->genCount, 0));
	}

	/* need to wait, thread might hold pointer to object that are going to be freed */
	for (i = 0; i < NUM_THREADS; i ++)
	{
		switch (threads[i].state) {
		case THREAD_WAIT_GENLIST:
			/* that's where we want the thread to be */
			continue;
		case THREAD_RUNNING:
			/* meshing/reading stuff: not good, need to stop */
			break;
		case THREAD_WAIT_BUFFER:
			/* waiting for mem block, will jump to sleep right after */
			SemAdd(staging.capa, 1);
		}
		/* need to wait for thread to stop though */
		double tick = FrameGetTime();
		/* active loop for 1ms */
		while (FrameGetTime() - tick < 1)
		{
			if (threads[i].state == THREAD_WAIT_GENLIST)
			{
				goto continue_loop;
			}
		}

		/* thread still hasn't stop, need to wait then :-/ */
		MutexEnter(threads[i].wait);
		MutexLeave(threads[i].wait);

		continue_loop: ;
	}

	if (exit == THREAD_EXIT)
	{
		/* need to be sure threads have exited */
		SemAdd(map->genCount, NUM_THREADS);
		for (i = 0; i < NUM_THREADS; i ++)
		{
			while (threads[i].state >= 0);
			MutexDestroy(threads[i].wait);
		}
	}

	memset(threads, 0, sizeof threads);
	/* clear staging area */
	memset(staging.usage, 0, sizeof staging.usage);
	SemAdd(staging.capa, staging.total);
	staging.total = 0;
	staging.chunkData = 0;

	threadStop = 0;
}

/* flush what the threads have been filling (called from main thread) */
void mapGenFlush(Map map)
{
	MutexEnter(staging.alloc);

	DATA8 index, eof;
	for (index = staging.start, eof = index + staging.chunkData; index < eof; )
	{
		/* is the chunk ready ? */
		DATA32 mem = staging.mem + index[0] * 1024;
		Chunk chunk = map->chunks + (mem[0] & 0xffff);
		ChunkData cd = chunk->layer[mem[0] >> 16];

		if (chunk->cflags & CFLAG_HASMESH)
		{
			/* yes, move all chunks into GPU and free staging area */
			int slot = index[0];
			int count = 0;
			renderFinishMesh(map, cd);
			for (;;)
			{
				staging.usage[slot >> 5] ^= 1 << (slot & 31);
				staging.total --;
				SemAdd(staging.capa, 1);

				/* should copy mem to GPU here */
				// from mem + 2 to mem + 1024 (4088 bytes)

				if (mem[1] == END_OF_LIST) break;
				slot = mem[1] >> 10;
				mem = staging.mem + mem[1];
				count ++;
			}
			memmove(index, index + 1, eof - index - 1);
			staging.chunkData --;
			eof --;
			fprintf(stderr, "transfering chunk %d, %d to GPU: %d blocks (%d)\n", chunk->X, chunk->Z, count, staging.total);
		}
		/* wait for next frame */
		else index ++;
	}

	MutexLeave(staging.alloc);
}

static void mapRedoGenList(Map map)
{
	int8_t * spiral;
	int      XC   = CPOS(map->cx) << 4;
	int      ZC   = CPOS(map->cz) << 4;
	int      n    = map->maxDist * map->maxDist;
	int      area = map->mapArea;

	mapGenStopThread(map, THREAD_EXIT_LOOP);
	ListNew(&map->genList);

	for (spiral = frustum.spiral; n > 0; n --, spiral += 2)
	{
		Chunk c = &map->chunks[(map->mapX + spiral[0] + area) % area + (map->mapZ + spiral[1] + area) % area * area];
		int X = XC + (spiral[0] << 4);
		int Z = ZC + (spiral[1] << 4);
		if (c->X != X || c->Z != Z)
		{
			chunkFree(c);
		}
		if ((c->cflags & CFLAG_HASMESH) == 0)
		{
			c->X = X;
			c->Z = Z;
			ListAddTail(&map->genList, &c->next);
			SemAdd(map->genCount, 1);
		}
	}
}

Bool mapMoveCenter(Map map, vec4 old, vec4 pos)
{
	int area = map->mapArea;
	int dx   = CPOS(pos[VX]) - CPOS(old[VX]);
	int dz   = CPOS(pos[VZ]) - CPOS(old[VZ]);

	/* current pos: needed to track center chunk coord */
	memcpy(&map->cx, pos, sizeof (float) * 3);

	if (dx || dz)
	{
		if (dx >= area || dz >= area)
		{
			/* reset map center */
			map->mapX = map->mapZ = area / 2;
		}
		else /* some chunks will still be useful */
		{
			map->mapX = (map->mapX + dx + area) % area;
			map->mapZ = (map->mapZ + dz + area) % area;
		}
		mapRedoGenList(map);
		map->center = map->chunks + (map->mapX + map->mapZ * area);
		//mapMarkLazyChunk(map);
		return True;
	}
	return False;
}

static int sortByDist(const void * item1, const void * item2)
{
	int8_t * c1 = (int8_t *) item1;
	int8_t * c2 = (int8_t *) item2;

	return c1[0] * c1[0] + c1[1] * c1[1] - (c2[0] * c2[0] + c2[1] * c2[1]);
}

Chunk mapAllocArea(int area)
{
	Chunk chunks = calloc(sizeof *chunks, area * area);
	Chunk c;
	int   i, j, n, dist = area - 3;

	if (chunks)
	{
		/* should be property of a map... */
		int8_t * ptr = realloc(frustum.spiral, dist * dist * 2 + (dist * 4 + 4) * 2);

		if (ptr)
		{
			/* vertical wrap (mask value from chunkInitStatic) */
			for (c = chunks, i = area-1, n = area * (area-1), c->neighbor = 1 * 16, c[n].neighbor = 6 * 16, c ++;
				 i > 1; i --, c->neighbor = 2 * 16, c[n].neighbor = 7 * 16, c ++);
			c[0].neighbor = 3 * 16;
			c[n].neighbor = 8 * 16;
			/* horizontal wrap */
			for (n = area, c = chunks + n, i = n-2; i > 0; i --, c[0].neighbor = 4 * 16, c[n-1].neighbor = 5 * 16, c += n);

			/* to priority load chunks closest to the player */
			for (j = 0, frustum.spiral = ptr; j < dist; j ++)
			{
				for (i = 0; i < dist; i ++, ptr += 2)
				{
					ptr[0] = i - (dist >> 1);
					ptr[1] = j - (dist >> 1);
				}
			}
			i = dist * dist;
			qsort(frustum.spiral, i, 2, sortByDist);
			frustum.lazy = frustum.spiral + i * 2;

			/* to quickly enumerate all lazy chunks (need when map center has changed) */
			for (ptr = frustum.lazy, j = 0, dist += 2, i = dist >> 1; j < dist; j ++, ptr += 4)
			{
				ptr[0] = ptr[2] = j - i;
				ptr[1] = - i;
				ptr[3] =   i;
			}
			for (j = 0, dist -= 2; j < dist; j ++, ptr += 4)
			{
				ptr[1] = ptr[3] = j - (dist >> 1);
				ptr[0] = - i;
				ptr[2] =   i;
			}
			frustum.lazyCount = (ptr - frustum.lazy) >> 1;

			/* reset chunkNeighbor table: it depends on map size */
			static uint8_t wrap[] = {0, 12, 4, 6, 8, 2, 9, 1, 3}; /* bitfield: &1:+Z, &2:+X, &4:-Z, &8:-X, ie: SENW */

			for (j = 0, dist = area, n = area*area; j < DIM(wrap); j ++)
			{
				int16_t * p;
				uint8_t   w = wrap[j];
				for (i = 0, p = chunkNeighbor + j * 16; i < 16; i ++, p ++)
				{
					int pos = 0;
					if (i & 1) pos += w & 1 ? dist-n : dist;
					if (i & 2) pos += w & 2 ? 1-dist : 1;
					if (i & 4) pos -= w & 4 ? dist-n : dist;
					if (i & 8) pos -= w & 8 ? 1-dist : 1;
					p[0] = pos;
				}
			}

			return chunks;
		}
		else free(chunks);
	}
	return NULL;
}


uint8_t multiplyDeBruijnBitPosition[] = {
	0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
	31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};
#define ZEROBITS(bits)        multiplyDeBruijnBitPosition[((uint32_t)((bits & -(signed)bits) * 0x077CB531U)) >> 27]

int mapFirstFree(DATA32 usage, int count)
{
	int base, i;
	for (i = count, base = 0; i > 0; i --, usage ++, base += 32)
	{
		uint32_t bits = *usage ^ 0xffffffff;
		if (bits == 0) continue;
		/* count leading 0 */
		bits = ZEROBITS(bits);
		*usage |= 1 << bits;
		return base + bits;
	}
	return -1;
}

static DATA32 mapGenAllocMem(struct Thread_t * thread, int first)
{
	thread->state = THREAD_WAIT_BUFFER;
	SemWait(staging.capa);

	/* it might have passed a long time since */
	if (threadStop) return NULL;

	MutexEnter(staging.alloc);

	int index = mapFirstFree(staging.usage, DIM(staging.usage));
	DATA32 mem = staging.mem + index * 1024;
	staging.total ++;
	if (first)
		staging.start[staging.chunkData++] = index;

	MutexLeave(staging.alloc);

	thread->state = THREAD_RUNNING;

	return mem;
}

/*
 * thread chunk loading/meshing
 */
void mapGenChunkAsync(void * arg)
{
	struct Thread_t * thread = arg;
	Map map = thread->map;
	int id = thread == threads ? 0 : 1;

	while (threadStop != THREAD_EXIT)
	{
		/* waiting for something to do... */
		fprintf(stderr, "thread %d: waiting\n", id);

		thread->state = THREAD_WAIT_GENLIST;
		SemWait(map->genCount);

		if (threadStop == THREAD_EXIT_LOOP) continue;
		if (threadStop == THREAD_EXIT) break;

		thread->state = THREAD_RUNNING;
		MutexEnter(thread->wait);

		/* process chunks /!\ need to unlock the mutex before exiting this branch!! */
		MutexEnter(map->genLock);
		Chunk list = (Chunk) ListRemHead(&map->genList);
		MutexLeave(map->genLock);
		if (! list || (list->cflags & CFLAG_HASMESH))
			goto bail;

		fprintf(stderr, "thread %d: processing %d, %d\n", id, list->X, list->Z);

		/* simulate loading */
		static uint8_t directions[] = {12, 4, 6, 8, 0, 2, 9, 1, 3};
		Chunk checkLater[9];
		int i, X, Z, check;

		/* load 8 surrounding chunks too (mesh generation will need this) */
		for (i = check = 0, X = list->X, Z = list->Z; i < DIM(directions); i ++)
		{
			int   dir  = directions[i];
			Chunk load = list + map->chunkOffsets[list->neighbor + dir];

			MutexEnter(map->genLock);
			if (load->processing)
			{
				/* being processed by another thread: process another one in the meantime */
				checkLater[check++] = load;
				MutexLeave(map->genLock);
				continue;
			}
			load->processing = 1;
			MutexLeave(map->genLock);

			if (chunkLoad(load, X + (dir & 8 ? -16 : dir & 2 ? 16 : 0),
					Z + (dir & 4 ? -16 : dir & 1 ? 16 : 0),  id))
			{
				load->cflags |= CFLAG_GOTDATA;
			}
			load->processing = 0;

			if (threadStop) goto bail;
		}

		/* need to be sure all chunks have been loaded */
		for (i = 0; i < check; i ++)
		{
			Chunk load = checkLater[i];
			while (load->processing)
			{
				/* not done yet: wait a bit */
				double timeMS = FrameGetTime();
				while (FrameGetTime() - timeMS < 0.5 && load->processing && ! threadStop);
			}
			if (threadStop) goto bail;
		}

		/* process chunk */
		for (i = 0; i < list->maxy; i ++)
		{
			ChunkData cd = list->layer[i];

			/* chunkUpdate(cd) should be called here */

			// XXX this part must be done wihtin chunkUpdate()
			DATA32 last = NULL;
			int    size = cd->glSize;
			while (size > 0)
			{
				DATA32 mem = mapGenAllocMem(thread, last == NULL);
				if (mem == NULL)
					/* need to stop now */
					goto bail;
				/* avoid storing pointers in this stream */
				mem[0] = (list - map->chunks) | (i << 16);
				mem[1] = END_OF_LIST;

				SIT_ForceRefresh();

				size -= 4096 - 8;
				if (last) last[1] = mem - staging.mem;
				//fprintf(stderr, "thread %d: alloc mem block %d (%d)\n", id, mem - staging.mem, last ? last - staging.mem : -1);
				last = mem;

				/* don't care about content */
			}
			/* mark the chunk as ready */
			list->cflags |= CFLAG_HASMESH;
		}

		bail:
		/* this is to inform the main thread that this thread has finished its work */
		MutexLeave(thread->wait);
	}
	thread->state = -1;
	fprintf(stderr, "thread %d: exiting\n", id);
}

/* before world is loaded, check that the map has a few chunks in it */
Map mapInitFromPath(int renderDist, int * XZ)
{
	Map map = calloc(sizeof *map, 1);

	if (! map) return NULL;

	/* we want one column and one row of leaway */
	map->maxDist = renderDist * 2 + 1;
	map->mapArea = renderDist * 2 + 4;
	map->mapZ    = map->mapX = renderDist + 1;
	map->cx      = XZ[0];
	map->cz      = XZ[1];

	map->genLock = MutexCreate();
	map->genCount = SemInit(0);

	map->chunks = mapAllocArea(map->mapArea);
	map->center = map->chunks + (map->mapX + map->mapZ * map->mapArea);
	map->chunkOffsets = chunkNeighbor;

	mapRedoGenList(map);

	if (! staging.alloc)
	{
		int nb;
		staging.mem = malloc(MAX_BUFFER);
		staging.capa = SemInit(MAX_BUFFER/4096);
		staging.alloc = MutexCreate();
		for (nb = 0; nb < NUM_THREADS; nb ++)
		{
			threads[nb].wait = MutexCreate();
			threads[nb].map  = map;
			ThreadCreate(mapGenChunkAsync, threads + nb);
		}
	}

	return map;
}

/* make happy memory leak debugging tool */
void mapFreeAll(Map map)
{
	mapGenStopThread(map, THREAD_EXIT);

	GPUBank bank, next;
	Chunk   chunk;
	int     i;

	for (chunk = map->chunks, i = map->mapArea * map->mapArea; i > 0; chunkFree(chunk), chunk ++, i --);
	free(map->chunks);
	MutexDestroy(map->genLock);
	SemClose(map->genCount);

	for (bank = next = HEAD(map->gpuBanks); bank; bank = next)
	{
		NEXT(next);
		free(bank->usedList);
		free(bank);
	}

	free(map);


	free(staging.mem);
	SemClose(staging.capa);
	MutexDestroy(staging.alloc);
	memset(&staging, 0, sizeof staging);
}

/*
 * for reference: single threaded chunk loading function -- shouldn't be used
 */
#if 0
void mapGenerateMesh(Map map)
{
	ULONG start = TimeMS();

	while (map->genList.lh_Head)
	{
		static uint8_t directions[] = {12, 4, 6, 8, 0, 2, 9, 1, 3};
		int i, j, X, Z;

		Chunk list = (Chunk) ListRemHead(&map->genList);
		memset(&list->next, 0, sizeof list->next);

		if (list->cflags & CFLAG_HASMESH)
			continue;

		/* load 8 surrounding chunks too (mesh generation will need this) */
		for (i = 0, X = list->X, Z = list->Z; i < DIM(directions); i ++)
		{
			int   dir  = directions[i];
			Chunk load = list + map->chunkOffsets[list->neighbor + dir];

			/* already loaded ? */
			if (chunkLoad(load, X + (dir & 8 ? -16 : dir & 2 ? 16 : 0),
					Z + (dir & 4 ? -16 : dir & 1 ? 16 : 0)))
				load->cflags |= CFLAG_GOTDATA;
		}
		if ((list->cflags & CFLAG_GOTDATA) == 0)
		{
			if (TimeMS() - start > 15)
				break;
			/* no chunk at this location */
			continue;
		}

		/* second: push data to the GPU (only the first chunk) */
		for (i = 0, j = list->maxy; j > 0; j --, i ++)
		{
			ChunkData cd = list->layer[i];
			if (cd)
			{
				/* this is the function that will convert chunk into triangles */
				//chunkUpdate(list, chunkAir, map->chunkOffsets, i);
				renderFinishMesh(map, cd);
			}
		}
		list->cflags |= CFLAG_HASMESH;

		/* we are in the main rendering loop: don't hog the CPU for too long */
		if (TimeMS() - start > 15)
			break;
	}
}
#endif
