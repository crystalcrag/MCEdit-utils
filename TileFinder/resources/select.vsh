/*
 * vertex selection lines
 */
#version 430 core

layout (location=0) in vec4 position;

uniform mat4 MVP;
uniform float backSide;

flat out int lineType;

void main(void)
{
	gl_Position = MVP * vec4(position.xyz, 1);
	lineType = int(position.w) | (int(backSide) << 4);
}
