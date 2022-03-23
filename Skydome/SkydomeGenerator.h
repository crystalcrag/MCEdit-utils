/*
 * main.h: datatype for main interface
 *
 * Written by T.Pierron, jan 2022.
 */

#ifndef SKYDOME_MAIN_H
#define SKYDOME_MAIN_H

typedef struct Image_t       Image_t;
typedef struct Image_t *     Image;
typedef NVGcontext *         NVGCTX;

struct Image_t
{
	DATA8 bitmap;
	int   width, height;
	int   stride, spp;
	int   nvgId;
};

struct SkydomePrivate_t
{
	NVGCTX     nvgCtx;
	SIT_Widget editor;
	SIT_Widget reftex, texture;
	SIT_Widget inpath, outpath;
	SIT_Widget save, ctrlpt;
	SIT_Widget red, green, blue, alpha, hdr;
	SIT_Widget saveAs, rotate, flipV, flipH;
	SIT_Widget addR, addG, addB, addA;
	SIT_Widget vert;
	int        channelFlags;
	int        vertical;
	int        mirror;
	ListHead   curves;             /* RGBCurve */
	RGBCurve   active;
	Image_t    refTexture;
	Image_t    dstGradient;
	int        HDR;
	int        wndWidth, wndHeight;
	int        gapSize;
	int        maxRefHeight, maxRefWidth;
	int        ctrlWidth, ctrlHeight;
	int        modified;
	float      ctrlPtOffsetY;
	TEXT       lastINI[80];
	TEXT       refPath[80];
	TEXT       saveAsPath[80];
};


#define TEX_IMAGE_W         512
#define TEX_IMAGE_H         512
#define CURVE_INTERP        50

#endif


