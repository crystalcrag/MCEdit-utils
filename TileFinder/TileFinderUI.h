/*
 * TileFinderUI.h : mostly private datatypes used to manage user interface of TileFinder.
 *
 * Written by T.Pierron, jan 2022
 */


#ifndef TILE_FINDERUI_H
#define TILE_FINDERUI_H


struct MainCtrl_t
{
	NVGcontext * nvgCtx;
	SIT_Widget list, app, full, manual, lab90;
	SIT_Widget tex, texUV, coords, model;
	SIT_Widget faces[8], radio[8], subdet;
	SIT_Widget center[3], centerCB, anim;
	SIT_Widget active;
	mat4       rotation;
	int        faceEdit, lastFaceSet;
	int        texUVMapId, nvgImgId;
	int        showActive, detailMode;
	int        shiftDX, shiftDY;
	uint8_t    cancelEdit;
};

typedef struct ViewImage_t *    ViewImage;

struct ViewImage_t
{
	int   imgId;                  /* nanovg id */
	int   imgWidth, imgHeight;    /* image size */
	int   dstWidth, dstHeight;    /* canvas size */
	int   rect[4];                /* location where it is displayed (X,Y,W,H) */
	int   selection[4];           /* box selected using image resolution (X,Y,W,H) */
	int   zoomIdx;                /* lock magnification to discrete steps */
	int   selInit[2];             /* coord of first click in selection */
	float zoomFact;               /* multiply image size by this to get display size */
	int   zoomChanged;
};

enum
{
	MENU_COPY,
	MENU_PASTE,
	MENU_RESETVIEW,
	MENU_RESETTEX,
	MENU_ROT90TEX,
	MENU_MIRRORTEX,
	MENU_COPYTEX,
	MENU_CLEARTEX,
	MENU_LOCATETEX,
	MENU_PREVBOX,
	MENU_NEXTBOX,
	MENU_PREVSIDE,
	MENU_NEXTSIDE,
	MENU_DUPBOX,
	MENU_RENAME,
	MENU_ANIMATE,
	MENU_ABOUT
};

#endif
