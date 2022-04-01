/*
 * TileFinderAnim.h : public functiosn and private datatypes to handle animation.
 *
 * Written by T.Pierron, Mar 2022.
 */

#ifndef TILE_FINDER_ANIM_H
#define TILE_FINDER_ANIM_H

void animInit(SIT_Widget parent);
void animSyncBox(Block, int add);
void animShow(void);

struct TileFinderAnim_t
{
	SIT_Widget applyTo;
	SIT_Widget graph, repeat;
	SIT_Widget params, time;
	Block      current;
};

#endif
