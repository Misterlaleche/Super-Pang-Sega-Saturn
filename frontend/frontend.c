#include "frontend.h"
#include "core.h"
#include "settings.h"
#include <string.h>

/* ART/data.c owns only the two additional immutable DATA banks. */
extern const uint8_t *frontend_data_bank(unsigned bank);
#define FRONTEND_AUDIO_CAPACITY 128u
#define ACTION_BITS (STAGE_START|STAGE_FIRE|STAGE_BACK)
static spfe_t frontend_core;
static spang_records frontend_records;
static stage_view frontend_view;
static uint8_t frontend_chars[4096],frontend_palette[4096],frontend_objects[4096];
static uint8_t frontend_globals[128],empty_state[1024];
static stage_audio_event frontend_fifo[FRONTEND_AUDIO_CAPACITY];
static unsigned fifo_head,fifo_count,audio_sequence,cadence,presentation_frames;
static unsigned scene_revision,old_screen,old_tiles,old_palette;
static unsigned queued_start,handoff_pending,handoff_halted,handoff_mode,handoff_stage;
static unsigned selected_stage,release_block[2],requested_players,handoff_players;
static uint32_t pending_pressed[2];
static unsigned demo_pending,demo_position,demo_mode,demo_scenario,audio_profile;
static unsigned postgame_mode,postgame_mask,postgame_timer[2],postgame_music;
static unsigned postgame_initial;
static unsigned options_selection,options_repeat,options_repeat_timer;
static const uint8_t *postgame_objects;
static unsigned postgame_object_count,postgame_scene,postgame_phase;
/* Compact score/progress signatures are only for deduplicating the final
 * shared handoff against the already consumed individual stage events. */
static uint8_t submitted[2][9],submitted_valid[2];
static int frontend_fault;
/* Presentation-only title/menu dissolve. Core palettes and M048 retain their
 * original ownership; the application applies a uniform hardware offset.
 *1: enter mode,2: reveal a committed screen,3: leave a confirmed selector. */
static unsigned mode_fade_phase,mode_fade_step;
static unsigned console_screen,console_selection;

static void put16(uint8_t *p,unsigned value){p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);}
static void queue_audio(unsigned kind,unsigned command,unsigned parameter)
{
 if(fifo_count==FRONTEND_AUDIO_CAPACITY){if(!frontend_fault)frontend_fault=-671;return;}
 stage_audio_event *event=frontend_fifo+(fifo_head+fifo_count)%FRONTEND_AUDIO_CAPACITY;
 event->tick=frontend_core.ticks;event->sequence=audio_sequence++;
 event->kind=(uint8_t)kind;event->command=(uint8_t)command;
 event->parameter=(uint8_t)parameter;event->reserved=0;fifo_count++;
}
static void collect_audio(void)
{
 for(unsigned n=0;n<frontend_core.sound_count;n++)
  queue_audio(frontend_core.sound_kinds[n]==SPFE_SOUND_OKI?STAGE_AUDIO_OKI:STAGE_AUDIO_COMMAND,
              frontend_core.sounds[n],frontend_core.sound_parameters[n]);
 frontend_core.sound_count=0;
}
static unsigned visible_screen(void)
{
 if(console_screen)return console_screen;
 return frontend_core.state==SPFE_CONFIRMED && frontend_core.select_done?
        FRONTEND_STAGE_SELECT:(unsigned)frontend_core.state;
}
static void update_view(void)
{
 spfe_t *f=&frontend_core;
 if(f->error && !frontend_fault)frontend_fault=-680-f->error;
 unsigned screen=visible_screen();
 if(old_screen!=screen){old_screen=screen;scene_revision++;}
 if(old_tiles!=f->background_serial){
  for(unsigned i=0;i<2048;i++)put16(frontend_chars+i*2,f->tiles[i]);
  old_tiles=f->background_serial;
 }
 if(old_palette!=f->palette_serial){
  for(unsigned i=0;i<2048;i++)put16(frontend_palette+i*2,f->palette[i]);
  old_palette=f->palette_serial;
 }
 if(screen!=FRONTEND_NAME_ENTRY)memset(frontend_objects,0,sizeof frontend_objects);
 for(unsigned i=0;i<f->sprite_count && screen!=FRONTEND_NAME_ENTRY;i++){
  const spfe_sprite_t *s=&f->sprites[i];uint8_t *p=frontend_objects+i*32;
  unsigned x=(unsigned)s->x&511u;
  p[0]=(uint8_t)s->code;p[1]=(uint8_t)(((s->code>>3)&0xe0)|s->palette|((x>>4)&16));
  p[2]=(uint8_t)s->y;p[3]=(uint8_t)x;p[4]=STAGE_TAG_GROUP;
  put16(p+5,x);put16(p+7,i+1);put16(p+9,x);put16(p+11,(unsigned)s->y);
  put16(p+13,16);put16(p+15,16);p[19]=s->layer;
  if(s->actor_group && s->actor_group<=8){
   const spfe_actor_t *a=f->actor+s->actor_group-1;const uint8_t *bank=f->banks[3];
   p[20]=s->actor_group;p[21]=a->type;put16(p+22,a->shape);
   put16(p+24,(unsigned)(a->x>>8));put16(p+26,a->y>>8);
   if(a->type==2){
    unsigned at=0x2bf3+a->variant,motion=bank[at]|((unsigned)bank[at+1]<<8);
    put16(p+28,bank[motion-0x8000+24]|((unsigned)bank[motion-0x8000+25]<<8));
    put16(p+30,bank[motion-0x8000+26]);
   }else if(a->type==4){
    unsigned at=0x33c1+a->variant;int speed=(int16_t)(bank[at]|((unsigned)bank[at+1]<<8));
    put16(p+28,(unsigned)((248*256+speed*43)>>8));put16(p+30,65);
   }
  }
 }
 if(screen!=FRONTEND_NAME_ENTRY){
  frontend_globals[0]=(uint8_t)f->state;frontend_globals[1]=f->confirm_phase;
  frontend_globals[2]=f->countdown_step;frontend_globals[3]=f->countdown_timer;
  frontend_globals[0x19]=(uint8_t)selected_stage;frontend_globals[0x1a]=f->mode;
 }
 frontend_globals[0x68]=f->stage_select;frontend_globals[0x69]=f->credits;
 frontend_globals[0x70]=f->title_settled;
 frontend_globals[0x71]=(uint8_t)console_selection;
 frontend_view.tick=f->ticks;frontend_view.presentation_frames=presentation_frames;
 frontend_view.tile_revision=f->background_serial;frontend_view.palette_revision=f->palette_serial;
 frontend_view.fault=frontend_fault;frontend_view.phase=STAGE_FRONTEND;
 frontend_view.object_count=screen==FRONTEND_NAME_ENTRY?(uint16_t)postgame_object_count:f->sprite_count;
 frontend_view.objects=screen==FRONTEND_NAME_ENTRY?postgame_objects:frontend_objects;frontend_view.attrs=f->attributes;
 frontend_view.chars=frontend_chars;frontend_view.palette=frontend_palette;
 if(screen!=FRONTEND_NAME_ENTRY){
  frontend_view.player=empty_state;frontend_view.player2=empty_state;frontend_view.balls=empty_state;
  frontend_view.items=empty_state;frontend_view.timer=empty_state;frontend_view.animals=empty_state;
  frontend_view.stage_index=(uint8_t)selected_stage;
 }
 frontend_view.globals=frontend_globals;frontend_view.best=frontend_best(f->mode);
 frontend_view.player_mask=(uint8_t)(screen==FRONTEND_NAME_ENTRY?postgame_mask:requested_players);
 if(screen==FRONTEND_NAME_ENTRY){frontend_globals[0x7c]=(uint8_t)postgame_scene;frontend_globals[0x7d]=(uint8_t)postgame_phase;}
 frontend_view.scene_index=(uint8_t)(64u+screen);frontend_view.scene_revision=scene_revision;
}
void frontend_init(void)
{
 spang_settings_reset();options_selection=options_repeat=options_repeat_timer=0;
 const uint8_t *banks[16]={0};
 banks[0]=stage_bank(0);banks[3]=stage_bank(3);banks[5]=stage_bank(5);banks[10]=stage_bank(10);
 banks[12]=frontend_data_bank(12);banks[13]=frontend_data_bank(13);
 memset(&frontend_view,0,sizeof frontend_view);memset(frontend_globals,0,sizeof frontend_globals);
 fifo_head=fifo_count=audio_sequence=cadence=presentation_frames=scene_revision=0;
 queued_start=handoff_pending=handoff_halted=selected_stage=0;
 release_block[0]=release_block[1]=pending_pressed[0]=pending_pressed[1]=0;
 requested_players=handoff_players=1;demo_pending=demo_position=audio_profile=0;
 postgame_mask=postgame_timer[0]=postgame_timer[1]=postgame_music=postgame_initial=0;
 postgame_objects=frontend_objects;postgame_object_count=postgame_scene=postgame_phase=0;
 memset(submitted_valid,0,sizeof submitted_valid);
 frontend_fault=0;old_screen=old_tiles=old_palette=~0u;
 mode_fade_phase=mode_fade_step=console_screen=console_selection=0;
 if(!spfe_init_mapped(&frontend_core,stage_fixed(),banks))frontend_fault=-670;
 spang_records_init(&frontend_records,banks[0]);
 update_view();
}
void frontend_return_to_title(void)
{
 console_screen=console_selection=0;
 mode_fade_phase=mode_fade_step=0;
 /* The explicit Saturn return ends any partially entered name. Do not leave
  * an old row owned by an editor when another game starts. */
 for(unsigned n=0;n<2;++n)if(frontend_records.entry[n].phase){
  frontend_records.entry[n].phase=2;
  (void)spang_records_tick(&frontend_records,n,0,0,frontend_core.banks[0]);
 }
 postgame_mask=postgame_initial=postgame_timer[0]=postgame_timer[1]=postgame_music=0;
 /* Retain FIFO ordering and credits; time/resource owners are not reset. */
 queue_audio(STAGE_AUDIO_STOP,0,0);queued_start=handoff_pending=handoff_halted=0;
 demo_pending=0;audio_profile=0;requested_players=1;
 selected_stage=0;cadence=0;pending_pressed[0]=pending_pressed[1]=0;
 release_block[0]=release_block[1]=ACTION_BITS;
 spfe_return_to_title(&frontend_core);update_view();
}
static void console_home(unsigned owner,unsigned selected)
{
 console_screen=FRONTEND_HOME;console_selection=selected;
 requested_players=owner?3:1;queued_start=0;mode_fade_phase=mode_fade_step=0;
 spfe_console_home(&frontend_core,selected);
 release_block[0]|=ACTION_BITS|STAGE_UP|STAGE_DOWN;
 release_block[1]|=ACTION_BITS|STAGE_UP|STAGE_DOWN;
 ++scene_revision;
}
static void options_draw(void)
{
 uint8_t settings[8];spang_settings_export(settings);
 spfe_console_options(&frontend_core,settings,options_selection,console_screen==FRONTEND_CENTER);
 /* update_view owns scene changes. Cursor/value edits only dirty their tiles. */
}
static void options_tick(uint32_t held,uint32_t pressed)
{
 unsigned directions=held&(STAGE_LEFT|STAGE_RIGHT|STAGE_UP|STAGE_DOWN);
 if(directions!=options_repeat){options_repeat=directions;options_repeat_timer=20;}
 else if(directions && options_repeat_timer && !--options_repeat_timer){pressed|=directions;options_repeat_timer=4;}
 if(pressed&STAGE_BACK){
  if(console_screen==FRONTEND_CENTER){console_screen=FRONTEND_OPTIONS;options_draw();}
  else console_home(requested_players==3,1);
  release_block[0]|=ACTION_BITS|15u|STAGE_UP|STAGE_DOWN;release_block[1]|=ACTION_BITS|15u|STAGE_UP|STAGE_DOWN;
  queue_audio(STAGE_AUDIO_COMMAND,13,0);return;
 }
 if(console_screen==FRONTEND_CENTER){
  int dx=(!!(pressed&STAGE_RIGHT))-(!!(pressed&STAGE_LEFT));
  int dy=(!!(pressed&STAGE_DOWN))-(!!(pressed&STAGE_UP));
  unsigned revision=spang_settings_revision();
  if(pressed&STAGE_FIRE)spang_settings_center();else spang_settings_move(dx,dy);
  if(revision!=spang_settings_revision())options_draw();
  return;
 }
 if((pressed&(STAGE_UP|STAGE_DOWN)) && (held&(STAGE_UP|STAGE_DOWN))!=(STAGE_UP|STAGE_DOWN)){
  options_selection=(options_selection+((pressed&STAGE_DOWN)?1:7))%8;options_draw();queue_audio(STAGE_AUDIO_COMMAND,13,0);return;
 }
 if(options_selection<6){
  int direction=(!!(pressed&STAGE_RIGHT))-(!!(pressed&STAGE_LEFT));
  if(spang_settings_adjust(options_selection,direction)){options_draw();queue_audio(STAGE_AUDIO_COMMAND,13,0);}
 }else if(pressed&(STAGE_START|STAGE_FIRE)){
  if(options_selection==6){console_screen=FRONTEND_CENTER;options_draw();}
  else console_home(requested_players==3,1);
  release_block[0]|=ACTION_BITS;release_block[1]|=ACTION_BITS;
 }
}
static void console_tick(uint32_t held,uint32_t pressed)
{
 spfe_t *f=&frontend_core;++f->ticks;++f->state_ticks;
 if(console_screen==FRONTEND_OPTIONS||console_screen==FRONTEND_CENTER){options_tick(held,pressed);return;}
 if(pressed&STAGE_BACK){frontend_return_to_title();return;}
 if((pressed&(STAGE_UP|STAGE_DOWN)) && (held&(STAGE_UP|STAGE_DOWN))!=(STAGE_UP|STAGE_DOWN)){
  unsigned next=(pressed&STAGE_DOWN)?1:0;
  if(next!=console_selection){console_selection=next;spfe_console_home(f,next);++scene_revision;queue_audio(STAGE_AUDIO_COMMAND,13,0);}
  return;
 }
 if(!(pressed&(STAGE_START|STAGE_FIRE)))return;
 release_block[0]|=ACTION_BITS;release_block[1]|=ACTION_BITS;
 if(console_selection){
  console_screen=FRONTEND_OPTIONS;options_selection=options_repeat=options_repeat_timer=0;options_draw();
  queue_audio(STAGE_AUDIO_COMMAND,3,0);return;
 }
 unsigned needed=requested_players==3?2:1;
 while(f->credits<needed){++f->credits;queue_audio(STAGE_AUDIO_OKI,0x17,0x80);}
 console_screen=0;mode_fade_phase=1;mode_fade_step=1;
 /* The existing fade performs the single mode-entry START at full black. */
 ++scene_revision;
}
static uint16_t input_bits(uint32_t input)
{
 uint16_t bits=0;
 if(input&STAGE_LEFT)bits|=SPFE_LEFT;
 if(input&STAGE_RIGHT)bits|=SPFE_RIGHT;
 if(input&STAGE_FIRE)bits|=SPFE_FIRE;
 if(input&STAGE_DOWN)bits|=SPFE_DOWN;
 if(input&STAGE_UP)bits|=SPFE_UP;
 return bits;
}
static void begin_handoff(void)
{
 handoff_mode=frontend_core.mode;handoff_stage=handoff_mode==STAGE_MODE_TOUR?selected_stage:0;
 handoff_players=requested_players;memset(submitted_valid,0,sizeof submitted_valid);
 postgame_mask=postgame_initial=postgame_timer[0]=postgame_timer[1]=postgame_music=0;
 queue_audio(STAGE_AUDIO_STOP,0,0);
 if(!frontend_fault)handoff_pending=handoff_halted=1;
}
static void begin_demo(void)
{
 static const uint8_t modes[5]={0,0,0,0,2},scenarios[5]={2,0,3,1,0};
 demo_mode=modes[demo_position];demo_scenario=scenarios[demo_position];demo_pending=1;
 frontend_core.state=SPFE_ATTRACT_REQUEST;handoff_halted=1;
 queue_audio(STAGE_AUDIO_STOP,0,0);
}
static void show_records(unsigned mode)
{
 handoff_halted=handoff_pending=demo_pending=0;cadence=0;
 pending_pressed[0]=pending_pressed[1]=0;spfe_open_records(&frontend_core,&frontend_records,mode);update_view();
}
static uint8_t raw_input(uint32_t bits)
{return (uint8_t)(((bits&STAGE_LEFT)?0x20:0)|((bits&STAGE_RIGHT)?0x10:0)|((bits&STAGE_FIRE)?8:0));}
static unsigned editing_mask(void)
{return (frontend_records.entry[0].phase?1u:0u)|(frontend_records.entry[1].phase?2u:0u);}
static unsigned footer_mask(void)
{return editing_mask()|postgame_initial|(postgame_timer[0]?1u:0u)|(postgame_timer[1]?2u:0u);}
static void end_player(unsigned owner)
{
 if(postgame_mode==SPFE_PANIC){postgame_timer[owner]=1;return;}
 spfe_draw_player_gameover(&frontend_core,owner);postgame_timer[owner]=212;
 /* BANK0:9AA4 compares priority10 against the partner. While stage owns PLAY
  * its music continues; only the final frontend owner requests M050. */
 if(audio_profile && !editing_mask() && !postgame_music){queue_audio(STAGE_AUDIO_COMMAND,50,0);postgame_music=1;}
}
static void initial_player(unsigned owner)
{
 if(frontend_records.entry[owner].phase)spfe_draw_name(&frontend_core,frontend_records.entry+owner,owner,1);
 else end_player(owner);
 postgame_initial&=~(1u<<owner);
}
static void tick_player(unsigned owner,uint32_t held,uint32_t pressed)
{
 spfe_t *f=&frontend_core;
 if(frontend_records.entry[owner].phase){
  unsigned changes=spang_records_tick(&frontend_records,owner,raw_input(held),raw_input(pressed),f->banks[0]);
  if(changes&SPANG_RECORD_SOUND)queue_audio(STAGE_AUDIO_COMMAND,13,0);
  if(changes&SPANG_RECORD_FINISHED)end_player(owner);
  else if(changes&SPANG_RECORD_CHANGED)spfe_draw_name(f,frontend_records.entry+owner,owner,0);
 }else if(postgame_timer[owner]){
  --postgame_timer[owner];if(postgame_timer[owner]==30)spfe_clear_player_footer(f,owner);
 }
}
static void postgame_tick(uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2)
{
 spfe_t *f=&frontend_core;++f->ticks;++f->state_ticks;
 const uint32_t held[2]={held1,held2},pressed[2]={pressed1,pressed2};
 for(unsigned n=0;n<2;++n)if(postgame_mask&(1u<<n)){
  tick_player(n,held[n],pressed[n]);
 }
 if(!footer_mask())show_records(postgame_mode);
}
static void logical_tick(uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2)
{
 spfe_t *f=&frontend_core;
 spfe_state_t previous_state=f->state;
 if(f->state==SPFE_NAME_ENTRY){postgame_tick(held1,pressed1,held2,pressed2);update_view();return;}
 uint32_t held=held1|held2,pressed=pressed1|pressed2;
 if(console_screen){console_tick(held,pressed);update_view();return;}
 if((pressed&STAGE_START) && (f->state==SPFE_TITLE || f->state==SPFE_CREDIT_TITLE || f->state==SPFE_ATTRACT_REQUEST)){
  console_home((pressed2&STAGE_START)!=0,0);update_view();return;
 }
 if(pressed&STAGE_BACK){f->ticks++;frontend_return_to_title();return;}
 if(mode_fade_phase){
  if(mode_fade_phase==1 && ++mode_fade_step==FRONTEND_TRANSITION_TICKS){
   /* One deferred original START spends the credit and queues M048 once.
    * Keep full black until main's scene commit gate releases this clock. */
   spfe_tick_edges(f,SPFE_START,SPFE_START);collect_audio();queued_start=0;
   if(requested_players==3 && f->credits)--f->credits;
   mode_fade_phase=2;
  }else if(mode_fade_phase==3){
   ++f->ticks;++f->state_ticks;
   if(++mode_fade_step==FRONTEND_TRANSITION_TICKS){
    if(f->mode==SPFE_TOUR && f->stage_select && !f->select_done){
     spfe_open_stage_select(f);mode_fade_phase=2;
    }else{selected_stage=f->select_done?f->select_index:0;begin_handoff();}
   }
  }else{
   ++f->ticks;++f->state_ticks;
   if(mode_fade_phase==2 && !--mode_fade_step){
    mode_fade_phase=0;
    release_block[0]|=held1&ACTION_BITS;release_block[1]|=held2&ACTION_BITS;
   }
  }
  update_view();return;
 }
 if(f->state==SPFE_RECORDS){
  if(pressed&STAGE_START){frontend_interrupt_demo((pressed2&STAGE_START)!=0);return;}
  spfe_tick_records(f,&frontend_records);if(!f->hold)frontend_return_to_title();update_view();return;
 }
 uint16_t input=input_bits(held|pressed),edge=input_bits(pressed);
 if(pressed&(STAGE_COIN)){input|=SPFE_COIN;edge|=SPFE_COIN;}
 if((pressed2&STAGE_START) && (f->state==SPFE_MODE || f->state==SPFE_STAGE_SELECT) && requested_players!=3){
  requested_players=3;
  /* Saturn START2 supplies the missing virtual credit once; mode selection
   * already spent player1's credit. This edge requests the second player. */
  if(f->credits)--f->credits;else queue_audio(STAGE_AUDIO_OKI,0x17,0x80);
  pressed2&=~STAGE_START;pressed=pressed1|pressed2;
 }
 if(queued_start && f->state==SPFE_CREDIT_TITLE){
  queued_start=0;input|=SPFE_START;edge|=SPFE_START;
 }else if((pressed&STAGE_START) && (f->state==SPFE_TITLE || f->state==SPFE_ATTRACT_REQUEST)){
  /* One physical Start becomes one original coin tick, then one original Start
   * tick. The held button never becomes a selector confirmation edge. */
  requested_players=(pressed2&STAGE_START)?3:1;
  unsigned needed=requested_players==3?2:1;
  if(f->credits>=needed){input|=SPFE_START;edge|=SPFE_START;}
  else{
   while((unsigned)f->credits+1u<needed){++f->credits;queue_audio(STAGE_AUDIO_OKI,0x17,0x80);}
   input|=SPFE_COIN;edge|=SPFE_COIN;
  }
  queued_start=1;
 }else if((pressed&STAGE_START) && f->state==SPFE_CREDIT_TITLE){
  requested_players=(pressed2&STAGE_START)?3:1;
  unsigned needed=requested_players==3?2:1;
  while(f->credits<needed){++f->credits;queue_audio(STAGE_AUDIO_OKI,0x17,0x80);}
  input|=SPFE_START;edge|=SPFE_START;
 }else if((pressed&STAGE_START) && (f->state==SPFE_MODE || f->state==SPFE_STAGE_SELECT)){
  input|=SPFE_FIRE;edge|=SPFE_FIRE;
 }
 if(f->state==SPFE_CREDIT_TITLE && (edge&SPFE_START) && f->credits){
  mode_fade_phase=1;mode_fade_step=1;
  ++f->ticks;++f->state_ticks;update_view();return;
 }
 spfe_tick_edges(f,input,edge);collect_audio();
 if(previous_state==SPFE_CREDIT_TITLE && f->state==SPFE_MODE){
  queued_start=0;if(requested_players==3 && f->credits)--f->credits;
 }
 if(f->state==SPFE_CONFIRMED){
  /* The original flash/confirm interval is already complete. Hold its final
   * pose while leaving, and publish the next owner only once it is black. */
  mode_fade_phase=3;mode_fade_step=1;
  release_block[0]|=held1&ACTION_BITS;release_block[1]|=held2&ACTION_BITS;
 }
 if(f->state==SPFE_STAGE_SELECT)selected_stage=f->select_index;
 if(f->state==SPFE_ATTRACT_REQUEST && !handoff_halted)begin_demo();
 update_view();
}
unsigned frontend_advance_vblanks(unsigned elapsed,uint32_t held,uint32_t pressed)
{return frontend_advance_vblanks_players(elapsed,held,pressed,0,0);}
unsigned frontend_advance_vblanks_players(unsigned elapsed,uint32_t held1,uint32_t pressed1,
                                         uint32_t held2,uint32_t pressed2)
{
 if(handoff_halted || frontend_fault)return 0;
 presentation_frames+=elapsed;release_block[0]&=held1;release_block[1]&=held2;
 held1&=~release_block[0];pressed1&=~release_block[0];pending_pressed[0]|=pressed1;
 held2&=~release_block[1];pressed2&=~release_block[1];pending_pressed[1]|=pressed2;
 uint64_t total=(uint64_t)cadence+(uint64_t)elapsed*2871u;
 unsigned due=(unsigned)(total/3000u),completed=0;cadence=(unsigned)(total%3000u);
 while(due--){
  unsigned before_scene=scene_revision;
  logical_tick(held1,pending_pressed[0],held2,pending_pressed[1]);pending_pressed[0]=pending_pressed[1]=0;completed++;
  if(frontend_fault || handoff_halted)break;
  if(before_scene!=scene_revision && console_screen){cadence=0;break;}
  /* A batched caller cannot fade in the new mode before it is displayed. */
  if(mode_fade_phase==2 && before_scene!=scene_revision){cadence=0;break;}
 }
 update_view();return completed;
}
const stage_view *frontend_get_view(void){return &frontend_view;}
unsigned frontend_transition_level(unsigned step)
{
 /* Rounded cubic smoothstep. Authored integer steps avoid a floating-point
  * dependency and leave more readable time at either end of the dissolve. */
 static const uint8_t levels[FRONTEND_TRANSITION_TICKS+1]={0,5,19,40,66,96,128,159,189,215,236,250,255};
 return levels[step>FRONTEND_TRANSITION_TICKS?FRONTEND_TRANSITION_TICKS:step];
}
unsigned frontend_transition_darkness(void)
{return frontend_transition_level(mode_fade_step);}
int frontend_take_start(unsigned *mode,unsigned *stage)
{unsigned mask;return frontend_take_start_players(mode,stage,&mask);}
int frontend_take_start_players(unsigned *mode,unsigned *stage,unsigned *mask)
{
 if(!handoff_pending || !mode || !stage || !mask)return 0;
 *mode=handoff_mode;*stage=handoff_stage;*mask=handoff_players;handoff_pending=0;return 1;
}
int frontend_take_demo(unsigned *mode,unsigned *scenario)
{
 if(!demo_pending || !mode || !scenario)return 0;
 *mode=demo_mode;*scenario=demo_scenario;demo_pending=0;return 1;
}
void frontend_demo_finished(void)
{
 if(!handoff_halted || frontend_core.state!=SPFE_ATTRACT_REQUEST)return;
 handoff_halted=0;queue_audio(STAGE_AUDIO_STOP,0,0);
 switch(demo_position++){
 case 0:case 2:begin_demo();break;
 case 1:show_records(SPFE_TOUR);break;
 case 3:show_records(SPFE_PANIC);break;
 default:demo_position=0;frontend_return_to_title();break;
 }
 update_view();
}
void frontend_interrupt_demo(unsigned owner)
{
 frontend_return_to_title();console_home(owner,0);update_view();
}
void frontend_set_credits(unsigned credits)
{frontend_core.credits=(uint8_t)(credits>99?99:credits);update_view();}
static void result_signature(unsigned mode,const uint8_t *player,uint8_t signature[9])
{
 const uint8_t *score=memcmp(player+0x40,player+0x44,4)>0?player+0x40:player+0x44;
 memcpy(signature,score,4);signature[4]=(uint8_t)mode;signature[5]=player[0x48];
 signature[6]=player[0x49];signature[7]=player[0x4d];signature[8]=player[0x4e];
}
void frontend_finish_player(unsigned mode,const uint8_t player[128],unsigned owner)
{
 if(!player || owner>1 || (mode!=SPFE_TOUR && mode!=SPFE_PANIC))return;
 (void)spang_records_submit(&frontend_records,mode,player,owner,stage_fixed());
 result_signature(mode,player,submitted[owner]);submitted_valid[owner]=1;
 postgame_mode=mode;postgame_mask|=1u<<owner;postgame_initial|=1u<<owner;postgame_timer[owner]=0;
}
static void copy_footer(const stage_view *source,unsigned owner)
{
 unsigned start=owner?0xddce:0xdd90;spfe_t *f=&frontend_core;
 for(unsigned y=0;y<4;++y)for(unsigned x=0;x<17;++x){
  unsigned index=(start-0xd000)/2+y*64+x;
  f->tiles[index]=(uint16_t)(source->chars[index*2]|(source->chars[index*2+1]<<8));
  f->attributes[index]=source->attrs[index];
 }
}
static void write_footer(const stage_view *source,unsigned owner,stage_tile_write_fn write,void *context)
{
 unsigned start=owner?0xddce:0xdd90;const spfe_t *f=&frontend_core;
 for(unsigned y=0;y<4;++y)for(unsigned x=0;x<17;++x){
  unsigned index=(start-0xd000)/2+y*64+x,code=f->tiles[index];
  if(source->attrs[index]!=f->attributes[index])write(context,(uint16_t)(0xc800+index),f->attributes[index]);
  if(source->chars[index*2]!=(uint8_t)code)write(context,(uint16_t)(0xd000+index*2),(uint8_t)code);
  if(source->chars[index*2+1]!=(uint8_t)(code>>8))write(context,(uint16_t)(0xd001+index*2),(uint8_t)(code>>8));
 }
}
unsigned frontend_stage_records_tick(unsigned mode,uint32_t tick,uint32_t held1,uint32_t pressed1,
                                     uint32_t held2,uint32_t pressed2,stage_tile_write_fn write,void *context)
{
 if(mode!=SPFE_TOUR && mode!=SPFE_PANIC)return 0;
 uint8_t player[128];unsigned owner;
 while(stage_take_finished_player(player,&owner))frontend_finish_player(mode,player,owner);
 const stage_view *source=stage_get_view();unsigned owned=footer_mask();
 if(!owned || !source || !source->chars || !source->attrs || !write)return owned;
 const uint32_t held[2]={held1,held2},pressed[2]={pressed1,pressed2};
 /* Stage owns the only clock. The editor uses its timestamp and never calls
  * frontend_advance_vblanks or changes the gameplay owner/view. */
 frontend_core.ticks=tick;
 for(unsigned n=0;n<2;++n)if(owned&(1u<<n)){
  if(postgame_initial&(1u<<n)){
   copy_footer(source,n);initial_player(n);
   /* Expiring CONTINUE may itself be a FIRE edge. BANK0:92B1 does not also
    * run the next state on that tick; default END must not eat that edge. */
  }else tick_player(n,held[n],pressed[n]);
  write_footer(source,n,write,context);
 }
 return footer_mask();
}
void frontend_finish_game(unsigned mode,const uint8_t p1[128],const uint8_t p2[128],unsigned mask)
{
 if((mode!=SPFE_TOUR && mode!=SPFE_PANIC) || mask>3 || !(mask|postgame_mask))return;
 /* The completed game consumed the old black handoff. Names/records are a
  * fresh visible frontend owner, never a continuation of its exit fade. */
 mode_fade_phase=mode_fade_step=0;
 const uint8_t *players[2]={p1,p2};
 for(unsigned n=0;n<2;++n)if((mask&(1u<<n)) && players[n]){
  uint8_t signature[9];result_signature(mode,players[n],signature);
  if(!submitted_valid[n] || memcmp(submitted[n],signature,9))frontend_finish_player(mode,players[n],n);
 }
 queue_audio(STAGE_AUDIO_STOP,0,0);audio_profile=1;postgame_mode=mode;postgame_mask|=mask;
 handoff_pending=handoff_halted=demo_pending=queued_start=0;cadence=0;
 pending_pressed[0]=pending_pressed[1]=release_block[0]=release_block[1]=0;
 postgame_music=0;
 const stage_view *previous=stage_get_view();spfe_t *f=&frontend_core;
 if(previous && previous->chars && previous->attrs && previous->palette){
  frontend_view=*previous;memcpy(f->attributes,previous->attrs,2048);
  for(unsigned i=0;i<2048;++i){f->tiles[i]=(uint16_t)(previous->chars[i*2]|(previous->chars[i*2+1]<<8));
   f->palette[i]=(uint16_t)(previous->palette[i*2]|(previous->palette[i*2+1]<<8));}
  if(previous->globals)memcpy(frontend_globals,previous->globals,128);
  postgame_objects=previous->objects;postgame_object_count=previous->object_count;
  postgame_scene=previous->scene_index;postgame_phase=previous->phase;
 }else{postgame_objects=frontend_objects;postgame_object_count=0;postgame_scene=0;postgame_phase=STAGE_GAME_OVER;}
 ++f->background_serial;++f->palette_serial;memset(f->actor,0,sizeof f->actor);f->sprite_count=0;
 f->state=SPFE_NAME_ENTRY;f->mode=(uint8_t)mode;f->state_ticks=0;
 if(editing_mask())queue_audio(STAGE_AUDIO_COMMAND,52,0);
 for(unsigned n=0;n<2;++n)if(postgame_initial&(1u<<n))initial_player(n);
 if(postgame_mode==SPFE_TOUR && !editing_mask() && footer_mask() && !postgame_music){
  queue_audio(STAGE_AUDIO_COMMAND,50,0);postgame_music=1;
 }
 /* Match the original next-title choice after this mode's ranking table. */
 demo_position=mode==SPFE_TOUR?2:4;
 if(!footer_mask())show_records(mode);else update_view();
}
const uint8_t *frontend_best(unsigned mode)
{/* FIXED360C/3AE9 import E158/E15C, initialized from the top-five table. */
 return frontend_records.rows[mode==SPFE_PANIC]+1;}
unsigned frontend_records_revision(void){return frontend_records.revision;}
void frontend_save_notice(int result)
{
 if(console_screen!=FRONTEND_HOME)return;
 spfe_console_notice(&frontend_core,result<0?"SAVE FAILED":result>0?"SETTINGS SAVED":0);++scene_revision;update_view();
}
int frontend_records_export(uint8_t out[192])
{
 if(frontend_records.entry[0].phase||frontend_records.entry[1].phase)return 0;
 memcpy(out,frontend_records.rows,160);memcpy(out+160,frontend_records.total,32);return 1;
}
int frontend_records_import(const uint8_t data[192])
{
 if(frontend_records.entry[0].phase||frontend_records.entry[1].phase)return 0;
 /* Reject an interrupted editor even if a future writer created a valid CRC. */
 for(unsigned row=0;row<12;row++)if(data[row*16+5])return 0;
 memcpy(frontend_records.rows,data,160);memcpy(frontend_records.total,data+160,32);
 ++frontend_records.revision;return 1;
}
unsigned frontend_audio_profile(void){return audio_profile;}
int frontend_audio_pop(stage_audio_event *event)
{
 if(!event || !fifo_count)return 0;
 *event=frontend_fifo[fifo_head];fifo_head=(fifo_head+1)%FRONTEND_AUDIO_CAPACITY;fifo_count--;return 1;
}
unsigned frontend_screen_kind(void){return visible_screen();}
unsigned frontend_credits(void){return frontend_core.credits;}

unsigned frontend_stage_select_used(void)
{return handoff_halted && frontend_core.mode==SPFE_TOUR && frontend_core.stage_select && frontend_core.select_done;}
