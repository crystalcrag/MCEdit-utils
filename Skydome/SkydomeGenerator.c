/*
 * SkydomeGenerator.c : generate skydome texture using bezier curves
 *
 * Written by T.Pierron, jan 2022
 */

#include <stdio.h>
#include <SDL/SDL.h>
#include <GL/GL.h>
#include "nanovg.h"
#include <malloc.h>
#include <math.h>
#include "SIT.h"
#include "curves.h"
#include "SkydomeGenerator.h"
#define PNGWRITE_IMPL
#include "PNGWrite.h"

struct SkydomePrivate_t skydome;

static int SDLKtoSIT[] = {
	SDLK_HOME,      SITK_Home,
	SDLK_END,       SITK_End,
	SDLK_PAGEUP,    SITK_PrevPage,
	SDLK_PAGEDOWN,  SITK_NextPage,
	SDLK_UP,        SITK_Up,
	SDLK_DOWN,      SITK_Down,
	SDLK_LEFT,      SITK_Left,
	SDLK_RIGHT,     SITK_Right,
	SDLK_LSHIFT,    SITK_LShift,
	SDLK_RSHIFT,    SITK_RShift,
	SDLK_LAST,      SITK_LAlt,
	SDLK_RALT,      SITK_RAlt,
	SDLK_LCTRL,     SITK_LCtrl,
	SDLK_RCTRL,     SITK_RCtrl,
	SDLK_LSUPER,    SITK_LCommand,
	SDLK_RSUPER,    SITK_RCommand,
	SDLK_MENU,      SITK_AppCommand,
	SDLK_RETURN,    SITK_Return,
	SDLK_CAPSLOCK,  SITK_Caps,
	SDLK_INSERT,    SITK_Insert,
	SDLK_DELETE,    SITK_Delete,
	SDLK_NUMLOCK,   SITK_NumLock,
	SDLK_PRINT,     SITK_Impr,
	SDLK_F1,        SITK_F1,
	SDLK_F2,        SITK_F2,
	SDLK_F3,        SITK_F3,
	SDLK_F4,        SITK_F4,
	SDLK_F5,        SITK_F5,
	SDLK_F6,        SITK_F6,
	SDLK_F7,        SITK_F7,
	SDLK_F8,        SITK_F8,
	SDLK_F9,        SITK_F9,
	SDLK_F10,       SITK_F10,
	SDLK_F11,       SITK_F11,
	SDLK_F12,       SITK_F12,
	SDLK_F13,       SITK_F13,
	SDLK_F14,       SITK_F14,
	SDLK_F15,       SITK_F15,
	SDLK_BACKSPACE, SITK_BackSpace,
	SDLK_ESCAPE,    SITK_Escape,
	SDLK_SPACE,     SITK_Space,
	SDLK_HELP,      SITK_Help,
};

static int SDLMtoSIT(int mod)
{
	int ret = 0;
	if (mod & KMOD_CTRL)  ret |= SITK_FlagCtrl;
	if (mod & KMOD_SHIFT) ret |= SITK_FlagShift;
	if (mod & KMOD_ALT)   ret |= SITK_FlagAlt;
	return ret;
}

static Coord curveToBezier(Coord ctrl, int nbCtrl, Coord curve, int nbPts)
{
	Coord tmp = alloca(nbCtrl * sizeof *tmp);
	Coord path, dest;
	int   i, j, k;

	for (i = 0, nbPts--, dest = curve; i <= nbPts; i ++, dest ++)
	{
		/* De Casteljau algorithm */
		memcpy(tmp, ctrl, nbCtrl * sizeof *tmp);

		float t, tm1;
		for (j = nbCtrl - 1, t = i / (float) nbPts, tm1 = 1.0-t; j > 0; j --)
		{
			for (k = 0, path = tmp; k < j; k ++, path ++)
			{
				path->x = path->x * tm1 + path[1].x * t;
				path->y = path->y * tm1 + path[1].y * t;
			}
		}

		dest[0] = tmp[0];
	}
	return curve;
}

static void curveDelPt(Curve curve, int nb)
{
	if (nb < curve->nbctrl)
	{
		Coord ctrl = curve->ctrl + nb;
		memmove(ctrl, ctrl + 1, (curve->nbctrl-nb-1) * sizeof *ctrl);
		curve->nbctrl --;
		curveToBezier(curve->ctrl, curve->nbctrl, curve->interp, curve->nbpt);
	}
}

static void curveElevateDegree(Curve curve)
{
	Coord src, dst;
	int   i, nbpt;

	/* first and last ctrl pt are the same */
	dst = curve->ctrl + 1;
	src = alloca(curve->nbctrl * sizeof *src);
	memcpy(src, curve->ctrl, curve->nbctrl * sizeof *src);
	nbpt = curve->nbctrl;

	for (i = 1, nbpt --, src ++; i <= nbpt; i ++, dst ++, src ++)
	{
		float isn = i / (float) (nbpt+1);

		dst->x = src[-1].x * isn + (1 - isn) * src->x;
		dst->y = src[-1].y * isn + (1 - isn) * src->y;
	}
	dst[0] = src[-1];
	curve->nbctrl ++;
	curveToBezier(curve->ctrl, curve->nbctrl, curve->interp, curve->nbpt);
}

static void curveInit(Curve curve, Coord points, int maxInterp, int maxCtrl)
{
	curve->interp  = points;
	curve->ctrl    = points + maxInterp;
	curve->nbctrl  = 2;
	curve->nbpt    = maxInterp;
	curve->maxCtrl = maxCtrl;

	/* linear curve to 0,255 to 0,255 */
	curve->ctrl[1].x = 1;
	curve->ctrl[1].y = 255;

	curveToBezier(curve->ctrl, curve->nbctrl, curve->interp, curve->nbpt);
}

/* older version has x coord tied to ref texture width */
static void curveNormalize(Curve curve)
{
	/* now, keep x coord between 0 and 1 */
	Coord coord;
	float max;
	int   i;

	for (coord = curve->ctrl, max = 0, i = curve->nbctrl; i > 0; i --, coord ++)
		if (max < coord->x) max = coord->x;

	if (max > 1)
	{
		/* need normalize */
		for (coord = curve->ctrl, i = curve->nbctrl; i > 0; i --, coord ++)
			coord->x /= max;
	}
}

/*
 * texture from gradient interpolation
 */

static void textureToValues(Curve curve, DATA8 line, int chan, int width)
{
	Coord tmp = alloca(curve->nbctrl * sizeof *tmp);
	Coord path;
	float factor;
	int   i, j, k, x, x2;

	line += chan;
	if ((skydome.channelFlags & (1 << chan)) == 0)
	{
		x = chan < 3 ? 0 : 255;
		for (i = 0; i < width; i ++, line[0] = x, line += 4);
		return;
	}

	for (path = curve->ctrl, i = curve->nbctrl, factor = 0; i > 0; path ++, i --)
		if (factor < path->x) factor = path->x;

	factor = (width - 1) / factor;

	for (i = 0, width--, x = 0; i <= width; i ++)
	{
		/* De Casteljau algorithm */
		for (j = curve->nbctrl, k = 0, path = curve->ctrl; k < j; k ++, path ++)
		{
			tmp[k].x = path[0].x * factor;
			tmp[k].y = path[0].y;
		}

		float t, tm1;
		for (j = curve->nbctrl - 1, t = i / (float) width, tm1 = 1.0-t; j > 0; j --)
		{
			for (k = 0, path = tmp; k < j; k ++, path ++)
			{
				path->x = path->x * tm1 + path[1].x * t;
				path->y = path->y * tm1 + path[1].y * t;
			}
		}

		j = tmp[0].y;
		if (j < 0) j = 0;
		if (j > 255) j = 255;
		x2 = tmp[0].x;
		while (x < x2) line[x*4] = 255 - j, x ++;
		line[x2 * 4] = 255 - j;
		x = x2;
	}
}

static void textureStoreLine(DATA8 source, int line)
{
	Image  dest = &skydome.dstGradient;
	DATA32 src  = (DATA32) source;
	DATA32 dst  = (DATA32) dest->bitmap;

	int i, x, y, w = dest->width, flags = skydome.mirror;

	for (i = 0; i < TEX_IMAGE_W; i ++, src ++)
	{
		x = i;
		y = line;
		/* take transformation into account, otherwise a simple memcpy would have been enough */
		if (flags & 1) x = y, y = i;
		if (flags & 2) y = TEX_IMAGE_H - y - 1;
		if (flags & 4) x = TEX_IMAGE_W - x - 1;
		dst[x+y*w] = src[0];
	}
}

static void textureRender(void)
{
	if (skydome.curves.lh_Head == NULL)
		return;

	RGBCurve curve, next;
	Image    dest = &skydome.dstGradient;
	DATA8    buffer;
	int      maxCtrl, i;

	/* starting area: repeat first line if control line not set at 0 */
	curve = HEAD(skydome.curves);
	maxCtrl = curve->red.nbctrl;
	if (maxCtrl < curve->green.nbctrl) maxCtrl = curve->green.nbctrl;
	if (maxCtrl < curve->blue.nbctrl)  maxCtrl = curve->blue.nbctrl;
	if (maxCtrl < curve->alpha.nbctrl) maxCtrl = curve->alpha.nbctrl;
	buffer = alloca(TEX_IMAGE_W * 4);

	if (curve->dstY > 0)
	{
		for (i = 0; i < curve->dstY; i ++)
		{
			textureToValues(&curve->red,   buffer, 0, TEX_IMAGE_W);
			textureToValues(&curve->green, buffer, 1, TEX_IMAGE_W);
			textureToValues(&curve->blue,  buffer, 2, TEX_IMAGE_W);
			textureToValues(&curve->alpha, buffer, 3, TEX_IMAGE_W);
			textureStoreLine(buffer, i);
		}
	}

	Coord start = alloca(sizeof *start * maxCtrl);

	/* interpolation between 2 ctrl lines */
	for (next = curve, NEXT(next); next; curve = next, NEXT(next))
	{
		int y1 = curve->dstY;
		int y2 = next ? next->dstY : TEX_IMAGE_H-1;
		int dist = next->dstY - curve->dstY;

		if (curve->interpStart > 0)
		{
			/* don't start interpolation right now */
			dist -= curve->interpStart;
			for (i = curve->interpStart; i > 0; i --, y1 ++)
			{
				textureToValues(&curve->red,   buffer, 0, TEX_IMAGE_W);
				textureToValues(&curve->green, buffer, 1, TEX_IMAGE_W);
				textureToValues(&curve->blue,  buffer, 2, TEX_IMAGE_W);
				textureToValues(&curve->alpha, buffer, 3, TEX_IMAGE_W);
				textureStoreLine(buffer, y1);
			}
		}

		for (i = 0; y1 < y2; i ++, y1 ++)
		{
			float t = i / (float) dist;
			int   chan, pt, maxPt;
			//if (y1 < TEX_IMAGE_H/2) t *= t;
			//else t = sqrtf(t);

			/* average R, G, B, A control points */
			for (chan = 0; chan < 4; chan ++)
			{
				Coord dst  = start;
				Coord src1 = (&curve->red)[chan].ctrl;
				Coord src2 = (&next->red)[chan].ctrl;
				maxPt = (&curve->red)[chan].nbctrl;
				for (pt = 0; pt < maxPt; pt ++, dst ++, src1 ++, src2 ++)
				{
					dst->x = src2->x * t + src1->x * (1-t);
					dst->y = src2->y * t + src1->y * (1-t);
				}

				struct Curve_t temp = {.ctrl = start, .nbctrl = maxPt};
				textureToValues(&temp, buffer, chan, TEX_IMAGE_W);
			}
			textureStoreLine(buffer, y1);
		}
	}

	/* trailer: repeat last ctrl line */
	curve = TAIL(skydome.curves);
	for (i = curve->dstY; i < TEX_IMAGE_H; i ++)
	{
		textureToValues(&curve->red,   buffer, 0, TEX_IMAGE_W);
		textureToValues(&curve->green, buffer, 1, TEX_IMAGE_W);
		textureToValues(&curve->blue,  buffer, 2, TEX_IMAGE_W);
		textureToValues(&curve->alpha, buffer, 3, TEX_IMAGE_W);
		textureStoreLine(buffer, i);
	}
	nvgUpdateImage(skydome.nvgCtx, dest->nvgId, dest->bitmap);
	SIT_ForceRefresh();
}

static int textureGetMaxRGB(int line)
{
	Image ref = &skydome.refTexture;
	DATA8 rgb = ref->bitmap + (int) (line * (ref->height-1)) * ref->stride;
	int   i, maxRGB, chan;

	for (i = ref->width, maxRGB = 0, chan = ref->spp; i > 0; rgb += chan, i --)
	{
		switch (chan) {
		case 4:
		case 3: if (maxRGB < rgb[2]) maxRGB = rgb[2];
		        if (maxRGB < rgb[1]) maxRGB = rgb[1];
		case 1: if (maxRGB < rgb[0]) maxRGB = rgb[0];
		}
	}
	return maxRGB;
}

static int textureSave(SIT_Widget w, APTR cd, APTR ud)
{
	Image dest = &skydome.dstGradient;

	STRPTR output;
	SIT_GetValues(skydome.outpath, SIT_Title, &output, NULL);

	if ((skydome.channelFlags & 8) == 0)
	{
		/* user don't have alpha, but dest is an RGBA image: need to remove it first :-/ */
		DATA8 bitmap = malloc(TEX_IMAGE_H * TEX_IMAGE_H * 3);
		DATA8 src, dst;
		int   i, j;

		for (src = dest->bitmap, dst = bitmap, i = TEX_IMAGE_H; i > 0; i --)
		{
			for (j = TEX_IMAGE_W; j > 0; j --, dst += 3, src += 4)
				dst[0] = src[0], dst[1] = src[1], dst[2] = src[2];
		}

		textureSavePNG(output, bitmap, 0, TEX_IMAGE_H, TEX_IMAGE_W, 3);
		free(bitmap);
	}
	else textureSavePNG(output, dest->bitmap, 0, TEX_IMAGE_H, TEX_IMAGE_W, dest->spp);

	CopyString(skydome.saveAsPath, output, sizeof skydome.saveAsPath);

	return 1;
}

/*
 * user-interface related functions
 */
static void editorSetModif(void)
{
	if (! skydome.modified)
	{
		SIT_SetValues(skydome.save, SIT_Style, "color: red", NULL);
		skydome.modified = 1;
	}
}

/* draw bezier curves and their control points */
static void editorDrawCurve(Curve curve, SIT_OnPaint * paint, DATA8 rgba)
{
	int i;

	/* linear curve */
	#define SCALEY(val)     (paint->y + skydome.ctrlPtOffsetY + (val * paint->h / 255))
	#define SCALEX(val)     (paint->x + (val * paint->w))
	NVGCTX vg = skydome.nvgCtx;
	Coord  pt = curve->ctrl;
	nvgStrokeColorRGBA8(vg, "\xcc\xcc\xcc\xff");
	nvgBeginPath(vg);
	nvgMoveTo(vg, SCALEX(pt->x), SCALEY(pt->y));
	for (i = 1, pt ++; i < curve->nbctrl; i ++, pt ++)
	{
		nvgLineTo(vg, SCALEX(pt->x), SCALEY(pt->y));
	}
	nvgStroke(vg);

	/* ctrl points */
	nvgStrokeColorRGBA8(vg, rgba);
	nvgBeginPath(vg);
	float gap = skydome.gapSize;
	for(i = 0, pt = curve->ctrl; i < curve->nbctrl; i ++, pt ++)
	{
		float x = SCALEX(pt->x);
		float y = SCALEY(pt->y);

		nvgMoveTo(vg, x-gap, y-gap); nvgLineTo(vg, x+gap, y+gap);
		nvgMoveTo(vg, x-gap, y+gap); nvgLineTo(vg, x+gap, y-gap);
	}
	nvgStroke(vg);

	/* bezier curve */
	pt = curve->interp;
	nvgStrokeColorRGBA8(vg, rgba);
	nvgBeginPath(vg);
	nvgMoveTo(vg, SCALEX(pt->x), SCALEY(pt->y));
	for (i = 1, pt ++; i < curve->nbpt; i ++, pt ++)
		nvgLineTo(vg, SCALEX(pt->x), SCALEY(pt->y));
	nvgStroke(vg);

}

/* check if click near a control point */
static int editorClickOnCtrl(int mx, int my)
{
	int i, j, gap = skydome.gapSize;
	if (skydome.active == NULL)
		return -1;

	SIT_OnPaint * paint = alloca(sizeof *paint);
	paint->x = 0;
	paint->y = 0;
	paint->w = skydome.ctrlWidth;
	paint->h = skydome.ctrlHeight;

	Curve curve;
	for (i = 0, curve = &skydome.active->red; i < 4; i ++, curve ++)
	{
		if (skydome.channelFlags & (1 << i))
		{
			Coord pt = curve->ctrl;
			for (j = 0; j < curve->nbctrl; j ++, pt ++)
			{
				int x = SCALEX(pt->x);
				int y = SCALEY(pt->y);

				if (x - gap < mx && mx < x + gap &&
					y - gap < my && my < y + gap)
					return j | (i << 8);
			}
		}
	}
	return -1;
	#undef SCALEX
	#undef SCALEY
}

/* show RGBA graph of reference texture */
static void editorPaintGraph(DATA8 rgb, float line, int off, SIT_OnPaint * paint)
{
	Image bg  = &skydome.refTexture;
	int   y   = line * (bg->height-1);
	DATA8 p   = bg->bitmap + y * bg->stride + off;
	int   bpp = bg->spp;
	int   max = bg->width;
	int   i;

	if (bg->bitmap == NULL) return;

	if (skydome.vertical)
	{
		y = line * (bg->width-1);
		p = bg->bitmap + y * bpp + off;
		bpp = bg->stride;
		max = bg->height;
	}

//	if (! gradient.HDR)
//		maxRGB = 255;

	#define SCALEY(val)     (paint->y + skydome.ctrlPtOffsetY + ((255 - val) * paint->h) / 255)
	#define SCALEX(val)     (paint->x + (val * paint->w) / max)
	NVGCTX vg = skydome.nvgCtx;
	nvgStrokeColorRGBA8(vg, rgb);
	nvgBeginPath(vg);
	nvgMoveTo(vg, paint->x, SCALEY(p[0])); p += bpp;

	for (i = 1; i < max; i ++, p += bpp)
		nvgLineTo(vg, SCALEX(i), SCALEY(p[0]));
	nvgStroke(vg);
	#undef SCALEY
	#undef SCALEX
}

static int editorPaintEditor(SIT_Widget w, APTR cd, APTR ud)
{
	if (skydome.active)
	{
		static uint8_t rgbaColorChan[] = {
			0xff, 0x00, 0x00, 255,
			0x00, 0xcc, 0x00, 255,
			0x00, 0x00, 0xff, 255,
			0x33, 0x33, 0x33, 255
		};

		SIT_OnPaint * paint = cd;
		NVGCTX vg = skydome.nvgCtx;
		float y = skydome.active->srcY;
		skydome.ctrlHeight = paint->h;
		skydome.ctrlWidth  = paint->w;
		nvgSave(vg);
		nvgIntersectScissor(vg, 0, paint->y, 1e6, paint->h);
		editorPaintGraph(rgbaColorChan,   y, 0, cd);
		editorPaintGraph(rgbaColorChan+4, y, 1, cd);
		editorPaintGraph(rgbaColorChan+8, y, 2, cd);
		if (skydome.channelFlags & 8)
			editorPaintGraph(rgbaColorChan+12, y, 3, cd);

		if (skydome.channelFlags & 1) editorDrawCurve(&skydome.active->red,   cd, rgbaColorChan);
		if (skydome.channelFlags & 2) editorDrawCurve(&skydome.active->green, cd, rgbaColorChan+4);
		if (skydome.channelFlags & 4) editorDrawCurve(&skydome.active->blue,  cd, rgbaColorChan+8);
		if (skydome.channelFlags & 8) editorDrawCurve(&skydome.active->alpha, cd, rgbaColorChan+12);
		nvgRestore(vg);
	}
	return 1;
}


/* redraw ref texture overlay */
static int editorPaintLineOverlay(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnPaint * paint = cd;
	RGBCurve      list;
	NVGCTX        vg = skydome.nvgCtx;

	skydome.maxRefWidth  = paint->w - 1;
	skydome.maxRefHeight = paint->h - 1;

	for (list = HEAD(skydome.curves); list; NEXT(list))
	{
		nvgStrokeColorRGBA8(vg, list == skydome.active ? "\x14\xff\x14\xff" : "\xff\x14\x14\xff");
		nvgBeginPath(vg);
		if (skydome.vertical)
		{
			float x = paint->x + skydome.maxRefWidth * list->srcY;
			nvgMoveTo(vg, x, paint->y);
			nvgLineTo(vg, x, paint->y + paint->h);
		}
		else /* horizontal lines */
		{
			float y = paint->y + skydome.maxRefHeight * list->srcY;
			nvgMoveTo(vg, paint->x, y);
			nvgLineTo(vg, paint->x + paint->w, y);
		}
		nvgStroke(vg);
	}
	return 1;
}

/* add a horizontal line as a reference for a group of bezier curves */
static RGBCurve editorAddLine(float y)
{
	RGBCurve curve = malloc(sizeof *curve + (50 + 10) * sizeof (Coord_t) * 4);
	Coord    points = (Coord) (curve + 1);

	memset(curve, 0, sizeof *curve);

	curve->dstY = y * TEX_IMAGE_H;
	curve->srcY = y;
	curve->interpStart = -1;
	curve->maxRGB = textureGetMaxRGB(y);

	if (curve->dstY >= TEX_IMAGE_H)
		curve->dstY = TEX_IMAGE_H-1;

	curveInit(&curve->red,   points, 50, 10); points += 60;
	curveInit(&curve->green, points, 50, 10); points += 60;
	curveInit(&curve->blue,  points, 50, 10); points += 60;
	curveInit(&curve->alpha, points, 50, 10);

	/* sort insert in increasing Y */
	RGBCurve insert;
	for (insert = HEAD(skydome.curves); insert && insert->srcY < y; NEXT(insert));
	if (insert)
		ListInsert(&skydome.curves, &curve->node, &insert->node);
	else
		ListAddTail(&skydome.curves, &curve->node);

//	fprintf(stderr, "added curve at %d\n", y);

	return curve;
}

static RGBCurve editorCheckNearestLine(int x, int y)
{
	RGBCurve list;
	int gap = skydome.gapSize;
	int height = skydome.maxRefHeight;
	if (skydome.vertical) y = x, height = skydome.maxRefWidth;
	for (list = HEAD(skydome.curves); list; NEXT(list))
	{
		int srcY = list->srcY * height;
		if (srcY - gap < y && y < srcY + gap)
			return list;
	}
	return NULL;
}

/* make sure all curves have same number of points */
static RGBCurve editorEqualize(RGBCurve rgbCurve)
{
	/* curve was added: equalize the number of control points for each channel */
	RGBCurve ref = HEAD(skydome.curves);
	Curve curve;
	int i;
	if (ref == rgbCurve)
		NEXT(ref);
	if (ref == NULL) return rgbCurve;
	for (i = 0, curve = &rgbCurve->red; i < 4; i ++, curve ++)
	{
		int max = (&ref->red)[i].nbctrl;
		while (curve->nbctrl < max)
			curveElevateDegree(curve);
	}
	return rgbCurve;
}

/* edit reference lines from ref texture */
static int editorMouse(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnMouse * msg = cd;

	RGBCurve curve;
	switch (msg->state) {
	case SITOM_ButtonPressed:
		switch (msg->button) {
		case SITOM_ButtonLeft: /* LMB - select */
			curve = skydome.active;
			skydome.active = editorCheckNearestLine(msg->x, msg->y);
			if (curve != skydome.active)
				SIT_ForceRefresh();

			if (skydome.active) return 2;
			break;
		case SITOM_ButtonRight: /* RMB - create */
			if (editorCheckNearestLine(msg->x, msg->y) == NULL)
			{
				int ref = skydome.vertical ? skydome.maxRefWidth : skydome.maxRefHeight;
				skydome.active = editorEqualize(editorAddLine(msg->y / (float) ref));
				editorSetModif();
				textureRender();
				SIT_ForceRefresh();
			}
			break;
		case SITOM_ButtonMiddle: /* MMB - delete */
			curve = editorCheckNearestLine(msg->x, msg->y);
			if (curve == NULL) break;
			ListRemove(&skydome.curves, &curve->node);
			free(curve);
			if (skydome.active == curve)
				skydome.active = NULL;
			textureRender();
			editorSetModif();
			SIT_ForceRefresh();
		default: break;
		}
		break;
	case SITOM_CaptureMove:
		if (skydome.active)
		{
			/* move control line */
			float    ref  = skydome.vertical ? skydome.maxRefWidth : skydome.maxRefHeight;
			RGBCurve min  = (RGBCurve) skydome.active->node.ln_Prev;
			RGBCurve max  = (RGBCurve) skydome.active->node.ln_Next;
			float    minY = (min ? min->srcY + skydome.gapSize / ref : 0) * ref;
			float    maxY = (max ? max->srcY - skydome.gapSize / ref: 1) * ref;
			float    pos  = skydome.vertical ? msg->x : msg->y;
			if (pos < minY) pos = minY;
			if (pos > maxY) pos = maxY;
			skydome.active->srcY = pos / ref;
			//skydome.active->maxRGB = textureGetMaxRGB(Y);
			editorSetModif();
			SIT_ForceRefresh();
		}
	default: break;
	}

	return 0;
}

/* remove a point from a curve channel */
static void editorCurveDel(int pt)
{
	RGBCurve curve;

	for (curve = HEAD(skydome.curves); curve; NEXT(curve))
	{
		/* all curve must have the same number of points */
		curveDelPt(&curve->red + (pt >> 8), pt & 255);
	}
}

/* bezier curve editor */
static int editorMouseEditor(SIT_Widget w, APTR cd, APTR ud)
{
	static int ctrlPt = -1;
	static int ctrlY  = -1;
	SIT_OnMouse * msg = cd;
	switch (msg->state) {
	case SITOM_ButtonPressed:
		if ((msg->flags & SITK_FlagShift) && msg->button == SITOM_ButtonLeft)
		{
			ctrlY = msg->y;
			ctrlPt = skydome.ctrlPtOffsetY;
			return 2;
		}
		ctrlPt = editorClickOnCtrl(msg->x, msg->y);
		if (ctrlPt >= 0)
		{
			if (msg->button == SITOM_ButtonMiddle)
			{
				editorSetModif();
				editorCurveDel(ctrlPt);
				return 0;
			}
			else return 2;
		}
		break;
	case SITOM_CaptureMove:
		if (ctrlY >= 0)
		{
			/* scroll whole view to be able to change ctrl points */
			skydome.ctrlPtOffsetY = ctrlPt + msg->y - ctrlY;
			SIT_ForceRefresh();
		}
		else if (ctrlPt >= 0)
		{
			/* move control point */
			Curve curve = &skydome.active->red + (ctrlPt >> 8);
			Coord coord = &curve->ctrl[ctrlPt&0xff];
			int width = skydome.maxRefWidth;
			int x = msg->x;
			int y = msg->y - skydome.ctrlPtOffsetY;
			if (x < 0) x = 0;
			if (x >= width) x = width;

			coord->x = x / (float) width;
			coord->y = y * 255 / (float) skydome.ctrlHeight;
			curveToBezier(curve->ctrl, curve->nbctrl, curve->interp, curve->nbpt);
			editorSetModif();
			SIT_ForceRefresh();
		}
		break;
	case SITOM_ButtonReleased:
		if (skydome.active)
		{
			if (ctrlY >= 0)
			{
				ctrlY = ctrlPt = -1;
			}
			else if (ctrlPt >= 0)
			{
				ctrlPt = -1;
				textureRender();
				SIT_ForceRefresh();
			}
		}
	default: break;
	}
	return 1;
}

static int editorAddPoint(SIT_Widget w, APTR cd, APTR ud)
{
	int chan = (int) ud;
	RGBCurve curve;
	for (curve = HEAD(skydome.curves); curve; NEXT(curve))
	{
		curveElevateDegree(&curve->red + chan);
	}
	return 1;
}

static int editorToggleVertical(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_GetValues(w, SIT_CheckState, &skydome.vertical, NULL);
	SIT_ForceRefresh();
	return 1;
}

/*
 * ctrl points toolbar
 */
static void ctrlTriangle(NVGCTX vg, int line, SIT_OnPaint * paint)
{
	int x = paint->x;
	int y = paint->y + line * paint->h / 512.0f;
	int sz = paint->w * 0.5;

	nvgMoveTo(vg, x, y);
	nvgLineTo(vg, x + paint->w - 1, y - sz);
	nvgLineTo(vg, x + paint->w - 1, y + sz);
	nvgClosePath(vg);
}

static int ctrlPaint(SIT_Widget w, APTR cd, APTR ud)
{
	SIT_OnPaint * paint = cd;
	RGBCurve      curve;
	NVGcontext *  vg = skydome.nvgCtx;

	nvgFillColorRGBA8(vg, "\xff\xff\xff\xff");
	nvgBeginPath(vg);
	int extra = 0;
	for (curve = HEAD(skydome.curves); curve; NEXT(curve))
	{
		ctrlTriangle(vg, curve->dstY, paint);
		if (curve->interpStart > 0) extra ++;
	}
	nvgFill(vg);

	if (extra)
	{
		nvgBeginPath(vg);
		nvgFillColorRGBA8(vg, "\x66\x66\x66\xbb");
		for (curve = HEAD(skydome.curves); curve; NEXT(curve))
		{
			if (curve->interpStart > 0)
				ctrlTriangle(vg, curve->dstY + curve->interpStart, paint);
		}
		nvgFill(vg);
	}
	return 1;
}

/* move gradient stops */
static int ctrlClickMove(SIT_Widget w, APTR cd, APTR ud)
{
	static RGBCurve clicked;
	static int ctrlType;
	SIT_OnMouse * msg = cd;
	int gap;
	switch (msg->state) {
	case SITOM_ButtonPressed:
		switch (msg->button) {
		case SITOM_ButtonLeft: // LMB - move reference line
			gap = skydome.gapSize;
			for (clicked = HEAD(skydome.curves), ctrlType = 0; clicked; NEXT(clicked))
			{
				int y = clicked->dstY;
				if (y - gap < msg->y && msg->y < y + gap)
					return 2;
				if (clicked->interpStart > 0)
				{
					y += clicked->interpStart;
					if (y - gap < msg->y && msg->y < y + gap)
					{
						ctrlType = 1;
						return 2;
					}
				}
			}
			break;
		case SITOM_ButtonRight: // RMB - modify <interpStart>
			for (clicked = HEAD(skydome.curves); clicked; NEXT(clicked))
			{
				RGBCurve next = (RGBCurve) clicked->node.ln_Next;
				if (next && clicked->dstY < msg->y && msg->y < next->dstY)
				{
					clicked->interpStart = msg->y - clicked->dstY;
					textureRender();
					SIT_ForceRefresh();
					break;
				}
			}
		default: break;
		}
		break;
	case SITOM_CaptureMove:
		if (clicked)
		{
			gap = skydome.gapSize;
			RGBCurve prev = clicked; PREV(prev);
			RGBCurve next = clicked; NEXT(next);
			int minY = prev ? prev->dstY + gap : 0;
			int maxY = next ? next->dstY - gap : TEX_IMAGE_H-1;
			int y = msg->y;
			if (y < minY) y = minY;
			if (y > maxY) y = maxY;
			if (ctrlType == 1)
			{
				clicked->interpStart = y - clicked->dstY;
			}
			else clicked->dstY = y;
			SIT_ForceRefresh();
		}
		break;
	case SITOM_ButtonReleased:
		if (clicked)
		{
			textureRender();
			clicked = NULL;
		}
	default: break;
	}
	return 0;
}

/*
 * read/write config files
 */

static void curveParse(Curve curve, STRPTR points, int chan)
{
	float * dest = &curve->ctrl->x;
	while (points)
	{
		float pos = strtof(points, &points);
		if (*points == ',') points ++; else points = NULL;
		*dest++ = pos;
	}
	curve->nbctrl = (dest - &curve->ctrl->x) >> 1;
}

static Bool readCurves(STRPTR file)
{
	TEXT buffer[256];
	FILE * in = fopen(file, "rb");

	if (in == NULL)
		return False;

	RGBCurve curve, next;
	float lastY = 0;

	/* clear previous config */
	for (curve = HEAD(skydome.curves); curve; curve = next)
	{
		next = (RGBCurve) curve->node.ln_Next;
		free(curve);
	}
	ListNew(&skydome.curves);

	if (skydome.refTexture.bitmap)
	{
		free(skydome.refTexture.bitmap);
		memset(&skydome.refTexture, 0, sizeof skydome.refTexture);
	}

	/* read new config */
	while (fgets(buffer, sizeof buffer, in))
	{
		StripCRLF(buffer);
		if (buffer[0] == '[')
		{
			curve = strcmp(buffer + 1, "StartCurve]") == 0 ? editorAddLine(lastY) : NULL;
			continue;
		}
		if (buffer[0] == '#') continue;
		STRPTR sep = strchr(buffer, '=');
		if (! sep) continue;
		*sep ++ = 0;
		if (curve == NULL)
		{
			/* global paramters */
			switch (FindInList("RefTexture,SaveAs,Vertical,Mirroring,ChannelFlags", buffer, 0)) {
			case 0: CopyString(skydome.refPath,    sep, sizeof skydome.refPath); break;
			case 1: CopyString(skydome.saveAsPath, sep, sizeof skydome.saveAsPath); break;
			case 2: skydome.vertical = atoi(sep); break;
			case 3: skydome.mirror = atoi(sep); break;
			case 4: skydome.channelFlags = atoi(sep);
			}
		}
		else switch (FindInList("CurveSrcY,CurveDstY,CurveInterpY,CurveR,CurveG,CurveB,CurveA", buffer, 0)) {
		case 0: curve->srcY = strtof(sep, NULL); lastY = curve->srcY+1; break;
		case 1: curve->dstY = strtof(sep, NULL); break;
		case 2: curve->interpStart = atoi(sep); break;
		case 3: curveParse(&curve->red,   sep, 0); break;
		case 4: curveParse(&curve->green, sep, 1); break;

		case 5: curveParse(&curve->blue,  sep, 2); break;
		case 6: curveParse(&curve->alpha, sep, 3); break;
		}
	}
	fclose(in);

	skydome.modified = 0;
	SIT_SetValues(skydome.inpath,  SIT_Title, BaseName(file), NULL);
	SIT_SetValues(skydome.outpath, SIT_Title, skydome.saveAsPath, NULL);
	int i;
	for (i = 0; i < 4; i ++)
		SIT_SetValues((&skydome.red)[i], SIT_CheckState, (skydome.channelFlags & (1 << i)) > 0, NULL);

	for (i = 0; i < 3; i ++)
		SIT_SetValues((&skydome.rotate)[i], SIT_CheckState, (skydome.mirror & (1 << i)) > 0, NULL);

	SIT_SetValues(skydome.vert, SIT_CheckState, skydome.vertical, NULL);

	CopyString(skydome.lastINI, file, sizeof skydome.lastINI);
	skydome.active = HEAD(skydome.curves);

	return True;
}

/* need to have a ref texture loaded before */
static void normCurves(void)
{
	RGBCurve curve;
	int height = skydome.refTexture.height-1;
	for (curve = HEAD(skydome.curves); curve; NEXT(curve))
	{
		Curve c;
		int   i;
		if (curve->srcY > height)
			curve->srcY = height;
		if (curve->srcY > 1)
			curve->srcY /= height;
		for (i = 0, c = &curve->red; i < 4; i ++, c ++)
		{
			curveNormalize(c);
			curveToBezier(c->ctrl, c->nbctrl, c->interp, c->nbpt);
		}
	}
	textureRender();
}


static int saveCurves(SIT_Widget w, APTR cd, APTR ud)
{
	STRPTR output;
	FILE * out;

	SIT_GetValues(skydome.inpath, SIT_Title, &output, NULL);
	out = fopen(output, "wb");

	RGBCurve list;

	if (! out)
	{
		SIT_Log(SIT_INFO, "%s: fail to open for writing: %s\n", output, GetError());
		return 0;
	}
	CopyString(skydome.lastINI, output, sizeof skydome.lastINI);

	/* global config first */
	SIT_GetValues(skydome.outpath, SIT_Title, &output, NULL);
	fprintf(out, "ChannelFlags=%d\n", skydome.channelFlags & 15);
	fprintf(out, "RefTexture=%s\n", skydome.refPath);
	fprintf(out, "SaveAs=%s\n", output);
	fprintf(out, "Vertical=%d\n", skydome.vertical);
	fprintf(out, "Mirroring=%d\n", skydome.mirror);

	for (list = HEAD(skydome.curves); list; NEXT(list))
	{
		fprintf(out, "[StartCurve]\n");
		fprintf(out, "CurveSrcY=%f\n", list->srcY);
		fprintf(out, "CurveDstY=%d\n", list->dstY);
		fprintf(out, "CurveInterpY=%d\n", list->interpStart);

		Curve chan;
		int   i, j;
		for (chan = &list->red, i = 0; i < 4; i ++, chan ++)
		{
			if ((skydome.channelFlags & (1 << i)) == 0) continue;
			fprintf(out, "Curve%c=", "RGBA"[i]);
			for (j = 0; j < chan->nbctrl; j ++)
			{
				if (j > 0) fputc(',', out);
				fprintf(out, "%f,%f", chan->ctrl[j].x, chan->ctrl[j].y);
			}
			fputc('\n', out);
		}
	}
	fclose(out);

	SIT_SetValues(skydome.save, SIT_Style, "color: initial", NULL);
	skydome.modified = 0;

	return 1;
}

static Bool loadRefImage(STRPTR file)
{
	Image_t image;
	/* we need the image's bitmap, can't let SITGL handle everything */
	image.bitmap = stbi_load(file, &image.width, &image.height, &image.spp, 4);

	if (image.bitmap)
	{
		NVGCTX vg = skydome.nvgCtx;
		TEXT bg[128];

		image.nvgId = nvgCreateImageRGBA(vg, image.width, image.height, 0, image.bitmap);
		image.stride = image.width * 4;

		sprintf(bg, "background-image: id(%d), url(checkboard.png); background-size: 100%% 100%%, auto auto", image.nvgId);

		SIT_SetValues(skydome.reftex, SIT_Style, bg, NULL);

		skydome.refTexture = image;
		return True;
	}
	else SIT_Log(SIT_INFO, "%s: unknown image format", file);
	return False;
}

static int dropFile(SIT_Widget w, APTR cd, APTR ud)
{
	STRPTR  file = ((STRPTR *)cd)[0];
	STRPTR  ext  = strrchr(file, '.');
	int     norm = 0;

	if (ext && strcasecmp(ext, ".ini") == 0)
	{
		/* curve file: replace everything in the interface */
		if (readCurves(file))
			file = skydome.refPath, norm = 1;
		else
			return 0;
	}

	if (loadRefImage(file))
	{
		if (norm)
			normCurves();
		else
			CopyString(skydome.refPath, file, sizeof skydome.refPath);
	}
	return 1;
}

/* channel toggle button handler */
static int editorToggleChan(SIT_Widget w, APTR cd, APTR ud)
{
	int checked = 0;
	int flag = (int) ud;
	SIT_GetValues(w, SIT_CheckState, &checked, NULL);
	if (checked)
		skydome.channelFlags |= flag;
	else
		skydome.channelFlags &= ~flag;
	textureRender();
	SIT_ForceRefresh();
	return 1;
}

/* mirror, flipV, flipH handler */
static int editorToggleOrient(SIT_Widget w, APTR cd, APTR ud)
{
	int checked = 0;
	int flag = (int) ud;
	SIT_GetValues(w, SIT_CheckState, &checked, NULL);
	if (checked)
		skydome.mirror |= flag;
	else
		skydome.mirror &= ~flag;
	textureRender();
	editorSetModif();
	SIT_ForceRefresh();
	return 1;
}

/* handle size proportionnal to font size */
static int getGapSize(SIT_Widget w, APTR cd, APTR ud)
{
	skydome.gapSize = SIT_EmToReal(w, SITV_Em(0.5));
	return 1;
}

static int centerGraph(SIT_Widget w, APTR cd, APTR ud)
{
	skydome.ctrlPtOffsetY = 0;
	return 1;
}

static void imageAlloc(Image dest)
{
	dest->width  = TEX_IMAGE_W;
	dest->height = TEX_IMAGE_H;
	dest->spp    = 4;
	dest->stride = 4 * TEX_IMAGE_W;
	dest->bitmap = calloc(dest->stride, dest->height);
	dest->nvgId  = nvgCreateImageRGBA(skydome.nvgCtx, TEX_IMAGE_W, TEX_IMAGE_H, 0, dest->bitmap);

	TEXT bg[128];
	sprintf(bg, "background-image: id(%d), url(checkboard.png); background-size: 100%% 100%%, auto auto", dest->nvgId);

	SIT_SetValues(skydome.texture, SIT_Style, bg, NULL);
}

static void createUI(SIT_Widget app)
{
	static TEXT help[] =
		"Controls are as follow:<br>"
		"&#x25cf; LMB = select, RMB = create, MMB = delete.<br>"
		"&#x25cf; Left pane = reference texture used to calibrate curves.<br>"
		"&#x25cf; Right pane = resulting texture.<br>"
		"&#x25cf; Shift-click on bottom left pane to scroll the view.<br>"
		"&#x25cf; Drag'n drop an image to load it onto left pane.<br>"
		"&#x25cf; Drag'n drop an INI file to load it.";

	SIT_CreateWidgets(app,
		"<label name=msg1 title='File:'>"
		"<editbox name=file title=", BaseName(skydome.lastINI), "left=WIDGET,msg1,0.5em width=10em>"

		"<button name=save  title=Save  left=WIDGET,file,0.5em>"
		"<button name=red   title=Red   left=WIDGET,save,1em    checkState=1 buttonType=", SITV_ToggleButton, ">"
		"<button name=green title=Green left=WIDGET,red,0.3em   checkState=1 buttonType=", SITV_ToggleButton, ">"
		"<button name=blue  title=Blue  left=WIDGET,green,0.3em checkState=1 buttonType=", SITV_ToggleButton, ">"
		"<button name=alpha title=Alpha left=WIDGET,blue,0.3em  checkState=1 buttonType=", SITV_ToggleButton, ">"
		"<button name=vert  title=Vertical left=WIDGET,alpha,0.3em buttonType=", SITV_ToggleButton, ">"
		"<canvas name=reftex left=FORM right=", SITV_AttachPosition, SITV_AttachPos(49), 0, "top=WIDGET,file,0.5em height=512/>"
		"<canvas name=ctrlpt  right=FORM width=1em top=WIDGET,file,0.5em height=512/>"
		"<canvas name=texture left=WIDGET,reftex,1em right=WIDGET,ctrlpt,0.2em top=WIDGET,file,0.5em height=512/>"

		"<button name=rot90   title='Rotate 90' left=OPPOSITE,texture     buttonType=", SITV_ToggleButton, ">"
		"<button name=mirrorV title='Mirror V'  left=WIDGET,rot90,0.3em   buttonType=", SITV_ToggleButton, ">"
		"<button name=mirrorH title='Mirror H'  left=WIDGET,mirrorV,0.3em buttonType=", SITV_ToggleButton, ">"

		"<label name=msg2 title='Output to:' left=WIDGET,mirrorH,1em top=MIDDLE,file>"
		"<editbox name=output title=tint.png left=WIDGET,msg2,0.5em width=7em>"
		"<button name=saveas  title='Save as PNG' left=WIDGET,output,0.5em>"

		"<label name=msg3 title='Control curves:' top=WIDGET,reftex,1em>"
		"<button name=center title=Center top=MIDDLE,msg3 left=WIDGET,msg3,0.5em>"
		"<canvas name=ctrlcurve.editor left=FORM right=", SITV_AttachPosition, SITV_AttachPos(49), 0, "top=WIDGET,msg3,0.5em bottom=FORM/>"
		"<label name=msg4 title='Add point:' top=OPPOSITE,msg3 left=WIDGET,center,0.5em>"
		"<button name=addR title=R left=WIDGET,msg4,0.5em top=MIDDLE,msg4 style='color:red'>"
		"<button name=addG title=G left=WIDGET,addR,0.2em top=MIDDLE,msg4 style='color:green'>"
		"<button name=addB title=B left=WIDGET,addG,0.2em top=MIDDLE,msg4 style='color:blue'>"
		"<button name=addA title=A left=WIDGET,addB,0.2em top=MIDDLE,msg4>"

		"<label name=help.dim top=OPPOSITE,ctrlcurve right=OPPOSITE,texture bottom=OPPOSITE,ctrlcurve left=OPPOSITE,texture title=", help, ">"

	);
	SIT_SetAttributes(app, "<msg1 top=MIDDLE,file>");
	SIT_GetValues(app, SIT_NVGcontext, &skydome.nvgCtx, NULL);

	skydome.channelFlags = 0xff;

	skydome.reftex  = SIT_GetById(app, "reftex");
	skydome.texture = SIT_GetById(app, "texture");
	skydome.editor  = SIT_GetById(app, "ctrlcurve");
	skydome.red     = SIT_GetById(app, "red");
	skydome.green   = SIT_GetById(app, "green");
	skydome.blue    = SIT_GetById(app, "blue");
	skydome.alpha   = SIT_GetById(app, "alpha");
	skydome.save    = SIT_GetById(app, "save");
	skydome.ctrlpt  = SIT_GetById(app, "ctrlpt");
	skydome.saveAs  = SIT_GetById(app, "saveas");
	skydome.hdr     = SIT_GetById(app, "hdr");
	skydome.inpath  = SIT_GetById(app, "file");
	skydome.outpath = SIT_GetById(app, "output");
	skydome.vert    = SIT_GetById(app, "vert");
	skydome.rotate  = SIT_GetById(app, "rot90");
	skydome.flipV   = SIT_GetById(app, "mirrorV");
	skydome.flipH   = SIT_GetById(app, "mirrorH");
	skydome.addR    = SIT_GetById(app, "addR");
	skydome.addG    = SIT_GetById(app, "addG");
	skydome.addB    = SIT_GetById(app, "addB");
	skydome.addA    = SIT_GetById(app, "addA");

	SIT_AddCallback(app, SITE_OnDropFiles, dropFile, NULL);
	SIT_AddCallback(app, SITE_OnResize,    getGapSize, NULL);
	SIT_AddCallback(SIT_GetById(app, "center"), SITE_OnActivate, centerGraph, NULL);

	SIT_AddCallback(skydome.saveAs,  SITE_OnActivate,  textureSave, NULL);
	SIT_AddCallback(skydome.red,     SITE_OnActivate,  editorToggleChan, (APTR) 1);
	SIT_AddCallback(skydome.green,   SITE_OnActivate,  editorToggleChan, (APTR) 2);
	SIT_AddCallback(skydome.blue,    SITE_OnActivate,  editorToggleChan, (APTR) 4);
	SIT_AddCallback(skydome.alpha,   SITE_OnActivate,  editorToggleChan, (APTR) 8);
	SIT_AddCallback(skydome.rotate,  SITE_OnActivate,  editorToggleOrient, (APTR) 1);
	SIT_AddCallback(skydome.flipV,   SITE_OnActivate,  editorToggleOrient, (APTR) 2);
	SIT_AddCallback(skydome.flipH,   SITE_OnActivate,  editorToggleOrient, (APTR) 4);
	SIT_AddCallback(skydome.addR,    SITE_OnActivate,  editorAddPoint, NULL);
	SIT_AddCallback(skydome.addG,    SITE_OnActivate,  editorAddPoint, (APTR) 1);
	SIT_AddCallback(skydome.addB,    SITE_OnActivate,  editorAddPoint, (APTR) 2);
	SIT_AddCallback(skydome.addA,    SITE_OnActivate,  editorAddPoint, (APTR) 3);
	SIT_AddCallback(skydome.reftex,  SITE_OnPaint,     editorPaintLineOverlay, NULL);
	SIT_AddCallback(skydome.reftex,  SITE_OnClickMove, editorMouse, NULL);
	SIT_AddCallback(skydome.editor,  SITE_OnPaint,     editorPaintEditor, NULL);
	SIT_AddCallback(skydome.editor,  SITE_OnClickMove, editorMouseEditor, NULL);
	SIT_AddCallback(skydome.vert,    SITE_OnActivate,  editorToggleVertical, NULL);
	SIT_AddCallback(skydome.save,    SITE_OnActivate,  saveCurves, NULL);
	SIT_AddCallback(skydome.ctrlpt,  SITE_OnPaint,     ctrlPaint, NULL);
	SIT_AddCallback(skydome.ctrlpt,  SITE_OnClickMove, ctrlClickMove, NULL);

	imageAlloc(&skydome.dstGradient);
}

static void readPrefs(void)
{
	INIFile ini = ParseINI("skydome.ini");
	strcpy(skydome.lastINI, "curves.ini");
	skydome.wndWidth  = GetINIValueInt(ini, "WndWidth",  1400);
	skydome.wndHeight = GetINIValueInt(ini, "WndHeight", 800);
	STRPTR value = GetINIValue(ini, "LastFile");
	if (value) CopyString(skydome.lastINI, value, sizeof skydome.lastINI);
	FreeINI(ini);
}

int main(int nb, char * argv[])
{
	SDL_Surface * screen;
	SIT_Widget    app;
	int           width, height, exitProg;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
		return 1;

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	readPrefs();
	width = skydome.wndWidth;
	height = skydome.wndHeight;
    screen = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER | SDL_OPENGL | SDL_RESIZABLE);
    if (screen == NULL) {
		fprintf(stderr, "failed to set video mode, aborting.\n");
		return 1;
	}
	SDL_WM_SetCaption("Skydome Generator", "Skydome Generator");

	app = SIT_Init(NVG_ANTIALIAS | NVG_STENCIL_STROKES, width, height, "resources/default.css", 1);

	if (app == NULL)
	{
		SIT_Log(SIT_ERROR, "could not init SITGL: %s.\n", SIT_GetError());
		return -1;
	}

	static SIT_Accel accels[] = {
		{SITK_FlagCapture + SITK_FlagAlt + SITK_F4, SITE_OnClose},
		{SITK_FlagCapture + SITK_Escape,            SITE_OnClose},
		{0}
	};

	exitProg = 0;
	SIT_SetValues(app,
		SIT_DefSBSize,   SITV_Em(0.5),
		SIT_RefreshMode, SITV_RefreshAsNeeded,
		SIT_AddFont,     "sans-serif",      "System",
		SIT_AddFont,     "sans-serif-bold", "System/Bold",
		SIT_AccelTable,  accels,
		SIT_ExitCode,    &exitProg,
		NULL
	);

	createUI(app);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);

	glViewport(0, 0, width, height);

	if (nb > 1 && FileExists(argv[1]))
		CopyString(skydome.lastINI, argv[1], sizeof skydome.lastINI);

	if (skydome.lastINI[0] && readCurves(skydome.lastINI) && loadRefImage(skydome.refPath))
		normCurves();

	FrameSetFPS(50);
	while (! exitProg)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				{
					int * sdlk;
					for (sdlk = SDLKtoSIT; sdlk < EOT(SDLKtoSIT); sdlk += 2)
					{
						if (sdlk[0] == event.key.keysym.sym) {
							SIT_ProcessKey(sdlk[1], SDLMtoSIT(event.key.keysym.mod), event.type == SDL_KEYDOWN);
							goto break_loop;
						}
					}
				}
				if (event.key.keysym.unicode > 0)
					SIT_ProcessChar(event.key.keysym.unicode, SDLMtoSIT(event.key.keysym.mod));
			break_loop:
				break;
			case SDL_VIDEOEXPOSE:
				SIT_ForceRefresh();
				break;
			case SDL_MOUSEBUTTONDOWN:
				SIT_ProcessClick(event.button.x, event.button.y, event.button.button-1, 1);
				break;
			case SDL_MOUSEBUTTONUP:
				SIT_ProcessClick(event.button.x, event.button.y, event.button.button-1, 0);
				break;
			case SDL_MOUSEMOTION:
				SIT_ProcessMouseMove(event.motion.x, event.motion.y);
				break;
			case SDL_VIDEORESIZE:
				width  = event.resize.w;
				height = event.resize.h;
				SIT_ProcessResize(width, height);
				glViewport(0, 0, width, height);
				break;
			case SDL_QUIT:
				exitProg = 1;
			case SDL_USEREVENT:
				break;
			default:
				continue;
			}
		}

		/* update and render */
		if (SIT_RenderNodes(FrameGetTime()))
			SDL_GL_SwapBuffers();

		FrameWaitNext();
	}

	SetINIValueInt("skydome.ini", "WndWidth",  width);
	SetINIValueInt("skydome.ini", "WndHeight", height);
	SetINIValue("skydome.ini", "LastFile", skydome.lastINI);

	SDL_FreeSurface(screen);
	SDL_Quit();

	return 0;
}

#ifdef	WIN32
#include <windows.h>
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR CmdLine,
    int CmdShow)
{
	/* CmdLine parameter is not unicode aware even with UNICODE macro set */
	int      nb, i;
	LPWSTR * argv = CommandLineToArgvW(GetCommandLineW(), &nb);

	/* convert strings to UTF8 */
	for (i = 0; i < nb; )
	{
		int len = wcslen(argv[i]);
		int sz  = UTF16ToUTF8(NULL, 0, (STRPTR) argv[i], len) + 1;

		CmdLine = alloca(sz);

		sz = UTF16ToUTF8(CmdLine, sz, (STRPTR) argv[i], len);

		argv[i++] = (LPWSTR) CmdLine;
	}
	return main(nb, (STRPTR *) argv);
}
#endif
