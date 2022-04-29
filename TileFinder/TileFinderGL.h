/*
 * TileFinderGL.c: public function to handle rendering of model.
 *
 * Written by T.Pierron, jan 2022.
 */

#ifndef TILE_FINDER_GL_H
#define TILE_FINDER_GL_H

#include "utils.h"

void renderCube(void);
int  renderInitStatic(void);
int  renderSetTexture(STRPTR path, int useTexId);
int  renderGetVPSize(SIT_Widget, APTR, APTR);
int  renderRotateView(SIT_Widget, APTR, APTR);
int  renderResetView(SIT_Widget, APTR, APTR);
int  renderSetFeature(SIT_Widget, APTR, APTR);
APTR renderMapBuffer(int);
void renderUnmapBuffer(int count);
int  gladLoadGL(void);


#define VERTEX_INT_SIZE     7
#define VERTEX_DATA_SIZE    28
#define SHADERDIR           "resources/"

#ifdef RENDER_IMPL
typedef struct NVGLUframebuffer *     NVGFBO;

struct Render_t
{
	int  vao, vaoSel, vbo, vboSel;
	int  shaderBlock, shaderSelect;
	int  uniformMVPB, uniformMVPS;
	int  uniformBiome, uniformSide;
	int  uniformFaces, uniformActive;
	int  uniformTexW, uniformTexH;
	int  vertexCount;
	int  linesCount;
	int  texId, mapBuf;
	int  vpSize[4];
	mat4 view, curRotation;
	mat4 MVP;
	vec4 rotation;
	NVGFBO fbo;
	NVGcontext * nvg;
};

int createGLSLProgram(const char * vertexShader, const char * fragmentShader, const char * geomShader);

#endif
#endif

