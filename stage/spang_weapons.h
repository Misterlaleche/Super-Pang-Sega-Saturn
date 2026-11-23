#ifndef SPANG_WEAPONS_H
#define SPANG_WEAPONS_H
#include <stdint.h>
/* One player's original two ropes and twelve 24-byte bullets. The owner
 * identity comes from player[17]; projectile[17] retains original 0/2. */
typedef struct {
 uint8_t harpoons[2][32],bullets[12][24];
 uint8_t harpoon_draw[2][32],bullet_draw[12][24];
 uint16_t bullet_visible;
 uint8_t harpoon_visible;
} spang_weapons;
typedef void (*spang_weapon_tile_fn)(void *context,const uint8_t actor[32],int count);
/* BANK2 fauna contact follows balls/lucky; nonzero unwinds past the
 * remaining task, including a sticky rope's lifetime decrement. */
typedef int (*spang_weapon_contact_fn)(void *context,uint8_t *actor,int bullet,uint8_t frame);
typedef void (*spang_weapon_sound_fn)(void *context,unsigned command);
void spang_weapons_init(spang_weapons *w);
/* Call on an eligible shot edge. Returns 0 blocked, 1 rope, 1..3 gun pellets,
 * or negative unsupported input. Player pose/cadence remains caller-owned. */
int spang_weapons_fire(spang_weapons *w,uint8_t player[128],const uint8_t bank2[16384]);
/* advance=0 retains snapshots and all actor bytes (death/paused world).
 * fire_edge is the new C edge, also consumed by attached sticky ropes.
 * tile callback receives the normalized actor once after grid mutation.
 * actor29/30 is start address; actor31&64 means upward vertical stride64,
 * otherwise horizontal with low-byte wrap, for count cells. */
int spang_weapons_step(spang_weapons *w,uint8_t frame,int fire_edge,int advance,
 const uint8_t bank2[16384],uint8_t grid[2048],uint8_t balls[1024],uint8_t player[128],
 spang_weapon_tile_fn tile_removed,void *context);
int spang_weapons_panic_step(spang_weapons *w,uint8_t frame,int fire_edge,int advance,
 const uint8_t bank2[16384],uint8_t grid[2048],uint8_t balls[1024],uint8_t player[128],
 spang_weapon_tile_fn tile_removed,void *context,uint8_t globals[128],uint8_t player2[128]);
/* FIXED0417/041A: all four ropes precede all 24 bullets. Original lucky
 * and fauna sampling uses globals[9], independently of motion cadence.
 * No extra persistent state; both pools keep their pre-contact snapshots. */
int spang_weapons_round_step(spang_weapons *w1,spang_weapons *w2,
 uint8_t frame,const int fire_edges[2],int advance,const uint8_t bank2[16384],
 uint8_t grid[2048],uint8_t balls[1024],uint8_t player1[128],uint8_t player2[128],
 uint8_t globals[128],uint8_t lucky[32],spang_weapon_tile_fn tile_removed,
 spang_weapon_contact_fn contact,void *context);
/* Runtime variant: original solid-grid F02 is dispatched immediately after
 * state86CD, preserving order with subsequent bullet/fauna contacts. Legacy
 * wrappers omit sound callbacks for device-free consumers. */
int spang_weapons_round_step_audio(spang_weapons *w1,spang_weapons *w2,
 uint8_t frame,const int fire_edges[2],int advance,const uint8_t bank2[16384],
 uint8_t grid[2048],uint8_t balls[1024],uint8_t player1[128],uint8_t player2[128],
 uint8_t globals[128],uint8_t lucky[32],spang_weapon_tile_fn tile_removed,
 spang_weapon_contact_fn contact,spang_weapon_sound_fn sound,void *context);
int spang_weapons_draw(const spang_weapons *w,const uint8_t bank2[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
#endif
