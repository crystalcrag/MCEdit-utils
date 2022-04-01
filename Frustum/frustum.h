

#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "utils.h"

#define PREFS_FILE        "config.ini"

struct Frustum_t
{
	SIT_Widget brush, label;
	SIT_Widget chunks;
	int        width, height;
	int        brushSize, drawMode, showCnx;
	int        brushX, brushY;
	vec4       camera;
	vec4       lookAt;
	mat4       matMVP;
	mat4       matInvMVP;
	int        FOV;
	float      angle;
	float      posX, posY;
	float      frustum[8];
	float      vpWidth, vpHeight;
	uint16_t   brushRange[30];
	PolyPath_t brushVector;
	DATA8      chunkBitmap;
	int        nvgChunkImg;
	APTR       nvgCtx;
};

#define CELLMAPX      20             /* number of chunkx in X axis */
#define CELLMAPY      16             /* number of chunks in Y axis (must not be above 16) */
#define CELLSZ        16             /* blocks in a chunks (must be 16 actually :-/) */
#define MARGIN        20             /* pixels from container border */
#define ZNEAR         0.1
#define ZFAR          150
#define IMAGESIZEX    (CELLMAPX * CELLSZ)
#define IMAGESIZEY    (CELLMAPY * CELLSZ)


#endif

