/*
 * TileFinderGL.c : handle rendering of blocks/models using opengl.
 *
 * rendering is handled using more or less the same pipeline than terrain shaders from MCEdit:
 * generate GL_POINT using 28 bytes per vertex, then use a geometry shader to convert this to a quad.
 *
 * Written by T.Pierron, jan 2022.
 */


#define RENDER_IMPL
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "glad.h"
#include "SIT.h"
#include "nanovg.h"
#include "nanovg_gl_utils.h"
#include "TileFinder.h"
#include "TileFinderGL.h"

static struct Render_t render;
extern struct Prefs_t  prefs;

uint8_t cubeVertex[] = {
	0, 0, 1,  1, 0, 1,  1, 1, 1,  0, 1, 1,
	0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0,
};

static uint8_t cubeLines[] = {
	0, 1, 1, 2, 2, 3, 3, 0,
	4, 5, 5, 6, 6, 7, 7, 4,
	3, 7, 2, 6, 1, 5, 0, 4
};

static float unitAxis[] = {
	0, 0, 0, 1,    1, 0, 0, 1,
	0, 0, 0, 2,    0, 1, 0, 2,
	0, 0, 0, 3,    0, 0, 1, 3,
};

#ifdef DEBUG
static void GLAPIENTRY debugGLError(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	STRPTR str, sev;
	TEXT   typeUnknown[64];
	switch (type) {
	case GL_DEBUG_TYPE_ERROR:               str = "ERROR"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: str = "DEPRECATED_BEHAVIOR"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  str = "UNDEFINED_BEHAVIOR"; break;
	case GL_DEBUG_TYPE_PORTABILITY:         str = "PORTABILITY"; break;
	case GL_DEBUG_TYPE_PERFORMANCE:         str = "PERFORMANCE"; break;
	case GL_DEBUG_TYPE_OTHER:               str = "OTHER"; break;
	default:                                sprintf(str = typeUnknown, "TYPE:%d", type);
	}
	switch (severity){
	case GL_DEBUG_SEVERITY_LOW:    sev = "LOW"; break;
	case GL_DEBUG_SEVERITY_MEDIUM: sev = "MEDIUM"; break;
	case GL_DEBUG_SEVERITY_HIGH:   sev = "HIGH"; break;
	default:                       return; /* info stuff, don't care */
	}
	fprintf(stderr, "src: %d, id: %d, type: %s, sev: %s, %s\n", source, id, str, sev, message);
}
#endif

static void renderSetViewMat(void)
{
	mat4 perspective;
	mat4 lookAt;
	vec4 center = {0,0,0,1};
	vec4 camera = {0,0,render.rotation[2],1};

	float dot = cosf(render.rotation[1]);
	float det = sinf(render.rotation[1]);
	mat4 mat = {1,0,0,0, 0,dot,det,0, 0,-det,dot,0, 0,0,0,1};

	dot = cosf(render.rotation[0]);
	det = sinf(render.rotation[0]);
	mat4 mat2 = {dot,0,-det,0, 0,1,0,0, det,0,dot,0, 0,0,0,1};

	matMult(mat, mat, mat2);

	/* render.curRotation = M, lookAt = V, perspective = P */
	matMult(render.curRotation, mat, render.view);

	matPerspective(perspective, 80, render.vpSize[2] / (float) render.vpSize[3], 0.1, 100);
	matLookAt(lookAt, camera, center, (vec4){0,1,0,1});
	matMult(render.MVP, perspective, lookAt);
	matMult(render.MVP, render.MVP, render.curRotation);

	glProgramUniformMatrix4fv(render.shaderBlock,  render.uniformMVPB, 1, GL_FALSE, render.MVP);
	glProgramUniformMatrix4fv(render.shaderSelect, render.uniformMVPS, 1, GL_FALSE, render.MVP);
}

int renderInitStatic(void)
{
	#ifdef DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(debugGLError, NULL);
	#endif

	/* first init shader: if something can fail, it is this part :-/ */
	render.shaderBlock  = createGLSLProgram("blocks.vsh", "blocks.fsh", "blocks.gsh");
	render.shaderSelect = createGLSLProgram("select.vsh", "select.fsh", NULL);

	if (! render.shaderBlock || ! render.shaderSelect)
		return 0;

	/* will use the same VAO configuration than blocks.vsh from MCEdit */
	glGenVertexArrays(2, &render.vao);
	glGenBuffers(2, &render.vbo);

	glBindVertexArray(render.vao);
	glBindBuffer(GL_ARRAY_BUFFER, render.vbo);

	/* alloc some mem for vbo */
	glBufferData(GL_ARRAY_BUFFER, 100 * 1024, NULL, GL_STATIC_DRAW);

	glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, VERTEX_DATA_SIZE, 0);
	glEnableVertexAttribArray(0);
	glVertexAttribIPointer(1, 3, GL_UNSIGNED_INT, VERTEX_DATA_SIZE, (void *) 16);
	glEnableVertexAttribArray(1);

	glBindVertexArray(render.vaoSel);
	glBindBuffer(GL_ARRAY_BUFFER, render.vboSel);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	/* already fill selection vertices */
	glBufferData(GL_ARRAY_BUFFER, (DIM(cubeLines) + 12) * 16, NULL, GL_STATIC_DRAW);
	float * vertex = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	int i;
	for (i = 0; i < DIM(cubeLines); i ++, vertex += 4)
	{
		DATA8 src = cubeVertex + cubeLines[i] * 3;
		vertex[0] = src[0] - 0.5f;
		vertex[1] = src[1] - 0.5f;
		vertex[2] = src[2] - 0.5f;
		vertex[3] = 0;
	}

	/* also add unit axis in the list */
	memcpy(vertex, unitAxis, sizeof unitAxis);

	glUnmapBuffer(GL_ARRAY_BUFFER);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	render.uniformBiome  = glGetUniformLocation(render.shaderBlock, "biomeColor");
	render.uniformFaces  = glGetUniformLocation(render.shaderBlock, "stencilBuffer");
	render.uniformActive = glGetUniformLocation(render.shaderBlock, "showActive");
	render.uniformMVPB   = glGetUniformLocation(render.shaderBlock, "MVP");
	render.uniformTexW   = glGetUniformLocation(render.shaderBlock, "texW");
	render.uniformTexH   = glGetUniformLocation(render.shaderBlock, "texH");
	render.uniformMVPS   = glGetUniformLocation(render.shaderSelect, "MVP");
	render.uniformSide   = glGetUniformLocation(render.shaderSelect, "backSide");
	render.rotation[2]   = 4; /* distance from objects */
	render.rotation[0]   = 0;

	matIdent(render.view);

	return 1;
}

int renderSetTexture(STRPTR path, int texId)
{
	int w, h, bpp, format, cspace;
	DATA8 data;

	data = stbi_load(path, &w, &h, &bpp, 0);

	if (data == NULL)
	{
		fprintf(stderr, "fail to load image: %s\n", path);
		return 0;
	}
	if (w != 512)
	{
		/* make the image 512x512 */
		DATA8 resize = calloc(512 * bpp, h);
		DATA8 s, d;
		int   i = h;
		if (h > 512) h = 512;
		for (s = data, d = resize; i > 0; i --, d += 512*bpp, s += w*bpp)
			memcpy(d, s, w*bpp);
		w = h = 512;
		free(data);
		data = resize;
	}
	if (h > 1024) h = 1024;

	switch (bpp) {
	case 1: format = GL_RED; cspace = GL_RED; break;
	case 2: format = GL_LUMINANCE8_ALPHA8; cspace = 0; break;
	case 3: format = GL_RGB8;  cspace = GL_RGB; break;
	case 4: format = GL_RGBA8; cspace = GL_RGBA; break;
	default: return 0; /* should not happen */
	}

	if (texId == 0)
		glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, cspace, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	free(data);

	float param;
	param = w; glProgramUniform1fv(render.shaderBlock, render.uniformTexW, 1, &param);
	param = h; glProgramUniform1fv(render.shaderBlock, render.uniformTexH, 1, &param);

	return render.texId = texId;
}

/* SITE_OnResize on 3d cube preview */
int renderGetVPSize(SIT_Widget w, APTR cd, APTR ud)
{
	NVGcontext * vg;
	SIT_GetValues(w, SIT_ClientRect, render.vpSize, SIT_NVGcontext, &vg, NULL);
	/* glViewport need lower left corner */
	render.vpSize[1] = prefs.height - (render.vpSize[1] + render.vpSize[3]);

	if (render.vpSize[2] > 0 && render.vpSize[3] > 0)
	{
		if (render.fbo)
			nvgluDeleteFramebuffer(render.fbo);

		render.fbo = nvgluCreateFramebuffer(vg, render.vpSize[2], render.vpSize[3], NVG_IMAGE_MASK | NVG_IMAGE_DEPTH);
	}
	renderSetViewMat();
	return 1;
}

/* get face id from our fake "stencil" buffer */
static int renderGetFaceId(int x, int y)
{
	/* glReadPixels returns more pixels than ask, seems to rounded to 4 bytes multiple per row :-/ */
	uint8_t pixels[8];
	nvgluBindFramebuffer(render.fbo);
	glReadPixels(x, render.vpSize[3] - y, 1, 1, GL_RED, GL_UNSIGNED_BYTE, pixels);
	nvgluBindFramebuffer(NULL);

	#if 0
	int width = (render.vpSize[2]+3) & ~3; /* why? */
	DATA8 pix = calloc(width, render.vpSize[3]);

	nvgluBindFramebuffer(render.fbo);
	glReadPixels(0, 0, render.vpSize[2], render.vpSize[3], GL_RED, GL_UNSIGNED_BYTE, pix);

	nvgluBindFramebuffer(NULL);

	FILE * out = fopen("stencil.ppm", "wb");
	fprintf(out, "P5\n%d %d 255\n", width, render.vpSize[3]);
	fwrite(pix, width, render.vpSize[3], out);
	fclose(out);
	free(pix);
	#endif

	//fprintf(stderr, "face id at %d, %d = %d\n", x, y, pixels[0]);
	return pixels[0];
}

/* SITE_OnClickMove on preview */
int renderRotateView(SIT_Widget w, APTR cd, APTR ud)
{
	static float yawPitch[2];
	static int startX, startY;
	SIT_OnMouse * msg = cd;

	switch (msg->state) {
	case SITOM_ButtonPressed:
		switch (msg->button) {
		case SITOM_ButtonLeft:
			startX = msg->x;
			startY = msg->y;
			yawPitch[0] = 0;
			yawPitch[1] = 0;
			return 2;
		case SITOM_ButtonRight:
			uiSelectFace(renderGetFaceId(msg->x, msg->y));
			break;
		case SITOM_ButtonWheelDown:
			render.rotation[0] = 0;
			render.rotation[1] = 0;
			render.rotation[2] *= 1.1;
			goto reset_view;
		case SITOM_ButtonWheelUp:
			render.rotation[0] = 0;
			render.rotation[1] = 0;
			render.rotation[2] *= 0.9;
			goto reset_view;
		default:
			break;
		}
		break;
	case SITOM_CaptureMove:
		if (startX > 0)
		{
			#define EPSILON  0.001
			float norm = 1 / (render.vpSize[0] * 0.5f);
			render.rotation[0] = yawPitch[0] + (msg->x - startX) * norm;
			render.rotation[1] = yawPitch[1] + (msg->y - startY) * norm;
			if (render.rotation[1] < -M_PI_2+EPSILON) render.rotation[1] = -M_PI_2+EPSILON;
			if (render.rotation[1] >  M_PI_2-EPSILON) render.rotation[1] =  M_PI_2-EPSILON;
			reset_view:
			renderSetViewMat();
			SIT_ForceRefresh();
		}
		break;
	case SITOM_ButtonReleased:
		if (startX > 0)
		{
			memcpy(render.view, render.curRotation, sizeof render.view);
		}
		startX = 0;
	default:
		break;
	}
	return 0;
}

Block boxGetCurrent(void);

/* biome checkbox: modulate gray with biome color */
int renderSetFeature(SIT_Widget w, APTR cd, APTR ud)
{
	int checked = 0;
	SIT_GetValues(w, SIT_CheckState, &checked, NULL);
	float value = checked;
	if (ud)
	{
		if (checked)
		{
			Block b = boxGetCurrent();
			for (value = 0; b; value ++, PREV(b));
		}
		else value = 0;
		glProgramUniform1fv(render.shaderBlock, render.uniformActive, 1, &value);
	}
	else glProgramUniform1fv(render.shaderBlock, render.uniformBiome, 1, &value);
	SIT_ForceRefresh();
	return 1;
}

/* SITE_OnActivate on <reset> button */
int renderResetView(SIT_Widget w, APTR cd, APTR ud)
{
	render.rotation[0] = 0;
	render.rotation[1] = 0;
	render.rotation[2] = 4;
	matIdent(render.view);
	renderSetViewMat();
	SIT_ForceRefresh();
	return 1;
}

APTR renderMapBuffer(int buffer)
{
	vec mem;
	render.mapBuf = buffer;
	switch (buffer) {
	case 0:
		glBindBuffer(GL_ARRAY_BUFFER, render.vbo);
		/* DATA32 */
		return glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		break;
	case 1:
		glBindBuffer(GL_ARRAY_BUFFER, render.vboSel);
		mem = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		return mem + render.linesCount * 4;
	case 2:
		render.linesCount = 24+6;
	}
	return NULL;
}

void renderUnmapBuffer(int count)
{
	switch (render.mapBuf) {
	case 0:
		glUnmapBuffer(GL_ARRAY_BUFFER);
		render.vertexCount = count;
		break;
	case 1:
		glUnmapBuffer(GL_ARRAY_BUFFER);
		render.linesCount += count;
	}
}

void renderCube(void)
{
	glViewport(render.vpSize[0], render.vpSize[1], render.vpSize[2], render.vpSize[3]);
	glClear(GL_DEPTH_BUFFER_BIT);

	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LEQUAL);
	glFrontFace(GL_CCW);
	glDepthMask(GL_TRUE);

	if (render.vertexCount > 0)
	{
		float stencil = 0;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, render.texId);

		glProgramUniform1fv(render.shaderBlock, render.uniformFaces, 1, &stencil);
		glUseProgram(render.shaderBlock);
		glBindVertexArray(render.vao);
		glDrawArrays(GL_POINTS, 0, render.vertexCount);

		/* simulate stencil buffer */
		stencil = 1;
		nvgluBindFramebuffer(render.fbo);
		glViewport(0, 0, render.vpSize[2], render.vpSize[3]);
		glClearColor(0, 0, 0, 1);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		glProgramUniform1fv(render.shaderBlock, render.uniformFaces, 1, &stencil);
		glDrawArrays(GL_POINTS, 0, render.vertexCount);
		nvgluBindFramebuffer(NULL);

		glBindVertexArray(0);
	}

	if (prefs.bbox)
	{
		float back = 0;
		glViewport(render.vpSize[0], render.vpSize[1], render.vpSize[2], render.vpSize[3]);
		glProgramUniform1fv(render.shaderSelect, render.uniformSide, 1, &back);
		glUseProgram(render.shaderSelect);
		glBindVertexArray(render.vaoSel);
		glDrawArrays(GL_LINES, 0, render.linesCount);

		back = 1;
		glDepthFunc(GL_GEQUAL);
		glProgramUniform1fv(render.shaderSelect, render.uniformSide, 1, &back);
		glDrawArrays(GL_LINES, 0, render.linesCount);

		glBindVertexArray(0);
	}

	glViewport(0, 0, prefs.width, prefs.height);
}


/*
 * GLSL shaders compilation, program linking
 */
static void printShaderLog(GLuint shader, const char * path)
{
	int len = 0, written;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
	if (len > 0)
	{
		char * log = alloca(len);
		glGetShaderInfoLog(shader, len, &written, log);
		SIT_Log(SIT_ERROR, "%s: error compiling shader:\n%s\n", path, log);
	}
}

static void printProgramLog(GLuint program, const char * path)
{
	int len = 0, written;

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
	if (len > 0)
	{
		char * log = alloca(len);
		glGetProgramInfoLog(program, len, &written, log);
		SIT_Log(SIT_ERROR, "%s: error linking program:\n%s\n", path, log);
	}
}

int checkOpenGLError(const char * function)
{
	int error = 0;
	int glErr = glGetError();
	while (glErr != GL_NO_ERROR)
	{
		SIT_Log(SIT_ERROR, "%s: glError: %d\n", function, glErr);
		error = 1;
		glErr = glGetError();
	}
	return error;
}

static int compileShader(const char * path, int type)
{
	char * source = alloca(strlen(path) + 32);
	int    size;

	strcpy(source, SHADERDIR);
	strcat(source, path);

	size = FileSize(source);
	if (size > 0)
	{
		FILE * in = fopen(source, "rb");
		char * buffer = alloca(size+1);

		if (in)
		{
			fread(buffer, 1, size, in);
			buffer[size] = 0;
			fclose(in);

			GLint shader = glCreateShader(type);
			GLint compiled = 0;

			glShaderSource(shader, 1, (const char **)&buffer, NULL);
			glCompileShader(shader);
			checkOpenGLError("glCompileShader");
			glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
			if (compiled)
				return shader;

			printShaderLog(shader, path);
		}
		else SIT_Log(SIT_ERROR, "%s: %s\n", source, GetError());
	}
	else SIT_Log(SIT_ERROR, "%s: %s\n", source, GetError());
	return 0;
}

int createGLSLProgram(const char * vertexShader, const char * fragmentShader, const char * geomShader)
{
	int linked;
	int vertex   = compileShader(vertexShader, GL_VERTEX_SHADER);      if (vertex   == 0) return 0;
	int fragment = compileShader(fragmentShader, GL_FRAGMENT_SHADER);  if (fragment == 0) return 0;
	int geometry = 0;

	if (geomShader)
	{
		geometry = compileShader(geomShader, GL_GEOMETRY_SHADER);
		if (geometry == 0) return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	if (geometry > 0)
		glAttachShader(program, geometry);
	glLinkProgram(program);
	checkOpenGLError("glLinkProgram");
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (! linked)
	{
		printProgramLog(program, vertexShader);
		return 0;
	}
	return program;
}

/*
 * dynamic loading of opengl functions needed for this program (only a subset from glad.h)
 */
#define UNICODE
#include <windows.h>

PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
PFNGLCREATESHADERPROC glad_glCreateShader;
PFNGLSHADERSOURCEPROC glad_glShaderSource;
PFNGLCOMPILESHADERPROC glad_glCompileShader;
PFNGLGETSHADERIVPROC glad_glGetShaderiv;
PFNGLATTACHSHADERPROC glad_glAttachShader;
PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation;
PFNGLLINKPROGRAMPROC glad_glLinkProgram;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
PFNGLDELETESHADERPROC glad_glDeleteShader;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
PFNGLGENBUFFERSPROC glad_glGenBuffers;
PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
PFNGLUSEPROGRAMPROC glad_glUseProgram;
PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
PFNGLBINDBUFFERPROC glad_glBindBuffer;
PFNGLBUFFERDATAPROC glad_glBufferData;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
PFNGLUNIFORM1IPROC glad_glUniform1i;
PFNGLUNIFORM2FVPROC glad_glUniform2fv;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
PFNGLMAPBUFFERRANGEPROC glad_glMapBufferRange;
PFNGLSCISSORPROC glad_glScissor;

PFNGLBINDTEXTUREPROC glad_glBindTexture;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
PFNGLGETERRORPROC glad_glGetError;
PFNGLGETINTEGERVPROC glad_glGetIntegerv;
PFNGLGENTEXTURESPROC glad_glGenTextures;
PFNGLPIXELSTOREIPROC glad_glPixelStorei;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D;
PFNGLENABLEPROC glad_glEnable;
PFNGLDISABLEPROC glad_glDisable;
PFNGLDRAWARRAYSPROC glad_glDrawArrays;
PFNGLCULLFACEPROC glad_glCullFace;
PFNGLFRONTFACEPROC glad_glFrontFace;
PFNGLVIEWPORTPROC glad_glViewport;
PFNGLCLEARCOLORPROC glad_glClearColor;
PFNGLCLEARPROC glad_glClear;
PFNGLGETSTRINGPROC glad_glGetString;
PFNGLMULTIDRAWARRAYSPROC glad_glMultiDrawArrays;
PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset;
PFNGLGETTEXIMAGEPROC glad_glGetTexImage;

PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
PFNGLDEBUGMESSAGECALLBACKPROC glad_glDebugMessageCallback;
PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer;
PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor;
PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase;
PFNGLPROGRAMUNIFORM4FVPROC glad_glProgramUniform4fv;
PFNGLPROGRAMUNIFORM3FVPROC glad_glProgramUniform3fv;
PFNGLPROGRAMUNIFORM1FVPROC glad_glProgramUniform1fv;
PFNGLPROGRAMUNIFORM1UIPROC glad_glProgramUniform1ui;
PFNGLDRAWELEMENTSPROC glad_glDrawElements;
PFNGLPOLYGONMODEPROC glad_glPolygonMode;
PFNGLPOINTSIZEPROC glad_glPointSize;
PFNGLBLENDFUNCPROC glad_glBlendFunc;
PFNGLDEPTHFUNCPROC glad_glDepthFunc;
PFNGLMULTIDRAWARRAYSINDIRECTPROC glad_glMultiDrawArraysIndirect;
PFNGLREADPIXELSPROC glad_glReadPixels;
PFNGLDEPTHMASKPROC glad_glDepthMask;
PFNGLGETBUFFERSUBDATAPROC glad_glGetBufferSubData;
PFNGLMAPBUFFERPROC glad_glMapBuffer;
PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer;
PFNGLCOPYTEXIMAGE2DPROC glad_glCopyTexImage2D;
PFNGLREADBUFFERPROC glad_glReadBuffer;
PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced;
PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture;
PFNGLPROGRAMUNIFORMMATRIX4FVPROC glad_glProgramUniformMatrix4fv;

typedef void* (APIENTRYP PFNGLXGETPROCADDRESSPROC_PRIVATE)(const char*);
PFNGLXGETPROCADDRESSPROC_PRIVATE gladGetProcAddressPtr;

HANDLE opengl;
void * load(const char * func)
{
	void * result = NULL;
	if (gladGetProcAddressPtr)
		result = gladGetProcAddressPtr(func);
	if (result == NULL)
		result = GetProcAddress(opengl, func);
	return result;
}

int gladLoadGL(void)
{
	opengl = LoadLibrary(L"opengl32.dll");

	if (opengl)
	{
		const char * name;
		/* note: opengl context needs to be created before */
		gladGetProcAddressPtr = (void *) GetProcAddress(opengl, "wglGetProcAddress");
		if ((glad_glGetString          = load(name = "glGetString"))
		 && (glad_glGetShaderInfoLog   = load(name = "glGetShaderInfoLog"))
		 && (glad_glGetProgramInfoLog  = load(name = "glGetProgramInfoLog"))
		 && (glad_glCreateProgram      = load(name = "glCreateProgram"))
		 && (glad_glCreateShader       = load(name = "glCreateShader"))
		 && (glad_glShaderSource       = load(name = "glShaderSource"))
		 && (glad_glCompileShader      = load(name = "glCompileShader"))
		 && (glad_glGetShaderiv        = load(name = "glGetShaderiv"))
		 && (glad_glAttachShader       = load(name = "glAttachShader"))
		 && (glad_glBindAttribLocation = load(name = "glBindAttribLocation"))
		 && (glad_glLinkProgram        = load(name = "glLinkProgram"))
		 && (glad_glGetProgramiv       = load(name = "glGetProgramiv"))
		 && (glad_glDeleteProgram      = load(name = "glDeleteProgram"))
		 && (glad_glDeleteShader       = load(name = "glDeleteShader"))
		 && (glad_glGetUniformLocation = load(name = "glGetUniformLocation"))
		 && (glad_glGenVertexArrays    = load(name = "glGenVertexArrays"))
		 && (glad_glGenBuffers         = load(name = "glGenBuffers"))
		 && (glad_glGenerateMipmap     = load(name = "glGenerateMipmap"))
		 && (glad_glUseProgram         = load(name = "glUseProgram"))
		 && (glad_glActiveTexture      = load(name = "glActiveTexture"))
		 && (glad_glBindBuffer         = load(name = "glBindBuffer"))
		 && (glad_glBufferData         = load(name = "glBufferData"))
		 && (glad_glBindVertexArray    = load(name = "glBindVertexArray"))
		 && (glad_glUniform1i          = load(name = "glUniform1i"))
		 && (glad_glUniform2fv         = load(name = "glUniform2fv"))
		 && (glad_glDeleteBuffers      = load(name = "glDeleteBuffers"))
		 && (glad_glDeleteVertexArrays = load(name = "glDeleteVertexArrays"))
		 && (glad_glBindTexture        = load(name = "glBindTexture"))
		 && (glad_glDeleteTextures     = load(name = "glDeleteTextures"))
		 && (glad_glGetError           = load(name = "glGetError"))
		 && (glad_glGetIntegerv        = load(name = "glGetIntegerv"))
		 && (glad_glScissor            = load(name = "glScissor"))
		 && (glad_glGenTextures        = load(name = "glGenTextures"))
		 && (glad_glPixelStorei        = load(name = "glPixelStorei"))
		 && (glad_glTexImage2D         = load(name = "glTexImage2D"))
		 && (glad_glTexParameteri      = load(name = "glTexParameteri"))
		 && (glad_glTexSubImage2D      = load(name = "glTexSubImage2D"))
		 && (glad_glEnable             = load(name = "glEnable"))
		 && (glad_glDisable            = load(name = "glDisable"))
		 && (glad_glDrawArrays         = load(name = "glDrawArrays"))
		 && (glad_glCullFace           = load(name = "glCullFace"))
		 && (glad_glFrontFace          = load(name = "glFrontFace"))
		 && (glad_glViewport           = load(name = "glViewport"))
		 && (glad_glClearColor         = load(name = "glClearColor"))
		 && (glad_glClear              = load(name = "glClear"))
		 && (glad_glBufferSubData      = load(name = "glBufferSubData"))
		 && (glad_glDrawElements       = load(name = "glDrawElements"))
		 && (glad_glPolygonMode        = load(name = "glPolygonMode"))
		 && (glad_glPointSize          = load(name = "glPointSize"))
		 && (glad_glBlendFunc          = load(name = "glBlendFunc"))
		 && (glad_glDepthFunc          = load(name = "glDepthFunc"))
		 && (glad_glReadPixels         = load(name = "glReadPixels"))
		 && (glad_glBindBufferBase     = load(name = "glBindBufferBase"))
		 && (glad_glDepthMask          = load(name = "glDepthMask"))
		 && (glad_glGetBufferSubData   = load(name = "glGetBufferSubData"))
		 && (glad_glMapBuffer          = load(name = "glMapBuffer"))
		 && (glad_glUnmapBuffer        = load(name = "glUnmapBuffer"))
		 && (glad_glCopyTexImage2D     = load(name = "glCopyTexImage2D"))
		 && (glad_glReadBuffer         = load(name = "glReadBuffer"))
		 && (glad_glPolygonOffset      = load(name = "glPolygonOffset"))
		 && (glad_glMultiDrawArrays    = load(name = "glMultiDrawArrays"))
		 && (glad_glGetTexImage        = load(name = "glGetTexImage"))
		 && (glad_glMapBufferRange     = load(name = "glMapBufferRange"))
		 && (glad_glGetUniformBlockIndex     = load(name = "glGetUniformBlockIndex"))
		 && (glad_glUniformBlockBinding      = load(name = "glUniformBlockBinding"))
		 && (glad_glEnableVertexAttribArray  = load(name = "glEnableVertexAttribArray"))
		 && (glad_glVertexAttribPointer      = load(name = "glVertexAttribPointer"))
		 && (glad_glDebugMessageCallback     = load(name = "glDebugMessageCallback"))
		 && (glad_glVertexAttribIPointer     = load(name = "glVertexAttribIPointer"))
		 && (glad_glVertexAttribDivisor      = load(name = "glVertexAttribDivisor"))
		 && (glad_glProgramUniform4fv        = load(name = "glProgramUniform4fv"))
		 && (glad_glProgramUniform3fv        = load(name = "glProgramUniform3fv"))
		 && (glad_glProgramUniform1fv        = load(name = "glProgramUniform1fv"))
		 && (glad_glProgramUniform1ui        = load(name = "glProgramUniform1ui"))
		 && (glad_glDrawArraysInstanced      = load(name = "glDrawArraysInstanced"))
		 && (glad_glMultiDrawArraysIndirect  = load(name = "glMultiDrawArraysIndirect"))
		 && (glad_glBindImageTexture         = load(name = "glBindImageTexture"))
		 && (glad_glProgramUniformMatrix4fv  = load(name = "glProgramUniformMatrix4fv")))
		{
			return 1;
		}
		fprintf(stderr, "fail to load function '%s'\n", name);
	}
	return 0;
}
