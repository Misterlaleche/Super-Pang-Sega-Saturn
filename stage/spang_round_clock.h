#ifndef SPANG_ROUND_CLOCK_H
#define SPANG_ROUND_CLOCK_H
#include <stdint.h>
#include "spang_video_commands.h"
/* Native translations of fixed 0CE9/0D5E and bank 3 87AE. The 32-byte timer
 * window represents E1C0..E1DF. Globals are E080..E0FF. No CPU interpreter.
 * Tables: 40 stage words, 4 difficulty words, 19 bonus bytes, 6 speed bytes.
 * Effects: speed increment, sound command, palette phase + 1, timeout.
 * video_busy is the separate E180 video-service mailbox, not a globals byte.
 * Caller owns the original speed/palette/sound consumers and phase scheduler. */
int spang_round_clock_init(uint8_t timer[32],uint8_t stage,uint8_t difficulty,const uint8_t tables[113]);
void spang_round_clock_step(uint8_t timer[32],const uint8_t globals[128],const uint8_t tables[113],uint8_t video_busy,uint8_t effects[4]);
/* Bonus selector is E098; it is distinct from speed selector E097. */
uint32_t spang_round_time_bonus(uint16_t time,uint8_t retries,uint8_t bonus_selector,const uint8_t tables[113]);
/* Fixed 0D8D/14F5/1512: original three 2x2 character groups at DDC2.
 * digit_table is the 20 DATA bytes at fixed 152E. Valid BCD time required. */
void spang_round_clock_draw(const uint8_t timer[32],const uint8_t digit_table[20],spang_video_write write,void *context);
#endif
