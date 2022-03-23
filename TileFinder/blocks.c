/*
 * blocks.c : manage list of primitives and conversion to mesh for GL part to do its job.
 *
 * this part does a somewhat similar job to blocks.c in MCEdit, except we are using a 28 bytes per quad
 * for mesh instead of a 10 bytes per vertex.
 *
 * Written by T.Pierron, jan 2022.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "SIT.h"
#include "TileFinder.h"
#include "TileFinderGL.h"

static struct ListHead_t boxList;     /* Block */
extern struct Prefs_t    prefs;

static char texCoord[] = {
	0,0,    0,1,    1,1,    1,0,
	0,1,    1,1,    1,0,    0,0,
	1,1,    1,0,    0,0,    0,1,
	1,0,    0,0,    0,1,    1,1,
};

static uint8_t cubeIndices[6*4] = { /* face (quad) of cube: S, E, N, W, T, B */
	9, 0, 3, 6,    6, 3, 15, 18,     18, 15, 12, 21,     21, 12, 0, 9,    21, 9, 6, 18,      15, 3, 0, 12
};

extern uint8_t cubeVertex[];

Block blockGetNth(int nth)
{
	Block b;
	for (b = HEAD(boxList); b && nth > 0; nth --, NEXT(b));
	return b;
}

STRPTR blockParseFormat(STRPTR fmt)
{
	STRPTR p;
	float  sz[3];
	int    faces, n;
	Block  b;

	/* remove C comment from fmt */
	for (p = fmt; *p; p ++)
	{
		if (p[0] == '/' && p[1] == '*')
		{
			STRPTR end;
			for (end = p + 2; *end && ! (end[0] == '*' && end[1] == '/'); end ++);
			if (*end) end += 2;
			memset(p, ' ', end - p);
			p = end-1;
		}
		else if (p[0] == '+' && p[1] == 'B')
		{
			/* remove constant */
			STRPTR end;
			for (end = p + 2; *end && *end != ','; end ++);
			if (*end) end ++;
			*p++ = ','; memset(p, ' ', end - p);
			p = end - 1;
		}
	}

	/* check for simplified form first: this form covers 90% of block definitions in blockTable.js */
	for (n = 0, p = fmt; n < 12; n ++, p ++)
	{
		faces = strtoul(p, &p, 10);
		if (faces > 31 || *p != ',') break;
	}


	if ((faces = strtoul(p, &p, 10)) < (1<<13) /* tex rotation */ && p[0] == 0)
	{
		/* simplified form */
		DATA16 tex;
		b = blockAddDefaultBox();
		prefs.rot90 = 0;
		prefs.detail = 0;
		b->detailTex = TEX_CUBEMAP;

		for (n = 0, tex = b->texUV; n < 6; n ++, faces >>= 2)
		{
			char * coord = texCoord + (faces & 3) * 8;
			int u = strtoul(fmt, &fmt, 10); fmt ++;
			int v = strtoul(fmt, &fmt, 10); fmt ++;
			int j;
			b->texTrans[n] = 0x80 | (faces&3);

			for (j = 0; j < 8; j += 2, tex += 2)
			{
				tex[0] = (u + coord[j])   * TILE_SIZE;
				tex[1] = (v + coord[j+1]) * TILE_SIZE;
			}
		}
		return p;
	}

	if (strncmp(fmt, "FACES,", 6) == 0)
	{
		/* FACES,63,TEX_CUBEMAP,INVERT,SIZE,4,16,4,TR,6,0,6,ROT,0,0,0,ROTCAS,0,0,0,REF,0,0,0,ROT90,0,TEX,INV_FACEID */
		DATA16 tex;
		b = blockAddDefaultBox();

		b->faces = strtol(fmt + 6, &fmt, 10);
		while (fmt)
		{
			if (*fmt == ',') fmt ++;
			if (! ('A' <= *fmt && *fmt <= 'Z')) return NULL;
			p = strchr(fmt, ',');
			if (p) *p++ = 0; else p = NULL;
			switch (FindInList("TEX_CUBEMAP,TEX_DETAIL,TEX_INHERIT,SIZE,TR,ROT,ROTCAS,REF,ROT90,TEX,INVERT,INC_FACEID", fmt, 0)) {
			case 0: b->detailTex = TEX_CUBEMAP; break;
			case 1: b->detailTex = TEX_DETAIL; break;
			case 2: b->detailTex = TEX_CUBEMAP_INHERIT; break;
			case 3: /* primitive size */
				if (sscanf(p,  "%f,%f,%f%n", b->size, b->size+1, b->size+2, &n) < 3)
					return NULL;
				p += n+1;
				break;
			case 4: /* TR: traanslation */
				if (sscanf(p, "%f,%f,%f%n", b->trans, b->trans+1, b->trans+2, &n) < 3)
					return NULL;
				p += n+1;
				break;
			case 5: /* ROT: rotation */
				if (sscanf(p, "%f,%f,%f%n", b->rotate, b->rotate+1, b->rotate+2, &n) < 3)
					return NULL;
				p += n+1;
				break;
			case 6: /* ROTCAS: cascading rotation */
				if (sscanf(p, "%f,%f,%f%n", b->cascade, b->cascade+1, b->cascade+2, &n) < 3)
					return NULL;
				p += n+1;
				break;
			case 7: /* REF: rotation reference */
				if (sscanf(p, "%f,%f,%f%n", b->rotateFrom, b->rotateFrom+1, b->rotateFrom+2, &n) < 3)
					return NULL;
				p += n+1;
				b->rotateCenter = 0;
				break;
			case 8: /* ROT90: rotate 90deg step */
				prefs.rot90 = strtol(p, &p, 10);
				break;
			case 9: /* TEX: texture coord */
				switch (b->detailTex) {
				case TEX_DETAIL: faces = b->faces; break;
				case TEX_CUBEMAP: faces = 63; break;
				case TEX_CUBEMAP_INHERIT: faces = 0;
				}
				parse_tex:
				for (n = 0, tex = b->texUV; faces; faces >>= 1, tex += 8, n ++)
				{
					if (faces & 1)
					{
						int i, minU = 0, minV = 0, min = -1;
						for (i = 0; i < 8; i += 2)
						{
							int U = strtoul(p, &p, 10);
							int V = U / 513; U %= 513;
							if (*p == ',') p ++;
							tex[i] = U;
							tex[i+1] = V;
							if (min < 0 || (U < minU && V < minV))
								min = i, minU = U, minV = V;
						}
						b->texTrans[n] = 3-min/2;
						if (abs(tex[0]-tex[6]) == 16 && abs(tex[1]-tex[3]) == 16)
						{
							b->texTrans[n] |= 0x80;
						}
					}
				}
				if (*p != ',')
					return p;
				break;
			case 10: /* invert normals */
				b->faces |= BHDR_INVERTNORM;
				break;
			default:
				return NULL;
			}
			fmt = p;
		}
	}
	else /* old format -- deprecated */
	{
		faces = strtol(fmt, &fmt, 10);
		prefs.rot90 = (faces >> 9) & 3;

		if (*fmt == ',' && sscanf(fmt + 1, "%f,%f,%f%n", sz, sz+1, sz+2, &n) >= 3)
		{
			b = blockAddDefaultBox();

			memcpy(b->size, sz, sizeof sz);

			fmt += n+2;
			/* faces + invert normal */
			b->faces = faces & 127;
			b->detailTex = (faces & BHDR_CUBEMAP) ? (b->node.ln_Prev ? TEX_CUBEMAP_INHERIT : TEX_CUBEMAP) : TEX_DETAIL;

			if (sscanf(fmt, "%f,%f,%f%n", b->trans, b->trans+1, b->trans+2, &n) < 3)
			{
				blockRemove(b);
				return NULL;
			}
			fmt += n+1;
			if (sscanf(fmt, "%f,%f,%f%n", b->rotate, b->rotate+1, b->rotate+2, &n) < 3)
			{
				blockRemove(b);
				return NULL;
			}
			fmt += n+1;
			if (sscanf(fmt, "%f,%f,%f%n", b->cascade, b->cascade+1, b->cascade+2, &n) < 3)
			{
				blockRemove(b);
				return NULL;
			}

			fmt += n+1;
			faces &= 63;
			switch (b->detailTex) {
			case TEX_CUBEMAP:
				/* first row define all sides */
				faces = 63;
				break;
			case TEX_CUBEMAP_INHERIT:
				/* inherit by a previous primitive */
				return fmt;
			}
			p = fmt;
			goto parse_tex;
		}
	}
	return NULL;
}

/* create a box with default dimension */
Block blockAddDefaultBox(void)
{
	DATA16 p;
	Block  box, first;
	int    i, j;

	first = HEAD(boxList);
	box = calloc(sizeof *box, 1);
	box->size[0] = box->size[1] = box->size[2] = 16;
	box->faces = 63;
	box->detailTex = first ? (prefs.detail ? TEX_DETAIL : TEX_CUBEMAP) : TEX_CUBEMAP_INHERIT;
	box->rotateCenter = 1;

	sprintf(box->name, "Box %d", ++ prefs.nbBlocks);

	for (i = 0, p = box->texUV; i < DIM(box->texUV); )
	{
		for (j = 0; j < 8; j += 2, i += 2, p += 2)
		{
			p[0] = (prefs.defU + texCoord[j])   * TILE_SIZE;
			p[1] = (prefs.defV + texCoord[j+1]) * TILE_SIZE;
		}
	}

	ListAddTail(&boxList, &box->node);

	return box;
}

#define ROUND_SIZE(x)      (((x) + 1023) & ~1023)
static void AddBytes(STRPTR * ret, STRPTR buffer, int max)
{
	uint32_t size = 0;
	STRPTR   mem  = *ret;
	int      cap  = 0;
	if (mem)
	{
		memcpy(&size, mem, 4);
		cap = ROUND_SIZE(size);
	}
	else size = 1;

	if (size + max + 4 > cap)
	{
		cap = ROUND_SIZE(size + max + 4);
		mem = *ret = realloc(mem, cap);
	}
	memcpy(mem + 4 + size - 1, buffer, max + 1); size += max;
	memcpy(mem, &size, 4);
}
#undef ROUND_SIZE

/* copy model into clipboard */
void blockCopy(void)
{
	Block  b = HEAD(boxList);
	STRPTR p;
	STRPTR mem;
	TEXT   block[512];
	int    j;
	for (mem = NULL; b; NEXT(b))
	{
		DATA16 tex;
		int    faces;
		/* faces: faces:0-5, inv normals:6, cubeMap:7, continue:8, rot90:9-10, detailFaces:11-16, incFaceId:17 */
		faces = b->faces | (!b->detailTex << 7) | ((b->node.ln_Next != NULL) << 8) | (prefs.rot90 << 9);
		//if (! detail && b->detailTex) faces |= b->detailFaces<<11;
		p = block;
		p += sprintf(p, "%d", faces);
		p += sprintf(p, ",%g,%g,%g", b->size[0], b->size[1], b->size[2]);
		p += sprintf(p, ",%g,%g,%g", b->trans[0], b->trans[1], b->trans[2]);
		p += sprintf(p, ",%g,%g,%g", b->rotate[0], b->rotate[1], b->rotate[2]);
		p += sprintf(p, ",%g,%g,%g", b->cascade[0], b->cascade[1], b->cascade[2]);
		faces &= 63;
		switch (b->detailTex) {
		case TEX_DETAIL:
			for (tex = b->texUV, faces &= 63; faces; faces >>= 1)
			{
				if (faces & 1)
				{
					for (j = 0; j < 8; j += 2, tex += 2)
						p += sprintf(p, ",%d", tex[0] + tex[1] * 513);
				}
				else tex += 8;
			}
			break;
		case TEX_CUBEMAP:
			for (tex = b->texUV, faces = 0; faces < 6; faces ++)
				for (j = 0; j < 8; j += 2, tex += 2)
					p += sprintf(p, ",%d", tex[0] + tex[1] * 513);
		}
		strcpy(p, ",\n");
		AddBytes(&mem, block, p + 2 - block);
	}
	if (mem)
	{
		SIT_CopyToClipboard(mem + 4, -1);
		free(mem);
	}
}

/* parse model from clipboard */
Bool blockPaste(void)
{
	ListHead boxes;
	TEXT     extract[20];
	int      detail = prefs.detail;
	int      rot90  = prefs.rot90;
	int      size   = 0;
	STRPTR   clip   = SIT_GetFromClipboard(&size);
	int      count  = prefs.nbBlocks;

	if (size > 16)
	{
		CopyString(extract, clip, 16);
		strcat(extract, "...");
	}
	else if (size > 0)
	{
		strcpy(extract, clip);
	}

	boxes = boxList;
	prefs.nbBlocks = 0;
	ListNew(&boxList);

	if (IsDef(clip))
		prefs.detail = (atoi(clip) & 128) == 0;
	while (IsDef(clip))
	{
		while (isspace(*clip)) clip ++;
		if (*clip == 0) break;
		clip = blockParseFormat(clip);
	}
	if (clip && prefs.nbBlocks > 0)
		return True;

	/* parsing failed: restore previous state */
	prefs.nbBlocks = count;
	prefs.detail = detail;
	prefs.rot90 = rot90;

	ListNode * node;
	while ((node = ListRemHead(&boxList))) free(node);
	boxList = boxes;

	for (clip = extract; *clip; clip ++)
		if (*clip == '\t') *clip = ' ';

	if (size == 0)
		SIT_Log(SIT_INFO, "Clipboard does not contain a block model");
	else
		SIT_Log(SIT_INFO, "Clipboard does not contain a block model:\n\n%s", extract);

	return False;
}

/* remove from list, return ref to first block name that has been adjusted (will need to update listview) */
int blockRemove(Block b)
{
	Block next = (Block) b->node.ln_Next;

	ListRemove(&boxList, &b->node);
	free(b);
	prefs.nbBlocks --;

	if (next)
	{
		/* auto-remove following box */
		int i, modif;
		for (i = 0, b = HEAD(boxList); b != next; NEXT(b), i ++);
		for (modif = -1; b; NEXT(b), i ++)
		{
			if (modif < 0) modif = i;
			sprintf(b->name, "Box %d", i+1);
		}
		return modif;
	}
	return -1;
}

void blockDeleteAll(void)
{
	Block b, next;

	for (b = next = HEAD(boxList); b; NEXT(next), free(b), b = next);

	ListNew(&boxList);

	prefs.nbBlocks = 0;
}

void blockResetTex(Block b)
{
	DATA16 p;
	int    i, j;
	for (i = 0, p = b->texUV; i < DIM(b->texUV); )
	{
		for (j = 0; j < 8; j += 2, i += 2, p += 2)
		{
			p[0] = (prefs.defU + texCoord[j])   * TILE_SIZE;
			p[1] = (prefs.defV + texCoord[j+1]) * TILE_SIZE;
		}
	}
}

void blockDelAllTex(void)
{
	Block b;
	for (b = HEAD(boxList); b; NEXT(b))
		blockResetTex(b);
}

void blockRotateTex(Block b, int face)
{
	uint16_t tmp[2];
	DATA16   tex = b->texUV + face * 8;
	memcpy(tmp, tex, sizeof tmp);
	memmove(tex, tex + 2, sizeof *tex * 6);
	memcpy(tex + 6, tmp, sizeof tmp);
}

void blockMirrorTex(Block b, int face)
{
	uint16_t tmp;
	DATA16   tex = b->texUV + face * 8;
	if (tex[0] != tex[6])
	{
		swap_tmp(tex[0], tex[6], tmp);
		swap_tmp(tex[2], tex[4], tmp);
	} else {
		swap_tmp(tex[1], tex[7], tmp);
		swap_tmp(tex[3], tex[5], tmp);
	}
}

/* original tex coord are aligned on tile boundary: reduce them to vertex boundary */
static DATA16 blockAdjustUV(float vertex[12], DATA16 tex)
{
	static uint8_t Ucoord[] = {0, 2, 0, 2, 0, 0};
	static uint8_t Vcoord[] = {1, 1, 1, 1, 2, 2};
	static uint8_t revers[] = {0, 1, 1, 0, 2, 0};
	static uint8_t norm2face[] = {1, 3, 4, 5, 0, 2};
	static uint16_t texBuffer[8];
	vec4   v1 = {vertex[3] - vertex[0], vertex[4] - vertex[1], vertex[5] - vertex[2], 1}; // v1 = V1 - V0
	vec4   v2 = {vertex[6] - vertex[0], vertex[7] - vertex[1], vertex[8] - vertex[2], 1}; // v2 = V2 - V0
	vec4   norm;
	DATA16 p;
	int    dir, i, U, V;

	vecCrossProduct(norm, v1, v2);

	/* convert norm into S,E,N,W,T,B direction (<dir>)*/
	dir = 0; v1[0] = norm[0];
	if (fabsf(v1[0]) < fabsf(norm[VY])) dir = 2, v1[0] = norm[VY];
	if (fabsf(v1[0]) < fabsf(norm[VZ])) dir = 4, v1[0] = norm[VZ];
	if (v1[0] < 0) dir ++;

	dir  = norm2face[dir];
	U    = Ucoord[dir];
	V    = Vcoord[dir];

	for (p = texBuffer, i = 0; i < 4; i ++, p += 2, vertex += 3)
	{
		float val = vertex[V] + 0.5;
		if (revers[dir] & 2) val = 1 - val;
		float pt1[] = {tex[2] + (tex[0]-tex[2]) * val, tex[3] + (tex[1] - tex[3]) * val};
		float pt2[] = {tex[4] + (tex[6]-tex[4]) * val, tex[5] + (tex[7] - tex[5]) * val};

		val = vertex[U] + 0.5;
		if (revers[dir] & 1) val = 1 - val;
		p[0] = roundf(pt1[0] + (pt2[0] - pt1[0]) * val);
		p[1] = roundf(pt1[1] + (pt2[1] - pt1[1]) * val);
	}

	return texBuffer;
}

#define BASEVTX            2048
#define ORIGINVTX          15360
#define MIDVTX             (1 << 13)
#define RELDX(x)           ((x) + MIDVTX - X1)
#define RELDY(x)           ((x) + MIDVTX - Y1)
#define RELDZ(x)           ((x) + MIDVTX - Z1)
#define VERTEX(x)          ((int) ((x) * BASEVTX) + ORIGINVTX)

/* chunk vertex data */
#define FLAG_TEX_KEEPX     (1 << 12)
#define FLAG_DUAL_SIDE     (1 << 13)

static uint8_t opposite[] = {2*4, 3*4, 0*4, 1*4, 5*4, 4*4};

/* convert list of block into vertex data for blocks shader (using 28 bytes per quad format) */
void blockGenVertexBuffer(void)
{
	DATA32 out = renderMapBuffer();
	DATA16 tex;
	float  trans[3];
	int    count = 0;
	mat4   rotation, rot90, rotCascade, tmp;
	int    i, j, nth, nbRot, nbRotCas, faces;
	Block  b = HEAD(boxList);

	matIdent(rotCascade);
	for (nbRotCas = 0, nth = 0; b; NEXT(b), nth ++)
	{
		void blockGetVertex(vec4 vtx, DATA8 point)
		{
			vtx[VX] = point[VX] * b->size[VX] / 16;
			vtx[VY] = point[VY] * b->size[VY] / 16;
			vtx[VZ] = point[VZ] * b->size[VZ] / 16;
			if (nbRot > 0)
			{
				float tr[3];
				if (b->rotateCenter > 0)
				{
					tr[VX] = b->size[VX] / 32;
					tr[VY] = b->size[VY] / 32;
					tr[VZ] = b->size[VZ] / 32;
				}
				else
				{
					tr[VX] = b->rotateFrom[VX] / 16 - 0.5f - trans[VX];
					tr[VY] = b->rotateFrom[VY] / 16 - 0.5f - trans[VY];
					tr[VZ] = b->rotateFrom[VZ] / 16 - 0.5f - trans[VZ];
				}
				vecSub(vtx, vtx, tr);
				matMultByVec3(vtx, rotation, vtx);
				vecAdd(vtx, vtx, tr);
			}
			vtx[0] += trans[0];
			vtx[1] += trans[1];
			vtx[2] += trans[2];
			if (nbRotCas > 0)
				matMultByVec3(vtx, rotCascade, vtx);
			if (prefs.rot90 > 0)
				matMultByVec3(vtx, rot90, vtx);
		}
		for (i = 0; i < 3; i ++)
		{
			if (b->cascade[i] != 0)
			{
				matRotate(tmp, DEG2RAD(b->cascade[i]), i);
				matMult(rotCascade, rotCascade, tmp);
				nbRotCas ++;
			}
		}

		/* rotation of a single box */
		nbRot = 0;
		if (b->rotate[0] != 0)
			matRotate(rotation, DEG2RAD(b->rotate[0]), 0), nbRot ++;
		else
			matIdent(rotation);

		if (b->rotate[1] != 0)
		{
			matRotate(tmp, DEG2RAD(b->rotate[1]), 1), nbRot ++;
			if (nbRot == 1)
				memcpy(rotation, tmp, sizeof tmp);
			else
				matMult(rotation, rotation, tmp);
		}

		if (b->rotate[2] != 0)
		{
			matRotate(tmp, DEG2RAD(b->rotate[2]), 2), nbRot ++;
			if (nbRot == 1)
				memcpy(rotation, tmp, sizeof tmp);
			else
				matMult(rotation, rotation, tmp);
		}

		/* global rotation */
		switch (prefs.rot90) {
		case 1: matRotate(rot90, M_PI_2, 1); break;
		case 2: matRotate(rot90, M_PI, 1); break;
		case 3: matRotate(rot90, M_PI+M_PI_2, 1);
		}
		trans[0] = b->trans[0] / 16 - 0.5;
		trans[1] = b->trans[1] / 16 - 0.5;
		trans[2] = b->trans[2] / 16 - 0.5;

		int invert = b->faces & BHDR_INVERTNORM;
		Block ref;

		if (b->detailTex == TEX_CUBEMAP_INHERIT)
		{
			for (ref = b; ref->node.ln_Prev && ref->detailTex != TEX_CUBEMAP; PREV(ref));
		}
		else ref = b;

		for (i = j = 0, tex = ref->texUV, faces = b->faces; i < DIM(cubeIndices); i += 4, tex += 8, faces >>= 1)
		{
			uint16_t X1, Y1, Z1;
			uint16_t X2, Y2, Z2;
			uint16_t X3, Y3, Z3;
			uint16_t texU, texV;
			uint8_t  norm = invert ? opposite[i>>2] : i;
			float    vertex[12];
			DATA16   texUV = tex;

			if ((faces & 1) == 0)
				continue;

			/* generate one quad */
			blockGetVertex(vertex,   cubeVertex + cubeIndices[i]);
			blockGetVertex(vertex+3, cubeVertex + cubeIndices[i+1]);
			blockGetVertex(vertex+6, cubeVertex + cubeIndices[i+2]);
			blockGetVertex(vertex+9, cubeVertex + cubeIndices[i+3]);

			if (invert)
			{
				memcpy(tmp, vertex,   12); memcpy(vertex,   vertex+9, 12); memcpy(vertex+9, tmp, 12);
				memcpy(tmp, vertex+3, 12); memcpy(vertex+3, vertex+6, 12); memcpy(vertex+6, tmp, 12);
			}

			if (ref->detailTex != TEX_DETAIL)
				texUV = blockAdjustUV(vertex, tex);

			texU = texUV[0]; if (texU == 512)  texU = 511;
			texV = texUV[1]; if (texV == 1024) texV = 1023;

			X1 = VERTEX(vertex[VX+9]);   X2 = VERTEX(vertex[VX]);   X3 = VERTEX(vertex[VX+6]);
			Y1 = VERTEX(vertex[VY+9]);   Y2 = VERTEX(vertex[VY]);   Y3 = VERTEX(vertex[VY+6]);
			Z1 = VERTEX(vertex[VZ+9]);   Z2 = VERTEX(vertex[VZ]);   Z3 = VERTEX(vertex[VZ+6]);

			out[0] = X1 | (Y1 << 16);
			out[1] = Z1 | (RELDX(X2) << 16) | ((texV & 512) << 21);
			out[2] = RELDY(Y2) | (RELDZ(Z2) << 14);
			out[3] = RELDX(X3) | (RELDY(Y3) << 14);
			out[4] = RELDZ(Z3) | (texU << 14) | (texV << 23);
			out[5] = ((texUV[4] + 128 - texU) << 16) |
			         ((texUV[5] + 128 - texV) << 24) | (norm << 7);
			/* face id + primitive id for selection (should be light values here) */
			out[6] = ((i+4) >> 2) | (nth << 3);

			/* flip tex */
			if (tex[0] == tex[6]) out[5] |= FLAG_TEX_KEEPX;

			count ++;
			out += VERTEX_INT_SIZE;
		}
	}

	fprintf(stderr, "quad count = %d\n", count);
	renderUnmapBuffer(count);
}

/* convert cube map coord to detail tex coord */
void blockCubeMapToDetail(Block b)
{
	DATA16 tex;
	int    i, j;

	if (b->detailTex == TEX_CUBEMAP_INHERIT)
	{
		Block prev;
		for (prev = (Block) b->node.ln_Prev; prev; PREV(prev))
		{
			if (prev->detailTex == TEX_CUBEMAP)
			{
				memcpy(b->texUV, prev->texUV, sizeof b->texUV);
				break;
			}
		}
	}

	for (i = 0, tex = b->texUV; i < 6; i ++, tex += 8)
	{
		float vertex[12];
		vec   vtx;
		DATA8 point;

		for (j = 0, vtx = vertex; j < 3; j ++, vtx += 3)
		{
			point = cubeVertex + cubeIndices[i * 4 + j];
			vtx[VX] = point[VX] * b->size[VX] / 16 + b->trans[0] / 16 - 0.5;
			vtx[VY] = point[VY] * b->size[VY] / 16 + b->trans[1] / 16 - 0.5;
			vtx[VZ] = point[VZ] * b->size[VZ] / 16 + b->trans[2] / 16 - 0.5;
		}
		memcpy(tex, blockAdjustUV(vertex, tex), 8 * sizeof *tex);
	}
}


/* return True if tex coord of Block can use simplified form (ie: tile coord instead of tex coord) */
Bool blockCanUseTileCoord(Block b)
{
	DATA16 coord;
	char * texBase;
	int    i, j, rotate;

	if ((b->faces & 63) != 63) return False;
	for (coord = b->texUV, i = 0, b->rotateUV = 0; i < 6; i ++, coord += 8)
	{
		/* must multiple of TILE_SIZE */
		if ((coord[0] & TILE_MASK) || (coord[1] & TILE_MASK)) return False;
		for (rotate = 0, texBase = texCoord; rotate < 4; rotate ++, texBase += 8)
		{
			int Ubase = texBase[0];
			int Vbase = texBase[1];
			for (j = 2; j < 8; j += 2)
			{
				if (coord[0] + (texBase[j]   - Ubase) * TILE_SIZE != coord[j] ||
					coord[1] + (texBase[j+1] - Vbase) * TILE_SIZE != coord[j+1]) break;
			}
			if (j == 8)
			{
				b->rotateUV |= rotate << (i * 2);
				goto continue_loop;
			}
		}
		return False;
		continue_loop:;
	}
	return True;
}

/* set texture for one face (ui callback) */
void blockSetFaceCoord(Block b, int face, int tileX, int tileY)
{
	if (b == NULL) return;
	DATA16 coord;
	DATA8 texBase = texCoord + ((b->rotateUV >> face*2) & 3) * 8;
	int i;

	for (i = 0, coord = b->texUV + face * 8; i < 4; i ++, coord += 2, texBase += 2)
	{
		coord[0] = (tileX + texBase[0]) * TILE_SIZE;
		coord[1] = (tileY + texBase[1]) * TILE_SIZE;
	}
}

void blockSetFaceTexCoord(Block b, int face, int * tex)
{
	if (b == NULL) return;
	DATA16 coord;
	DATA8  texBase = texCoord;
	int    rect[4], i;

	memcpy(rect, tex, sizeof rect);
	rect[2] += rect[0];
	rect[3] += rect[1];

	for (i = 0, coord = b->texUV + face * 8; i < 4; i ++, coord += 2, texBase += 2)
	{
		coord[0] = texBase[0] == 0 ? rect[0] : rect[2];
		coord[1] = texBase[1] == 0 ? rect[1] : rect[3];
	}
}

void blockGetTexRect(Block b, int face, int * outXYWH)
{
	Block first = HEAD(boxList);
	if (b != first && b->detailTex == TEX_CUBEMAP_INHERIT)
	{
		while (b->node.ln_Prev && b->detailTex != TEX_CUBEMAP)
			PREV(b);
	}

	DATA16 tex = b->texUV + face * 8;
	int    rect[] = {1e6, 1e6, 0, 0};
	int    i;

	if ((tex[0] >= prefs.defU * TILE_SIZE && tex[1] >= prefs.defV * TILE_SIZE) || face >= 6)
	{
		rect[0] = rect[1] = 0;
	}
	else for (i = 0; i < 4; i ++, tex += 2)
	{
		if (rect[0] > tex[0]) rect[0] = tex[0];
		if (rect[1] > tex[1]) rect[1] = tex[1];
		if (rect[2] < tex[0]) rect[2] = tex[0];
		if (rect[3] < tex[1]) rect[3] = tex[1];
	}
	rect[2] -= rect[0];
	rect[3] -= rect[1];
	memcpy(outXYWH, rect, sizeof rect);
}
