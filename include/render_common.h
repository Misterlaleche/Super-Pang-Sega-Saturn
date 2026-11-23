#ifndef HK320_RENDER_COMMON_H
#define HK320_RENDER_COMMON_H
#include <stdint.h>
#define RC_MAX_OBJECTS 128
#define RC_MAX_WIDTH 320 /* Destination columns, including native status text. */
#define RC_MAX_SOURCE_WIDTH 352 /* Arcade Lucky Bonus before removing blank columns. */
#define RC_MAX_HEIGHT 160
/* A tall ladder is narrow. The largest actual group uses10240 pixels; this
 * area cap leaves room for it while keeping CELLS.N4 in the high-RAM heap. */
#define RC_MAX_PIXELS 32768u
typedef struct { uint16_t code,group; int16_t x,y; uint8_t palette,ladder_row; } rc_object;
typedef struct {
    int16_t left,top;
    uint16_t width,height,id,first,count,source_code;
    uint8_t palette,role;
} rc_group;
typedef struct {
    rc_object objects[RC_MAX_OBJECTS];
    rc_group groups[RC_MAX_OBJECTS];
    unsigned object_count,group_count;
} rc_scene;
int rc_screen_x(int world_x);
/* First destination pixel whose world sample is at least source_x.
 * Uses the same S(x)=64+floor((6*x+3)/5) grid as the world bitmap. */
int rc_world_boundary_x(int source_x);
/* Original map window: source X96..415, with one source pixel per pixel. */
int rc_map_screen_x(int world_x);
/* Native HUD: keep original pixels, remove only its empty inter-block gap.
 * Destination0..255 uses source64..319;256..319 uses source336..399. */
unsigned rc_hud_source_x(unsigned native_x);
/* Preserve the original BONUS letter separator on the native tile grid. */
unsigned rc_background_glyph_x(unsigned code,unsigned source_column);
uint16_t rc_rgb555(uint16_t xrgb444);
int rc_collect(rc_scene *scene,const uint8_t *objects,unsigned count);
/* Original ladder cells206/207 and odd-tail20E/20F determine actual rungs. */
unsigned rc_ladder_rungs(const rc_scene *scene,unsigned group);
/* Validated hidden original-cell bits; no resampling or geometry clipping. */
int rc_ladder_mask(const rc_scene *scene,unsigned group,uint32_t *mask);
void rc_group_key(const rc_scene *scene,unsigned group,uint32_t key[2]);
/* BE16 code count, BE16 unique count, then sorted BE16 code/ordinal pairs. */
int rc_glyph_catalog_valid(const uint8_t *map,unsigned map_bytes,unsigned glyph_bytes);
/* The immutable catalog must pass rc_glyph_catalog_valid before lookup. */
int rc_glyph_offset(const uint8_t *map,unsigned code);
/* Dense original code -> unique pattern (BE16 ordinals), optional for callers
 * retaining the legacy dense 2048-cell catalog. */
int rc_cell_catalog_valid_split(const uint8_t *catalog,unsigned head_bytes,unsigned tail_bytes);
int rc_cell_catalog_valid(const uint8_t *catalog,unsigned bytes);
const uint8_t *rc_cell_pixels_split(const uint8_t *patterns,const uint8_t *map,const uint8_t *tail,unsigned split,unsigned code);
const uint8_t *rc_cell_pixels(const uint8_t *patterns,const uint8_t *map,unsigned code);
int rc_compose_indexed(const rc_scene *scene,unsigned group,const uint8_t *patterns,const uint8_t *map,
               uint8_t *source_scratch,uint8_t *output,unsigned capacity,
               unsigned *width,unsigned *height,unsigned *anchor_x,int native);
int rc_compose_split(const rc_scene *scene,unsigned group,const uint8_t *patterns,const uint8_t *map,
               const uint8_t *tail,unsigned split,uint8_t *source_scratch,uint8_t *output,unsigned capacity,
               unsigned *width,unsigned *height,unsigned *anchor_x,int native);
/* Three immutable platform textures, at X phases0,2,4 relative to source64.
 * Decode the original group once; each consecutive packed image has the
 * returned padded width and height. Padding and original transparency are0. */
int rc_compose_platform_split(const rc_scene *scene,unsigned group,const uint8_t *patterns,const uint8_t *map,
               const uint8_t *tail,unsigned split,uint8_t *source_scratch,uint8_t *output,unsigned capacity,
               unsigned *width,unsigned *height);
/* Independently compressed <=4096-byte glyph blocks. Offset table and all
 * back-references are checked before touching caller memory outside capacity. */
int rc_glyph_block_decode(const uint8_t *catalog,unsigned bytes,unsigned block,uint8_t *out,unsigned capacity);
int rc_glyph_blocks_valid(const uint8_t *catalog,unsigned bytes,unsigned expected,uint8_t *scratch,unsigned capacity);
/* Fixed asset grid, independent of position. Input pens already map arcade
 * transparent15 to Saturn0; original opaque0..14 map to1..15. */
int rc_compose(const rc_scene *scene,unsigned group,const uint8_t *cells,
               uint8_t *source_scratch,uint8_t *output,unsigned capacity,
               unsigned *width,unsigned *height,unsigned *anchor_x);
/* Map actors share the map's 1:1 pixel geometry and centered coordinates. */
int rc_compose_native(const rc_scene *scene,unsigned group,const uint8_t *cells,
               uint8_t *source_scratch,uint8_t *output,unsigned capacity,
               unsigned *width,unsigned *height,unsigned *anchor_x);
#endif
