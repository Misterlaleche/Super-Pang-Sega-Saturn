#ifndef SPANG_ROUND_BONUS_BOX_H
#define SPANG_ROUND_BONUS_BOX_H
#include "spang_video_commands.h"
/* Bank3 8859, P1-only E0C1=2. Ordered border, attributes and ball-count
 * writes. digit_table is fixed152E DATA. Counter must be valid packed BCD.
 * queue[0:2] returns D:E for the separate 10E3 request (08:10). Caller sets
 * E14F=10 and consumes that queue request before these direct writes.
 * No invented label graphics: the queued original command owns them. */
int spang_round_bonus_box(uint8_t counter,const uint8_t digit_table[20],spang_video_write write,void *context,uint8_t queue[2]);
int spang_round_bonus_box_player(unsigned player,uint8_t counter,const uint8_t digit_table[20],spang_video_write write,void *context,uint8_t queue[2]);
/* Bank3 8599 P1-only branch (caller must have E0C1=2). Returns 1 exactly
 * once: caller then increments E152 and awards original A:DE from award
 * [low, middle, high], with E14F=10. Returns 0 on subsequent calls, -1 for
 * invalid selector/BCD. Score/life/HUD service remains caller-owned. */
int spang_round_bonus_p1(uint8_t actor[32],uint8_t timer[32],uint8_t retries,uint8_t group,const uint8_t bank3[16384],const uint8_t clock_tables[113],const uint8_t digit_table[20],spang_video_write write,void *context,uint8_t award[3]);
/* BANK3:8599..8703. Eligibility is E0C1 bits2/4, independent of active.
 * Returns awarded player mask1/2/3 once; wins receives only the sole winner.
 * awards contains each player's original low/middle/high BCD amount. */
int spang_round_bonus_players(uint8_t actor[32],uint8_t timer[32],uint8_t retries,uint8_t group,uint8_t eligibility,uint8_t pops1,uint8_t pops2,const uint8_t bank3[16384],const uint8_t clock_tables[113],const uint8_t digit_table[20],spang_video_write write,void *context,uint8_t awards[6],uint8_t *wins);
#endif
