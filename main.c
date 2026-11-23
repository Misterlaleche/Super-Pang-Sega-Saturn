#include <jo/jo.h>
#include "include/runtime.h"
#include "stage/stage.h"
#include "audio/audio.h"
#include "frontend/frontend.h"
#include "frontend/settings.h"
#include "include/hardware_diag.h"
#include "include/savegame.h"
#include "include/stack_usage.h"
#include "include/boot_diag.h"

__attribute__((used,externally_visible)) void *memcpy(void *dest,const void *src,unsigned int count)
{jo_memcpy(dest,src,count);return dest;}
__attribute__((used,externally_visible)) void *memset(void *dest,int value,unsigned int count)
{jo_memset(dest,value,count);return dest;}
__attribute__((used,externally_visible)) void *memmove(void *dest,const void *src,unsigned int count)
{
    unsigned char *d=dest;const unsigned char *s=src;
    if((uintptr_t)d>(uintptr_t)s){while(count){--count;d[count]=s[count];}}
    else{for(unsigned int i=0;i<count;++i)d[i]=s[i];}
    return dest;
}
__attribute__((used,externally_visible)) int memcmp(const void *left,const void *right,unsigned int count)
{
    const unsigned char *a=left,*b=right;unsigned int i;
    for(i=0;i<count;++i)if(a[i]!=b[i])return (int)a[i]-(int)b[i];
    return 0;
}
static unsigned last_vblank;
static unsigned last_scene_vblank;
/* A missing VDP1/VDP2 presentation must not silently freeze the logical
 * clock forever. This detector requires the main callback and VBlank IRQ to
 * continue running; it does not reset hardware or bypass a pending flip. */
enum {SCENE_WAIT_LIMIT_VBLANKS=600,SCENE_WAIT_ERROR=-301};
static unsigned scene_wait_start,scene_wait_presented;
static int scene_wait_active,scene_wait_has_presented;
static uint32_t prepared_scene_revision;
static unsigned prepared_game_phase,prepared_frontend_profile;
static int scene_vblank_seen;
static uint32_t previous_held[2];
enum {OWNER_RETURN_TITLE=1,OWNER_EXIT_BIOS=16,OWNER_RESET=OWNER_RETURN_TITLE|OWNER_EXIT_BIOS};
static unsigned pending_owner_action;
static unsigned reset_chord_latched;
static unsigned reset_origin_title[2];
static unsigned fault_presented;
static unsigned bios_exit_requested;
static unsigned game_paused;
static unsigned pending_player_start;
static uint32_t resume_release_block[2];
static unsigned game_entry_step,game_entry_cadence;
/* Read-only native diagnostics: the live owner is explicit even while the
 * inactive gameplay view retains a previous session. */
struct {uint32_t in_game,mode,stage,transitions,screen,credits,tick,phase,player_mask,demo;}
app_status __attribute__((used,externally_visible));
static int frontend_failed(void)
{
    int code=frontend_get_view()->fault;
    if(code)rt_report_fault_code(RT_FAULT_SIMULATION,code);
    return code!=0;
}
static void dispatch_audio(const stage_audio_event *sound)
{
    if(sound->kind==STAGE_AUDIO_COMMAND)audio_command(sound->command,sound->parameter);
    else if(sound->kind==STAGE_AUDIO_OKI)audio_oki(sound->command,sound->parameter);
    else if(sound->kind==STAGE_AUDIO_STOP)audio_stop();
    if(audio_get_status()->fault)rt_report_fault_code(RT_FAULT_AUDIO,audio_get_status()->fault);
}
static void drain_frontend(void)
{stage_audio_event sound;if(frontend_failed())return;while(!rt_fault_flags() && frontend_audio_pop(&sound))dispatch_audio(&sound);}
static void drain_game(void)
{stage_audio_event sound;while(!rt_fault_flags() && stage_audio_pop(&sound))dispatch_audio(&sound);}
static void prepare_frontend(void)
{
    if(frontend_failed())return;
    unsigned profile=frontend_audio_profile();
    if(profile==prepared_frontend_profile)return;
    /* The owner/profile changed at an explicit stopped boundary. Prepare
     * before its FIFO is dispatched, including the first original name cue. */
    audio_stop();
    savegame_flush();
    int result=profile?audio_prepare_postgame():audio_prepare_frontend();
    if(result)rt_report_fault_code(RT_FAULT_AUDIO,result);
    prepared_frontend_profile=profile;
}
static void prepare_game(const stage_view *view,unsigned force)
{
    unsigned phase=view->phase;
    if(!force && phase!=STAGE_READY && phase!=STAGE_ENDING &&
       phase!=STAGE_CONTINUE && phase!=STAGE_GAME_OVER)return;
    if(!force && view->scene_revision==prepared_scene_revision && phase==prepared_game_phase)return;
    audio_stop();
    int result;
    if(stage_is_demo())result=audio_prepare_tutorial();
    else if(phase==STAGE_CONTINUE || phase==STAGE_GAME_OVER)result=audio_prepare_postgame();
    else if(phase==STAGE_ENDING)result=audio_prepare_ending();
    else if(app_status.mode==STAGE_MODE_PANIC)result=audio_prepare_panic();
    else result=audio_prepare_stage(view->stage_index);
    if(result)rt_report_fault_code(RT_FAULT_AUDIO,result);
    prepared_scene_revision=view->scene_revision;prepared_game_phase=phase;
}
static void activate_frontend(void)
{
    if(frontend_failed())return;
    if(app_status.in_game)stage_finish_session();
    game_paused=pending_player_start=0;resume_release_block[0]=resume_release_block[1]=0;
    game_entry_step=game_entry_cadence=0;
    app_status.in_game=0;app_status.demo=0;++app_status.transitions;
    prepared_scene_revision=prepared_game_phase=prepared_frontend_profile=~0u;
    prepare_frontend();drain_frontend();rt_invalidate_scene();
}
static void return_to_title(void)
{
    if(app_status.in_game && !stage_is_demo())frontend_set_credits(stage_credits());
    frontend_return_to_title();activate_frontend();
}
/* frame() owns the chord on both pads, including the terminal fault exit. */
static void keep_restart_chord_in_app(void) {}
static int pad_key(unsigned player,jo_gamepad_keys key)
{return player?jo_is_pad2_key_pressed(key):jo_is_pad1_key_pressed(key);}
static uint32_t read_input(unsigned player)
{
    uint32_t held=0;
    if(pad_key(player,JO_KEY_LEFT))held|=STAGE_LEFT;
    if(pad_key(player,JO_KEY_RIGHT))held|=STAGE_RIGHT;
    if(pad_key(player,JO_KEY_UP))held|=STAGE_UP;
    if(pad_key(player,JO_KEY_DOWN))held|=STAGE_DOWN;
    if(pad_key(player,JO_KEY_A)||pad_key(player,JO_KEY_B))held|=STAGE_FIRE;
    if(pad_key(player,JO_KEY_START))held|=STAGE_START;
    if(pad_key(player,JO_KEY_C))held|=STAGE_BACK;
    return held;
}
static unsigned reset_chord_held(unsigned player)
{
    return pad_key(player,JO_KEY_START) && pad_key(player,JO_KEY_A) &&
        pad_key(player,JO_KEY_B) && pad_key(player,JO_KEY_C);
}
static void present_fault(const stage_view *view,unsigned in_game,unsigned mode,unsigned stage);
static void start_game(unsigned mode,unsigned start,unsigned mask,unsigned demo)
{
    game_paused=pending_player_start=0;resume_release_block[0]=resume_release_block[1]=0;
    game_entry_step=!demo && frontend_transition_darkness()==255?FRONTEND_TRANSITION_TICKS:0;
    game_entry_cadence=0;
    audio_stop();
    const spang_settings *settings=spang_settings_get();
    stage_set_rules(settings->difficulty,settings->lives);
    audio_set_options(settings->music,settings->effects,settings->mono,demo&&!settings->demo_sounds);
    int result=demo?stage_start_demo(mode,start):frontend_stage_select_used()?
        stage_start_selected_game_players(start,mask):stage_start_game_players(mode,start,mask);
    if(result){
        rt_report_fault_code(RT_FAULT_SIMULATION,result);
        present_fault(stage_get_view(),1,mode,start);return;
    }
    if(!demo){stage_set_credits(frontend_credits());stage_set_best(frontend_best(mode));}
    app_status.in_game=1;app_status.mode=mode;app_status.demo=demo;++app_status.transitions;
    prepared_frontend_profile=~0u;
    prepare_game(stage_get_view(),1);drain_game();rt_invalidate_scene();
}
static void present_fault(const stage_view *view,unsigned in_game,unsigned mode,unsigned stage)
{
    if(fault_presented || !rt_fault_flags())return;
    /* A terminal fault cannot safely continue the game or its active sound.
     * Keep its first context intact while IRQ paints a photographable report. */
    rt_fault_report report;
    report.flags=rt_fault_flags();report.first_flags=rt_fault_first_flags();
    report.code=rt_fault_code();report.in_game=in_game;
    report.mode=mode;report.stage=stage;
    report.scene=view->scene_index;report.phase=view->phase;report.tick=view->tick;
    report.vdp_free=jo_sprite_free_memory();
    rt_hw_diag_latch_fault(&report);fault_presented=1;
    audio_stop();game_paused=pending_player_start=0;
}
static int pause_allowed(void)
{
    unsigned phase=stage_get_view()->phase;
    return phase!=STAGE_CONTINUE && phase!=STAGE_GAME_OVER &&
        phase!=STAGE_COMPLETE && phase!=STAGE_FAULT && phase!=STAGE_FRONTEND;
}
static void frame(void)
{
    static unsigned boot_first_frame=1;
    if(boot_first_frame){boot_first_frame=0;boot_diag_mark(50);rt_invalidate_scene();}

    if(bios_exit_requested)return;
    rt_hw_diag_frame_begin();
    if(audio_get_status()->fault)rt_report_fault_code(RT_FAULT_AUDIO,audio_get_status()->fault);
    uint32_t held[2]={read_input(0),read_input(1)};
    unsigned chord=(reset_chord_held(0)?1u:0u)|(reset_chord_held(1)?2u:0u);
    /* A fault can arrive with the chord already held and the bitmap still
     * pending. Capture and stop live audio before handing control to BIOS. */
    if(rt_fault_flags() && chord){
        const stage_view *view=app_status.in_game?stage_get_view():frontend_get_view();
        present_fault(view,app_status.in_game,app_status.mode,view->stage_index);
        bios_exit_requested=1;
        rt_hw_diag_frame_end();
        jo_core_exit_to_multiplayer();
        return;
    }
    if(rt_hw_diag_repaint_needed())rt_invalidate_scene();
    uint32_t pressed[2]={held[0]&~previous_held[0],held[1]&~previous_held[1]};
    unsigned now=jo_core_get_vblank_count(),elapsed=now-last_vblank;
    unsigned simulation_elapsed=elapsed,presented_at;
    unsigned owner_action=0;
    const stage_view *view;
    /* A gesture starts at the first constituent, before ordinary START/C
     * can change the screen. Each pad retains that origin until every one of
     * its A/B/C/START buttons is up; normal standalone controls stay immediate. */
    const uint32_t reset_inputs=STAGE_FIRE|STAGE_BACK|STAGE_START;
    unsigned screen=frontend_screen_kind();
    for(unsigned i=0;i<2;++i){
        if(!(held[i]&reset_inputs))reset_origin_title[i]=0;
        else if(!(previous_held[i]&reset_inputs))
            reset_origin_title[i]=!app_status.in_game &&
                (screen==FRONTEND_TITLE || screen==FRONTEND_CREDIT_TITLE);
    }
    previous_held[0]=held[0];previous_held[1]=held[1];last_vblank=now;
    /* The raw four-button state is independent of which button arrived last.
     * Capture the destination before waiting for a pending scene: one chord
     * cannot become a second BIOS action when its return reveals the title. */
    if(reset_chord_latched && !((held[0]|held[1])&reset_inputs) &&
       !(pending_owner_action&OWNER_RESET))reset_chord_latched=0;
    if(chord && !reset_chord_latched){
        pending_owner_action=reset_origin_title[(chord&1u)?0:1]?
            OWNER_EXIT_BIOS:OWNER_RETURN_TITLE;
        reset_chord_latched=1;pending_player_start=0;
    }
    /* Consume the chord and partial releases on both pads. Keep raw history
     * above so a held constituent cannot turn into a fresh menu/pause edge. */
    if(reset_chord_latched || (pending_owner_action&OWNER_RESET))
        for(unsigned i=0;i<2;++i){held[i]&=~reset_inputs;pressed[i]&=~reset_inputs;}
    /* Entry on either pad owns START before pause on both pads. Retain only
     * this priority until a logical tick consumes the stage's existing input
     * queue; replaying START/COIN here would create duplicate credits. */
    if(app_status.in_game && !stage_is_demo() && !game_paused && !game_entry_step){
        for(unsigned i=0;i<2;++i)
            if((pressed[i]&STAGE_START) && stage_player_can_start(i))pending_player_start|=1u<<i;
        if(pending_player_start)pending_owner_action&=~8u;
    }
    /* Keep a short input edge until the displayed frame releases its hidden
     * bitmap. Ownership changes must obey the same hold as scene time. */
    if(app_status.in_game && stage_is_demo()){
        if(pressed[0]&STAGE_START)pending_owner_action|=2;
        if(pressed[1]&STAGE_START)pending_owner_action|=4;
    }
    else if(app_status.in_game && !game_entry_step && !pending_player_start && (game_paused || pause_allowed())){
        for(unsigned i=0;i<2;++i)
            if((pressed[i]&STAGE_START) && (game_paused || !stage_player_can_start(i)))
                pending_owner_action|=8;
    }
    /* Loading/repainting a hidden scene must not consume its readable time.
     * Music and PCM below still receive the real elapsed VBlanks. */
    if(rt_scene_pending()){
        unsigned stamp=0;int has_stamp=rt_scene_presented_vblank(&stamp);
        simulation_elapsed=0;
        /* The IRQ may have displayed A during the timestamp query. B can
         * subsequently be queued by this callback, so don't count that gap
         * as time B was waiting. Only main queues another scene. */
        if(!rt_scene_pending())scene_wait_active=scene_wait_has_presented=0;
        else if(!scene_wait_active || (has_stamp &&
           (!scene_wait_has_presented || stamp!=scene_wait_presented))){
            /* A VBlank may present a frame after 'now' was sampled above.
             * Start the deadline no earlier than the observation itself. */
            scene_wait_active=1;scene_wait_start=jo_core_get_vblank_count();
            scene_wait_has_presented=has_stamp;scene_wait_presented=stamp;
        }else if(now-scene_wait_start>=SCENE_WAIT_LIMIT_VBLANKS)
            rt_report_fault_code(RT_FAULT_VIDEO,SCENE_WAIT_ERROR);
    }
    else{
    scene_wait_active=scene_wait_has_presented=0;
    owner_action=pending_owner_action;pending_owner_action=0;
    if(rt_scene_presented_vblank(&presented_at) &&
            (!scene_vblank_seen || presented_at!=last_scene_vblank))
    {
        /* A VBlank can commit after 'now' was sampled. Keep that stamp
         * unconsumed until the clock catches up; subtraction wraps at32bits. */
        if((int32_t)(now-presented_at)<0)simulation_elapsed=0;
        else
        {
            unsigned visible_elapsed=now-presented_at;
            if(visible_elapsed<simulation_elapsed)simulation_elapsed=visible_elapsed;
            last_scene_vblank=presented_at;scene_vblank_seen=1;
        }
    }
    }
    if(!rt_fault_flags())
    {
        if(owner_action&OWNER_EXIT_BIOS){
            audio_stop();bios_exit_requested=1;
            rt_hw_diag_frame_end();jo_core_exit_to_multiplayer();return;
        }
        if(owner_action&OWNER_RETURN_TITLE)return_to_title();
        else if(app_status.in_game)
        {
            if(stage_is_demo() && (owner_action&6)){
                frontend_interrupt_demo((owner_action&4)?1:0);activate_frontend();
            }else if(game_entry_step){
                /* Keep READY readable while its new bitmap fades into view.
                 * Pending/late commits already clamp simulation_elapsed.
                 * Audio below retains the real elapsed clock. */
                if(simulation_elapsed>FRONTEND_TRANSITION_TICKS+1)game_entry_step=0;
                else{
                    game_entry_cadence+=simulation_elapsed*2871u;
                    unsigned ticks=game_entry_cadence/3000u;game_entry_cadence%=3000u;
                    game_entry_step=ticks<game_entry_step?game_entry_step-ticks:0;
                }
                for(unsigned i=0;i<2;++i)resume_release_block[i]=held[i]&~STAGE_START;
            }else if((owner_action&8) || game_paused || (pending_owner_action&8)){
                if(owner_action&8){
                    game_paused=!game_paused;audio_pause(game_paused!=0);
                    /* Inputs used while stopped cannot become delayed shots,
                     * movement or menu edges when START releases the clock. */
                    for(unsigned i=0;i<2;++i)resume_release_block[i]=held[i]&~STAGE_START;
                }
            }else{
                for(unsigned i=0;i<2;++i){
                    resume_release_block[i]&=held[i];
                    held[i]&=~resume_release_block[i];pressed[i]&=~resume_release_block[i];
                    if((pressed[i]&STAGE_START) && stage_player_can_start(i))pressed[i]|=STAGE_COIN;
                }
                if(stage_advance_vblanks_players(simulation_elapsed,held[0],pressed[0],held[1],pressed[1]))pending_player_start=0;
                view=stage_get_view();
                if(view->fault)rt_report_fault_code(RT_FAULT_SIMULATION,view->fault);
                if(!rt_fault_flags()){prepare_game(view,0);drain_game();}
                /* A finished player's name editor shares the stage clock
                 * while the partner plays. Its F13 is already loaded;
                 * draining it does not change the live music profile. */
                if(!rt_fault_flags() && !stage_is_demo())drain_frontend();
                if(!rt_fault_flags() && stage_is_demo()){
                    if(stage_demo_finished()){
                        unsigned mode,scenario;
                        frontend_demo_finished();
                        /* Tutorials hand directly to their Tour demo. A halted
                         * ATTRACT_REQUEST is not a visible frontend scene:
                         * publishing it would still own the hidden bitmap when
                         * the next demo requests its READY background. */
                        if(frontend_failed()){
                            /* Retain the old owner/context on a failed handoff. */
                        }else if(frontend_take_demo(&mode,&scenario)){
                            drain_frontend();
                            if(!rt_fault_flags())start_game(mode,scenario,3,1);
                        }else activate_frontend();
                    }
                }else if(!rt_fault_flags()){
                    uint8_t actor[128];unsigned index;
                    while(stage_take_finished_player(actor,&index))frontend_finish_player(app_status.mode,actor,index);
                    if(stage_game_finished()){
                        frontend_set_credits(stage_credits());
                        frontend_finish_game(app_status.mode,view->player,view->player2,view->player_mask);
                        activate_frontend();
                    }
                }
            }
        }
        else
        {
            unsigned mode,stage,mask;
            frontend_advance_vblanks_players(simulation_elapsed,held[0],pressed[0],held[1],pressed[1]);
            view=frontend_get_view();
            if(view->fault)rt_report_fault_code(RT_FAULT_SIMULATION,view->fault);
            if(!rt_fault_flags()){prepare_frontend();drain_frontend();}
            if(!rt_fault_flags()){
                if(frontend_take_start_players(&mode,&stage,&mask))start_game(mode,stage,mask,0);
                else if(frontend_take_demo(&mode,&stage))start_game(mode,stage,3,1);
            }
        }
    }
    view=app_status.in_game?stage_get_view():frontend_get_view();
    const spang_settings *settings=spang_settings_get();
    audio_set_options(settings->music,settings->effects,settings->mono,app_status.in_game&&stage_is_demo()&&!settings->demo_sounds);
    if(!app_status.in_game && app_status.screen==FRONTEND_OPTIONS && frontend_screen_kind()==FRONTEND_HOME){
        unsigned before=savegame_status.writes;audio_stop();savegame_flush();
        frontend_save_notice(savegame_status.error?-1:savegame_status.writes>before?1:0);
    }
    rt_set_screen_offset(settings->x,settings->y);
    if(view->fault)rt_report_fault_code(RT_FAULT_SIMULATION,view->fault);
    if(!rt_fault_flags())audio_service(elapsed);
    if(audio_get_status()->fault)rt_report_fault_code(RT_FAULT_AUDIO,audio_get_status()->fault);
    app_status.screen=frontend_screen_kind();app_status.credits=app_status.in_game?stage_credits():frontend_credits();
    app_status.tick=view->tick;app_status.phase=view->phase;
    if(app_status.in_game){app_status.stage=view->stage_index;app_status.player_mask=view->player_mask;}
    if(!rt_fault_flags()){
        short offset=(short)-(int)(app_status.in_game?
            frontend_transition_level(game_entry_step):frontend_transition_darkness());
        jo_set_screen_color_filter_a(JO_NBG1_SCREEN|JO_SPRITE_SCREEN,offset,offset,offset);
        rt_draw_stage(view);
        if(game_paused && !rt_fault_flags())rt_draw_pause();
    }
    if(rt_fault_flags()){
        jo_set_screen_color_filter_a(JO_NBG1_SCREEN|JO_SPRITE_SCREEN,0,0,0);
        present_fault(view,app_status.in_game,app_status.mode,view->stage_index);
        /* Early video-reservation failure has no safe bitmap for the IRQ
         * panel. Preserve the tiny VDP1 fallback only for that case. */
        if(!rt_video_ready())rt_draw_fault();
    }
    rt_hw_diag_frame_end();
    boot_diag_finish();
    stack_usage_sample();
}
void jo_main(void)
{
    jo_core_init(JO_COLOR_Black);
    boot_diag_mark(4);
    stack_usage_init();
    jo_core_set_restart_game_callback(keep_restart_chord_in_app);
    rt_hw_diag_init();
    jo_core_set_vblank_probes(rt_hw_diag_vblank_begin,rt_hw_diag_vblank_draw,rt_hw_diag_vblank_end);
    jo_core_set_screen_single_interlace(false);
    /* Jo also owns LowRAM through its normal allocator; no C13 fixed buffers. */
    jo_add_memory_zone((unsigned char *)JO_WORK_RAM_LOW,JO_WORK_RAM_SIZE);
    boot_diag_mark(5);
    frontend_init();
    boot_diag_mark(6);
    savegame_load();
    stage_set_records_tick(frontend_stage_records_tick);
    boot_diag_mark(20);
    int art_ready=rt_init_art(frontend_get_view());
    if(rt_video_ready())rt_hw_diag_enable_video();
    if(art_ready)
    {
        boot_diag_mark(30);
        int result=audio_init_frontend();
        if(result)rt_report_fault_code(RT_FAULT_AUDIO,result);
    }
    prepared_scene_revision=prepared_game_phase=~0u;prepared_frontend_profile=frontend_audio_profile();
    previous_held[0]=previous_held[1]=0;pending_owner_action=fault_presented=game_paused=pending_player_start=0;
    bios_exit_requested=reset_chord_latched=0;
    reset_origin_title[0]=reset_origin_title[1]=0;
    game_entry_step=game_entry_cadence=0;
    resume_release_block[0]=resume_release_block[1]=0;scene_vblank_seen=0;
    scene_wait_active=scene_wait_has_presented=0;scene_wait_start=scene_wait_presented=0;
    last_vblank=jo_core_get_vblank_count();
    boot_diag_mark(40);
    jo_core_add_callback(frame);jo_core_run();
}
