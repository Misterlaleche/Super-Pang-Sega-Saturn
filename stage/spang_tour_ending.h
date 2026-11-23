#ifndef SPANG_TOUR_ENDING_H
#define SPANG_TOUR_ENDING_H
#include <stdint.h>
#include "spang_tour_region.h"
#include "spang_video_commands.h"
/* Original BANK3:9632 actor18. a is an actor within pool[320], normally
 * pool[0] for the main controller. Other live slots have selector a[15]=2/4.
 * extra = E14F number attribute, E152/E153 bonus-win counts, FCE7 language.
 * Call with tile write for selector0, original sprite-page write otherwise.
 * events = direct FM command, final controller finished. Native C, no CPU VM. */
int spang_tour_ending_step(uint8_t a[40],uint8_t pool[320],uint8_t g[128],
 uint8_t p1[128],uint8_t p2[128],const uint8_t input[4],uint8_t extra[4],
 const uint8_t bank3[16384],const uint8_t digits[20],
 spang_tour_queue_fn queue,spang_video_write write,void *context,uint8_t events[2]);
#endif
