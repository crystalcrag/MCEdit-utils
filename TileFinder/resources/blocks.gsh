/*
 * blocks.gsh : convert GL_POINT from blocks.vsh into GL_QUAD.
 *
 * check doc/internals.html for vertex format: abandon all hope without reading this first.
 */
#version 430 core

uniform mat4 MVP;

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in vec3 vertex1[];
in vec3 vertex2[];
in vec3 vertex3[];
in vec4 texCoord[];
in uint faceId[];
in uint normFlags[];
in vec3 offsets[];

out vec2  tc;
out float skyLight;
out float blockLight;
flat out uint  rswire;
flat out uint  faceNum;
flat out int   normal;

float shading(int normal)
{
	switch (normal) {
	case 0:  return 0.9;
	case 1:  return 0.8;
	case 2:  return 0.9;
	case 3:  return 0.8;
	case 4:  return 1.0;
	case 5:  return 0.7;
	default: return 1.0;
	}
}

void main(void)
{
	bool keepX = (normFlags[0] & (1 << 3)) > 0;

	normal = int(normFlags[0] & 7);

	vec3 V1 = vertex1[0];
	vec3 V2 = vertex2[0];
	vec3 V3 = vertex3[0];
	vec3 V4 = vertex3[0] + (vertex2[0] - vertex1[0]);
	#if 0
	if ((normFlags[0] & (1 << 4)) > 0 && dot(vertex1[0] - camera.xyz, cross(V3-V1, V2-V1)) < 0)
	{
		// this face must not be culled by back-face culling, but using current vertex emit order, it will
		V2 = V1; V1 = vertex2[0];
		V3 = V4; V4 = vertex3[0];
	}
	#endif

	/*
	 * lighting per face: block light (or torch light) will used a fixed shading per face.
	 * skylight will be directionnal.
	 */
	float shade = normal < 6 ? shading(normal) / 15 : 1/15.;
	uint  skyBlockLight = 0xf0f0f0f0;

	// first vertex
	gl_Position = MVP * vec4(V1, 1);
	faceNum     = faceId[0];
	skyLight    = float(bitfieldExtract(skyBlockLight, 28, 4)) * shade;
	blockLight  = float(bitfieldExtract(skyBlockLight, 24, 4)) * shade;
	tc          = keepX ? vec2(texCoord[0].x, texCoord[0].w) :
						  vec2(texCoord[0].y, texCoord[0].z) ;
	EmitVertex();

	// second vertex
	gl_Position = MVP * vec4(V2, 1);
	skyLight    = float(bitfieldExtract(skyBlockLight, 4, 4)) * shade;
	blockLight  = float(bitfieldExtract(skyBlockLight, 0, 4)) * shade;
	tc          = vec2(texCoord[0].x, texCoord[0].z);
	EmitVertex();
			
	// third vertex
	gl_Position = MVP * vec4(V3, 1);
	skyLight    = float(bitfieldExtract(skyBlockLight, 20, 4)) * shade;
	blockLight  = float(bitfieldExtract(skyBlockLight, 16, 4)) * shade;
	tc          = vec2(texCoord[0].y, texCoord[0].w);
	EmitVertex();

	// fourth vertex
	gl_Position = MVP * vec4(V4, 1);
	skyLight    = float(bitfieldExtract(skyBlockLight, 12, 4)) * shade;
	blockLight  = float(bitfieldExtract(skyBlockLight, 8,  4)) * shade;
	tc          = keepX ? vec2(texCoord[0].y, texCoord[0].z) :
						  vec2(texCoord[0].x, texCoord[0].w) ;
	EmitVertex();

	EndPrimitive();
}
