#ifndef SPANG_SCORE_DRAW_H
#define SPANG_SCORE_DRAW_H
#include "spang_video_commands.h"
/* ROM 2E0A..2E43. Seven nibbles from four original BCD score bytes;
 * the final low nibble is not displayed. Leading zero tiles are untouched,
 * not cleared. Only low tile bytes are written. Address increments wrap its
 * low byte, preserving the high byte. suppressed models E9FF != 0; pass 0
 * for the unconditional 2DF2 entry. Caller routes the original video bus. */
void spang_score_draw(const uint8_t score[4], uint16_t destination,
                      int suppressed, spang_video_write write, void *context);
#endif
