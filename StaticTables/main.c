/*
 * staticMCEditTables.c: generate some tables from MCEdit, because they take less
 *		space in binary form than filling them programmatically.
 *
 * Written by T.Pierron.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef uint8_t *   DATA8;

uint16_t faceCnx[64];
uint16_t hasCnx[64];
uint8_t  chunkOffsets[27];
uint32_t chunkNeighbors[8];

#define PUTC(ch)    putc(ch, stdout)

static uint8_t boxPts[] = {
	/* coords X, Z, Y */
	0, 0, 0,
	1, 0, 0,
	0, 1, 0,
	1, 1, 0,
	0, 0, 1,
	1, 0, 1,
	0, 1, 1,
	1, 1, 1,
};

static uint8_t cubeVertex[] = { /* 8 vertices of a 1x1x1 cube */
	0,0,1,  1,0,1,  1,1,1,  0,1,1,
	0,0,0,  1,0,0,  1,1,0,  0,1,0,
};
static uint8_t cubeIndices[6*4] = { /* face (quad) of cube: S, E, N, W, T, B */
	9, 0, 3, 6,    6, 3, 15, 18,     18, 15, 12, 21,     21, 12, 0, 9,    21, 9, 6, 18,      0, 12, 15, 3
};

int main(int nb, char * argv[])
{
	int i, x, y, pos;

	#if 0
	// XXX what's that for?
	printf("uint8_t hasVoxelBySide[27] = {\n\t");
	memset(chunkOffsets, 0, 27);
	for (i = 0 ; i < 4; i ++)
	{
		for (x = 0; x < 4; x ++)
		{
			for (y = 0; y < 4; y ++)
			{
				#define MOD2(x)    ((x)&1)
				pos = (x + 1)/2 + (i + 1)/2*3 + (y + 1)/2*9;
				chunkOffsets[pos] |= 1 << (MOD2(x+1) + MOD2(y+1)*4 + MOD2(i+1)*2);
				#undef MOD2
			}
		}
	}
	for (i = 0 ; i < 27; i ++)
	{
		printf("%d,", chunkOffsets[i]);
	}
	printf("\n};\n\n");
	#endif

	printf("#define DXYZ(dx,dy,dz)       (dx+1) | ((dy+1)<<2) | ((dz+1)<<4)\n");
	printf("uint8_t sampleOffset[48] = {\n");

	DATA8 indices;
	for (i = 0, indices = cubeIndices; i < 6; i ++)
	{
		static uint8_t axis1[] = {0, 2, 0, 2, 0, 0};
		static uint8_t axis2[] = {1, 1, 1, 1, 2, 2};
		static int8_t normal[] = {2, 0,-3,-1, 1,-2};
		char dxyz[3], norm;

		norm = normal[i];
		for (x = 0; x < 4; x ++, indices ++)
		{
			DATA8 vertex = cubeVertex + *indices;
			uint8_t axis;
			axis = axis1[i]; dxyz[axis] = vertex[axis] == 0 ? -1 : 1;
			axis = axis2[i]; dxyz[axis] = vertex[axis] == 0 ? -1 : 1;
			axis = norm < 0 ? -norm-1 : norm; dxyz[axis] = norm < 0 ? -1 : 1;
			printf("DXYZ(%2d,%2d,%2d), ", -vertex[0], -vertex[1], -vertex[2]);
			printf("DXYZ(%2d,%2d,%2d), ", dxyz[0], dxyz[1], dxyz[2]);
		}
		putc('\n', stdout);
	}

	printf("};\n#undef DXYZ\n");

	memset(chunkOffsets, 0xff, sizeof chunkOffsets);

	/* half-block meshing */
	printf("uint8_t blockIndexToXYZ[27] = {\n\t");
	for (i = 0; i < 27; i ++)
	{
		printf("%d,", (i % 3) + ((i / 9)<<4) + ((i / 3) % 3 << 2));
	}
	printf("\n};\n\n");

	/* frustum culling tables (first one is only needed to compute the others) */
	chunkOffsets[13] = 0;

	for (i = 0, pos = 1; i < 27; i ++)
	{
		static int8_t dirs[] = { /* ordered S, E, N, W, T, B */
			 0,  0,  1, 0,
			 1,  0,  0, 0,
			 0,  0, -1, 0,
			-1,  0,  0, 0,
			 0,  1,  0, 0,
			 0, -1,  0, 0,
		};
		int8_t * dir;
		int j = (DATA8) memchr(chunkOffsets, i, 27) - chunkOffsets;
		int8_t x = j%3;
		int8_t z = (j/3)%3;
		int8_t y = j/9;

		for (j = 0, dir = dirs; j < 6; j ++, dir += 4)
		{
			int8_t x2 = x + dir[0];
			int8_t y2 = y + dir[1];
			int8_t z2 = z + dir[2];
			if (0 <= x2 && x2 <= 2 && 0 <= y2 && y2 <= 2 && 0 <= z2 && z2 <= 2)
			{
				x2 += z2 * 3 + y2 * 9;
				if (chunkOffsets[x2] == 255)
					chunkOffsets[x2] = pos ++;
			}
		}
	}

	/* frustum culling tables */
	printf(".neighbors = {\n");
	for (i = 0; i < 8; i ++)
	{
		DATA8 ptr = boxPts + i * 3;
		int   neighbor = 0;
		/* 7 boxes sharing that vertex (excluding the one we are in: 0,0,0) */
		for (x = 1; x < 8; x ++)
		{
			int8_t xoff = (x&1);
			int8_t zoff = (x&2)>>1;
			int8_t yoff = (x&4)>>2;

			if (ptr[0] == 0) xoff = -xoff;
			if (ptr[2] == 0) yoff = -yoff;
			if (ptr[1] == 0) zoff = -zoff;

			/* offset of neighbor chunk (-1, 0 or 1) */
			neighbor |= 1 << chunkOffsets[(xoff+1) + (zoff+1)*3 + (yoff+1)*9];
			//printf("%d ", chunkOffsets[(xoff+1) + (zoff+1)*3 + (yoff+1)*9]);
		}
		/* 0 is center: we don't care because that chunk is guaranted to be included in frustum, and therefore simply start at 1 (>> 1) */
		printf("0x%08x,\n", neighbor >> 1);
	}

	printf("\n};\n\n");
	printf(".chunkOffsets = {\n\t");

	for (i = 0; i < 27; i ++)
	{
		static uint8_t dirs[] = {8, 0, 2, 4, 0, 1, 32, 0, 16};
		pos = (DATA8) memchr(chunkOffsets, i, 27) - chunkOffsets;
		int8_t x = pos%3;
		int8_t z = (pos/3)%3;
		int8_t y = pos/9;
		printf("%d,", dirs[x] | dirs[z+3] | dirs[y+6]);
	}

	printf("\n};\n\n");

	for (i = 0; i < 27; i += 3)
	{
		fprintf(stderr, "%2d %2d %2d\n", chunkOffsets[i], chunkOffsets[i+1], chunkOffsets[i+2]);
	}

	/* faceCnx: cave culling */

	/* given a S,E,N,W,T,B bitfield, will give what face connections we can reach */
	printf("uint8_t hasCnx[] = {\n");
	for (i = nb = 0; i < 64; i ++)
	{
		if (__builtin_popcount(i) == 2)
		{
			int bit;
			for (x = 1, pos = 0, y = 5, bit = i; (bit & 1) == 0; bit >>= 1, x <<= 1, pos += y, y --);
			for (bit >>= 1; (bit & 1) == 0; pos ++, bit >>= 1);
			hasCnx[i] = 1 << pos;
		}
		else hasCnx[i] = 0;
		if (nb == 0) PUTC('\t'), nb = 4;
		nb += printf("%d,", hasCnx[i]);
		if (nb > 70) PUTC('\n'), nb = 0;
	}

	/* used by cave culling function */
	printf("\n};\n\nuint8_t faceCnx[] = {\n");

	for (i = nb = 0; i < 64; i ++)
	{
		/* first cnx */
		int bit;
		for (pos = 0, bit = 1, y = i; y > 0; y >>= 1, bit <<= 1)
		{
			if ((y & 1) == 0) continue;
			int rem;
			for (rem = y >> 1, x = bit<<1; rem > 0; rem >>= 1, x <<= 1)
			{
				if (rem & 1) pos |= hasCnx[bit | x];
			}
		}
		if (nb == 0) PUTC('\t'), nb = 4;
		nb += printf("%d,", pos);
		if (nb > 70) PUTC('\n'), nb = 0;
	}
	printf("\n};\n\n");

	/* chunk update */
	printf("static uint32_t chunkNearby[64] = {\n");
	for (i = nb = 0; i < 64; i ++)
	{
		/* cannot have opposite direction at the same time */
		if ((i & 5) != 5 && (i & 10) != 10 && (i & 48) != 48)
		{
			uint8_t bits[6];
			uint32_t nearby = 0;
			memset(bits, 0, sizeof bits);
			for (x = pos = 0, y = 1; y <= 32; y <<= 1)
				if (i & y) bits[x++] = y;
			/* need to enumerate all combinations of bits set to 1 in <i> */
			for (x = 1 << __builtin_popcount(i), y = 1; y < x; y ++)
			{
				uint8_t val, j, bit;
				for (bit = y, j = val = 0; bit > 0; bit >>= 1, j ++)
					if (bit & 1) val |= bits[j];
				/* val is a 6 bits number with at most 3 bits set (S, E, N, W, T, B bitfield) */
				/* need to convert that number to [0-26] */
				j = 13;
				if (val & 2)  j ++;
				if (val & 8)  j --;
				if (val & 1)  j += 3;
				if (val & 4)  j -= 3;
				if (val & 16) j += 9;
				if (val & 32) j -= 9;
				if (j == 13) continue;
				if (j >= 13)  j --;
				nearby |= 1 << j;
			}
			nb += printf("0x%08x, ", nearby);
		}
		else nb += printf("0x00000000, ");
		if (nb > 70) printf("\n"), nb = 0;
	}
	printf("\n};");

	return 0;
}
