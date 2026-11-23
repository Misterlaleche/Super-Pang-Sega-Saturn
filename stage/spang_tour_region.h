#ifndef SPANG_TOUR_REGION_H
#define SPANG_TOUR_REGION_H
#include <stdint.h>
typedef void (*spang_tour_queue_fn)(void *,unsigned,unsigned);
/* BANK3:8BF0, Asia completion. The caller retains original map actor layout.
 * Events: direct FM command, scene complete. Queue callback: original D/E. */
int spang_tour_region_step(uint8_t actors[320],uint8_t globals[128],
 const uint8_t inputs[6],const uint8_t bank3[16384],
 spang_tour_queue_fn queue,void *context,uint8_t events[2]);
#endif
