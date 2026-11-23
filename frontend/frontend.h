#ifndef R10_FRONTEND_H
#define R10_FRONTEND_H
#include <stdint.h>
#include "stage/stage.h"
enum {
 FRONTEND_VERSION=0,FRONTEND_WARNING,FRONTEND_TITLE,FRONTEND_CREDIT_TITLE,
 FRONTEND_MODE,FRONTEND_CONFIRMED,FRONTEND_ATTRACT,FRONTEND_STAGE_SELECT,
 FRONTEND_RECORDS,FRONTEND_NAME_ENTRY,FRONTEND_HOME,FRONTEND_OPTIONS,FRONTEND_CENTER
};
void frontend_init(void);
void frontend_return_to_title(void);
/* Advance at2871/3000 logical ticks per nominal60Hz field. Press edges survive
 * fields with no logical tick. Handoff halts controller time until return. */
unsigned frontend_advance_vblanks(unsigned elapsed,uint32_t held,uint32_t pressed);
unsigned frontend_advance_vblanks_players(unsigned elapsed,uint32_t held1,uint32_t pressed1,
                                         uint32_t held2,uint32_t pressed2);
/* Original byte buffers are LE, with phase FRONTEND and scene64+screen.
 * Sprite first4bytes are original; metadata byte19 retains F01 layer0/1/2. */
const stage_view *frontend_get_view(void);
/* Exactly one event per confirmed game. Mode Tour0/Panic2, stage0..39. */
int frontend_take_start(unsigned *mode,unsigned *stage);
int frontend_take_start_players(unsigned *mode,unsigned *stage,unsigned *player_mask);
/* Attract ownership freezes frontend time. Tour scenarios0/1 are the normal
 * demonstrations;2/3 the two original tutorials. Panic uses scenario0. */
int frontend_take_demo(unsigned *mode,unsigned *scenario);
void frontend_demo_finished(void);
void frontend_interrupt_demo(unsigned player_index);
void frontend_set_credits(unsigned credits);
void frontend_save_notice(int result);
/* Only committed record bytes are persisted; live name editors stay in RAM. */
unsigned frontend_records_revision(void);
int frontend_records_export(uint8_t out[192]);
int frontend_records_import(const uint8_t data[192]);
/* Register this with stage_set_records_tick once. It consumes individual
 * finish events on the stage logical tick, draws only their original lower
 * HUD cells, and returns actors still owned by name/game-over processing.
 * PLAY ownership and stage music stay active; drain frontend FIFO after the
 * stage FIFO for live F13. No frontend clock/profile preparation is needed. */
unsigned frontend_stage_records_tick(unsigned mode,uint32_t tick,uint32_t held1,uint32_t pressed1,
                                     uint32_t held2,uint32_t pressed2,stage_tile_write_fn write,void *context);
/* Direct result wrapper and final shared handoff. The final function keeps
 * live names/timers, suppresses duplicate snapshots, and includes participants
 * whose original E08C bit has already cleared before displaying records. */
void frontend_finish_player(unsigned mode,const uint8_t player[128],unsigned player_index);
void frontend_finish_game(unsigned mode,const uint8_t player1[128],
                          const uint8_t player2[128],unsigned player_mask);
const uint8_t *frontend_best(unsigned mode);
/*0 menu/attract,1 names/postgame. Main prepares this profile BEFORE draining
 * new FIFO commands. STOP precedes the first52/50 on profile entry. */
unsigned frontend_audio_profile(void);
/* Ordered FIFO; never filter/coalesce. Overflow is a visible controller fault. */
int frontend_audio_pop(stage_audio_event *event);
unsigned frontend_screen_kind(void);
enum {FRONTEND_TRANSITION_TICKS=12};
/* Shared ease-in/ease-out curve,0..255, for the two presentation owners.
 * A game handoff is emitted only at full black. Main retains that level until
 * the new scene is committed, then uses this curve in reverse for12 ticks. */
unsigned frontend_transition_level(unsigned step);
/* Presentation offset0..255. The pending-scene clock gate holds full black
 * across title/mode/stage-grid commits; source palettes stay authoritative. */
unsigned frontend_transition_darkness(void);
unsigned frontend_credits(void);
/* True at handoff only when the original stage selector was used, even stage0. */
unsigned frontend_stage_select_used(void);
#endif
