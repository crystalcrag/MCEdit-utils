/*
 * Utils.c: utility function to deal with opengl and 3d math
 */


#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include "UtilityLibLite.h"
#include "Utils.h"

/*
 * classical matrix related operations
 */
void matTranspose(mat4 A)
{
	float tmp;

	tmp = A[A10]; A[A10] = A[A01]; A[A01] = tmp;
	tmp = A[A20]; A[A20] = A[A02]; A[A02] = tmp;
	tmp = A[A30]; A[A30] = A[A03]; A[A03] = tmp;
	tmp = A[A12]; A[A12] = A[A21]; A[A21] = tmp;
	tmp = A[A13]; A[A13] = A[A31]; A[A31] = tmp;
	tmp = A[A23]; A[A23] = A[A32]; A[A32] = tmp;
}

void matAdd(mat4 res, mat4 A, mat4 B)
{
	int i;
	for (i = 0; i < 16; i ++)
		res[i] = A[i] + B[i];
}

void matMult(mat4 res, mat4 A, mat4 B)
{
	mat4 tmp;

	tmp[A00] = A[A00]*B[A00] + A[A01]*B[A10] + A[A02]*B[A20] + A[A03]*B[A30];
	tmp[A10] = A[A10]*B[A00] + A[A11]*B[A10] + A[A12]*B[A20] + A[A13]*B[A30];
	tmp[A20] = A[A20]*B[A00] + A[A21]*B[A10] + A[A22]*B[A20] + A[A23]*B[A30];
	tmp[A30] = A[A30]*B[A00] + A[A31]*B[A10] + A[A32]*B[A20] + A[A33]*B[A30];
	tmp[A01] = A[A00]*B[A01] + A[A01]*B[A11] + A[A02]*B[A21] + A[A03]*B[A31];
	tmp[A11] = A[A10]*B[A01] + A[A11]*B[A11] + A[A12]*B[A21] + A[A13]*B[A31];
	tmp[A21] = A[A20]*B[A01] + A[A21]*B[A11] + A[A22]*B[A21] + A[A23]*B[A31];
	tmp[A31] = A[A30]*B[A01] + A[A31]*B[A11] + A[A32]*B[A21] + A[A33]*B[A31];
	tmp[A02] = A[A00]*B[A02] + A[A01]*B[A12] + A[A02]*B[A22] + A[A03]*B[A32];
	tmp[A12] = A[A10]*B[A02] + A[A11]*B[A12] + A[A12]*B[A22] + A[A13]*B[A32];
	tmp[A22] = A[A20]*B[A02] + A[A21]*B[A12] + A[A22]*B[A22] + A[A23]*B[A32];
	tmp[A32] = A[A30]*B[A02] + A[A31]*B[A12] + A[A32]*B[A22] + A[A33]*B[A32];
	tmp[A03] = A[A00]*B[A03] + A[A01]*B[A13] + A[A02]*B[A23] + A[A03]*B[A33];
	tmp[A13] = A[A10]*B[A03] + A[A11]*B[A13] + A[A12]*B[A23] + A[A13]*B[A33];
	tmp[A23] = A[A20]*B[A03] + A[A21]*B[A13] + A[A22]*B[A23] + A[A23]*B[A33];
	tmp[A33] = A[A30]*B[A03] + A[A31]*B[A13] + A[A32]*B[A23] + A[A33]*B[A33];

	memcpy(res, tmp, sizeof tmp);
}

void matMultByVec(vec4 res, mat4 A, vec4 B)
{
	vec4 tmp;

	tmp[VX] = A[A00]*B[VX] + A[A01]*B[VY] + A[A02]*B[VZ] + A[A03]*B[VT];
	tmp[VY] = A[A10]*B[VX] + A[A11]*B[VY] + A[A12]*B[VZ] + A[A13]*B[VT];
	tmp[VZ] = A[A20]*B[VX] + A[A21]*B[VY] + A[A22]*B[VZ] + A[A23]*B[VT];
	tmp[VT] = A[A30]*B[VX] + A[A31]*B[VY] + A[A32]*B[VZ] + A[A33]*B[VT];

	memcpy(res, tmp, sizeof tmp);
}

/*
 * taken from glm library: convert a matrix intended for vertex so that it can be applied to a vector
 * (has to ignore translation). Not: normalization will still be required if used on a normal.
 */
void matInverseTranspose(mat4 res, mat4 m)
{
	float SubFactor00 = m[A22] * m[A33] - m[A32] * m[A23];
	float SubFactor01 = m[A21] * m[A33] - m[A31] * m[A23];
	float SubFactor02 = m[A21] * m[A32] - m[A31] * m[A22];
	float SubFactor03 = m[A20] * m[A33] - m[A30] * m[A23];
	float SubFactor04 = m[A20] * m[A32] - m[A30] * m[A22];
	float SubFactor05 = m[A20] * m[A31] - m[A30] * m[A21];
	float SubFactor06 = m[A12] * m[A33] - m[A32] * m[A13];
	float SubFactor07 = m[A11] * m[A33] - m[A31] * m[A13];
	float SubFactor08 = m[A11] * m[A32] - m[A31] * m[A12];
	float SubFactor09 = m[A10] * m[A33] - m[A30] * m[A13];
	float SubFactor10 = m[A10] * m[A32] - m[A30] * m[A12];
	float SubFactor11 = m[A10] * m[A31] - m[A30] * m[A11];
	float SubFactor12 = m[A12] * m[A23] - m[A22] * m[A13];
	float SubFactor13 = m[A11] * m[A23] - m[A21] * m[A13];
	float SubFactor14 = m[A11] * m[A22] - m[A21] * m[A12];
	float SubFactor15 = m[A10] * m[A23] - m[A20] * m[A13];
	float SubFactor16 = m[A10] * m[A22] - m[A20] * m[A12];
	float SubFactor17 = m[A10] * m[A21] - m[A20] * m[A11];

	mat4 Inverse;
	Inverse[A00] = + (m[A11] * SubFactor00 - m[A12] * SubFactor01 + m[A13] * SubFactor02);
	Inverse[A01] = - (m[A10] * SubFactor00 - m[A12] * SubFactor03 + m[A13] * SubFactor04);
	Inverse[A02] = + (m[A10] * SubFactor01 - m[A11] * SubFactor03 + m[A13] * SubFactor05);
	Inverse[A03] = - (m[A10] * SubFactor02 - m[A11] * SubFactor04 + m[A12] * SubFactor05);
	Inverse[A10] = - (m[A01] * SubFactor00 - m[A02] * SubFactor01 + m[A03] * SubFactor02);
	Inverse[A11] = + (m[A00] * SubFactor00 - m[A02] * SubFactor03 + m[A03] * SubFactor04);
	Inverse[A12] = - (m[A00] * SubFactor01 - m[A01] * SubFactor03 + m[A03] * SubFactor05);
	Inverse[A13] = + (m[A00] * SubFactor02 - m[A01] * SubFactor04 + m[A02] * SubFactor05);
	Inverse[A20] = + (m[A01] * SubFactor06 - m[A02] * SubFactor07 + m[A03] * SubFactor08);
	Inverse[A21] = - (m[A00] * SubFactor06 - m[A02] * SubFactor09 + m[A03] * SubFactor10);
	Inverse[A22] = + (m[A00] * SubFactor07 - m[A01] * SubFactor09 + m[A03] * SubFactor11);
	Inverse[A23] = - (m[A00] * SubFactor08 - m[A01] * SubFactor10 + m[A02] * SubFactor11);
	Inverse[A30] = - (m[A01] * SubFactor12 - m[A02] * SubFactor13 + m[A03] * SubFactor14);
	Inverse[A31] = + (m[A00] * SubFactor12 - m[A02] * SubFactor15 + m[A03] * SubFactor16);
	Inverse[A32] = - (m[A00] * SubFactor13 - m[A01] * SubFactor15 + m[A03] * SubFactor17);
	Inverse[A33] = + (m[A00] * SubFactor14 - m[A01] * SubFactor16 + m[A02] * SubFactor17);

	float Determinant =
		+ m[A00] * Inverse[A00]
		+ m[A01] * Inverse[A01]
		+ m[A02] * Inverse[A02]
		+ m[A03] * Inverse[A03];

	int i;
	for (i = 0; i < 16; i ++)
		Inverse[i] /= Determinant;

	memcpy(res, Inverse, sizeof Inverse);
}


/* taken from Mesa3d */
void matInverse(mat4 res, mat4 m)
{
	float det;
	mat4  inv;
	int   i;

	inv[0] = m[5]  * m[10] * m[15] -
	         m[5]  * m[11] * m[14] -
	         m[9]  * m[6]  * m[15] +
	         m[9]  * m[7]  * m[14] +
	         m[13] * m[6]  * m[11] -
	         m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] +
	          m[4]  * m[11] * m[14] +
	          m[8]  * m[6]  * m[15] -
	          m[8]  * m[7]  * m[14] -
	          m[12] * m[6]  * m[11] +
	          m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] -
	         m[4]  * m[11] * m[13] -
	         m[8]  * m[5] * m[15] +
	         m[8]  * m[7] * m[13] +
	         m[12] * m[5] * m[11] -
	         m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] +
	           m[4]  * m[10] * m[13] +
	           m[8]  * m[5] * m[14] -
	           m[8]  * m[6] * m[13] -
	           m[12] * m[5] * m[10] +
	           m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] +
	          m[1]  * m[11] * m[14] +
	          m[9]  * m[2] * m[15] -
	          m[9]  * m[3] * m[14] -
	          m[13] * m[2] * m[11] +
	          m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] -
	         m[0]  * m[11] * m[14] -
	         m[8]  * m[2] * m[15] +
	         m[8]  * m[3] * m[14] +
	         m[12] * m[2] * m[11] -
	         m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] +
	          m[0]  * m[11] * m[13] +
	          m[8]  * m[1] * m[15] -
	          m[8]  * m[3] * m[13] -
	          m[12] * m[1] * m[11] +
	          m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] -
	          m[0]  * m[10] * m[13] -
	          m[8]  * m[1] * m[14] +
	          m[8]  * m[2] * m[13] +
	          m[12] * m[1] * m[10] -
	          m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] -
	         m[1]  * m[7] * m[14] -
	         m[5]  * m[2] * m[15] +
	         m[5]  * m[3] * m[14] +
	         m[13] * m[2] * m[7] -
	         m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] +
	          m[0]  * m[7] * m[14] +
	          m[4]  * m[2] * m[15] -
	          m[4]  * m[3] * m[14] -
	          m[12] * m[2] * m[7] +
	          m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] -
	          m[0]  * m[7] * m[13] -
	          m[4]  * m[1] * m[15] +
	          m[4]  * m[3] * m[13] +
	          m[12] * m[1] * m[7] -
	          m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] +
	           m[0]  * m[6] * m[13] +
	           m[4]  * m[1] * m[14] -
	           m[4]  * m[2] * m[13] -
	           m[12] * m[1] * m[6] +
	           m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
	          m[1] * m[7] * m[10] +
	          m[5] * m[2] * m[11] -
	          m[5] * m[3] * m[10] -
	          m[9] * m[2] * m[7] +
	          m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
	         m[0] * m[7] * m[10] -
	         m[4] * m[2] * m[11] +
	         m[4] * m[3] * m[10] +
	         m[8] * m[2] * m[7] -
	         m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
	           m[0] * m[7] * m[9] +
	           m[4] * m[1] * m[11] -
	           m[4] * m[3] * m[9] -
	           m[8] * m[1] * m[7] +
	           m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
	          m[0] * m[6] * m[9] -
	          m[4] * m[1] * m[10] +
	          m[4] * m[2] * m[9] +
	          m[8] * m[1] * m[6] -
	          m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (fabsf(det) > EPSILON)
	{
		det = 1.0f / det;

		for (i = 0; i < 16; i++)
			res[i] = inv[i] * det;
	}
	else matIdent(res);
}

/* perspective projection (https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml) */
void matPerspective(mat4 res, float fov_deg, float aspect, float znear, float zfar)
{
	memset(res, 0, sizeof (mat4));

	/*
	 * Will convert a vertex coordinates into a screen coordinates, while z being the distance to the camera.
	 * All values will be normalized between -1 and 1. 0,0 is the center of the screen.
	 */
	double q = 1 / tan(fov_deg * M_PI / 360);
	res[A00] = q / aspect;
	res[A11] = q;
	res[A22] = (znear + zfar) / (znear - zfar);
	res[A23] = 2 * znear * zfar / (znear - zfar);
	res[A32] = -1;
}

/* orthographic projection */
void matOrtho(mat4 res, float left, float right, float top, float bottom, float znear, float zfar)
{
	memset(res, 0, sizeof (mat4));
	res[A00] = 2 / (right - left);
	res[A11] = 2 / (top - bottom);
	res[A22] = 1 / (zfar - znear);
	res[A03] = - (right + left) / (right - left);
	res[A13] = - (top + bottom) / (bottom - top);
	res[A23] = - znear / (zfar - znear);
	res[A33] = 1;
}

/* similar to gluLookAt */
void matLookAt(mat4 res, vec4 eye, vec4 center, vec4 up)
{
	vec4 fwd = {center[VX] - eye[VX], center[VY] - eye[VY], center[VZ] - eye[VZ]};
	vec4 side;

	memset(res, 0, sizeof (mat4));

	vecNormalize(fwd, fwd);
	vecCrossProduct(side, fwd, up);
	vecNormalize(side, side);
	vecCrossProduct(up, side, fwd);
	vecNormalize(up, up);

	/* from book */
	res[A00] = side[VX];
	res[A01] = side[VY];
	res[A02] = side[VZ];
	res[A03] = - vecDotProduct(side, eye);
	res[A10] = up[VX];
	res[A11] = up[VY];
	res[A12] = up[VZ];
	res[A13] = - vecDotProduct(up, eye);
	res[A20] = -fwd[VX];
	res[A21] = -fwd[VY];
	res[A22] = -fwd[VZ];
	res[A23] = vecDotProduct(fwd, eye);
	res[A33] = 1;
}

/* generate a transformation matrix in res */
void matIdent(mat4 res)
{
	memset(res, 0, sizeof (mat4));
	res[A00] = res[A11] = res[A22] = res[A33] = 1;
}

void matTranslate(mat4 res, float x, float y, float z)
{
	matIdent(res);
	res[A03] = x;
	res[A13] = y;
	res[A23] = z;
}

void matScale(mat4 res, float x, float y, float z)
{
	memset(res, 0, sizeof *res);
	res[A00] = x;
	res[A11] = y;
	res[A22] = z;
	res[A33] = 1;
}

void matRotate(mat4 res, float theta, int axis_0X_1Y_2Z)
{
	float fcos = cosf(theta);
	float fsin = sinf(theta);
	matIdent(res);
	switch (axis_0X_1Y_2Z) {
	case 0: /* along X axis */
		res[A11] = fcos;
		res[A12] = -fsin;
		res[A21] = fsin;
		res[A22] = fcos;
		break;
	case 1: /* along Y axis */
		res[A00] = fcos;
		res[A02] = -fsin;
		res[A20] = fsin;
		res[A22] = fcos;
		break;
	case 2: /* along Z axis */
		res[A00] = fcos;
		res[A01] = -fsin;
		res[A10] = fsin;
		res[A11] = fcos;
	}
}

void matPrint(mat4 A)
{
	int i;
	fputc('[', stderr);
	for (i = 0; i < 16; i ++)
	{
		fprintf(stderr, "\t%f", A[i]);
		if ((i & 3) == 3) fputc('\n', stderr);
	}
	fputs("];\n", stderr);
}

/*
 * classical vector operations
 */
float vecLength(vec4 A)
{
	return sqrt(A[VX]*A[VX] + A[VY]*A[VY] + A[VZ]*A[VZ]);
}

void vecNormalize(vec4 res, vec4 A)
{
	float len = vecLength(A);
	res[VX] = A[VX] / len;
	res[VY] = A[VY] / len;
	res[VZ] = A[VZ] / len;
}

float vecDotProduct(vec4 A, vec4 B)
{
	return A[VX]*B[VX] + A[VY]*B[VY] + A[VZ]*B[VZ];
}

/* get perpendicular vector to A and B */
void vecCrossProduct(vec4 res, vec4 A, vec4 B)
{
	vec4 tmp;

	tmp[VX] = A[VY]*B[VZ] - A[VZ]*B[VY];
	tmp[VY] = A[VZ]*B[VX] - A[VX]*B[VZ];
	tmp[VZ] = A[VX]*B[VY] - A[VY]*B[VX];

	memcpy(res, tmp, sizeof tmp);
}

/*
 * transform bitmap into a contour polygon
 */
static int8_t directions[] = { /* right, bottom, left, top */
	1, 0,    0, 1,    -1, 0,    0, -1,
};

enum
{
	SIDE_RIGHT  = 0,
	SIDE_BOTTOM = 2,
	SIDE_LEFT   = 4,
	SIDE_TOP    = 6,
};

enum
{
	FLAG_RIGHT  = 1,
	FLAG_BOTTOM = 2,
	FLAG_LEFT   = 4,
	FLAG_TOP    = 8,
	FLAG_VISIT  = 16,
	FLAG_PIXEL  = 128
};

static void polyPathAdd(PolyPath path, int value)
{
	int nb = path->count;
	int max = (nb + 127) & ~127;
	if (nb + 1 > max)
	{
		int max = (nb + 128) & ~127;
		path->coords = realloc(path->coords, max * sizeof *path->coords);
	}
	DATA16 coord = path->coords + path->count;
	if (path->start >= 0)
	{
		if (path->count > 3 && (coord[-1] & 15) == value)
		{
			coord[-1] += 16;
			return;
		}
		path->coords[path->start] ++;
	}
	coord[0] = value;
	path->count++;
}

static void polyPathInit(PolyPath path, int x, int y, int outer)
{
	path->start = -1;
	polyPathAdd(path, outer);
	polyPathAdd(path, x);
	polyPathAdd(path, y);
	path->start = path->count - 3;
}

/*
 * bitmap: bitfield with the following flags per pixels:
 * - 1: pixel processed on right side.
 * - 2: processed on bottom side.
 * - 4: left side.
 * - 8: top side.
 * - 16: pixel visited (cheap trick to check if pixel is inside or outside a path)
 * - 128: pixel (only flag set when entering this function).
 */
static void polygonAdd(PolyPath path, int x, int y, int initFlag, int inner, DATA8 bitmap, int width, int height);

void vectorize(PolyPath path, DATA8 bitmap, int width, int height)
{
	DATA8 p, eof;
	int   x, y;

	memset(path, 0, sizeof *path);

	/* clear flags from bitmap */
	for (p = bitmap, eof = p + width * height; p < eof; p[0] &= FLAG_PIXEL, p ++);

	for (p = bitmap; p < eof; )
	{
		/* find a starting location */
		while (p < eof && p[0] != FLAG_PIXEL) p ++;
		if (p == eof) break; /* all done ! */


		/* two possible directions at this point: right or bottom */
		y = (p - bitmap) / width;
		x = (p - bitmap) % width;

		/* find outer polygon */
		polygonAdd(path, x, y, FLAG_TOP, 0, bitmap, width, height);


		#if 0
		/* check for inner walls */
		DATA16 track;
		int pos = 0, last = 0, max = (3 * MAX(width, height) + 1) & ~1;
		track = alloca(max * 2);
		track[last++] = x;
		track[last++] = y;

		while (pos != last)
		{
			x = track[pos];
			y = track[pos+1]; pos += 2;
			if (pos == max) pos = 0;

			/* check where we can go from x, y */
			DATA8 px = bitmap + x + y * width;
			int   i;
			p[0] |= FLAG_VISIT;
			for (i = 0; i < 4; i ++)
			{
				if (px[0] & (1 << i)) continue; /* this is outside of area or already been here */
				int nx = x + directions[i<<1];
				int ny = y + directions[(i<<1)+1];
				uint8_t cell = bitmap[nx+ny*width];
				if (cell & FLAG_VISIT) continue;
				if (cell & FLAG_PIXEL)
				{
					track[last++] = nx;
					track[last++] = ny;
					if (last == max) last = 0;
				}
				else /* we have a hole in the polygon */
				{
					polygonAdd(path, x, y, 1 << i, 1, bitmap, width, height);
				}
			}
		}
		#endif
		break;
	}
}

/* follow the starting point until a closed loop is found */
static void polygonAdd(PolyPath path, int x, int y, int initFlag, int inner, DATA8 bitmap, int width, int height)
{
	int startX, startY;
	int curX, curY, dir, flag;

	startX = curX = x;
	startY = curY = y;
	flag   = initFlag;
	switch (flag) {
	case FLAG_BOTTOM: curX ++; dir = SIDE_RIGHT; startY = ++ curY; break;
	case FLAG_RIGHT:  curX ++; dir = SIDE_TOP;   startY --; startX ++; break;
	case FLAG_TOP:    curX ++; dir = SIDE_RIGHT; break;
	case FLAG_LEFT:   curY ++; dir = SIDE_BOTTOM;
	}

	polyPathInit(path, startX, startY, inner ? 0x8000 : 0);
	polyPathAdd(path, flag);
	bitmap[x+y*width] |= flag;

	while (curX != startX || curY != startY)
	{
		int nx = x + directions[dir];
		int ny = y + directions[dir+1];
		if (nx < 0 || ny < 0 || nx >= width || ny >= height || (bitmap[nx+ny*width] & FLAG_PIXEL) == 0)
		{
			/* nothing in this neighbor: add a segment then */
			if (inner)
			{
				dir -= 2;
				flag >>= 1;
				if (dir < 0) dir = 6;
				if (flag == 0) flag = 8;
			}
			else
			{
				dir += 2;
				flag <<= 1;
				if (dir == 8) dir = 0;
				if (flag > FLAG_TOP) flag = 1;
			}
			DATA8 px = &bitmap[x+y*width];
			px[0] |= flag;
			polyPathAdd(path, flag);
			curX += directions[dir];
			curY += directions[dir+1];
		}
		else /* there is a pixel here */
		{
			int newdir = 0;
			x = nx;
			y = ny;
			switch (flag) {
			case FLAG_TOP:    ny --; newdir = SIDE_TOP;    break;
			case FLAG_BOTTOM: ny ++; newdir = SIDE_BOTTOM; break;
			case FLAG_LEFT:   nx --; newdir = SIDE_LEFT;   break;
			case FLAG_RIGHT:  nx ++; newdir = SIDE_RIGHT;  break;
			}
			if (nx < 0 || ny < 0 || nx >= width || ny >= height || (bitmap[nx+ny*width] & FLAG_PIXEL) == 0)
			{
				/* no pixel blocking the side: add to list */
				DATA8 px = &bitmap[x+y*width];
				px[0] |= flag;
				polyPathAdd(path, flag);
				curX += directions[dir];
				curY += directions[dir+1];
			}
			else /* pixel is blocking: change direction */
			{
				switch (flag) {
				case FLAG_TOP:
				case FLAG_BOTTOM: flag = dir == SIDE_RIGHT ? FLAG_LEFT : FLAG_RIGHT; break;
				case FLAG_LEFT:
				case FLAG_RIGHT: flag = dir == SIDE_BOTTOM ? FLAG_TOP : FLAG_BOTTOM;
				}
				dir = newdir;
			}
		}
	}
}
