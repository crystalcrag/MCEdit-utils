/*
 * vertex shader for drawing opaque and transparent blocks only.
 *
 * check doc/internals.html for vertex format: abandon all hope without reading this first.
 */
#version 430 core

layout (location=0) in uvec4 position;
layout (location=1) in uvec3 info;

uniform mat4 MVP;

/* GL_POINT needs to be converted to quad */
out vec3 vertex1;
out vec3 vertex2;
out vec3 vertex3;
out vec4 texCoord;
out uint faceId;
out uint normFlags;

#define ORIGINVTX      15360
#define BASEVTX        0.00048828125
#define MIDVTX         4
#define TEX_COORD_X    (1/512.)
#define TEX_COORD_Y    (1/512.)

void main(void)
{
	/* simply extract value from vertex buffers, and let geometry shader output real value for fsh */
	uint Usz = bitfieldExtract(info.y, 16, 8);
	uint Vsz = bitfieldExtract(info.y, 24, 8);
	uint U   = bitfieldExtract(info.x, 14, 9);
	uint V   = bitfieldExtract(info.x, 23, 9) | (bitfieldExtract(position.y, 30, 1) << 9);

	/* only 10 and 9 bits of precision, ideally we woud need 11 and 10, but that trick does the job nicely */
	if (V+Vsz-128 == 1023) Vsz ++;
	if (U+Usz-128 == 511)  Usz ++; else
	if (U == 511) U ++, Usz --;

	vertex1 = vec3(
		(float(bitfieldExtract(position.x,  0, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(position.x, 16, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(position.y,  0, 16)) - ORIGINVTX) * BASEVTX
	);

	/* 2nd and 3rd vertex are relative to 1st (saves 2 bits per coord, 6 in total) */
	vertex2 = vec3(
		float(bitfieldExtract(position.y, 16, 14)) * BASEVTX - MIDVTX + vertex1.x,
		float(bitfieldExtract(position.z,  0, 14)) * BASEVTX - MIDVTX + vertex1.y,
		float(bitfieldExtract(position.z, 14, 14)) * BASEVTX - MIDVTX + vertex1.z
	);
	vertex3 = vec3(
		float(bitfieldExtract(position.w,  0, 14)) * BASEVTX - MIDVTX + vertex1.x,
		float(bitfieldExtract(position.w, 14, 14)) * BASEVTX - MIDVTX + vertex1.y,
		float(bitfieldExtract(info.x,      0, 14)) * BASEVTX - MIDVTX + vertex1.z
	);

	texCoord = vec4(
		float(U) * TEX_COORD_X, float(U + Usz - 128) * TEX_COORD_X,
		float(V) * TEX_COORD_Y, float(V + Vsz - 128) * TEX_COORD_Y
	);
	faceId = info.z;
	normFlags = bitfieldExtract(info.y, 9, 7);
}
