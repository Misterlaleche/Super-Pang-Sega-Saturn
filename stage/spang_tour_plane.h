#ifndef SPANG_TOUR_PLANE_H
#define SPANG_TOUR_PLANE_H
#include <stdint.h>
typedef void (*spang_tour_write)(void *context,uint16_t address,uint8_t value);
/* Original BANK3:8B22 ordinary flights for worlds0..3 and5.
 * input = current P1, previous P1, current P2, previous P2, lives P1/P2.
 * events[0] = sound queue command; events[1] = one-shot map completion.
 * Completion owner must perform original 2BA7/2C54 text/queue cleanup.
 * This is one actor, not the entire map or a complete scene transition. */
int spang_tour_plane_step(uint8_t actor[40],uint8_t globals[128],
 const uint8_t input[6],const uint8_t bank3[16384],
 spang_tour_write write,void *context,uint8_t events[2]);
#endif
