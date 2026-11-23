#ifndef SPANG_PANIC_VIDEO_H
#define SPANG_PANIC_VIDEO_H
#include <stdint.h>

enum { SPANG_PANIC_VIDEO_TILES=1, SPANG_PANIC_VIDEO_PALETTE=2 };
/* Original video buffers: attributes C800..CFFF, LE characters D000..DFFF,
 * LE palette words. Globals are E080..E0FF. Return changed-buffer flags.
 * Init preserves the loaded background and score/lives, replaces the Tour HUD,
 * then executes the original Panic border/LEVEL/gauge drawing primitives.
 * Update consumes E0D0/E0D8 palette transfer counts like the original ISR. */
unsigned spang_panic_video_init(uint8_t g[128],uint8_t attrs[2048],
                               uint8_t chars[4096],uint8_t palette[4096]);
unsigned spang_panic_video_update(uint8_t g[128],uint8_t attrs[2048],
                                 uint8_t chars[4096],uint8_t palette[4096]);
/* Request20: E188 animation phase0/2/4/6. Request22: original Tour phase0..39. */
unsigned spang_panic_video_palette(unsigned request,unsigned parameter,uint8_t palette[4096]);
#endif
