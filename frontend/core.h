/* R10 adaptation of C13 vendor/frontend_f01, preserved separately.
 * Native Super Pang World 900914 presentation controller.
 * Newly written C translation: no CPU interpreter and no captured-frame stream.
 * Static-ROM verified counters; absolute MAME/hardware phase still unvalidated.
 */
#ifndef R10_FRONTEND_CORE_H
#define R10_FRONTEND_CORE_H
#include <stdint.h>
#include <stddef.h>
#include "records.h"
typedef enum {SPFE_VERSION,SPFE_WARNING,SPFE_TITLE,SPFE_CREDIT_TITLE,
 SPFE_MODE,SPFE_CONFIRMED,SPFE_ATTRACT_REQUEST,SPFE_STAGE_SELECT,
 SPFE_RECORDS,SPFE_NAME_ENTRY} spfe_state_t;
enum {SPFE_TOUR=0,SPFE_PANIC=2};
enum {SPFE_LEFT=1,SPFE_RIGHT=2,SPFE_FIRE=4,SPFE_DOWN=8,SPFE_COIN=16,SPFE_START=32,SPFE_UP=64};
typedef struct {uint16_t code; int16_t x,y;uint8_t palette,layer,actor_group;} spfe_sprite_t;
typedef struct {
 uint8_t active,type,variant,state,sub,count,phase;
 uint16_t delay,shape,motion;int32_t x;uint16_t y;int16_t vx,vy;
} spfe_actor_t;
typedef struct {
 const uint8_t *fixed; const uint8_t *banks[16];
 uint16_t tiles[2048],palette[2048];uint8_t attributes[2048];
 spfe_sprite_t sprites[127];uint16_t sprite_count;
 spfe_actor_t actor[8];
 spfe_state_t state;uint32_t ticks,state_ticks,background_serial,palette_serial;
 uint16_t previous_input,hold;uint8_t mode,countdown_step,countdown_timer;
 uint8_t confirm_phase,confirm_timer,confirm_flashes,confirm_toggle;
 uint8_t heading_timer,heading_color,panel_pending,stage_select,credits;
 uint8_t error,sound_count,sounds[16],sound_kinds[16],sound_parameters[16];
 uint8_t title_blink,title_blink_phase,displayed_credits,title_ready,title_settled;
 uint8_t select_index,select_sub,select_repeat,select_last,select_delay,select_finish,select_done;
 uint16_t select_remaining;
 uint8_t record_row[2],record_phase[2],record_timer[2];
 uint8_t name_draw_count[2];
} spfe_t;
typedef struct {uint64_t phase;} spfe_clock_t;
enum {SPFE_SOUND_COMMAND=1,SPFE_SOUND_OKI=2};
int spfe_init(spfe_t *,const uint8_t *,size_t);
/* References existing immutable ROM banks; no full-ROM allocation/copy. */
int spfe_init_mapped(spfe_t *,const uint8_t *,const uint8_t *const banks[16]);
void spfe_tick_edges(spfe_t *,uint16_t input,uint16_t pressed);
void spfe_return_to_title(spfe_t *);
/* Console navigation uses the original settled title and font cells. */
void spfe_console_home(spfe_t *,unsigned selected);
void spfe_console_options(spfe_t *,const uint8_t settings[8],unsigned selected,int center);
void spfe_console_notice(spfe_t *,const char *message);
void spfe_open_stage_select(spfe_t *);
void spfe_tick(spfe_t *,uint16_t);
/* Diagnostic entry and integration seam. Normal path requires coin + Start. */
void spfe_open_mode(spfe_t *);
void spfe_open_records(spfe_t *,const spang_records *,unsigned mode);
void spfe_tick_records(spfe_t *,const spang_records *);
void spfe_draw_name(spfe_t *,const spang_record_entry *,unsigned owner,int initial);
void spfe_clear_player_footer(spfe_t *,unsigned owner);
void spfe_draw_player_gameover(spfe_t *,unsigned owner);
/* sr_num/sr_den is the measured or platform-derived output refresh rate. */
unsigned spfe_clock_step(spfe_clock_t *,uint32_t sr_num,uint32_t sr_den);
/* Hardware-neutral viewport layout. Title is proportional 11/12 resampling;
 * other screens use tile-aligned reflow. Returns 0 for off-screen cells. */
int spfe_cell_destination(spfe_state_t state,unsigned index,int *x,int *y);
void spfe_sprite_destination(spfe_state_t state,const spfe_sprite_t *,int *,int *,int *,int *);
#endif
