/*
 * fragment shader for drawing selection.
 */
#version 430 core

out vec4 color;

flat in int lineType;

void main(void)
{
	float a = (lineType & 0xf0) > 0 ? 0.33 : 1;
	switch (lineType & 15) {
	default: color = vec4(1,1,1,a); break;
	case 1:  color = vec4(1,0,0,a); break;
	case 2:  color = vec4(0,1,0,a); break;
	case 3:  color = vec4(0,0,1,a);
	}
}
