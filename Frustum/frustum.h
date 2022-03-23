

#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "utils.h"

#define PREFS_FILE        "config.ini"

struct Frustum_t
{
	SIT_Widget brush;
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
	float      frustum[8];
	float      vpWidth, vpHeight;
	uint16_t   brushRange[30];
	PolyPath_t brushVector;
	PolyPath_t chunkVector;
	DATA8      chunkBitmap;
	int        nvgChunkImg;
	APTR       nvgCtx;
};

#define CELLMAP       20
#define CELLSZ        16
#define MARGIN        20
#define ZNEAR         0.1
#define ZFAR          150
#define IMAGESIZE     (CELLMAP * CELLSZ)


#endif

