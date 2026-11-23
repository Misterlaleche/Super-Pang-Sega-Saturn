#ifndef SPANG_ROUND_CLEAR_H
#define SPANG_ROUND_CLEAR_H
#include <stdint.h>
/* A8F1, bank 0: fixed victory pose and reserve-life tile request.
 * Legacy output name "palette" actually contains the ROM DE character code
 * sent to fixed 166E/167C (life icons), NOT a sprite palette update.
 * FFFF means no icon update. Do not tick gameplay animation during victory.
 * Caller enters with the 38FF transition already applied (02=2,03=6,04=05=0). */
int spang_round_clear_player(uint8_t player[128],const uint8_t bank0[16384],uint16_t *palette);
/* 8477, bank 3: sliding STAGE CLEAR actor E1E0. Globals E080..E0FF.
 * effects[0]: video command (E18F, with E180=0C); 0 means none.
 * effects[1] bits: 1=produce sprite via bank 3 9550, 2=draw score box 8859,
 * 4=draw second special-bonus sprite using mutated actor, 8=score parameter5C
 * for owner (globals[4E]-2)/2 (original queue word0A5C or0C5C).
 * draw_actor receives the pre-update sprite input when bit 1 is set. Render
 * that snapshot, not the mutated actor: ROM draws before moving/removing it.
 * Includes E0CE2/4 special clear. Scheduler/render/score are separate. */
int spang_round_clear_actor(uint8_t actor[32],uint8_t globals[128],uint8_t lives1,uint8_t lives2,const uint8_t bank3[16384],uint8_t effects[2],uint8_t draw_actor[32]);
/* Bank 3 9550: cumulative signed X/Y offsets and original object bytes.
 * Returns sprite count, 0 for insufficient room, -1 for unsupported data.
 * Pass draw_actor, the pre-update snapshot returned above. */
int spang_round_clear_sprite(const uint8_t actor[32],const uint8_t bank3[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
/* Fixed 246F: three columns of two sprites for the wide victory pose. */
int spang_round_victory_sprite(const uint8_t player[128],uint8_t code,uint8_t attr,uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
#endif
