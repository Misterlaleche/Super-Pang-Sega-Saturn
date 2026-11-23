#include "stage.h"
#include "stage_data.h"
#include "spang_player_control.h"
#include "player_vertical.h"
#include "spang_player_damage.h"
#include "spang_round_ball.h"
#include "spang_round_scene.h"
#include "spang_stage_bootstrap.h"
#include "spang_stage_session.h"
#include "spang_round_frame.h"
#include "spang_round_aux.h"
#include "spang_round_rewards.h"
#include "spang_round_clear.h"
#include "spang_round_clock.h"
#include "spang_round_flow.h"
#include "spang_round_bonus_mask.h"
#include "spang_round_bonus_box.h"
#include "spang_round_speed.h"
#include "spang_platform_effect.h"
#include "spang_items.h"
#include "spang_life_sequence.h"
#include "spang_score.h"
#include "spang_score_draw.h"
#include "spang_lives_draw.h"
#include "spang_motion_bonus.h"
#include "spang_weapons.h"
#include "spang_video_commands.h"
#include "spang_frame_clock.h"
#include "spang_tour_map.h"
#include "spang_tour_ending.h"
#include "spang_map_presentation.h"
#include "spang_scene_data.h"
#include "spang_tour_region.h"
#include "spang_panic.h"
#include "spang_panic_video.h"
#include "fauna.h"
#include "fauna_transients.h"
#include "spang_attract_input.h"
#include "spang_attract_tutorial.h"
#include <string.h>

enum { AUDIO_CAPACITY=512 };
static uint8_t objects[4096],attrs[2048],chars[4096],palette[4096],grid[2048];
static uint8_t player[128],player2[128],globals[128],balls[1024],items[256];
static uint8_t timer[32],clear_actor[32],clear_draw[32],special_clear[32];
static uint8_t ready_actor[32],ready_objects[288],platform_fx[128];
static uint8_t best[4],last_input,last_input2,dead_flags,music_request[2];
static uint8_t map_actors[320],animal_actors[320],map_input,map_input2;
static spang_map_presentation map_presentation;
static uint8_t animal_scheduler[32],animal_draw[8][40],animal_visible;
static uint8_t animal_fx[256];
static uint8_t ladders[128];
static unsigned ladder_objects;
static uint8_t bonus_mask_selector,tour_bonus_wins[2],system_input,previous_system;
static uint8_t ending_extra[4],ending_input,ending_input2;
static uint8_t scene_buffer[SPANG_SCENE_BYTES];
static const uint8_t *under_attrs=scene_buffer,*under_chars=scene_buffer+2048;
typedef struct {
    const uint8_t *ready,*bonus;
    unsigned scene;
} stage_scene;
enum { NATIVE_TOUR_STAGES=40 };
static const stage_scene scenes[NATIVE_TOUR_STAGES]={
    {hk_ready_palette,hk_bonus_palette,0},{java1_ready_palette,java1_bonus_palette,2},
    {java2_ready_palette,java2_bonus_palette,3},{java3_ready_palette,java3_bonus_palette,4},
    {silk1_ready_palette,silk1_bonus_palette,6},{silk2_ready_palette,silk2_bonus_palette,7},
    {silk3_ready_palette,silk3_bonus_palette,8},{himar1_ready_palette,himar1_bonus_palette,10},
    {himar2_ready_palette,himar2_bonus_palette,11},{himar3_ready_palette,himar3_bonus_palette,12},
    {istanbul1_ready_palette,istanbul1_bonus_palette,14},{istanbul2_ready_palette,istanbul2_bonus_palette,15},
    {istanbul3_ready_palette,istanbul3_bonus_palette,16},{germany1_ready_palette,germany1_bonus_palette,18},
    {germany2_ready_palette,germany2_bonus_palette,19},{germany3_ready_palette,germany3_bonus_palette,20},
    {venice1_ready_palette,venice1_bonus_palette,22},{venice2_ready_palette,venice2_bonus_palette,23},
    {spain1_ready_palette,spain1_bonus_palette,25},{spain2_ready_palette,spain2_bonus_palette,26},
    {spain3_ready_palette,spain3_bonus_palette,27},{france1_ready_palette,france1_bonus_palette,29},
    {france2_ready_palette,france2_bonus_palette,30},{france3_ready_palette,france3_bonus_palette,31},
    {tower1_ready_palette,tower1_bonus_palette,33},{tower2_ready_palette,tower2_bonus_palette,34},
    {tower3_ready_palette,tower3_bonus_palette,35},{canada1_ready_palette,canada1_bonus_palette,37},
    {canada2_ready_palette,canada2_bonus_palette,38},
    {canada3_ready_palette,canada3_bonus_palette,39},
    {usa1_ready_palette,usa1_bonus_palette,41},
    {usa2_ready_palette,usa2_bonus_palette,42},
    {usa3_ready_palette,usa3_bonus_palette,43},
    {mexico1_ready_palette,mexico1_bonus_palette,45},
    {mexico2_ready_palette,mexico2_bonus_palette,46},
    {brazil1_ready_palette,brazil1_bonus_palette,48},
    {brazil2_ready_palette,brazil2_bonus_palette,49},
    {brazil3_ready_palette,brazil3_bonus_palette,50},
    {argentina1_ready_palette,argentina1_bonus_palette,52},
    {argentina2_ready_palette,argentina2_bonus_palette,53}
};
static unsigned scene_index;
static uint32_t scene_revision;
static spang_round_frame round_objects;
/* Original E148 survives attract demos; E146 is restored at FIXED:34F5.
 * BANK0:9B52 snapshots the last real game's seed when all players finish. */
static uint16_t session_seed=0x01c3,start_seed=0x01c3;
static unsigned configured_difficulty=1,configured_lives=3,game_difficulty=1,game_lives=3;
int stage_set_rules(unsigned difficulty,unsigned lives)
{if(difficulty>3||lives<1||lives>4)return -1;configured_difficulty=difficulty;configured_lives=lives;return 0;}
static spang_weapons weapons,weapons2;
static spang_panic panic;
static uint8_t panic_input,panic_input2;
static unsigned panic_victory_hold;
static int panic_actor_tick;
static stage_view view;
static stage_audio_event audio_fifo[AUDIO_CAPACITY];
static unsigned audio_head,audio_count,audio_sequence;
static unsigned cadence,shot_pose,clear_visible,special_visible,retry_pending,timeout_hold;
static unsigned shot_pose2,credit_count,start_mask=1,continue_music_pending,gameover_hold;
static uint8_t imported_best[4],best_imported;
static uint32_t second_held,second_pressed,pending_pressed2;
static spang_attract_input demo_input;
static spang_attract_tutorial demo_tutorial;
static unsigned demo_kind,demo_age,demo_done;
typedef struct {uint8_t score[4],location[4];uint32_t sequence;} finished_player;
static finished_player finished[2];
static unsigned finished_pending,finished_mask,finished_sequence;
static stage_records_tick_fn records_tick_callback;
static unsigned records_lock_mask;
static unsigned frame_cursor,remaining,group_id;
static uint32_t ticks,world_steps,presentation_frames,tile_revision,palette_revision,pending_pressed;
static stage_phase phase;
static int fault;

static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static int player_dying(const uint8_t *p){return p[0]==5 && (p[2]==4 || p[2]==6);}
static int dying(void){return player_dying(player) || player_dying(player2);}
static uint8_t *actor(unsigned n){return n?player2:player;}
static spang_weapons *weapon_pool(unsigned n){return n?&weapons2:&weapons;}
static uint8_t arcade_input(uint32_t held)
{return (uint8_t)(((held&STAGE_LEFT)?0x20:0)|((held&STAGE_RIGHT)?0x10:0)|((held&STAGE_FIRE)?8:0)|((held&STAGE_UP)?0x80:0)|((held&STAGE_DOWN)?0x40:0));}
static void fail(int code){if(!fault)fault=code;phase=STAGE_FAULT;}
static void audio(unsigned kind,unsigned command,unsigned parameter)
{
    if(audio_count==AUDIO_CAPACITY){fail(-501);return;}
    stage_audio_event *event=audio_fifo+(audio_head+audio_count)%AUDIO_CAPACITY;
    event->tick=ticks;event->sequence=audio_sequence++;
    event->kind=(uint8_t)kind;event->command=(uint8_t)command;
    event->parameter=(uint8_t)parameter;event->reserved=0;++audio_count;
}
static void command(unsigned value){audio(STAGE_AUDIO_COMMAND,value,0);}
unsigned stage_audio_pending(void){return audio_count;}
int stage_audio_pop(stage_audio_event *event)
{
    if(!audio_count || !event)return 0;
    *event=audio_fifo[audio_head];audio_head=(audio_head+1)%AUDIO_CAPACITY;--audio_count;return 1;
}
static void flush_actor_audio(void)
{
    if(music_request[0]){command(music_request[1]);music_request[0]=music_request[1]=0;}
    for(unsigned n=0;n<8;n+=2)if(round_objects.sounds[n+1]){
        audio(STAGE_AUDIO_OKI,round_objects.sounds[n],round_objects.sounds[n+1]);
        round_objects.sounds[n]=round_objects.sounds[n+1]=0;
    }
}
static void tile_write(void *context,uint16_t address,uint8_t value)
{
    (void)context;uint8_t *dest=0;
    /* Translate the emulated address before forming an array pointer. */
    if(address>=0xc800 && address<0xd000)dest=attrs+(address-0xc800);
    else if(address>=0xd000 && address<0xe000)dest=chars+(address-0xd000);
    else{fail(-502);return;}
    if(*dest!=value){*dest=value;++tile_revision;}
}
static void palette_copy(unsigned offset,const uint8_t *source,unsigned size)
{
    if(offset+size>sizeof palette){fail(-503);return;}
    if(memcmp(palette+offset,source,size)){memcpy(palette+offset,source,size);++palette_revision;}
}
static void palette_service(unsigned request)
{
    if(request==22 && globals[0x1a]==2){
        if(spang_panic_video_palette(request,globals[0x19],palette))++palette_revision;
    }
    else if(request==8)palette_copy(0,hk_clear_palette,32);
    else if(request==10)palette_copy(0,hk_timeout_palette,32);
    else if(request==12)palette_copy(0,hk_gameover_palette,32);
    else if(request==18)palette_copy(0,hk_continue_palette,32);
    else if(request==22 && globals[0x19]<NATIVE_TOUR_STAGES)palette_copy(0,scenes[globals[0x19]].ready,32);
    else if(request==26 && globals[0x19]<NATIVE_TOUR_STAGES)palette_copy(0x1e0,scenes[globals[0x19]].bonus,32);
    else fail(-504);
}
static void flash(unsigned phase_value)
{
    if(phase_value>(globals[0x1a]==2?6u:4u) || (phase_value&1)){fail(-505);return;}
    if(globals[0x1a]==2){if(spang_panic_video_palette(20,phase_value,palette))++palette_revision;}
    else palette_copy(0x3e0,hk_flash_palettes+phase_value*16,32);
}
static int object_x(unsigned at){return objects[at+3]+((objects[at+1]&16)<<4);}
static int object_y(unsigned at){return ((objects[at+2]+8)&255)-8;}
static unsigned object_code(unsigned at){return objects[at]+((objects[at+1]&0xe0)<<3);}
static void mark_group(unsigned first,unsigned end,unsigned tag,int anchor,unsigned role)
{
    if(end<=first)return;
    if((first&31)||(end&31)||end>0xfe0){fail(-506);return;}
    int left=32767,top=32767,right=-32768,bottom=-32768;
    for(unsigned at=first;at<end;at+=32){
        int x=object_x(at),y=object_y(at);
        if(x<left)left=x;
        if(y<top)top=y;
        if(x+16>right)right=x+16;
        if(y+16>bottom)bottom=y+16;
    }
    ++group_id;
    for(unsigned at=first;at<end;at+=32){
        objects[at+4]=(uint8_t)tag;put(objects+at+5,(unsigned)anchor);
        put(objects+at+7,group_id);put(objects+at+9,(unsigned)left);put(objects+at+11,(unsigned)top);
        put(objects+at+13,(unsigned)(right-left));put(objects+at+15,(unsigned)(bottom-top));objects[at+17]=(uint8_t)role;
    }
}
static int original_ball_code(unsigned code,unsigned pal)
{
    for(unsigned type=0;type<=36;type+=2){
        if(!(0x77df7u&(1u<<(type>>1))))continue;
        for(unsigned special=0;special<=(type<8);++special){
            unsigned first=word(hk_bank1+(special?0x1a46:0x1a1a)+type),anim=first;
            for(unsigned n=0;n<16;++n){
                if(anim<0x8000 || anim>0xbff9)break;
                unsigned attr=hk_bank1[anim+4-0x8000];
                if(code==hk_bank1[anim+3-0x8000]+((attr&0xe0)<<3) && pal==(attr&15))return 1;
                unsigned next=anim+5;if(hk_bank1[anim-0x8000]&128)next=word(hk_bank1+(next-0x8000));
                if(next==anim || next==first)break;
                anim=next;
            }
        }
    }
    if(globals[0x1a]==2)for(unsigned type=0;type<=42;type+=2){
        if(type<=36 && !(0x77df7u&(1u<<(type>>1))))continue;
        for(unsigned variant=0;variant<(type>=38?4u:1u);++variant){
            unsigned table=variant==0?0xa985+type:variant==1?0x9a1a+type:0x99da+type*3+2*(variant-1);
            unsigned first=word(hk_bank1+(table-0x8000)),anim=first;
            for(unsigned n=0;n<16;++n){
                if(anim<0x8000 || anim>0xbff7)break;
                unsigned route=word(hk_bank1+(anim+1-0x8000));
                unsigned icon=route==0x2221 || route==0x278e || route==0x238a || route==0x2931;
                unsigned attr=hk_bank1[anim+4+2*icon-0x8000];
                if(code==hk_bank1[anim+3+icon-0x8000]+((attr&0xe0)<<3) && pal==(attr&15))return 1;
                if(icon){attr=hk_bank1[anim+5-0x8000];if(code==hk_bank1[anim+3-0x8000]+((attr&0xe0)<<3) && pal==(attr&15))return 1;}
                unsigned next=anim+5+2*icon;if(hk_bank1[anim-0x8000]&128)next=word(hk_bank1+(next-0x8000));
                if(next==anim || next==first)break;
                anim=next;
            }
        }
    }
    return 0;
}
/* C13's round-frame producer emits square grids column-serpentine, all cells
 * already sharing A6/A7+anchor. Recover that producer boundary, never infer it
 * from a colour mask or screen-connected components. */
static void mark_square_groups(unsigned first,unsigned end)
{
    for(unsigned at=first;at<end;){
        /* Panic's incoming bodies retain their signed original origin even
         * when a source row is hidden atY=248. The producer supplies bounds;
         * deriving them from the clipped byte would enlarge/cache a false
         * compound. Special icons remain independent16px cells. */
        if(globals[0x1a]==2 && (objects[at+18]&0x80)){
            unsigned width=objects[at+18]&7,count=width*width,last=at+count*32;
            if(!width || width>4 || last>end){fail(-598);return;}
            int left=(int16_t)word(objects+at+20),top=(int16_t)word(objects+at+22);
            unsigned tag=objects[at+4],anchor=word(objects+at+5);
            unsigned role=tag==STAGE_TAG_BALL?(original_ball_code(object_code(at),objects[at+1]&15)?STAGE_ROLE_BALL:STAGE_ROLE_POP):STAGE_ROLE_ITEM;
            mark_group(at,last,tag,(int)anchor,role);
            for(unsigned p=at;p<last;p+=32){put(objects+p+9,(unsigned)left);put(objects+p+11,(unsigned)top);put(objects+p+13,width*16);put(objects+p+15,width*16);}
            at=last;continue;
        }
        unsigned tag=objects[at+4],anchor=word(objects+at+5),width=1,base=object_code(at),pal=objects[at+1]&15;
        int x=object_x(at),y=object_y(at);
        if(tag==STAGE_TAG_BALL || tag==STAGE_TAG_NATIVE){
            while(width<4 && at+(width+1)*32<=end){unsigned p=at+width*32;
                unsigned expected=(base&0x700)|((base+width*8)&255);
                if(objects[p+4]!=tag || word(objects+p+5)!=anchor || object_x(p)!=x || object_y(p)!=y+(int)width*16 || object_code(p)!=expected || (objects[p+1]&15)!=pal)break;
                ++width;
            }
            int valid=at+width*width*32<=end;
            for(unsigned col=0;col<width && valid;++col)for(unsigned j=0;j<width;++j){
                unsigned row=(col&1)?width-1-j:j,p=at+(col*width+j)*32;
                unsigned expected=(base&0x700)|((base+col+row*8)&255);
                if(objects[p+4]!=tag || word(objects+p+5)!=anchor || object_x(p)!=x+(int)col*16 || object_y(p)!=y+(int)row*16 || object_code(p)!=expected || (objects[p+1]&15)!=pal)valid=0;
            }
            if(!valid)width=1;
        }else{tag=STAGE_TAG_GROUP;anchor=(unsigned)x;}
        unsigned role=tag==STAGE_TAG_BALL?(original_ball_code(base,pal)?STAGE_ROLE_BALL:STAGE_ROLE_POP):STAGE_ROLE_ITEM;
        mark_group(at,at+width*width*32,tag,(int)anchor,role);at+=width*width*32;
    }
}
static void begin_objects(void){memset(objects,0,sizeof objects);frame_cursor=0;remaining=127-ladder_objects;group_id=0;}
static void reserve_code_hud(uint8_t p[128])
{
    if(!p[0] && p[0x5f]==2)return;
    if(!p[0x60] && p[0x61]>=6)return;
    unsigned x0=p[17]?45:10;
    int code=spang_life_reserve_code(p,hk_bank0);
    if(code<0 || (p[0x60]&15)>9 || (p[0x60]>>4)>9){fail(-507);return;}
    for(unsigned y=29;y<31;++y)for(unsigned x=x0;x<x0+7;++x){unsigned t=y*64+x;
        tile_write(0,(uint16_t)(0xc800+t),0x10);tile_write(0,(uint16_t)(0xd000+t*2),0x20);tile_write(0,(uint16_t)(0xd001+t*2),0);
    }
    /* FIXED166B also draws DEC/DAA 99 reserves when an active actor's
     * packed-BCD life count wraps to 00. Eliminated actors stay cleared. */
    if(p[0x60]>1 || (!p[0x60] && p[0])){
        uint8_t scratch[3];spang_lives_draw(p[17],p[0x60],scratch,tile_write,0);
        unsigned n=p[0x60]-1;if(n>3)n=3;
        if(p[3]==10)for(unsigned i=0;i<n;++i)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){
            unsigned t=29*64+x0+2*i+y*64+x,v=(unsigned)code+y*16+x;
            tile_write(0,(uint16_t)(0xd000+t*2),(uint8_t)v);tile_write(0,(uint16_t)(0xd001+t*2),(uint8_t)(v>>8));
        }
    }
}
static void update_hud(void)
{
    if((globals[0x0c]&1) && (player[0x60] || player[0x61]<6))spang_score_draw(player+0x40,0xdd98,0,tile_write,0);
    if(globals[0x1a]!=2 && demo_kind!=3 && demo_kind!=4)spang_score_draw(best,0xdf3c,0,tile_write,0);
    if(globals[0x0c]&1)reserve_code_hud(player);
    if(globals[0x0c]&2){if(player2[0x60] || player2[0x61]<6)spang_score_draw(player2+0x40,0xddde,0,tile_write,0);reserve_code_hud(player2);}
    if(globals[0x1a]==2){
        unsigned changed=spang_panic_video_update(globals,attrs,chars,palette);
        if(changed&SPANG_PANIC_VIDEO_TILES)++tile_revision;
        if(changed&SPANG_PANIC_VIDEO_PALETTE)++palette_revision;
    }
}
static void update_view(void)
{
    view.tick=ticks;view.presentation_frames=presentation_frames;view.tile_revision=tile_revision;view.palette_revision=palette_revision;
    view.fault=fault;view.phase=fault?STAGE_FAULT:phase;view.random_seed=round_objects.seed;view.object_count=(uint16_t)(frame_cursor/32);
    view.lives=player[0x60];view.retries=globals[0x0f];view.remaining_weight=globals[0x20];view.ready_active=ready_actor[0];
    view.objects=objects;view.attrs=attrs;view.chars=chars;view.palette=palette;view.player=player;view.globals=globals;
    view.balls=balls;view.items=items;view.timer=timer;view.best=best;
    view.stage_index=globals[0x19];view.world_index=globals[0x18];view.scene_index=(uint8_t)scene_index;view.scene_revision=scene_revision;
    view.player2=player2;view.player_mask=globals[0x0c];view.records_mask=(uint8_t)records_lock_mask;
    view.animals=animal_actors;view.animal_count=0;for(unsigned i=0;i<8;++i)if(animal_actors[i*40])++view.animal_count;
}
static void append_player(const uint8_t p[128])
{
    if(!p[0] || ((p[0]==3 || p[0]==4) && !p[1]))return;
    unsigned animation=word(p+6);
    if(animation<0x8000 || animation+4>=0xc000){fail(-508);return;}
    const uint8_t *f=hk_bank0+(animation-0x8000);
    unsigned route=word(f+1),first=frame_cursor;uint16_t cursor=(uint16_t)frame_cursor;uint8_t room=(uint8_t)remaining;int count;
    if(route==0xbde6)count=spang_player_sprite(p,f[3],f[4],(uint8_t)world_steps,objects,&cursor,&room);
    else if(route==0x246f)count=spang_round_victory_sprite(p,f[3],f[4],objects,&cursor,&room);
    else if(route==0x2702)count=spang_player_death_sprite(p,f[3],f[4],objects,&cursor,&room);
    else{fail(-509);return;}
    if(count<0){fail(-510);return;}
    frame_cursor=cursor;remaining=room;mark_group(first,frame_cursor,STAGE_TAG_NATIVE,(int)word(p+10),STAGE_ROLE_PLAYER);
    /*2702 clamps off-screen constituent objects toY=248. Metadata retains
     * the original un-clipped compound and16-bit actorY for authored-frame
     * selection/registration; no actual arcade object byte is changed. */
    unsigned width=route==0x246f?48:32;
    int left=(int)word(p+10)-(int)width/2;
    int top=(route==0x2702?(int)(int16_t)word(p+13):(int)p[13])-16;
    for(unsigned at=first;at<frame_cursor;at+=32){
        put(objects+at+9,(unsigned)left);put(objects+at+11,(unsigned)top);put(objects+at+13,width);put(objects+at+15,32);
    }
}
static void append_weapons(const spang_weapons *w)
{
    unsigned first=frame_cursor;uint16_t cursor=(uint16_t)frame_cursor;uint8_t room=(uint8_t)remaining;
    if(spang_weapons_draw(w,hk_bank2,objects,&cursor,&room)<0){fail(-511);return;}
    frame_cursor=cursor;remaining=room;
    /* R01 authored weapons are per-cell assets, so each original rope/bullet
     * cell is deliberately its own16x16 group. */
    for(unsigned at=first;at<frame_cursor;at+=32)mark_group(at,at+32,STAGE_TAG_GROUP,object_x(at),STAGE_ROLE_WEAPON);
}
static void append_clear(const uint8_t *actor)
{
    unsigned first=frame_cursor;uint16_t cursor=(uint16_t)frame_cursor;uint8_t room=(uint8_t)remaining;
    if(spang_round_clear_sprite(actor,hk_bank3,objects,&cursor,&room)<0){fail(-512);return;}
    frame_cursor=cursor;remaining=room;mark_group(first,frame_cursor,STAGE_TAG_GROUP,(int)word(actor+10),STAGE_ROLE_CLEAR);
}
static void append_shield(uint8_t p[128],int advance)
{
    if(!p[0])return;
    if((p[0]==3 || p[0]==4) && !p[1])return;
    unsigned first=frame_cursor;uint8_t saved=p[0x54];uint16_t cursor=(uint16_t)frame_cursor;uint8_t room=(uint8_t)remaining;
    if(spang_items_shield_sprite(p,(uint8_t)world_steps,hk_bank0,objects,&cursor,&room)<0){fail(-513);return;}
    if(!advance || player_dying(p))p[0x54]=saved;
    frame_cursor=cursor;remaining=room;mark_group(first,frame_cursor,STAGE_TAG_NATIVE,(int)word(p+10),STAGE_ROLE_SHIELD);
}
static void bonus_object_write(void *context,uint16_t address,uint8_t value)
{(void)context;objects[address-0xd000]=value;}
static void append_masked_ladders(void)
{
    unsigned tail=0xfe0-ladder_objects*32,at=tail;
    for(unsigned i=0;i<8 && ladders[i*16];++i){
        const uint8_t *a=ladders+i*16;unsigned rows=(a[15]+1)/2;
        for(unsigned col=0;col<2;++col)for(unsigned row=0;row<rows;++row){
            unsigned x=(word(a+10)-16+col*16)&65535;
            objects[at]=(uint8_t)(6+col+((a[15]&1) && row+1==rows?8:0));
            objects[at+1]=(uint8_t)(0x4d|((x>>4)&16));objects[at+2]=(uint8_t)(a[13]+row*16);
            objects[at+3]=(uint8_t)x;objects[at+4]=(uint8_t)(col*2);objects[at+18]=(uint8_t)(row+1);at+=32;
        }
    }
    uint8_t proxy[128]={0};put(proxy+0x10,0xd000+frame_cursor);put(proxy+0x12,0xd000+tail);
    proxy[0x14]=(uint8_t)remaining;proxy[0x16]=(uint8_t)(127-ladder_objects);proxy[0x41]=bonus_mask_selector;
    unsigned front=frame_cursor;
    if(spang_round_bonus_mask(proxy,objects,hk_bank3,bonus_object_write,0)){fail(-593);return;}
    frame_cursor=word(proxy+0x10)-0xd000;remaining=proxy[0x14];
    if(frame_cursor>tail){fail(-594);return;}
    for(unsigned p=front;p<frame_cursor;p+=32)mark_group(p,p+32,STAGE_TAG_NATIVE,object_x(p)+8,STAGE_ROLE_CLEAR);
    memmove(objects+frame_cursor,objects+tail,ladder_objects*32);
    for(unsigned i=0;i<8 && ladders[i*16];++i){
        const uint8_t *a=ladders+i*16;unsigned end=frame_cursor+((a[15]+1)/2)*64;
        /* Never infer compound geometry from the hidden F8 coordinate. */
        unsigned group=++group_id;
        for(unsigned p=frame_cursor;p<end;p+=32){
            objects[p+4]=STAGE_TAG_NATIVE;put(objects+p+5,word(a+10));put(objects+p+7,group);
            put(objects+p+9,word(a+10)-16);put(objects+p+11,a[13]);put(objects+p+13,32);
            put(objects+p+15,((a[15]+1)/2)*16);objects[p+17]=STAGE_ROLE_LADDER;
        }
        frame_cursor=end;
    }
}
static void append_ladders(void)
{
    if(bonus_mask_selector!=255){append_masked_ladders();return;}
    /* BANK5:8000..8099 draws these static cells into the reserved object
     * tail before front actors run. Compact the unused gap, retaining their
     * tail priority and the exact front-actor budget (24 cells on Silk Road). */
    remaining+=ladder_objects;
    for(unsigned i=0;i<8 && ladders[i*16];++i){
        const uint8_t *a=ladders+i*16;unsigned first=frame_cursor;
        unsigned rows=(a[15]+1)/2;
        if(remaining<rows*2 || frame_cursor+rows*64>0xfe0){fail(-586);return;}
        for(unsigned col=0;col<2;++col)for(unsigned row=0;row<rows;++row){
            unsigned x=(word(a+10)-16+col*16)&65535,at=frame_cursor;
            unsigned code=6+col+((a[15]&1) && row+1==rows?8:0);
            objects[at]=(uint8_t)code;objects[at+1]=(uint8_t)(0x4d|((x>>4)&16));
            objects[at+2]=(uint8_t)(a[13]+row*16);objects[at+3]=(uint8_t)x;
            frame_cursor+=32;--remaining;
        }
        mark_group(first,frame_cursor,STAGE_TAG_NATIVE,(int)word(a+10),STAGE_ROLE_LADDER);
    }
}
static void finish_objects(int gameplay,int shield_advance)
{
    append_player(player);append_player(player2);
    if(gameplay){append_weapons(&weapons);append_weapons(&weapons2);}
    if(clear_visible)append_clear(clear_draw);
    if(special_visible)append_clear(special_clear);
    if(gameplay){append_shield(player,shield_advance);append_shield(player2,shield_advance);}
    append_ladders();
    if(frame_cursor<=0xfe0)memset(objects+frame_cursor,0,4096-frame_cursor);
}
static void draw_status_word_at(unsigned descriptor,unsigned role,unsigned left,unsigned top)
{
    begin_objects();unsigned count=hk_bank2[descriptor+1-0x8000],attr=hk_bank2[descriptor+2-0x8000];
    if(!count || count>127 || descriptor+3+count>0xc000){fail(-514);return;}
    for(unsigned i=0;i<count;++i){unsigned x=left+i*16,at=i*32;
        objects[at]=hk_bank2[descriptor+3+i-0x8000];objects[at+1]=(uint8_t)(attr|((x>>4)&16));objects[at+2]=(uint8_t)top;objects[at+3]=(uint8_t)x;
    }
    frame_cursor=count*32;remaining=127-count;mark_group(0,frame_cursor,STAGE_TAG_GROUP,(int)left,role);
}
static void draw_status_word(unsigned descriptor,unsigned role)
{draw_status_word_at(descriptor,role,168,112);}
static void extra_lives_for(uint8_t p[128])
{
    uint8_t effects[2];if(spang_motion_bonus(p,hk_fixed+0x149,effects)<0)fail(-515);else if(effects[0])command(0x13);
}
static void restore_platform(void *context,const uint8_t *h,int count)
{
    (void)context;unsigned start=word(h+29)-0xf000;
    for(unsigned i=0;i<(unsigned)count;++i){unsigned t=(h[31]&64)?start-i*64:((start&0xff00)|((start+i)&255));
        if(t>=2048){fail(-516);return;}
        tile_write(0,(uint16_t)(0xc800+t),under_attrs[t]);
        tile_write(0,(uint16_t)(0xd000+2*t),under_chars[2*t]);tile_write(0,(uint16_t)(0xd001+2*t),under_chars[2*t+1]);
    }
    if(count>0){
        if(spang_platform_effect_spawn(platform_fx,h,hk_bank2)<0)fail(-517);
        uint8_t *owner=h[17]?player2:player;
        spang_score_add(owner+0x40,(uint16_t)word(hk_fixed+0x2e44+10));spang_score_promote(best,owner+0x40);extra_lives_for(owner);
    }
}
static unsigned pending_actor_commands(int advance)
{
    unsigned pending=0;if(!advance)return 0;
    for(unsigned i=0;i<2;++i){const uint8_t *a=round_objects.lucky+i*16;if(a[0] && word(a+2)==0xac97)pending|=a[15]==0x4e?2:1;}
    for(unsigned i=0;i<4;++i){const uint8_t *a=platform_fx+i*32;if(a[0] && word(a+2)==0x9253)pending|=4;}
    return pending;
}
static void deliver_actor_commands(unsigned pending,int advance)
{
    if(!advance)return;
    if(pending&1)command(4);
    if(pending&2)command(0x13);
    if(pending&4)command(6);
    for(unsigned i=0;i<8;++i)if(round_objects.events[i]&SPANG_ITEM_SOUND){unsigned type=items[i*32+15];command(type==14?0x13:(type==18 || type==20)?0x19:3);}
}
static void draw_transients(unsigned kind,int advance,uint16_t *cursor,uint8_t *room)
{
    const uint8_t *bank=kind?hk_bank2:hk_bank3;
    unsigned base=kind?128:0,count=kind?8:4;
    for(unsigned i=0;i<count;++i){
        uint8_t draw[16];int r=fauna_transient_step(animal_fx+base+i*16,bank,kind,advance,globals[0x2b],draw);
        if(r<0){fail(r);return;}
        if(!r)continue;
        unsigned first=*cursor;
        r=fauna_transient_sprite(draw,bank,objects,cursor,room);
        if(r<0){fail(r);return;}
        mark_group(first,*cursor,STAGE_TAG_NATIVE,(int)word(draw+10),STAGE_ROLE_ANIMAL);
    }
}
static void draw_round(int advance)
{
    begin_objects();uint16_t cursor=0;uint8_t room=(uint8_t)remaining;
    int service_advance=globals[0x1a]==2?panic_actor_tick:advance;
    /* READY and the stop-clock are alternate owners of original E1E0. */
    if(globals[0x1a]==2 && round_objects.stop[0])ready_actor[0]=0;
    if((globals[0x1a]==2 || demo_kind) && ready_actor[0]){
        if(service_advance){
            int done=spang_life_ready_tick(ready_actor,ready_objects);
            if(done<0){fail(-521);return;}
            if(done)palette_service(22);
        }
        if(ready_actor[0] && room>=9){
            memcpy(objects,ready_objects,288);cursor=288;room-=9;
            mark_group(0,192,STAGE_TAG_NATIVE,256,STAGE_ROLE_READY);
            for(unsigned at=192;at<288;at+=32)mark_group(at,at+32,STAGE_TAG_GROUP,object_x(at),STAGE_ROLE_READY);
        }
    }
    unsigned first=cursor;
    int r=spang_round_stop_step(&round_objects,globals,hk_bank3,objects,&cursor,&room,service_advance);
    if(r){fail(r);return;}
    mark_group(first,cursor,STAGE_TAG_NATIVE,(int)word(round_objects.stop+10),STAGE_ROLE_CLOCK);
    first=cursor;
    r=spang_round_scores_step_with_music(&round_objects,globals,0,0,music_request,hk_bank2,objects,&cursor,&room,advance);
    if(r){fail(r);return;}
    for(unsigned at=first;at<cursor;at+=64)mark_group(at,at+64,STAGE_TAG_NATIVE,(int)word(objects+at+5),STAGE_ROLE_SCORE);
    /* FIXED03E3/BANK3:95AC impact actors precede the ball task. */
    draw_transients(0,advance,&cursor,&room);if(fault)return;
    first=cursor;
    if(globals[0x1a]==2){
        if(advance && spang_panic_schedule(&panic,globals,balls,player,player2,panic_input,panic_input2,hk_bank2,&round_objects.seed)){fail(-599);return;}
        r=spang_round_frame_panic_step(&round_objects,balls,items,globals,player,player2,hk_bank1,grid,objects,&cursor,&room,globals[9],advance,&panic,hk_fixed);
    }else r=spang_round_frame_step(&round_objects,balls,items,globals,player,player2,hk_bank1,grid,objects,&cursor,&room,globals[9],advance);
    if(r){fail(r);return;}
    mark_square_groups(first,cursor);frame_cursor=cursor;remaining=room;
}
static void fauna_command(void *context,unsigned value){(void)context;command(value);}
static fauna_context animal_context(uint8_t input)
{
    fauna_context result={animal_actors,animal_scheduler,globals,player,player2,balls,items,timer,
        grid,hk_bank4,hk_bank1,hk_fixed,&round_objects,fauna_command,0,input,arcade_input(second_held),animal_fx};
    return result;
}
static void animal_step(int advance,uint8_t input)
{
    if(demo_kind==3 || demo_kind==4)return; /* Original tutorial0541 omits BANK4 fauna. */
    if(advance){
        fauna_context context=animal_context(input);
        int r=fauna_schedule_system(&context,previous_system);
        if(r<0){fail(r);return;}
        animal_visible=0;
        for(unsigned i=0;i<8;++i)if(animal_actors[i*40]){
            r=fauna_step_with_ladders(&context,i,animal_draw[i],ladders);
            if(r<0){fail(r);return;}if(r)animal_visible|=(uint8_t)(1u<<i);
        }
    }else{
        /* BANK4:8A06 consults current visibility byte1 and current actor
         * position/animation. Birds can move after their advancing draw. */
        animal_visible=0;
        for(unsigned i=0;i<8;++i)
            if(fauna_frozen_snapshot(animal_actors+i*40,animal_draw[i]))animal_visible|=(uint8_t)(1u<<i);
    }
    for(unsigned i=0;i<8;++i)if(animal_visible&(1u<<i)){
        unsigned first=frame_cursor;uint16_t cursor=(uint16_t)frame_cursor;
        uint8_t room=(uint8_t)(remaining>24?remaining-24:0);const uint8_t *a=animal_draw[i];
        int count=fauna_sprite(a,hk_bank4,objects,&cursor,&room);
        if(count<0){fail(count);return;}
        frame_cursor=cursor;remaining-=(unsigned)count;
        mark_group(first,frame_cursor,STAGE_TAG_NATIVE,(int)word(a+10),STAGE_ROLE_ANIMAL);
        unsigned ptr=word(a+6),route=word(hk_bank4+(ptr+1-0x8000));
        unsigned width=route==0x2635?64:route==0x246f?48:route==0x2112?16:32,height=route==0x2112?16:32;
        int left=(int)word(a+10)-(int)width/2,top=(route==0x2702?(int)(int16_t)word(a+13):(int)a[13])-(int)height/2;
        for(unsigned at=first;at<frame_cursor;at+=32){put(objects+at+9,(unsigned)left);put(objects+at+11,(unsigned)top);put(objects+at+13,width);put(objects+at+15,height);}
    }
}
static int animal_weapon_contact(void *context,uint8_t *weapon,int bullet,uint8_t frame)
{
    (void)context;
    return bullet?fauna_bullet_contact(animal_actors,weapon,frame):fauna_rope_contact(animal_actors,weapon,frame);
}
static void platform_step(int advance)
{
    unsigned first=frame_cursor;uint16_t cursor=(uint16_t)frame_cursor;uint8_t room=(uint8_t)(remaining>48?remaining-48:0),drops[16];
    if(spang_platform_effect_step(platform_fx,hk_bank2,objects,&cursor,&room,advance,drops)<0){fail(-518);return;}
    remaining-=(cursor-first)/32;frame_cursor=cursor;
    /* Platform compositions are consecutive cells along one axis. Associate
     * each emitted range with the corresponding original live actor. */
    unsigned at=first;
    for(unsigned i=0;i<4;++i){const uint8_t *a=platform_fx+i*32;
        if(!a[0] || word(a+2)!=0x9303 || a[5]==16)continue;
        unsigned p=word(a+6),count=p>=0x8000 && p<0xbfff?hk_bank2[p+1-0x8000]:0;
        if(count && at+count*32<=cursor){mark_group(at,at+count*32,STAGE_TAG_GROUP,(int)word(a+10),STAGE_ROLE_PLATFORM);at+=count*32;}
    }
    for(;at<cursor;at+=32)mark_group(at,at+32,STAGE_TAG_GROUP,object_x(at),STAGE_ROLE_PLATFORM);
    for(unsigned i=0;i<4;++i){
        if(drops[4*i]==1){
            /* BANK2:9363 adds the offset to both X bytes, including carry. */
            const uint8_t *effect=platform_fx+i*32;
            unsigned x=word(effect+10)+hk_bank2[0x1382+effect[15]];
            spang_items_spawn_platform(items,drops[4*i+1],x,drops[4*i+3]);
        }
        else if(drops[4*i]==2 && globals[0x19]>=3){
            /* BANK2:932C..9370 writes E890 directly for invisible blocks;
             * the existing BANK1 ABD4 owner handles its stage type/guard. */
            const uint8_t *effect=platform_fx+i*32;uint8_t *reward=round_objects.lucky+16;
            unsigned x=word(effect+10)+hk_bank2[0x1382+effect[15]];
            reward[15]=drops[4*i+1];put(reward+2,0xabd4);put(reward+10,x);
            reward[13]=drops[4*i+3];reward[0]=2;
        }
    }
}
static void ready_frame(int advance)
{
    begin_objects();uint8_t room=(uint8_t)remaining;
    if(spang_round_scene_step(balls,objects,&room,hk_bank1,grid,(uint16_t)word(globals+0x22),(uint16_t)word(globals+0x24),0)){fail(-519);return;}
    /* The legacy ready producer resets its own budget to127. Its static
     * bootstrap balls precede the reserved tail; apply the reservation only
     * after recovering its emitted count, before any further producers. */
    frame_cursor=(127-room)*32;
    if(room<ladder_objects){fail(-589);return;}
    room-=(uint8_t)ladder_objects;remaining=room;mark_square_groups(0,frame_cursor);
    if(advance){
        uint16_t cursor=(uint16_t)frame_cursor;
        if(spang_round_scores_step_with_music(&round_objects,globals,0,0,music_request,hk_bank2,objects,&cursor,&room,1)){fail(-520);return;}
        frame_cursor=cursor;remaining=room;
        int done=spang_life_ready_tick(ready_actor,ready_objects);
        if(done<0){fail(-521);return;}
        if(done){palette_service(22);phase=STAGE_PLAYING;}
    }
    if(ready_actor[0] && remaining>=9){
        unsigned first=frame_cursor;memcpy(objects+first,ready_objects,288);frame_cursor+=288;remaining-=9;
        mark_group(first,first+192,STAGE_TAG_NATIVE,256,STAGE_ROLE_READY);
        /* Three original transparent FF filler cells keep their bytes/count,
         * but cannot enlarge the visible READY group's bounds while blinking. */
        for(unsigned at=first+192;at<frame_cursor;at+=32)mark_group(at,at+32,STAGE_TAG_GROUP,object_x(at),STAGE_ROLE_READY);
    }
    finish_objects(1,0);
}
/* FIXED13DA/13F8/1402: two identities, independent attempts and lives. */
static void fresh_players(void)
{
    memset(player,0,sizeof player);memset(player2,0,sizeof player2);player2[17]=1;
    globals[0x0c]=globals[0x0d]=(uint8_t)start_mask;
    for(unsigned n=0;n<2;++n){uint8_t *p=actor(n);
        if(start_mask&(1u<<n)){
            p[0x60]=(uint8_t)game_lives;p[0x61]=4;p[0x72]=0x80;p[0x4b]=hk_fixed[0x149];p[0x4c]=hk_fixed[0x14a];
        }else if(globals[0x1a]){p[0x61]=12;p[0x62]=6;}
    }
}
static void clear_player_hud(unsigned n);
/* BANK0:A557 calls9CE2, B37A and FIXED165C. The per-player header comes from
 * B35C/B36B; the weapon box is the original B3B9 tile rectangle. */
static void player_entry_hud(const uint8_t p[128])
{
    if(!p[0x60])return;
    /*9CE2 sends P2 directly to9D24's17x4 character clear. Scene exports
     * contain the1P TOUR/MODE footer, which must not survive P2 entry. */
    if(p[17])clear_player_hud(1);
    if(spang_video_stream(0x117c,p[17]?0xb36b:0xb35c,hk_bank0,tile_write,0)){fail(-616);return;}
    if(globals[0x1a])return;
    unsigned dest=p[17]?0xddd0:0xdda8;
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){
        unsigned d=dest+y*128+x*2,source=0x33b9+y*8+x*2;
        tile_write(0,(uint16_t)d,hk_bank0[source]);tile_write(0,(uint16_t)(d+1),hk_bank0[source+1]);
        tile_write(0,(uint16_t)(0xc800+(d-0xd000)/2),x<2?0x11:0x91);
    }
}
static void bootstrap_player(unsigned n,int protect)
{
    uint8_t *p=actor(n);
    if(!p[0x60]){p[0]=0;return;}
    spang_stage_player(p,globals[0x19],hk_bank0);
    if(globals[0x1a]==2){put(p+10,240+n*32);p[13]=192;p[0x21]=(uint8_t)n;p[0x57]=(uint8_t)(2+n*8);}
    if(protect){p[0]=4;p[0x24]=210;p[0x23]=0;}
    if(spang_player_input(p,0,0,hk_bank0))fail(-524);
    player_entry_hud(p);
}
static void reset_life(int fresh)
{
    bonus_mask_selector=255;
    uint16_t seed=fresh?start_seed:round_objects.seed;
    if(fresh){memset(tour_bonus_wins,0,sizeof tour_bonus_wins);system_input=previous_system=0;memset(globals,0,sizeof globals);fresh_players();globals[0x52]=hk_bank0[0xde3+game_difficulty];globals[0x19]=(uint8_t)(fresh-1);}
    unsigned current=globals[0x19];if(current>=NATIVE_TOUR_STAGES){fail(-580);return;}
    scene_index=scenes[current].scene;++scene_revision;
    if(spang_scene_load(scene_index,scene_buffer)){fail(-589);return;}
    memcpy(attrs,under_attrs,sizeof attrs);memcpy(chars,under_chars,sizeof chars);
    memcpy(palette,scene_buffer+6144,sizeof palette);++tile_revision;++palette_revision;
    memset(timer,0,sizeof timer);memset(clear_actor,0,sizeof clear_actor);memset(clear_draw,0,sizeof clear_draw);memset(special_clear,0,sizeof special_clear);
    memset(ready_actor,0,sizeof ready_actor);memset(ready_objects,0,sizeof ready_objects);memset(platform_fx,0,sizeof platform_fx);
    shot_pose=shot_pose2=clear_visible=special_visible=retry_pending=timeout_hold=0;last_input=last_input2=dead_flags=0;world_steps=0;cadence=0;
    globals[0]=8;globals[1]=globals[2]=globals[4]=globals[5]=globals[6]=0;globals[0x0a]=1;
    globals[0x18]=hk_fixed[0x1260+current];globals[0x17]=hk_fixed[0x128a+globals[0x18]];
    if(spang_round_clock_init(timer,current,(uint8_t)game_difficulty,hk_clock_tables))fail(-522);
    spang_round_clock_draw(timer,hk_fixed+0x152e,tile_write,0);
    /* FIXED0D2E..0D5A establishes the countdown's four attribute rows. */
    for(unsigned row=0;row<4;++row)for(unsigned x=0;x<6;++x)tile_write(0,(uint16_t)(0xcee1+row*64+x),0x10);
    memset(grid,0,sizeof grid);spang_stage_grid_clear(grid);
    if(spang_stage_platforms(current,grid,attrs,chars,hk_bank5))fail(-523);
    memset(ladders,0,sizeof ladders);ladder_objects=0;
    if(spang_stage_ladders(current,ladders,hk_bank5))fail(-587);
    for(unsigned i=0;i<8 && ladders[i*16];++i)ladder_objects+=2*((ladders[i*16+15]+1)/2);
    if(ladder_objects>127){fail(-588);return;}
    bootstrap_player(0,0);bootstrap_player(1,0);
    spang_round_frame_init(&round_objects,seed);spang_weapons_init(&weapons);spang_weapons_init(&weapons2);spang_items_init(items);
    fauna_init(animal_scheduler,animal_actors,globals);memset(animal_draw,0,sizeof animal_draw);animal_visible=0;
    memset(animal_fx,0,sizeof animal_fx);
    if(spang_stage_session_begin(globals,round_objects.scores,&round_objects.seed,hk_bank5,hk_fixed))fail(-525);
    music_request[0]=music_request[1]=0;flash(0);
    spang_stage_pool_clear(balls);
    if(spang_stage_balls(current,balls,hk_bank5,globals+0x20))fail(-526);
    for(unsigned i=0;i<32;++i)if(balls[i*32] && spang_round_ball_init(balls+i*32,0,hk_bank1))fail(-527);
    spang_life_ready_begin(ready_actor,ready_objects);phase=STAGE_READY;
    ready_frame(0);update_hud();update_view();
}
static void panic_background(unsigned current,int initial)
{
    if(current>=NATIVE_TOUR_STAGES){fail(-600);return;}
    globals[0x19]=(uint8_t)current;globals[0x18]=hk_fixed[0x1260+current];globals[0x17]=hk_fixed[0x128a+globals[0x18]];
    scene_index=scenes[current].scene;++scene_revision;
    if(spang_scene_load(scene_index,scene_buffer)){fail(-589);return;}
    if(initial){
        memcpy(attrs,under_attrs,sizeof attrs);memcpy(chars,under_chars,sizeof chars);
        memcpy(palette,scene_buffer+6144,sizeof palette);
        (void)spang_panic_video_init(globals,attrs,chars,palette);
    }else{
        /* Original3C30 replaces the visual field only. Runtime actors, RNG,
         * scheduler, scores, HUD gauge and collision floor remain owned. */
        for(unsigned y=2;y<26;++y)for(unsigned x=9;x<55;++x){
            unsigned at=y*64+x;attrs[at]=under_attrs[at];memcpy(chars+at*2,under_chars+at*2,2);
        }
        memcpy(palette+0x400,scene_buffer+6144+0x400,0x200);
        palette_service(22);
    }
    ++tile_revision;++palette_revision;
}
static void reset_panic(int fresh)
{
    uint16_t seed=fresh?start_seed:round_objects.seed;
    if(fresh){
        memset(globals,0,sizeof globals);globals[0x1a]=2;fresh_players();
        memset(tour_bonus_wins,0,sizeof tour_bonus_wins);system_input=previous_system=0;
        globals[0x19]=38;
        if(spang_panic_init(&panic,globals,hk_fixed,game_difficulty)){fail(-601);return;}
    }else if(spang_panic_restart(&panic,globals,hk_fixed)){fail(-602);return;}
    bonus_mask_selector=255;panic_victory_hold=0;panic_input=panic_input2=0;panic_actor_tick=0;
    memset(timer,0,sizeof timer);memset(clear_actor,0,sizeof clear_actor);memset(clear_draw,0,sizeof clear_draw);memset(special_clear,0,sizeof special_clear);
    memset(ready_actor,0,sizeof ready_actor);memset(ready_objects,0,sizeof ready_objects);memset(platform_fx,0,sizeof platform_fx);
    memset(animal_actors,0,sizeof animal_actors);memset(animal_scheduler,0,sizeof animal_scheduler);memset(animal_draw,0,sizeof animal_draw);memset(animal_fx,0,sizeof animal_fx);animal_visible=0;
    memset(ladders,0,sizeof ladders);ladder_objects=0;
    memset(grid,0,sizeof grid);spang_stage_grid_clear(grid);
    spang_round_frame_init(&round_objects,seed);spang_weapons_init(&weapons);spang_weapons_init(&weapons2);spang_items_init(items);spang_stage_pool_clear(balls);
    if(spang_stage_session_reset(globals)){fail(-603);return;}
    globals[0]=0x18;globals[1]=4;globals[2]=globals[4]=globals[5]=globals[6]=0;globals[0x0a]=1;
    bootstrap_player(0,0);bootstrap_player(1,0);
    shot_pose=shot_pose2=clear_visible=special_visible=retry_pending=timeout_hold=0;last_input=last_input2=dead_flags=0;world_steps=0;cadence=0;
    music_request[0]=music_request[1]=0;
    panic_background(globals[0x19],1);flash(0);player_entry_hud(player);player_entry_hud(player2);
    spang_life_ready_begin(ready_actor,ready_objects);phase=STAGE_READY;
    draw_round(0);finish_objects(1,0);update_hud();update_view();
}
static int start_game_players_seed(unsigned mode,unsigned start_stage,unsigned player_mask,uint16_t seed,int demonstration)
{
    if((mode!=STAGE_MODE_TOUR && mode!=STAGE_MODE_PANIC) || !player_mask || player_mask>3 ||
       (mode==STAGE_MODE_TOUR?start_stage>=NATIVE_TOUR_STAGES:start_stage!=0))return -1;
    game_difficulty=demonstration?1:configured_difficulty;game_lives=demonstration?3:configured_lives;
    start_seed=seed;start_mask=player_mask;demo_kind=demo_age=demo_done=0;finished_pending=finished_mask=finished_sequence=records_lock_mask=0;
    memset(&demo_input,0,sizeof demo_input);memset(&demo_tutorial,0,sizeof demo_tutorial);memset(finished,0,sizeof finished);
    map_input=map_input2=ending_input=ending_input2=panic_input=panic_input2=0;memset(ending_extra,0,sizeof ending_extra);
    fault=0;ticks=world_steps=presentation_frames=tile_revision=palette_revision=pending_pressed=pending_pressed2=0;
    second_held=second_pressed=0;continue_music_pending=gameover_hold=0;
    scene_revision=0;scene_index=0;memset(map_actors,0,sizeof map_actors);memset(animal_actors,0,sizeof animal_actors);
    audio_head=audio_count=audio_sequence=0;
    if(best_imported)memcpy(best,imported_best,4);else{memset(best,0,sizeof best);best[1]=0x50;}
    audio(STAGE_AUDIO_STOP,0,0);
    if(mode==STAGE_MODE_PANIC)reset_panic(1);else reset_life((int)start_stage+1);
    return fault?fault:0;
}
int stage_start_game_players(unsigned mode,unsigned start_stage,unsigned player_mask)
{return start_game_players_seed(mode,start_stage,player_mask,session_seed,0);}
void stage_finish_session(void)
{if(!demo_kind&&!fault)session_seed=round_objects.seed;}
int stage_start_game(unsigned mode,unsigned start_stage)
{return stage_start_game_players(mode,start_stage,1);}
void stage_init(void){session_seed=0x01c3;configured_difficulty=1;configured_lives=3;(void)stage_start_game(STAGE_MODE_TOUR,0);}
int stage_start_selected_game_players(unsigned start_stage,unsigned player_mask)
{
    int result=stage_start_game_players(STAGE_MODE_TOUR,start_stage,player_mask);if(result)return result;
    /* BANK3:A7F8 awards every entered player before FIXED3658 rank +5. */
    for(unsigned n=0;n<2;++n)if(player_mask&(1u<<n))
        for(unsigned i=0;i<start_stage;++i)spang_score_add(actor(n)+0x40,0x10);
    globals[0x52]+=5;
    if(spang_round_speed_apply(globals,0,hk_fixed))fail(-597);
    update_hud();update_view();return fault?fault:0;
}
int stage_start_selected_game(unsigned start_stage)
{return stage_start_selected_game_players(start_stage,1);}
void stage_set_credits(unsigned credits){credit_count=credits>99?99:credits;}
unsigned stage_credits(void){return credit_count;}
void stage_set_best(const uint8_t score[4])
{if(score){memcpy(imported_best,score,4);memcpy(best,score,4);best_imported=1;update_hud();update_view();}}
static void fire(unsigned n)

{
    uint8_t *p=actor(n);unsigned *pose=n?&shot_pose2:&shot_pose;
    if(p[0x23]){command(9);return;} /* BANK0:AC51..AC6A. */
    int spawned=spang_weapons_fire(weapon_pool(n),p,hk_bank2);
    if(spawned<0){fail(-528);return;}if(!spawned)return;
    command((p[0x57]&7)==6?0:8);
    unsigned anim=word(hk_bank0+0x2e10+4*p[17]+2*p[0x21]);
    if(anim<0x8000 || anim+4>=0xc000){fail(-529);return;}
    p[0x20]=10;p[4]=2;p[0x56]=0;put(p+6,anim);*pose=p[8]=hk_bank0[anim-0x8000]&127;
}
static void game_over(void)
{
    /* FIXED3876..38A5: main state10 owns Game Over, palette request0C.
     * Main state0E/request12 is the separate Tour continue presentation. */
    phase=STAGE_GAME_OVER;globals[0]=0x10;globals[1]=2;
    audio(STAGE_AUDIO_STOP,0,0);palette_service(12);spang_weapons_init(&weapons);spang_weapons_init(&weapons2);
    gameover_hold=globals[0x1a]==2?210:0;continue_music_pending=0x32;
    draw_status_word(0xa049,STAGE_ROLE_GAMEOVER);
}
static void remember_attempt(uint8_t p[128])
{
    if(memcmp(p+0x44,p+0x40,4)<0)memcpy(p+0x44,p+0x40,4);
}
static void finish_player(unsigned n)
{
    uint8_t *p=actor(n);if(demo_kind || (finished_mask&(1u<<n)))return;
    remember_attempt(p);memcpy(finished[n].score,p+0x44,4);
    finished[n].location[0]=p[0x48];finished[n].location[1]=p[0x49];
    finished[n].location[2]=p[0x4d];finished[n].location[3]=p[0x4e];
    finished[n].sequence=++finished_sequence;finished_pending|=1u<<n;finished_mask|=1u<<n;records_lock_mask|=1u<<n;
}
int stage_take_finished_player(uint8_t out[128],unsigned *owner)
{
    if(!out || !owner || !finished_pending)return 0;
    unsigned n=(finished_pending&1)?0:1;finished_pending&=~(1u<<n);*owner=n;
    memset(out,0,128);out[17]=(uint8_t)n;memcpy(out+0x40,finished[n].score,4);memcpy(out+0x44,finished[n].score,4);
    out[0x48]=finished[n].location[0];out[0x49]=finished[n].location[1];out[0x4d]=finished[n].location[2];out[0x4e]=finished[n].location[3];
    return (int)finished[n].sequence;
}
unsigned stage_finished_mask(void){return finished_mask;}
void stage_set_records_tick(stage_records_tick_fn callback){records_tick_callback=callback;}
static void records_tick(uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2)
{
    if(records_tick_callback && !fault){
        unsigned mask=records_tick_callback(globals[0x1a],ticks,held1,pressed1,held2,pressed2,tile_write,0)&3;
        unsigned complete=records_lock_mask&~mask;
        /* BANK0:9B00..9B3B, after the editor and original game-over footer.
         * Keep the immutable finish snapshot; E08D is deliberately retained. */
        for(unsigned n=0;n<2;++n)if(complete&(1u<<n)){
            uint8_t *p=actor(n);memset(p+0x40,0,8);p[0x72]=p[0x61]=p[0x62]=0;
            if(globals[0x1a]){p[0x61]=12;p[0x62]=6;}
            globals[0x0c]&=(uint8_t)((n+1)^3);
        }
        records_lock_mask=mask;
    }
    update_view();
}
unsigned stage_game_finished(void)
{return !demo_kind && (phase==STAGE_COMPLETE || (phase==STAGE_GAME_OVER && !gameover_hold));}
unsigned stage_player_can_start(unsigned n)
{
    if(n>1 || demo_kind || globals[0x1a] || globals[0x1d] || phase==STAGE_COMPLETE || phase==STAGE_ENDING || phase==STAGE_MAP || phase==STAGE_GAME_OVER)return 0;
    const uint8_t *p=actor(n);return !(records_lock_mask&(1u<<n)) && !p[0x60] && p[0x61]<=6;
}
static void clear_player_hud(unsigned n)
{
    unsigned dest=n?0xddce:0xdd90;
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<17;++x){
        unsigned at=dest+y*128+x*2;tile_write(0,(uint16_t)at,0x20);tile_write(0,(uint16_t)(at+1),0);
    }
}
static void session_text(unsigned source)
{if(spang_video_stream(0x14a5,(uint16_t)source,hk_bank0,tile_write,0))fail(-611);}
static void session_colour(unsigned dest,unsigned rows,unsigned width,uint8_t colour)
{for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<width;++x)tile_write(0,(uint16_t)(dest+y*64+x),colour);}
static void waiting_player(unsigned n)
{
    /* BANK0:9C4D's18 states; blinking occupies only the owner's lower
     * panel, preserving the score above it (9D56 and9EB0/9EC8). */
    uint8_t *p=actor(n);unsigned state=p[0x62];
    if(state>34 || (state&1)){fail(-620);return;}
    unsigned handler=word(hk_bank0 + 0x1c5e + state);
    if(handler==0x9c82){
        if(!actor(n^1)[0])return;
        p[0x62]=2;p[0x65]=1;p[0x66]=40;p[0x67]=10;p[0x71]=0x59;put(p+0x6a,n?0x9ec8:0x9eb0);
        unsigned dest=n?0xde4e:0xde10;
        for(unsigned y=0;y<3;++y)for(unsigned x=0;x<17;++x){unsigned d=dest+y*128+x*2;tile_write(0,(uint16_t)d,0x20);tile_write(0,(uint16_t)(d+1),0);}
        dest=n?0xddd0:0xdda8;for(unsigned x=0;x<4;++x){tile_write(0,(uint16_t)(dest+x*2),0x20);tile_write(0,(uint16_t)(dest+x*2+1),0);}
        session_text(n?0xa1c6:0xa192);return;
    }
    if(--p[0x65])return;
    p[0x62]+=2;
    if(handler==0x9386 || handler==0x93b9){
        p[0x65]=p[handler==0x9386?0x66:0x67];
        session_colour(n?0xcf69:0xcf49,2,15,handler==0x9386?p[0x71]:0x44);
    }else if(handler==0x9cc3){
        p[0x65]=p[0x66];
        if(spang_video_stream(0x117c,(uint16_t)(n?0xa1d5:0xa1a1),hk_bank0,tile_write,0))fail(-611);
    }else if(handler==0x93d2){if(p[0x62]>=0x24)p[0x62]=0;}
    else fail(-620);
}
static void continue_begin(unsigned n)
{
    uint8_t *p=actor(n);remember_attempt(p);
    p[0x72]=0x40;p[0x62]=2;p[0x65]=5;p[0x66]=0;p[0x70]=0x20;p[0x68]=10;
    clear_player_hud(n);session_text(n?0xa0fd:0xa0d2);
    if(spang_video_stream(0x117c,(uint16_t)(n?0xa11b:0xa0f0),hk_bank0,tile_write,0))fail(-611);
}
static void death_rank(uint8_t p[128])
{
    /* FIXED3142: only alternating deaths adjust rank in a shared session. */
    if(globals[0x19]>=40)return;
    unsigned who=p[17];
    if(who+1!=globals[0x0c]){if(who!=globals[0x46])return;globals[0x46]^=1;}
    unsigned retries=globals[0x0f],amount=0;
    if(retries<3)amount=hk_fixed[0x31aa+retries*32+(spang_items_random(&round_objects.seed)&31)];
    else for(unsigned i=0;i<20;++i)if(hk_fixed[0x3196+i]==retries){amount=1;break;}
    if(!amount || globals[0x52]<amount)return;
    if(spang_round_speed_adjust(globals,(uint8_t)(0-amount),hk_fixed) || spang_round_speed_apply(globals,round_objects.slow[0]!=0,hk_fixed))fail(-612);
}
static void end_life(unsigned n)
{
    uint8_t *p=actor(n);if(!p[0] || !p[0x60])return;
    if(globals[0x1a]){if(spang_panic_death_reset(globals,hk_fixed))fail(-606);}else death_rank(p);
    spang_weapons_init(weapon_pool(n));if(n)shot_pose2=0;else shot_pose=0;
    dead_flags&=(uint8_t)~(1u<<n);globals[0x2d]=dead_flags;
    round_objects.stop[0]=8;round_objects.stop[17]=8;put(round_objects.stop+2,0x9379);
    globals[0x2b]=1;globals[0x2c]=0;
    if(!spang_player_life_end(p)){
        p[0x5f]=1;p[0x48]=globals[0x17];p[0x49]=globals[0x19];
        unsigned level=globals[0x53];p[0x4d]=(uint8_t)((level/10)*16+level%10);
        p[0x61]=(uint8_t)(globals[0x1a]?12:6);p[0x62]=0;p[0x70]=0x20;
        remember_attempt(p);
        if(globals[0x1a]){p[0x65]=180;p[0x62]=2;p[0x72]=0x40;clear_player_hud(n);session_text(n?0xa142:0xa128);}
    }
}
static void no_lives(void)
{
    if(!globals[0x1a] && (player[0x61]==6 || player2[0x61]==6)){
        phase=STAGE_CONTINUE;globals[0]=0x0e;globals[1]=2;
        /* FIXED0E68 command0A selects BANK2:9F91, whose descriptor is
         * A057 CONTINUE at192,88. A049 GAME OVER belongs to command06
         * and palette12; pairing it with18 blacks out its OVER fill. */
        audio(STAGE_AUDIO_STOP,0,0);palette_service(18);
        draw_status_word_at(0xa057,STAGE_ROLE_CONTINUE,192,88);
        continue_music_pending=0x33;
    }else game_over();
}
static void death_players(void)
{
    for(unsigned n=0;n<2;++n){uint8_t *p=actor(n);if(!player_dying(p))continue;
        if(p[2]==4){uint8_t sound,pal;
            if(p[3]==0)flash(4);else if(p[3]==2 && p[0x25]==1)flash(2);
            if(spang_player_death(p,hk_bank0,&sound,&pal))fail(-531);else if(sound!=255)command(sound);
        }else end_life(n);
    }
    /* Attract owners3E17/3EE8/40A4 call only the frame task. The live
     * session owners378D/3B9D (retry/wait/respawn) are not dispatched. */
    if(dying() || fault || demo_kind)return;
    if(globals[0x1a]){
        for(unsigned n=0;n<2;++n){uint8_t *p=actor(n),*other=actor(n^1);
            if(!p[0] && p[0x2f]==4 && p[0x60] && other[0] && !other[0x2f])bootstrap_player(n,1);
        }
    }
    if(player[0] || player2[0])return;
    if(player[0x60] || player2[0x60]){
        if(!globals[0x1a] && spang_round_retry_account(globals,player,player2,hk_fixed))fail(-532);
        phase=STAGE_RETRY;retry_pending=1;
    }else no_lives();
}
static void enter_player(unsigned n)
{
    uint8_t *p=actor(n);int continued=p[0x61]==6;
    if(!continued)memset(p+0x44,0,4);
    p[0x51]=(uint8_t)(continued?2:0);memset(p+0x40,0,4);
    p[0x60]=(uint8_t)game_lives;p[0x61]=4;p[0x62]=0;p[0x72]=0x80;p[0x4e]=2;p[0x4a]=0;
    p[0x4b]=hk_fixed[0x149];p[0x4c]=hk_fixed[0x14a];p[0x5f]=0;
    globals[0x0c]|=(uint8_t)(1u<<n);globals[0x0d]=globals[0x0c];finished_mask&=~(1u<<n);
    clear_player_hud(n);bootstrap_player(n,1);spang_weapons_init(weapon_pool(n));
    if(n){last_input2=0;shot_pose2=0;}else{last_input=0;shot_pose=0;}
    if(phase==STAGE_CONTINUE){
        continue_music_pending=0;audio(STAGE_AUDIO_STOP,0,0);
        phase=STAGE_RETRY;retry_pending=1;
    }
}
static void session_players(uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2)
{
    const uint32_t held[2]={held1,held2},pressed[2]={pressed1,pressed2};
    for(unsigned n=0;n<2;++n)if((pressed[n]&STAGE_COIN) && credit_count<99)++credit_count;
    for(unsigned n=0;n<2;++n){uint8_t *p=actor(n);
        if(stage_player_can_start(n) && (pressed[n]&STAGE_START) && credit_count){--credit_count;enter_player(n);continue;}
        if(p[0x60])continue;
        if(p[0x61]==6){
            if(!p[0x62]){continue_begin(n);continue;}
            int decrement=(pressed[n]&STAGE_FIRE)!=0;
            if(held[n]&STAGE_FIRE){if(!decrement && !--p[0x68])decrement=1;}else p[0x68]=10;
            if(!decrement && !--p[0x65]){p[0x65]=5;unsigned dest=n?0xdf56:0xdf16;
                if(p[0x66]<9){tile_write(0,(uint16_t)(dest+p[0x66]*2),0x2e);++p[0x66];}
                else{p[0x66]=0;decrement=1;for(unsigned i=0;i<9;++i)tile_write(0,(uint16_t)(dest+i*2),0x20);}
            }
            if(decrement){p[0x68]=10;
                if(!p[0x70]){p[0x61]=8;p[0x62]=0;p[0x72]=0x20;finish_player(n);}
                else{p[0x70]=(uint8_t)(p[0x70]-((p[0x70]&15)?1:7));
                    if(p[0x70]==5)session_colour(n?0xcee8:0xcec8,4,17,0x57);}
            }
            if(p[0x61]==6){unsigned dest=n?0xdee8:0xdea8;
                tile_write(0,(uint16_t)dest,(uint8_t)(p[0x70]>>4));tile_write(0,(uint16_t)(dest+1),0);
                tile_write(0,(uint16_t)(dest+2),(uint8_t)(p[0x70]&15));tile_write(0,(uint16_t)(dest+3),0);
            }
        }else if(p[0x61]==12 && p[0x62] && p[0x65]){
            if(!--p[0x65]){
                if(p[0x62]==2){p[0x62]=4;p[0x65]=30;clear_player_hud(n);}
                else{p[0x61]=8;p[0x62]=0;finish_player(n);}
            }
        }
    }
    if(phase==STAGE_CONTINUE && player[0x61]!=6 && player2[0x61]!=6 && !retry_pending){
        /* BANK0:9631 enters main state10 after both continues expire. */
        phase=STAGE_GAME_OVER;globals[0]=0x10;globals[1]=2;palette_service(12);
        draw_status_word(0xa049,STAGE_ROLE_GAMEOVER);
        gameover_hold=0;continue_music_pending=0;
    }
}
static void enter_timeout(void)
{
    /* New native integration of original FIXED0DA5,37FD..3843. It is not a
     * fabricated collision: timeout removes a life without death flight. */
    globals[0]=0x0c;globals[1]=2;globals[5]=180;timeout_hold=180;phase=STAGE_TIMEOUT;
    palette_service(10);for(unsigned n=0;n<2;++n)if(actor(n)[0])end_life(n);
    globals[0x2b]=1;globals[0x2c]=0;player[0x5f]=player2[0x5f]=0;
    if(spang_round_speed_adjust(globals,(uint8_t)(0-globals[0x57]),hk_fixed))fail(-530);
    spang_weapons_init(&weapons);spang_weapons_init(&weapons2);shot_pose=shot_pose2=0;dead_flags=0;
    audio(STAGE_AUDIO_STOP,0,0);draw_status_word(0xa03b,STAGE_ROLE_TIMEOUT);
}
static void control_player(unsigned n,uint8_t input,uint8_t previous,int edge,int vertical)
{
    uint8_t *p=actor(n);unsigned *pose=n?&shot_pose2:&shot_pose;
    if(vertical || ladder_objects || globals[0x19]>=5){
        uint8_t sound;int r=player_vertical_step(p,input,previous,edge,grid,ladders,hk_bank0,hk_bank2,weapon_pool(n),&sound);
        if(r)fail(r);else if(sound!=255)command(sound);
    }else{
        if(*pose){p[8]=(uint8_t)--*pose;if(!*pose){p[4]=0;if(spang_player_input(p,0,0,hk_bank0))fail(-536);}}
        else if(edge)fire(n);
        else if(spang_player_input(p,input&0x30,previous&0x30,hk_bank0))fail(-537);
        if(!fault && (p[0x20]==6 || p[0x20]==8)){
            if(spang_player_animate(p,hk_bank0) || spang_player_walk(p,p[0x20]==6?1:-1))fail(-538);
            else if(spang_player_side(p,grid)<0)fail(-539);
        }
    }
    if(n)last_input2=input;else last_input=input;
}
static void player_contacts(unsigned n)
{
    uint8_t *p=actor(n);
    if(p[0x54]){int r=spang_items_shield_contact(p,balls,(uint8_t)(world_steps+16),globals[0x2b]);if(r<0)fail(-540);else if(r)command(0x0b);}
    else if(spang_player_contact(p,balls,(uint8_t)(world_steps+16),globals[0x2b],&dead_flags)<0)fail(-541);
    globals[0x2d]=dead_flags;
}
static void gameplay_tick(uint32_t held,uint32_t pressed)
{
    uint8_t input=arcade_input(held),input2=arcade_input(second_held);
    const uint8_t inputs[2]={input,input2},previous[2]={last_input,last_input2};
    const int edges[2]={!!((pressed&STAGE_FIRE) || ((input&8) && !(last_input&8))),
        !!((second_pressed&STAGE_FIRE) || ((input2&8) && !(last_input2&8)))};
    int frozen=dying();if(frozen)death_players();
    if(phase==STAGE_CONTINUE || phase==STAGE_GAME_OVER)return;
    int advance=!frozen && !retry_pending;
    unsigned pending=pending_actor_commands(advance);
    if(advance){
        globals[9]=(uint8_t)world_steps;
        for(unsigned n=0;n<2;++n)if(actor(n)[0]){fauna_protection_tick(actor(n),(uint8_t)world_steps);spang_items_shield_tick(actor(n),(uint8_t)world_steps);}
    }
    draw_round(advance);
    if(!fault)animal_step(advance,input);
    if(fault)return;
    if(advance){
        uint8_t lives=player[0x60],lives2=player2[0x60];
        if(spang_round_reward_queue(&round_objects,player,player2,hk_fixed+0x2e44,best,hk_fixed+0x149)<0)fail(-533);
        if(player[0x60]!=lives || player2[0x60]!=lives2)command(0x13);
        for(unsigned i=0;i<8;++i)if((round_objects.events[i]&SPANG_ITEM_SCORE) && items[i*32+15]<8)
            if(spang_items_weapon_hud(items[i*32+15],items[i*32+24]/2,hk_bank1,tile_write,0))fail(-534);
    }
    /* Preserve the delivered R03 task cadence. In the two added phases,
     * original weapons and bubbles precede the platform-effect task. */
    int late_platform=globals[0x19]>=3;
    if(!late_platform)platform_step(advance);
    deliver_actor_commands(late_platform?pending&~4u:pending,advance);++world_steps;
    if(fault)return;
    if(spang_weapons_round_step_audio(&weapons,&weapons2,(uint8_t)(world_steps+16),edges,advance,hk_bank2,grid,balls,player,player2,globals,round_objects.lucky,restore_platform,animal_weapon_contact,fauna_command,0)<0)fail(-535);
    if(!fault){
        /* FIXED041D/BANK2:9CBB bubbles follow weapon tasks. */
        uint16_t cursor=(uint16_t)frame_cursor;uint8_t room=(uint8_t)remaining;
        draw_transients(1,advance,&cursor,&room);frame_cursor=cursor;remaining=room;
    }
    if(!fault && late_platform){
        unsigned platform_commands=pending_actor_commands(advance)&4;
        platform_step(advance);
        if(platform_commands)command(6);
    }
    if(fault)return;
    if(advance)for(unsigned n=0;n<2;++n){uint8_t *p=actor(n);if(!p[0] || player_dying(p))continue;
        control_player(n,inputs[n],previous[n],edges[n],0);
        player_contacts(n);
    }
    if(globals[0]==8 && (player[0] || player2[0]) && !dying() && !retry_pending && globals[0x4e]<2 && demo_kind<3){
        uint8_t effects[4];unsigned old=word(timer+1);
        spang_round_clock_step(timer,globals,hk_clock_tables,0,effects);
        if(effects[0]){globals[0x57]+=effects[0];if(spang_round_speed_adjust(globals,effects[0],hk_fixed) || spang_round_speed_apply(globals,round_objects.slow[0]!=0,hk_fixed))fail(-542);}
        if(effects[1])command(effects[1]);
        if(effects[2])palette_copy(0x3e0,hk_warning_palettes+((effects[2]-1)/2)*32,32);
        if(old!=word(timer+1))spang_round_clock_draw(timer,hk_fixed+0x152e,tile_write,0);
        if(effects[3]){enter_timeout();return;}
    }
    if(!fault && !dying() && !retry_pending){
        if(spang_round_slow_step(&round_objects,globals,hk_fixed,1))fail(-543);
        for(unsigned n=0;n<2;++n){uint8_t *p=actor(n);if(!p[0])continue;
            unsigned slot=(globals[9]+p[17]*2)&3;
            for(unsigned i=slot;i<8;i+=4)if(spang_items_collect(items+i*32,p,globals)<0)fail(-544);
            fauna_context context=animal_context(input);fauna_player_contact(&context,p,globals[9]);
            if(spang_round_lucky_contact(&round_objects,p,globals[9])<0)fail(-545);
        }
    }
    phase=retry_pending?STAGE_RETRY:dying()?STAGE_DYING:globals[0]==0x12?STAGE_CLEAR:STAGE_PLAYING;
}
static void clear_tick(void)
{
    ++world_steps;begin_objects();uint16_t life_code;
    for(unsigned n=0;n<2;++n)if(actor(n)[0] && spang_round_clear_player(actor(n),hk_bank0,&life_code))fail(-546);
    if(globals[1]==2 && globals[2]==0){
        uint8_t effects[2];
        if(spang_round_clear_actor(clear_actor,globals,player[0x60],player2[0x60],hk_bank3,effects,clear_draw)){fail(-547);return;}
        if(effects[0])palette_service(effects[0]);
        clear_visible=effects[1]&1;
        if(effects[1]&4){memcpy(special_clear,clear_actor,32);special_visible=1;}
        if(effects[1]&8){uint8_t *owner=globals[0x4e]==4?player2:player;
            spang_score_add(owner+0x40,(uint16_t)word(hk_fixed+0x2e44+0x5c));spang_score_promote(best,owner+0x40);extra_lives_for(owner);}
        if(effects[1]&2)for(int n=1;n>=0;--n)if(globals[0x41]&(2u<<n)){
            uint8_t queue[2];
            if(spang_round_bonus_box_player((unsigned)n,actor((unsigned)n)[0x58],hk_fixed+0x152e,tile_write,0,queue) ||
               spang_video_command(0x1496,queue[1],hk_bank10,tile_write,0))fail(-548);
        }
    }else if(globals[1]==2 && globals[2]==2){
        uint8_t awards[6],wins;
        int result=spang_round_bonus_players(clear_actor,timer,globals[0x0f],globals[0x18],globals[0x41],player[0x58],player2[0x58],hk_bank3,hk_clock_tables,hk_fixed+0x152e,tile_write,0,awards,&wins);
        if(result<0)fail(-549);
        else for(unsigned n=0;n<2;++n)if(result&(1u<<n)){
            uint8_t *p=actor(n);if(wins&(1u<<n))++tour_bonus_wins[n];
            spang_score_add_low(p+0x40,(uint16_t)word(awards+n*3+1),awards[n*3]);spang_score_promote(best,p+0x40);extra_lives_for(p);
        }
    }
    phase=globals[1]==2 && globals[2]>=2?STAGE_BONUS:STAGE_CLEAR;
}
/* Original first map, BANK3 plane/markers plus BANK5 FA00 bootstrap.
 * Object writes use the original alternate video page D000, kept separate
 * from tile writes by callback instead of emulated hardware banking. */
static void map_sprite_write(void *context,uint16_t address,uint8_t value)
{
    (void)context;if(address<0xd000 || address>=0xe000){fail(-581);return;}
    objects[address-0xd000]=value;
}
static void begin_map(void)
{
    bonus_mask_selector=255;
    unsigned world=globals[0x18];
    /* Venice has two stages. Scene identity is not world*4 thereafter. */
    static const uint8_t journeys[14]={1,5,9,13,17,21,24,28,32,36,40,44,47,51};
    if(world>=sizeof journeys){fail(-590);return;}
    scene_index=journeys[world];++scene_revision;phase=STAGE_MAP;map_input=last_input&8;map_input2=last_input2&8;
    spang_map_presentation_begin(&map_presentation);
    if(spang_scene_load(scene_index,scene_buffer)){fail(-589);return;}
    memcpy(attrs,scene_buffer,sizeof attrs);
    memcpy(chars,scene_buffer+2048,sizeof chars);
    memcpy(palette,scene_buffer+6144,sizeof palette);
    ++tile_revision;++palette_revision;globals[1]=8;globals[14]=2;globals[0x1c]=globals[0x17];
    if(spang_tour_spawn(map_actors,hk_bank5))fail(-582);
    memset(animal_actors,0,sizeof animal_actors);spang_weapons_init(&weapons);
    memset(animal_fx,0,sizeof animal_fx);
    memset(ladders,0,sizeof ladders);ladder_objects=0;
    clear_visible=special_visible=0;begin_objects();
}
static void map_queue(void *context,unsigned request,unsigned parameter)
{
    (void)context;
    if(request==8){
        if(spang_video_command(0x1496,(uint8_t)parameter,hk_bank10,tile_write,0))fail(-591);
    }else if(request==10 || request==12){
        uint8_t *p=request==12?player2:player;
        spang_score_add(p+0x40,(uint16_t)word(hk_fixed+0x2e44+parameter));
        spang_score_promote(best,p+0x40);extra_lives_for(p);
    }else fail(-592);
}
static void map_tick(uint32_t held)
{
    uint8_t previous1=map_input,previous2=map_input2;
    map_input=(held&STAGE_FIRE)?8:0;map_input2=(second_held&STAGE_FIRE)?8:0;
    spang_map_presentation_step(&map_presentation,(uint8_t)(map_input|map_input2),map_actors[2],map_actors[19]);
    begin_objects();put(globals+0x10,0xd000);globals[0x14]=127;
    /* BANK3 owns edge acceptance and player-life gating. A held button from
     * the previous tick is not a fresh edge, including the entry to state4. */
    uint8_t inputs[6]={map_input,previous1,map_input2,previous2,player[0x60],player2[0x60]},events[2];
    int regional=globals[0x18]==4 || globals[0x18]==9;
    if(regional){
        if(spang_tour_region_step(map_actors,globals,inputs,hk_bank3,map_queue,0,events)){fail(-583);return;}
    }else if(spang_tour_plane_step(map_actors,globals,inputs,hk_bank3,map_sprite_write,0,events)){fail(-583);return;}
    frame_cursor=word(globals+0x10)-0xd000;remaining=globals[0x14];
    mark_group(0,frame_cursor,STAGE_TAG_NATIVE,(int)word(map_actors+10),STAGE_ROLE_MAP_PLANE);
    if(events[0]){if(regional)command(events[0]);else audio(STAGE_AUDIO_OKI,events[0],0x10);}
    for(unsigned i=1;i<7;++i){
        uint8_t *a=map_actors+i*40;unsigned first=frame_cursor;unsigned selected=a[0] && a[2]==4 && globals[0x14]>=4;
        if(spang_tour_marker_step(a,globals,hk_bank3,map_sprite_write,0)){fail(-584);return;}
        frame_cursor=word(globals+0x10)-0xd000;remaining=globals[0x14];
        /* The selected-city ring and text use different palettes. */
        if(selected && frame_cursor>=first+128){mark_group(first,first+128,STAGE_TAG_NATIVE,(int)word(a+10),STAGE_ROLE_MAP_MARKER);first+=128;}
        mark_group(first,frame_cursor,STAGE_TAG_NATIVE,(int)word(a+10),STAGE_ROLE_MAP_MARKER);
    }
    if(regional){
        /* BANK3:823C/8269 disables the sweep on a region-completion map.
         * Its two initial label commands are already in the scene data. */
        uint8_t *a=map_actors+280;
        if(a[0] && !a[2]){a[2]=2;a[19]=10;a[0]=0;}
    }else spang_tour_timer_step(map_actors+280,attrs,hk_bank3,tile_write,0);
    if(events[1]){
        /* BANK3:8BEA..8BED: FIXED2BA7 resets FM, then 2C54 stops OKI.
         * The plane module leaves this scene-owner cleanup to its caller. */
        audio(STAGE_AUDIO_STOP,0,0);
        if(spang_round_advance(globals,hk_fixed))fail(-585);else reset_life(0);
    }
}
static void ending_queue(void *context,unsigned request,unsigned parameter)
{
    (void)context;
    unsigned target=word(hk_fixed+0x4460+request);
    if((target!=0x116d && target!=0x1496) ||
       spang_video_command((uint16_t)target,(uint8_t)parameter,hk_bank10,tile_write,0))fail(-595);
}
static void begin_ending(void)
{
    bonus_mask_selector=255;phase=STAGE_ENDING;scene_index=54;++scene_revision;
    ending_input=ending_input2=0;ending_extra[0]=0x10;ending_extra[1]=tour_bonus_wins[0];ending_extra[2]=tour_bonus_wins[1];ending_extra[3]=4;
    audio(STAGE_AUDIO_STOP,0,0);
}
static void ending_tick(uint32_t held)
{
    if(globals[1]==10){
        /* FIXED3A43/10B9: blank both original pages and retain no ladders. */
        globals[0x1d]=1;globals[1]=12;
        if(spang_scene_load(54,scene_buffer)){fail(-589);return;}
        memcpy(attrs,scene_buffer,sizeof attrs);memcpy(chars,scene_buffer+2048,sizeof chars);++tile_revision;
        memset(ladders,0,sizeof ladders);ladder_objects=0;memset(animal_actors,0,sizeof animal_actors);
        memset(map_actors,0,sizeof map_actors);clear_visible=special_visible=0;begin_objects();return;
    }
    if(globals[1]==12){
        /* FIXED3A58/1E8F and BANK5:81FD,8920 sequence5. */
        globals[1]=14;globals[0x0e]=5;globals[0x1e]=8;
        palette_copy(0x400,scene_buffer+6144+0x400,512);
        for(unsigned i=0;i<8;++i)map_actors[i*40+16]=4;
        map_actors[0]=1;map_actors[17]=18;return;
    }
    begin_objects();put(globals+0x10,0xd000);globals[0x14]=127;
    uint8_t input=(held&STAGE_FIRE)?8:0;
    if(held&STAGE_LEFT)input|=0x20;
    if(held&STAGE_RIGHT)input|=0x10;
    if(held&STAGE_UP)input|=0x80;
    if(held&STAGE_DOWN)input|=0x40;
    uint8_t input2=arcade_input(second_held);
    uint8_t inputs[4]={input,ending_input,input2,ending_input2},done=0;
    ending_input=input;ending_input2=input2;
    for(unsigned i=0;i<8;++i){
        uint8_t *a=map_actors+i*40,events[2];if(!a[0])continue;
        unsigned first=frame_cursor;
        if(spang_tour_ending_step(a,map_actors,globals,player,player2,inputs,ending_extra,
            hk_bank3,hk_fixed+0x152e,ending_queue,a[15]?map_sprite_write:tile_write,0,events)){fail(-596);return;}
        frame_cursor=word(globals+0x10)-0xd000;remaining=globals[0x14];
        if(a[15])mark_group(first,frame_cursor,STAGE_TAG_NATIVE,(int)word(a+10),STAGE_ROLE_CLEAR);
        if(events[0])command(events[0]);
        done|=events[1];
    }
    spang_score_promote(best,player+0x40);spang_score_promote(best,player2+0x40);
    if(done){
        /* FIXED3A7F..3AC4 clears active lives after the original ending. */
        globals[1]=16;
        uint8_t *players[2]={player,player2};
        for(unsigned n=0;n<2;++n){uint8_t *p=players[n];p[0x60]=p[0]=p[2]=p[3]=p[4]=p[5]=0;
            p[0x48]=2;p[0x49]=39;p[0x4d]=0x99;if(p[0x61]<5){p[0x61]=6;p[0x62]=0;}}
        for(unsigned n=0;n<2;++n)if(globals[0x0c]&(1u<<n))finish_player(n);
        phase=STAGE_COMPLETE;
    }
}
static void flow_tick(void)
{
    if(globals[0]==8)for(unsigned n=0;n<2;++n)if(actor(n)[0x5f]==2)waiting_player(n);
    if((!player[0] && !player2[0]) || dying() || retry_pending || phase==STAGE_TIMEOUT || phase==STAGE_GAME_OVER || phase==STAGE_CONTINUE)return;
    uint8_t effects[2];int r=spang_round_flow(globals,player,player2,clear_actor,hk_fixed,effects);
    if(r<0){fail(-551);return;}
    if(effects[0])command(effects[0]);
    if(effects[1]&1){
        if(globals[0x41]>6 || (globals[0x41]&1)){fail(-593);return;}
        bonus_mask_selector=globals[0x41];
    }
    if(effects[1]&2){begin_map();return;}
    if(effects[1]&4){reset_life(0);return;}
    if(effects[1]&8){begin_ending();return;}
    if(globals[0]==0x12)phase=globals[1]==2 && globals[2]>=2?STAGE_BONUS:STAGE_CLEAR;
}
static void panic_events(void)
{
    for(unsigned n=0;n<panic.event_count;++n){
        const uint8_t *e=panic.events+n*3;
        if(e[0]==SPANG_PANIC_FM)audio(STAGE_AUDIO_COMMAND,e[1],e[2]);
        else if(e[0]==SPANG_PANIC_QUEUE){
            if(round_objects.queue_length>62){fail(-604);return;}
            unsigned at=round_objects.queue_length;round_objects.queue[at]=e[1];round_objects.queue[at+1]=e[2];round_objects.queue_length+=2;
        }else if(e[0]==SPANG_PANIC_PALETTE && e[1]==13 && e[2]==20)flash(2);
        else{fail(-605);return;}
    }
    spang_panic_events_reset(&panic);
}
static void panic_tick(uint32_t held,uint32_t pressed)
{
    int victory=panic_victory_hold!=0;
    uint8_t input=arcade_input(held),input2=arcade_input(second_held);
    panic_input=victory?0:input;panic_input2=victory?0:input2;
    const uint8_t inputs[2]={input,input2},previous[2]={last_input,last_input2};
    const int edges[2]={!!((pressed&STAGE_FIRE) || ((input&8) && !(last_input&8))),
        !!((second_pressed&STAGE_FIRE) || ((input2&8) && !(last_input2&8)))};
    int frozen=dying();if(frozen)death_players();
    if(phase==STAGE_CONTINUE || phase==STAGE_GAME_OVER)return;
    int advance=!frozen && !globals[0x2d] && !retry_pending;
    globals[9]=(uint8_t)world_steps;
    unsigned pending=pending_actor_commands(advance);
    if(advance)for(unsigned n=0;n<2;++n)if(actor(n)[0]){fauna_protection_tick(actor(n),(uint8_t)world_steps);spang_items_shield_tick(actor(n),(uint8_t)world_steps);}
    panic_actor_tick=1;draw_round(advance);panic_actor_tick=0;if(fault)return;
    /* The frame owner clears its reward FIFO. Scheduler and ball events are
     * merged after that reset, then consumed once after the service tasks. */
    panic_events();if(fault)return;
    if(victory){uint16_t code;
        for(unsigned n=0;n<2;++n)if(actor(n)[0] && spang_round_clear_player(actor(n),hk_bank0,&code)){fail(-546);return;}
    }else if(advance)for(unsigned n=0;n<2;++n)if(actor(n)[0] && !player_dying(actor(n))){
        control_player(n,inputs[n],previous[n],edges[n],1);if(fault)return;player_contacts(n);if(fault)return;
    }
    if(spang_weapons_round_step_audio(&weapons,&weapons2,(uint8_t)(world_steps+16),edges,advance,hk_bank2,grid,balls,player,player2,globals,round_objects.lucky,restore_platform,0,fauna_command,0)<0){fail(-607);return;}
    if(advance){
        if(spang_round_slow_step(&round_objects,globals,hk_fixed,1)){fail(-543);return;}
        if(spang_panic_services(&panic,globals,player,player2,hk_fixed)){fail(-608);return;}
        for(unsigned n=0;n<2;++n){uint8_t *p=actor(n);if(!p[0] || player_dying(p))continue;
            unsigned slot=(globals[9]+p[17]*2)&3;
            for(unsigned i=slot;i<8;i+=4)if(spang_items_collect(items+i*32,p,globals)<0){fail(-544);return;}
            if(spang_round_lucky_contact(&round_objects,p,globals[9])<0){fail(-545);return;}
        }
        panic_events();if(fault)return;
        uint8_t lives=player[0x60],lives2=player2[0x60];
        if(spang_round_reward_queue(&round_objects,player,player2,hk_fixed+0x2e44,best,hk_fixed+0x149)<0){fail(-533);return;}
        if(player[0x60]!=lives || player2[0x60]!=lives2)command(0x13);
        deliver_actor_commands(pending,1);
    }
    ++world_steps;
    phase=victory?STAGE_CLEAR:retry_pending?STAGE_RETRY:dying()?STAGE_DYING:ready_actor[0]?STAGE_READY:STAGE_PLAYING;
    finish_objects(1,advance);
    if(victory){
        if(!--panic_victory_hold){globals[0]=0x12;globals[1]=10;begin_ending();}
    }else if(!retry_pending && !dying() && globals[0x56]){
        if(globals[0x56]!=255){
            unsigned background=globals[0x56];globals[0x56]=0;panic_background(background,0);
        }else if(!globals[0x21]){
            globals[1]=6;panic_victory_hold=360;phase=STAGE_CLEAR;
            for(unsigned n=0;n<2;++n)if(actor(n)[0]){uint8_t *p=actor(n);p[2]=2;p[3]=6;p[4]=p[5]=0;}
            spang_weapons_init(&weapons);spang_weapons_init(&weapons2);command(0x36);
        }
    }
}
static void tick_game(uint32_t held,uint32_t pressed)
{
    if(phase==STAGE_ENDING){ending_tick(held);flush_actor_audio();update_view();return;}
    if(globals[0x1a]==2){
        if(retry_pending)reset_panic(0);else panic_tick(held,pressed);
        if(phase!=STAGE_ENDING)update_hud();
        flush_actor_audio();update_view();return;
    }
    if(phase==STAGE_MAP){map_tick(held);flush_actor_audio();update_view();return;}
    if(retry_pending){reset_life(0);flush_actor_audio();update_view();return;}
    if(phase==STAGE_TIMEOUT){
        if(timeout_hold)--timeout_hold;
        globals[5]=(uint8_t)timeout_hold;
        if(!timeout_hold){if(player[0x60] || player2[0x60]){phase=STAGE_RETRY;retry_pending=1;}else no_lives();}
        update_hud();flush_actor_audio();update_view();return;
    }
    if(ready_actor[0] && !demo_kind)ready_frame(1);
    else{
        int gameplay=globals[0]==8 || (globals[0]==0x12 && globals[1]==0);
        if(gameplay)gameplay_tick(held,pressed);else clear_tick();
        uint32_t old_scene=scene_revision;
        if(!fault && !demo_kind)flow_tick();
        if(scene_revision==old_scene && phase!=STAGE_MAP && phase!=STAGE_ENDING && phase!=STAGE_TIMEOUT && phase!=STAGE_GAME_OVER && phase!=STAGE_CONTINUE && !fault){
            if(globals[0]==0x12 && globals[1])begin_objects();
            finish_objects(!(globals[0]==0x12 && globals[1]),!dying()&&!retry_pending);
        }
    }
    if(phase!=STAGE_MAP)update_hud();
    flush_actor_audio();update_view();
}
static uint32_t native_input(uint8_t input)
{return ((input&0x20)?STAGE_LEFT:0)|((input&0x10)?STAGE_RIGHT:0)|((input&8)?STAGE_FIRE:0)|((input&0x80)?STAGE_UP:0)|((input&0x40)?STAGE_DOWN:0);}
static void demo_frozen(void)
{
    if(ready_actor[0]){int done=spang_life_ready_tick(ready_actor,ready_objects);if(done<0)fail(-521);else if(done)palette_service(22);}
    draw_round(0);if(!globals[0x1a])animal_step(0,0);finish_objects(1,0);flush_actor_audio();update_view();
}
static int start_tutorial(unsigned which)
{
    /* FIXED3D4D/3D9D/3DCE and BANK5:83F3. Scene0 supplies only the
     * shared boot palettes and the unchanged1738 border. Tutorial geometry,
     * balls, items and input are the original table entries40/41. */
    unsigned current=40+which;demo_kind=3+which;scene_index=55+which;++scene_revision;
    for(unsigned t=0;t<2048;++t){
        attrs[t]=0x10;put(chars+t*2,0x20);
        if(t>=0x40 && t<0x680){attrs[t]=(uint8_t)(0x1d+which);put(chars+t*2,0x70e);}
    }
    for(unsigned y=1;y<=26;++y)for(unsigned x=8;x<=55;++x)
        if(y==1 || y==26 || x==8 || x==55){unsigned t=y*64+x;attrs[t]=under_attrs[t];memcpy(chars+t*2,under_chars+t*2,2);}
    memcpy(scene_buffer,attrs,sizeof attrs);memcpy(scene_buffer+2048,chars,sizeof chars);
    /* Original BANK12:8608/8628, retained when1E66 changes palette0. */
    for(unsigned i=0;i<16;++i){put(palette+0x3a0+i*2,i<15?0x01bf:0);put(palette+0x3c0+i*2,i<15?0x0119:0);}
    palette_copy(0,hk_ready_palette,32);++tile_revision;++palette_revision;
    memset(globals,0,sizeof globals);globals[0]=8;globals[0x0c]=globals[0x0d]=(uint8_t)(1u<<which);
    globals[0x19]=(uint8_t)current;globals[0x45]=1;globals[0x52]=5;
    if(spang_stage_session_reset(globals) || spang_round_speed_apply(globals,0,hk_fixed))return -617;
    memset(player,0,sizeof player);memset(player2,0,sizeof player2);player2[17]=1;
    uint8_t *p=actor(which);p[0x60]=4;p[0x4a]=10;
    spang_stage_player(p,0,hk_bank0);put(p+10,128);p[13]=192;p[0x21]=0;
    unsigned anim=word(hk_bank0+0x2df8+which*4);put(p+6,anim);p[8]=hk_bank0[anim-0x8000]&127;
    player_entry_hud(p);
    memset(timer,0,sizeof timer);memset(ready_actor,0,sizeof ready_actor);memset(ready_objects,0,sizeof ready_objects);
    memset(grid,0,sizeof grid);spang_stage_grid_clear(grid);memset(ladders,0,sizeof ladders);ladder_objects=0;
    if(spang_stage_platforms(current,grid,attrs,chars,hk_bank5) || spang_stage_ladders(current,ladders,hk_bank5))return -618;
    for(unsigned i=0;i<8 && ladders[i*16];++i)ladder_objects+=2*((ladders[i*16+15]+1)/2);
    spang_round_frame_init(&round_objects,0x01c3);spang_weapons_init(&weapons);spang_weapons_init(&weapons2);spang_items_init(items);
    memset(animal_actors,0,sizeof animal_actors);memset(animal_scheduler,0,sizeof animal_scheduler);memset(animal_draw,0,sizeof animal_draw);animal_visible=0;
    music_request[0]=music_request[1]=0;spang_stage_pool_clear(balls);
    if(spang_stage_balls(current,balls,hk_bank5,globals+0x20))return -619;
    for(unsigned i=0;i<32;++i)if(balls[i*32] && spang_round_ball_init(balls+i*32,0,hk_bank1))return -527;
    if(which){
        for(unsigned i=0;i<8;++i){const uint8_t *record=hk_bank5+0xad4+i*4;uint8_t *a=i<7?items+i*32:round_objects.lucky;
            a[0]=2;put(a+2,i<7?0xb041:0xabcb);put(a+10,word(record));a[13]=record[2];a[15]=record[3];}
    }
    if(spang_attract_tutorial_init(&demo_tutorial,which))return -614;
    if(spang_attract_input_init(&demo_input,hk_bank5,which))return -613;
    phase=STAGE_PLAYING;draw_round(0);finish_objects(1,0);update_hud();update_view();return fault?fault:0;
}
int stage_start_demo(unsigned mode,unsigned scenario)
{
    if((mode==STAGE_MODE_TOUR && scenario>3) || (mode==STAGE_MODE_PANIC && scenario) || (mode!=0 && mode!=2))return -1;
    if(mode==STAGE_MODE_TOUR && scenario>=2){int r=start_game_players_seed(mode,0,1u<<(scenario-2),0x01c3,1);return r?r:start_tutorial(scenario-2);}
    int r=start_game_players_seed(mode,mode==0?(scenario?5:2):0,3,0x01c3,1);if(r)return r;
    demo_kind=mode==0?1+scenario:5;demo_age=demo_done=0;globals[0x0a]=0;
    player[0x60]=player2[0x60]=4;round_objects.seed=0x01c3;
    if(mode==0){globals[0x52]=5;if(spang_round_speed_apply(globals,0,hk_fixed))return -597;}
    else{globals[0x53]=31;globals[0x19]=18;if(spang_panic_level(globals,hk_fixed,1))return -601;reset_panic(0);globals[0x0a]=0;}
    if(spang_attract_input_init(&demo_input,hk_bank5,mode==0?2+scenario:4))return -613;
    update_hud();update_view();return fault?fault:0;
}
unsigned stage_is_demo(void){return demo_kind!=0;}
unsigned stage_demo_finished(void){return demo_done;}
void stage_tick_players(uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2)
{
    if(fault || (demo_kind && demo_done))return;
    second_held=held2;second_pressed=pressed2;
    if(phase==STAGE_COMPLETE && !demo_kind)return;
    ++ticks;previous_system=system_input;system_input=(uint8_t)(((held1&STAGE_START)?8:0)|((held2&STAGE_START)?2:0));
    if(demo_kind){
        system_input=previous_system=0;++demo_age;
        if(demo_kind==3 || demo_kind==4){uint16_t request=0;
            if(spang_attract_tutorial_tick(&demo_tutorial,hk_bank3,attrs,chars,&request)){fail(-614);return;}
            ++tile_revision;
            if(request && spang_video_command(0x11e8,(uint8_t)request,hk_bank10,tile_write,0)){fail(-615);return;}
            if(demo_tutorial.done){demo_done=1;audio(STAGE_AUDIO_STOP,0,0);update_view();return;}
            if(demo_tutorial.paused){demo_frozen();return;}
        }else{
            unsigned play=demo_kind==5?1200:600;
            if(demo_age>=play+120){demo_done=1;audio(STAGE_AUDIO_STOP,0,0);update_view();return;}
            if(demo_age<=60 || demo_age>play+60){demo_frozen();return;}
        }
        uint8_t inputs[2];
        if(!dying()){
            if(spang_attract_input_tick(&demo_input,hk_bank5,inputs)){fail(-613);return;}
            held1=native_input(inputs[0]);second_held=native_input(inputs[1]);pressed1=second_pressed=0;
        }else{held1=second_held=0;pressed1=second_pressed=0;}
        tick_game(held1,pressed1);return;
    }
    session_players(held1,pressed1,held2,pressed2);
    /* Player entry owns this tick before the deferred continue cue. A START
     * accepted here cancels M051 before READY retires the postgame cache. */
    if(continue_music_pending){command(continue_music_pending);continue_music_pending=0;}
    if(phase==STAGE_GAME_OVER){if(gameover_hold)--gameover_hold;flush_actor_audio();update_view();records_tick(held1,pressed1,held2,pressed2);return;}
    if(phase==STAGE_CONTINUE){flush_actor_audio();update_view();records_tick(held1,pressed1,held2,pressed2);return;}
    tick_game(held1,pressed1);
    records_tick(held1,pressed1,held2,pressed2);
}
void stage_tick(uint32_t held,uint32_t pressed)
{
    /* Compatibility entry: terminal START still begins ordinary Tour0. */
    if((phase==STAGE_COMPLETE || (phase==STAGE_GAME_OVER && !gameover_hold)) && (pressed&STAGE_START)){stage_init();return;}
    stage_tick_players(held,pressed,0,0);
}
unsigned stage_advance_vblanks_players(unsigned elapsed,uint32_t held1,uint32_t pressed1,uint32_t held2,uint32_t pressed2)
{
    presentation_frames+=elapsed;pending_pressed|=pressed1;pending_pressed2|=pressed2;
    unsigned due=spang_frame_clock_due(&cadence,elapsed),completed=0;
    while(due--){
        uint32_t previous_scene_revision=scene_revision;
        stage_tick_players(held1,pending_pressed,held2,pending_pressed2);pending_pressed=pending_pressed2=0;if(fault)break;++completed;
        if(scene_revision!=previous_scene_revision){cadence=0;break;}
    }
    update_view();return completed;
}
unsigned stage_advance_vblanks(unsigned elapsed,uint32_t held,uint32_t pressed)
{
    if((phase==STAGE_COMPLETE || (phase==STAGE_GAME_OVER && !gameover_hold)) && (pressed&STAGE_START)){stage_init();return 0;}
    return stage_advance_vblanks_players(elapsed,held,pressed,0,0);
}
const stage_view *stage_get_view(void){return &view;}
unsigned stage_ending_page(void)
{
    if(scene_index!=54)return 0;
    if(phase==STAGE_COMPLETE)return 3;
    const uint8_t *a=map_actors;
    if(a[3]!=2)return 0;
    switch(a[4]){
    case 4:case 6:return 1;
    case 8:case 10:return 2;
    case 12:case 14:case 16:return 3;
    default:return 0;
    }
}
const uint8_t *stage_bank(unsigned bank)
{
    switch(bank){case 0:return hk_bank0;case 1:return hk_bank1;case 2:return hk_bank2;case 3:return hk_bank3;case 4:return hk_bank4;case 5:return hk_bank5;case 10:return hk_bank10;default:return 0;}
}
const uint8_t *stage_fixed(void){return hk_fixed;}
static uint32_t hash_bytes(uint32_t hash,const void *p,unsigned n)
{const uint8_t *b=p;while(n--){hash^=*b++;hash*=16777619u;}return hash;}
uint32_t stage_state_hash(void)
{
    uint32_t h=2166136261u;
    /* Explicit byte fields and little-endian scalars give the same digest on
     * the host and big-endian SH-2; never hash C structure padding/pointers.
     * Output FIFO consumption and presentation/revision telemetry are omitted.
     * Every owned logical actor, tile, palette and render-list byte is included. */
#define HASH_ARRAY(a) h=hash_bytes(h,(a),sizeof(a))
    HASH_ARRAY(player);HASH_ARRAY(player2);HASH_ARRAY(globals);HASH_ARRAY(balls);HASH_ARRAY(items);
    HASH_ARRAY(timer);HASH_ARRAY(attrs);HASH_ARRAY(chars);HASH_ARRAY(palette);HASH_ARRAY(grid);HASH_ARRAY(objects);
    HASH_ARRAY(clear_actor);HASH_ARRAY(clear_draw);HASH_ARRAY(special_clear);HASH_ARRAY(ready_actor);HASH_ARRAY(ready_objects);
    if(tour_bonus_wins[0] || tour_bonus_wins[1])h=hash_bytes(h,tour_bonus_wins,sizeof tour_bonus_wins);
    if(phase==STAGE_ENDING){h=hash_bytes(h,ending_extra,sizeof ending_extra);h=hash_bytes(h,&ending_input,1);}
    if(system_input || previous_system){h=hash_bytes(h,&system_input,1);h=hash_bytes(h,&previous_system,1);}
    if(bonus_mask_selector!=255)h=hash_bytes(h,&bonus_mask_selector,1);
    HASH_ARRAY(platform_fx);HASH_ARRAY(best);HASH_ARRAY(music_request);HASH_ARRAY(map_actors);HASH_ARRAY(animal_actors);HASH_ARRAY(animal_scheduler);HASH_ARRAY(animal_draw);
    HASH_ARRAY(round_objects.fx);HASH_ARRAY(round_objects.scores);HASH_ARRAY(round_objects.stop);HASH_ARRAY(round_objects.lucky);
    HASH_ARRAY(round_objects.slow);HASH_ARRAY(round_objects.sounds);HASH_ARRAY(round_objects.queue);HASH_ARRAY(round_objects.events);HASH_ARRAY(round_objects.direct);
    HASH_ARRAY(weapons.harpoons);HASH_ARRAY(weapons.bullets);HASH_ARRAY(weapons.harpoon_draw);HASH_ARRAY(weapons.bullet_draw);
    HASH_ARRAY(weapons2.harpoons);HASH_ARRAY(weapons2.bullets);HASH_ARRAY(weapons2.harpoon_draw);HASH_ARRAY(weapons2.bullet_draw);
    HASH_ARRAY(demo_input.remaining);h=hash_bytes(h,&demo_input.loaded,1);
    HASH_ARRAY(demo_tutorial.actor);h=hash_bytes(h,&demo_tutorial.which,1);h=hash_bytes(h,&demo_tutorial.paused,1);
    h=hash_bytes(h,&demo_tutorial.done,1);h=hash_bytes(h,&demo_tutorial.error,1);
    if(records_lock_mask){uint8_t mask=(uint8_t)records_lock_mask;h=hash_bytes(h,&mask,1);}
    for(unsigned n=0;n<2;++n){h=hash_bytes(h,finished[n].score,4);h=hash_bytes(h,finished[n].location,4);}
#undef HASH_ARRAY
    const uint32_t values[]={ticks,world_steps,cadence,round_objects.seed,(uint32_t)phase,(uint32_t)fault,
        shot_pose,clear_visible,special_visible,retry_pending,timeout_hold,frame_cursor,remaining,group_id,pending_pressed,
        scene_index,map_input,animal_visible,last_input,dead_flags,round_objects.queue_length,round_objects.direct_length,weapons.bullet_visible,weapons.harpoon_visible,
        shot_pose2,pending_pressed2,map_input2,ending_input2,panic_input2,last_input2,weapons2.bullet_visible,weapons2.harpoon_visible,
        credit_count,continue_music_pending,gameover_hold,demo_kind,demo_age,demo_done,demo_input.pointer[0],demo_input.pointer[1],
        finished_pending,finished_mask,finished_sequence,finished[0].sequence,finished[1].sequence,0}; /* Retain the retired delay slot in non-map replay hashes. */
    for(unsigned i=0;i<sizeof values/sizeof values[0];++i)for(unsigned shift=0;shift<32;shift+=8){uint8_t b=(uint8_t)(values[i]>>shift);h=hash_bytes(h,&b,1);}
    if(phase==STAGE_MAP){
        const uint32_t map_values[]={map_presentation.age,map_presentation.source_steps,
            map_presentation.initial_hold,map_presentation.previous_fire,
            map_presentation.release_seen,map_presentation.skip_requested};
        for(unsigned i=0;i<sizeof map_values/sizeof map_values[0];++i)
            for(unsigned shift=0;shift<32;shift+=8){uint8_t b=(uint8_t)(map_values[i]>>shift);h=hash_bytes(h,&b,1);}
    }
    /* Preserve the existing ladder-free replay digest; new owned descriptors
     * join the digest only on a stage which actually has that actor pool. */
    if(ladder_objects)h=hash_bytes(h,ladders,sizeof ladders);
    if(globals[0x1a]==2){
        h=hash_bytes(h,(const uint8_t *)&panic,sizeof panic);h=hash_bytes(h,&panic_input,1);
        for(unsigned shift=0;shift<32;shift+=8){uint8_t b=(uint8_t)(panic_victory_hold>>shift);h=hash_bytes(h,&b,1);}
    }
    for(unsigned i=0;i<sizeof animal_fx;++i)if(animal_fx[i]){h=hash_bytes(h,animal_fx,sizeof animal_fx);break;}
    return h;
}

#ifdef STAGE_TESTING
/* Prepared-state lifecycle fixtures exist only in the host-test build. They
 * never enter the release runtime and are labelled separately from play. */
void stage_test_timeout_next_tick(void)
{ready_actor[0]=0;phase=STAGE_PLAYING;timer[0]=1;timer[1]=timer[2]=0;globals[0x2b]=0;globals[0x4e]=0;player[0]=3;player[1]=1;}
void stage_test_last_ball(void)
{
    ready_actor[0]=0;phase=STAGE_PLAYING;spang_stage_pool_clear(balls);spang_items_init(items);spang_weapons_init(&weapons);
    spang_round_frame_init(&round_objects,round_objects.seed);memset(platform_fx,0,sizeof platform_fx);
    uint8_t *a=balls;a[0]=2;a[15]=0;a[18]=0;put(a+10,256);a[13]=a[14]=176;put(a+2,0x82b0);spang_round_ball_init(a,0,hk_bank1);
    globals[0]=8;globals[1]=globals[2]=0;globals[0x20]=1;globals[0x2b]=globals[0x2c]=0;
    put(player+10,256);player[0]=3;player[1]=1;player[13]=192;last_input=0;shot_pose=0;
}
void stage_test_contact_next_tick(void)
{
    ready_actor[0]=0;phase=STAGE_PLAYING;spang_stage_pool_clear(balls);
    unsigned slot=(world_steps+17)&1;uint8_t *a=balls+slot*32;
    a[0]=2;a[15]=0;a[18]=0;put(a+10,word(player+10));a[13]=a[14]=(uint8_t)(player[13]-8);put(a+2,0x82b0);
    spang_round_ball_init(a,0,hk_bank1);globals[0x20]=1;globals[0x2b]=0;player[0]=1;player[0x54]=0;
}
void stage_test_set_lives(unsigned lives){player[0x60]=(uint8_t)lives;}
#endif
