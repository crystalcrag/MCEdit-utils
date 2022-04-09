/*
 * vertex shader for drawing opaque and transparent blocks only.
 *
 * check doc/internals.html for vertex format: abandon all hope without reading this first.
 */
#version 430 core

layout (location=0) in uvec4 position;
layout (location=1) in uvec3 info;

uniform mat4 MVP;
uniform float texW;
uniform float texH;

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

void main(void)
{
	/* simply extract value from vertex buffers, and let geometry shader output real value for fsh */
	uint Usz = bitfieldExtract(info.y, 16, 8);
	uint Vsz = bitfieldExtract(info.y, 24, 8);
	uint U   = bitfieldExtract(info.x, 16, 9);
	uint V   = bitfieldExtract(info.x, 25, 7) | (bitfieldExtract(info.y, 0, 3) << 7);

	/* only 10 and 9 bits of precision, ideally we woud need 11 and 10, but that trick does the job nicely */
	if (V+Vsz-128 == 1023) Vsz ++;
	if (U+Usz-128 == 511)  Usz ++; else
	if (U == 511) U ++, Usz --;

	vertex1 = vec3(
		(float(bitfieldExtract(position.x,  0, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(position.x, 16, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(position.y,  0, 16)) - ORIGINVTX) * BASEVTX
	);

	/* slightly different from blocks.vsh : need more precision for V2 and V3 */
	vertex2 = vec3(
		(float(bitfieldExtract(position.y, 16, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(position.z,  0, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(position.z, 16, 16)) - ORIGINVTX) * BASEVTX
	);
	vertex3 = vec3(
		(float(bitfieldExtract(position.w,  0, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(position.w, 16, 16)) - ORIGINVTX) * BASEVTX,
		(float(bitfieldExtract(info.x,      0, 16)) - ORIGINVTX) * BASEVTX
	);

	texCoord = vec4(
		float(U) / texW, float(U + Usz - 128) / texW,
		float(V) / texH, float(V + Vsz - 128) / texH
	);
	faceId = info.z;
	normFlags = bitfieldExtract(info.y, 9, 7);
}
