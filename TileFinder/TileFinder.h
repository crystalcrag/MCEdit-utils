/*
 * TileFinder.h : public function for main interface
 *
 * Written by T.Pierron, Feb 2020
 */

#ifndef TILEFINDER_H
#define TILEFINDER_H

#include "utils.h"

typedef struct Block_t *    Block;
typedef struct Anim_t *     Anim;
typedef struct TexBank_t *  TexBank;


/* from TileFinderUI.c */
void uiCreate(SIT_Widget app);
void uiSelectFace(int faceId);
void uiSetFaceCoord(int cellX, int cellY);
void uiSetFaceTexCoord(int * texCoords);
void uiYesNo(SIT_Widget parent, STRPTR msg, SIT_CallProc cb, Bool yesNo);

/* from blocks.c */
STRPTR blockParseFormat(STRPTR format);
Block  blockAddDefaultBox(void);
Block  blockGetNth(int nth);
void   blockCopy(void);
Bool   blockPaste(void);
int    blockRemove(Block);
void   blockDeleteAll(void);
void   blockGenVertexBuffer(void);
Bool   blockCanUseTileCoord(Block);
void   blockSetFaceCoord(Block, int face, int cellX, int cellY);
void   blockSetFaceTexCoord(Block, int face, int * tex);
void   blockResetTex(Block);
void   blockDelAllTex(void);
void   blockRotateTex(Block, int face);
void   blockMirrorTex(Block, int face);
void   blockGetTexRect(Block, int face, int * outXYWH);
void   blockCubeMapToDetail(Block b);

struct Block_t
{
	ListNode node;
	uint8_t  name[32];
	uint16_t texUV[48];
	uint8_t  texTrans[6];
	uint16_t rotateUV;
	float    size[3];            /* size (px) */
	float    rotate[3];          /* rotation (only on this primitive */
	float    trans[3];           /* translation (px) */
	float    rotateFrom[3];      /* rotation center (if rotateCenter > 0) */
	float    cascade[3];         /* rotation that carries to next primitive */
	uint8_t  faces;              /* S, E, N, W, T, B bitfield */
	uint8_t  detailTex;          /* TEX_* */
	uint8_t  rotateCenter;       /* 0 or 1 */
	uint8_t  ref;                /* primitive used as reference (*) */
	uint8_t  incFaceId;          /* informative, used by MCEdit only */
	uint8_t  custName;
	Anim     anim;
};

enum
{
	TEX_CUBEMAP = 0,
	TEX_CUBEMAP_INHERIT = 1,
	TEX_DETAIL = 2
};

struct Anim_t
{
	float   endSize[3];
	float   endRotate[3];
	float   endTrans[3];
	float   points[16];
	uint8_t curve;
	uint8_t nbPt;
};

struct Prefs_t
{
	ListHead banks;
	int  width, height;
	int  detail, bbox;
	int  defU, defV;
	int  nbBlocks, rot90;
	int  boxSel, anim;
	TEXT lastTex[128];
	APTR nvg;
};

struct TexBank_t
{
	ListNode node;
	TEXT     path[1];
};

#define MIN_IMAGE_SIZE           64        /* min size in px that image will be zoomed out */
#define TILE_SIZE                16        /* size in px of tiles in main texture atlas */
#define TILE_MASK                (TILE_SIZE-1)

#define BHDR_FACESMASK           63
#define BHDR_INVERTNORM          0x40
#define BHDR_DUALSIDE            0x80
#define BHDR_CUBEMAP             0x80
#define BHDR_ROT90SHIFT          9
#define BHDR_INCFACEID           (1<<17)

#ifdef __GNUC__
 #define COMPILER     "gcc " TOSTRING(__GNUC__) "." TOSTRING(__GNUC_MINOR__) "." TOSTRING(__GNUC_PATCHLEVEL__)
 #ifdef WIN32
  #if __x86_64__
   #define PLATFORM   "MS-Windows-x64"
  #else
   #define PLATFORM   "MS-Windows-x86"
  #endif
 #elif LINUX
  #if __x86_64__
   #define PLATFORM   "GNU-Linux-x64"
  #else
   #define PLATFORM   "GNU-Linux-x86"
  #endif
 #else
  #if __x86_64__
   #define PLATFORM   "Unknown-x64"
  #else
   #define PLATFORM   "Unknown-x86"
  #endif
 #endif
#else
 #define COMPILER    "unknown compiler"
 #ifdef WIN32
  #define PLATFORM   "MS-Windows-x86"
 #else
  #define PLATFORM   "GNU-Linux-x86"
 #endif
#endif

#endif
