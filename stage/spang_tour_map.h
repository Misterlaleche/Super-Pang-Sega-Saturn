#ifndef SPANG_TOUR_MAP_H
#define SPANG_TOUR_MAP_H
#include "spang_tour_plane.h"
int spang_tour_marker_step(uint8_t actor[40],uint8_t globals[128],const uint8_t bank[16384],spang_tour_write write,void *context);
int spang_tour_spawn(uint8_t actors[320],const uint8_t bank5[16384]);
void spang_tour_timer_step(uint8_t actor[40],uint8_t attrs[2048],const uint8_t bank3[16384],spang_tour_write write,void *context);

#endif
