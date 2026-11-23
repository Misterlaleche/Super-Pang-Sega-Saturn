#ifndef SPANG_BALL_PHASE_H
#define SPANG_BALL_PHASE_H
#include <stdint.h>
typedef uint8_t (*spang_ball_read_fn)(void *context, uint16_t address);
/* Bank 1: 9621..9640 (reverse==0), 9641..9695 (reverse!=0).
 * actor is a 32-byte arcade record; step is the word at E0A2.
 * read supplies the original bank-1 program data view at the requested
 * 16-bit address, including wrap; pointers must be valid. flag_e0aa is
 * cleared. Returns original carry, including height-add overflow on the
 * non-reflected paths. No collision, scheduling or sprite effects here. */
uint8_t spang_ball_phase_step(uint8_t actor[32], uint16_t step, int reverse,
    spang_ball_read_fn read, void *context, uint8_t *flag_e0aa);
#endif
