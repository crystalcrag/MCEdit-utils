/*
 * SkyLight.h : skylight update public function
 *
 * Written by T.Pierron, Aug 2019
 */

#ifndef SKYLIGHT_H
#define SKYLIGHT_H

#include "UtilityLibLite.h"

void skyGenTerrain(void);
void skySetBlockInit(int x, int y);
void skyUnsetBlockInit(int x, int y);
void skyGetNextCell(int XY[2]);
int  skySetBlock(void);
int  skyUnsetBlock(void);

struct SkyLight_t
{
	APTR  blocks[3], step;
	DATA8 skyLight;
	DATA8 blockIds;
	DATA8 heightMap;
	float cellSz;
	int   width, height;
	int   blockType;
	int   stepByStep;
	int   stepping;
	int   rect[4];
	int   cellXY[2];
	APTR  nvg;
};

enum /* possible values for blockType */
{
	BLOCK_AIR,
	BLOCK_OPAQUE = 0x70,
	BLOCK_LEAVE  = 0xa0,
	BLOCK_WATER  = 0x80
};

enum /* vlaues for stepping */
{
	STEP_DONE,
	STEP_SETBLOCK ,
	STEP_UNSETBLOCK,
};

struct TrackUpdate_t
{
	int8_t * coord;
	int16_t  max;
	int16_t  pos, last, usage, maxUsage;
	uint8_t  unique;
	int      startX, startY;
};

#define CELLW         64
#define CELLH         64
#define MAXSKY        8
#define SAVEFILE      "skylight.map"



#endif
