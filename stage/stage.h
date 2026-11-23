#ifndef HK320_STAGE_H
#define HK320_STAGE_H
#include <stdint.h>

/* Portable original Tour/Panic players. No Jo/VDP/audio-device ownership.
 * The original visible world is x64..447,y8..247 (384x240 samples). Rendering
 * alone selects native320 placement and separately adapted sprite dimensions. */
enum { STAGE_LEFT=1,STAGE_RIGHT=2,STAGE_FIRE=4,STAGE_START=8,
       STAGE_UP=16,STAGE_DOWN=32,STAGE_BACK=64,STAGE_COIN=128 };
enum { STAGE_MODE_TOUR=0,STAGE_MODE_PANIC=2 };
typedef enum {
    STAGE_READY,STAGE_PLAYING,STAGE_DYING,STAGE_RETRY,
    STAGE_CLEAR,STAGE_BONUS,STAGE_COMPLETE,STAGE_GAME_OVER,STAGE_FAULT,STAGE_TIMEOUT,STAGE_MAP,STAGE_ENDING,STAGE_FRONTEND,STAGE_CONTINUE
} stage_phase;
typedef enum { STAGE_AUDIO_COMMAND=1,STAGE_AUDIO_OKI=2,STAGE_AUDIO_STOP=3 } stage_audio_kind;
typedef struct {
    uint32_t tick,sequence;
    uint8_t kind,command,parameter,reserved;
} stage_audio_event;
typedef struct {
    uint32_t tick,presentation_frames,tile_revision,palette_revision;
    int fault;
    stage_phase phase;
    uint16_t random_seed,object_count;
    uint8_t lives,retries,remaining_weight,ready_active;
    const uint8_t *objects; /*4096bytes; 32bytes/object, original first4bytes. */
    const uint8_t *attrs;   /*2048tile attributes. */
    const uint8_t *chars;   /*4096bytes, little-endian16-bit character codes. */
    const uint8_t *palette; /*4096bytes, little-endian original xRGB444words. */
    const uint8_t *player;  /*128original actor bytes; read-only. */
    const uint8_t *globals; /*128bytes E080..E0FF; read-only. */
    const uint8_t *balls;   /*1024bytes EA00..EDFF; read-only. */
    const uint8_t *items;   /*256bytes; read-only. */
    const uint8_t *timer;   /*32bytes E1C0..E1DF; original BCD time. */
    const uint8_t *best;    /*4 original BCD score bytes. */
    uint8_t stage_index,world_index,scene_index,animal_count;
    uint32_t scene_revision;
    const uint8_t *animals; /*320 bytes: eight original40-byte animal records.*/
    const uint8_t *player2; /*128 original actor bytes, identity1. */
    uint8_t player_mask;    /*E08C participant bits1/2, not an alive mask. */
    uint8_t records_mask;   /*Name/game-over footers currently own these HUDs. */
} stage_view;

void stage_init(void);
/* Snapshot a real game's RNG before returning to the frontend. Demos do not
 * replace this value. stage_init is the explicit cold reset. */
void stage_finish_session(void);
int stage_set_rules(unsigned difficulty,unsigned lives);
/* Start a fresh game from the frontend. Tour uses stage0..39; Panic starts
 * at level1 (start_stage0). Invalid requests leave the current game intact.
 * stage_init remains the ordinary Tour0 entry for existing callers. */
int stage_start_game(unsigned mode,unsigned start_stage);
int stage_start_game_players(unsigned mode,unsigned start_stage,unsigned player_mask);
/* Original DOWN+Tour selector also awards index*1000 and raises its initial
 * rank by5, including selected stage0. Ordinary Tour entry is unchanged. */
int stage_start_selected_game(unsigned start_stage);
int stage_start_selected_game_players(unsigned start_stage,unsigned player_mask);
/* Each call advances one logical arcade tick. START begins a fresh game only
 * after release completion/game over; it cannot silently abandon active play. */
void stage_tick(uint32_t held,uint32_t pressed);
void stage_tick_players(uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2);
/* Original2871/3000 cadence, referenced to nominal60Hz presentation. Edges
 * survive frames with no logical tick; deferred sound survives until drained. */
unsigned stage_advance_vblanks(unsigned elapsed,uint32_t held,uint32_t pressed);
unsigned stage_advance_vblanks_players(unsigned elapsed,uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2);
/* Shared credit counter is transferred at frontend/stage owner changes.
 * COIN is a separate edge; START only spends credits when entry is allowed. */
void stage_set_credits(unsigned credits);
unsigned stage_credits(void);
unsigned stage_player_can_start(unsigned player_index);
unsigned stage_game_finished(void);
int stage_take_finished_player(uint8_t out[128],unsigned *player_index);
unsigned stage_finished_mask(void);
/* The application registers the shared records owner once. Live stage ticks
 * call it after finish generation and the gameplay HUD, including every tick
 * in a batched VBlank advance. It consumes stage_take_finished_player events,
 * advances each original editor once, and returns owner bits whose name or
 * postgame footer still blocks a new entry. There is no independent clock.
 * Writes use original C800..CFFF attributes / D000..DFFF character bytes. */
typedef void (*stage_tile_write_fn)(void *context,uint16_t address,uint8_t value);
typedef unsigned (*stage_records_tick_fn)(unsigned mode,uint32_t tick,
    uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2,
    stage_tile_write_fn write,void *context);
void stage_set_records_tick(stage_records_tick_fn callback);
void stage_set_best(const uint8_t score[4]);
/* Original attract scenes: Tour scenarios0/1 normal demos,2/3 tutorials;
 * Panic scenario0. Inputs come from immutable original tuple streams. */
int stage_start_demo(unsigned mode,unsigned scenario);
unsigned stage_is_demo(void);
unsigned stage_demo_finished(void);
const stage_view *stage_get_view(void);
/* Semantic original ending page: tally0, staff1/2, final3. Read-only and0
 * outside the ending; COMPLETE retains the final page until START. */
unsigned stage_ending_page(void);
/* FIFO: caller drains after EVERY logical/presentation update. No filtering,
 * coalescing or sorting. STOP is lifecycle control; COMMAND and OKI retain
 * original identifiers/parameters. Overflow reports a stage fault. */
unsigned stage_audio_pending(void);
int stage_audio_pop(stage_audio_event *event);
/* Read-only data refs for interpretation/diagnostics, not CPU emulation.
 * Available banks0,1,2,3,5,10; unsupported index returns0. */
const uint8_t *stage_bank(unsigned index);
const uint8_t *stage_fixed(void);
/* Endian-independent logical/video-state digest. Excludes FIFO consumption
 * and presentation/revision telemetry; includes all actor pools and cadence. */
uint32_t stage_state_hash(void);

/* Renderer metadata; first4arcadebytes never change:
 *4tag;5..6 original existing anchorXLE(A6left/A7centre/AAleft);
 *7..8 groupIDLE(unique nonzero per list);9..10 sourceleftX signedLE;
 *11..12 sourcetopY signedLE;13..14 sourcewidthLE;15..16 sourceheightLE;
 *17role below. Bounds include original transparent/filler cells. */
#define STAGE_TAG_BALL 0xa6
#define STAGE_TAG_NATIVE 0xa7
#define STAGE_TAG_GROUP 0xaa
enum { STAGE_ROLE_BALL=1,STAGE_ROLE_PLAYER,STAGE_ROLE_WEAPON,STAGE_ROLE_POP,
       STAGE_ROLE_ITEM,STAGE_ROLE_READY,STAGE_ROLE_CLEAR,STAGE_ROLE_SCORE,
       STAGE_ROLE_SHIELD,STAGE_ROLE_CLOCK,STAGE_ROLE_LUCKY,STAGE_ROLE_PLATFORM,
       STAGE_ROLE_TIMEOUT,STAGE_ROLE_GAMEOVER,STAGE_ROLE_MAP_PLANE,
       STAGE_ROLE_MAP_MARKER,STAGE_ROLE_ANIMAL,STAGE_ROLE_LADDER,
       STAGE_ROLE_CONTINUE };
#endif
