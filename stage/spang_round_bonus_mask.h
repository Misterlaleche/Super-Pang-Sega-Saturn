#ifndef SPANG_ROUND_BONUS_MASK_H
#define SPANG_ROUND_BONUS_MASK_H
#include "spang_video_commands.h"
/* BANK3:8706..87AD. The synchronous callback writes the actual object page,
 * so the second pass observes P1's hidden Y values. globals are original
 * E080..E0FF; this helper changes E090/E094 only. */
int spang_round_bonus_mask(uint8_t globals[128],const uint8_t objects[4096],
 const uint8_t bank3[16384],spang_video_write write,void *context);
#endif
