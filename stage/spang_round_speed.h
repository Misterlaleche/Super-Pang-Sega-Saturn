#ifndef SPANG_ROUND_SPEED_H
#define SPANG_ROUND_SPEED_H
#include <stdint.h>
/* Fixed 311D/2F33: E080..E0FF globals, fixed decoded DATA ROM.
 * delta is the original 8-bit signed adjustment. freeze is F6E0.
 * Normal Tour domain: stage 0..39 and speed rank 0..31. */
int spang_round_speed_adjust(uint8_t globals[128],uint8_t delta,const uint8_t fixed[32768]);
int spang_round_speed_apply(uint8_t globals[128],uint8_t freeze,const uint8_t fixed[32768]);
#endif
