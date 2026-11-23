#ifndef SPANG_SPRITE_OVERLAY_H
#define SPANG_SPRITE_OVERLAY_H
#include <stdint.h>
/* C05 links spang_sprite_overlay_352.c: unflipped uniform 11/12 projection,
 * full 384x240 source -> 352x220 at y=2, tags below ignored.
 * The legacy implementation remains the unflipped 320x240 diagnostic path. Untagged objects use reference sampling.
 * Optional diagnostic-only metadata in otherwise unused object bytes:
 * +4 = SPANG_SPRITE_ANCHOR_TAG, +5/+6 = signed LE arcade-X group origin.
 * Every tile in one ball shares that origin. Sampling is then relative to
 * the group, so translation cannot change its shape or internal tile seams.
 * frame is an inactive 512x256 RGB1555 bitmap, initially background.
 * Restore every previous sprite footprint before drawing current objects.
 * Objects use full original 4096-byte layout, FE0 excluded. Returns writes.
 * No allocation, flip, palette change, synchronization or VDP ownership. */
#define SPANG_SPRITE_ANCHOR_TAG 0xa6
/* Native-size actor group: +5/+6 hold its arcade center. Only that center
 * is projected; tile offsets and every source pixel remain unscaled.
 * Use for protagonists and, when integrated, helper animals. */
#define SPANG_SPRITE_NATIVE_TAG 0xa7
/* Centered 320px map viewport, arcade x=96..415; no scaling. */
#define SPANG_SPRITE_MAP_TAG 0xa8
/* Proportional 5:6 in X and Y, 20-pixel vertical letterboxing. */
#define SPANG_SPRITE_FRONTEND_TAG 0xa9
unsigned spang_sprite_overlay(const uint8_t gfx[262144],
    const uint16_t colors[256],const uint8_t objects[4096],
    const uint8_t previous[4096],const uint16_t background[131072],
    volatile uint16_t frame[131072]);
/* Diagnostic overlay excludes rows owned by text, including restoration. */
unsigned spang_sprite_overlay_below(const uint8_t gfx[262144],const uint16_t colors[256],const uint8_t objects[4096],const uint8_t previous[4096],const uint16_t background[131072],volatile uint16_t frame[131072],unsigned first_row);
#endif
