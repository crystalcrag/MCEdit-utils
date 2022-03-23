/*
 * fragment shader for drawing opaque and transparent blocks.
 */
#version 430 core

out vec4 color;

in  vec2  tc;
in  float skyLight;
in  float blockLight;
in  float fogFactor;
flat in uint rswire;
flat in uint faceNum;
flat in int  normal;

layout (binding=0) uniform sampler2D blockTex; // Main texture for blocks

uniform float biomeColor;
uniform float stencilBuffer;
uniform float showActive;

void main(void)
{
	if (showActive > 0 && showActive != float((faceNum>>3) + 1))
		discard;
		// color *= vec4(0.86, 0.57, 1, 1);

	color = texture(blockTex, tc);
	// prevent writing to the depth buffer: easy way to handle opacity for transparent block
	if (color.a < 0.004)
		discard;

	if (stencilBuffer > 0)
	{
		color = vec4(float(faceNum)/255.0, 0, 0, 1);
		return;
	}
	if (rswire >= 1)
	{
		// use color from terrain to shade wire: coord are located at tile 31x3.5 to 32x3.5
		color *= texture(blockTex, vec2(0.96875 + float(rswire-1) * 0.001953125, 0.0556640625));
		return;
	}

	if (biomeColor > 0 && color.x == color.y && color.y == color.z)
	{
		color *= vec4(0.411765, 0.768627, 0.294118, 1);
	}

	float sky = 0.9 * skyLight * skyLight + 0.1; if (sky < 0) sky = 0;
	float block = (blockLight * blockLight) * (1 - sky);  if (block < 0) block = 0;
	color *= vec4(sky, sky, sky, 1) + vec4(1.5 * block, 1.2 * block, 1 * block, 0);
}
