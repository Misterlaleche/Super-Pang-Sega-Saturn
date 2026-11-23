#ifndef FRONTEND_TITLE_NATIVE_H
#define FRONTEND_TITLE_NATIVE_H
#include <stdint.h>
/* PANG uses the captured Mega Drive raster with Saturn palette pens. The
 * legacy cell decoder returns1 for a known native PANG cell and0 otherwise. */
int frontend_title_native_cell(unsigned code,unsigned phase_x,unsigned phase_y,
                               unsigned width,unsigned height,uint8_t *output);
/* Bit0 owns the settled P/A pair; bit1 the N/G pair. Core publishes these in
 * frontend globals[0x70] at the original tile-commit milestones. Return an
 * original palette index or-1 for the blue field. Bounding box is x0..319,
 * y112..191; callers may paint all of it when a mask/palette/scene changes. */
int frontend_title_native_background(unsigned settled_mask,int x,int y);
/* Returns letter*2+shadow for an original PANG actor shape, or -1. */
int frontend_title_native_actor(unsigned shape);
/* Decode one immutable 4bpp whole-letter texture with 8-pixel width alignment.
 * The shadow is clipped at the original settled lower edge. */
int frontend_title_native_raster(unsigned actor,uint8_t *output,unsigned capacity,
                                 int *left,int *top,unsigned *width,unsigned *height);
#endif
