#ifndef SPANG_PLAYER_DAMAGE_H
#define SPANG_PLAYER_DAMAGE_H
#include <stdint.h>
/* B937..BCC2: normal, unshielded player against 32 original ball slots.
 * frame is E089, blocked is E0AB; updates E0AD through dead_flags.
 * Return 1 on impact, 0 on no impact, -1 for unsupported shield/selector. */
int spang_player_contact(uint8_t player[128],const uint8_t balls[1024],
 uint8_t frame,uint8_t blocked,uint8_t *dead_flags);
/* A95E death dispatcher: actor changes, original ROM velocities/animations.
 * sound is 0xff if none; palette is 0xff or the original AF20 source phase.
 * Screen-shake globals and audio playback are outside this pure routine. */
int spang_player_death(uint8_t player[128],const uint8_t bank[16384],
 uint8_t *sound,uint8_t *palette);
/* 2702/2B9B: four-tile death sprite with full 16-bit Y clipping. */
int spang_player_death_sprite(const uint8_t player[128],uint8_t code,uint8_t attr,
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
/* AAF5..AB4B actor effects for loss of one life; caller owns round restart. */
uint8_t spang_player_life_end(uint8_t player[128]);
#endif
