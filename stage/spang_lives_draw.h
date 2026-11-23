#ifndef SPANG_LIVES_DRAW_H
#define SPANG_LIVES_DRAW_H
#include "spang_video_commands.h"
/* ROM166B..16CD plus1542. player is IX+11 (zero=P1, nonzero=P2);
 * lives is original packed-BCD IX+60. globals output is E14F,E0A6,E0A7.
 * Emits ordered logical video writes, leaves old background untouched.
 * No clearing, clipping, Saturn bank routing or CPU flag effects are added. */
void spang_lives_draw(uint8_t player, uint8_t lives, uint8_t globals[3],
                      spang_video_write write, void *context);
#endif
