/* Original catalog/PCM playback through Jo Engine 2026.3; R5 F00 provenance
 * and its bounded format exception are recorded in audio/r10_r5/manifest.json.
 * No Yaul backend, fixed WRAM banks, extra-RAM requirement or invented tones.
 */
#include "audio.h"
#include "catalog.h"
#ifdef AUDIO_HOST_TEST
#include "tests/jo_audio_mock.h"
#else
#include <jo/jo.h>
#endif
#include <string.h>

enum { FM_CHANNEL=0,OKI_FIRST_CHANNEL=1,VOICE_COUNT=5 };
enum { CACHE_NONE,CACHE_TOUR,CACHE_ENDING,CACHE_FRONTEND,CACHE_PANIC,CACHE_POSTGAME,CACHE_TUTORIAL,
       CACHE_SLOTS=4 };
enum { AUDIO_ERR_DIRECTORY=-201,AUDIO_ERR_LOAD=-202,AUDIO_ERR_RESIDENT=-203,
       AUDIO_ERR_ASSET=-204,AUDIO_ERR_MUSIC=-205,AUDIO_ERR_SAMPLE=-206,
       AUDIO_ERR_PREPARE=-207,AUDIO_ERR_PREPARE_STATE=-208,AUDIO_ERR_CACHE=-209 };
typedef struct {
    jo_sound *base;
    jo_sound playing;
    uint32_t samples,position,phase;
    uint8_t active,volume;
} audio_voice;
/* Unique symbol for read-only emulator save-state diagnostics. No code writes
 * a test state into the Saturn; runtime owns every field through this module. */
audio_status hk_audio_status __attribute__((used,externally_visible));
#define status hk_audio_status
static jo_sound effect_sounds[AUDIO_FX_COUNT],oki_sounds[AUDIO_OKI_COUNT];
static jo_audio_prepared_music prepared_music[AUDIO_MUSIC_COUNT];
static audio_voice voices[VOICE_COUNT];
static uint32_t last_vblank,fade_phase,priority_position,priority_phase;
static uint32_t last_underruns;
static uint8_t fade_control,fade_count;
static int pending_music=-1;
static uint8_t cache_kind;
static unsigned prepare_command=0xff,stream_command=0xff;
static unsigned music_level=10,effects_level=10;
static bool output_mono,output_muted;
static unsigned scaled_volume(unsigned volume,unsigned level)
{
 /* 10..1 represent 100%..10% amplitude, rounded to SCSP's 0.75 dB
  * per Jo step. Multiplying the register value would nearly mute level5. */
 static const uint8_t attenuation[11]={127,27,19,14,11,8,6,4,3,1,0};
 if(output_muted||!level||volume<=attenuation[level])return 0;
 return volume-attenuation[level];
}

static void fail(int fault)
{
    if (!status.fault)status.fault=fault;
}
static void stop_voice(unsigned channel)
{
    jo_audio_stop_channel((unsigned char)channel);
    voices[channel].active=0;
    if (channel==FM_CHANNEL)status.effect_priority=0;
}
static void start_voice(unsigned channel)
{
    audio_voice *v=&voices[channel];
    jo_audio_stop_channel((unsigned char)channel);
    if (!v->active || v->position>=v->samples || status.paused)return;
    unsigned sample_bytes=(v->base->mode&JoSoundMono8Bit)?1u:2u;
    uint32_t skipped=v->position*sample_bytes;
    v->playing=*v->base;
    if(v->playing.resident)v->playing.resident+=skipped;
    else v->playing.data+=skipped;
    v->playing.data_length=(v->samples-v->position+1)*sample_bytes;
    v->playing.volume=(uint8_t)scaled_volume(v->volume,effects_level);
    jo_audio_play_sound_on_channel(&v->playing,(unsigned char)channel);
}
static void new_voice(unsigned channel,jo_sound *sound,uint32_t samples,uint8_t volume)
{
    audio_voice *v=&voices[channel];
    /* Stop before upload. C13 has one exclusive FM voice; a replacement
     * effect cannot overwrite PCM still used by another FM effect. */
    stop_voice(channel);
    v->base=sound;v->samples=samples;v->position=v->phase=0;
    v->active=1;v->volume=volume;
    start_voice(channel);
}
static void set_fade_volume(void)
{
    /* Original pre-mixed C13 fade is approximately3dB per level. SCSP TL is
     * stepped in0.75dB for Jo's127-scale volume; no per-channel OPLL envelopes. */
    jo_audio_set_music_volume((unsigned char)scaled_volume(127-4*status.fade_level,music_level));
}
void audio_set_options(unsigned music,unsigned effects,bool mono,bool muted)
{
 if(music>10||effects>10)return;
 if(music_level==music&&effects_level==effects&&output_mono==mono&&output_muted==muted)return;
 music_level=music;effects_level=effects;output_mono=mono;output_muted=muted;
 jo_audio_set_music_mono(mono);set_fade_volume();
 for(unsigned i=0;i<VOICE_COUNT;i++)if(voices[i].active)
  jo_audio_set_channel_volume((unsigned char)i,(unsigned char)scaled_volume(voices[i].volume,effects));
}
static void observe_underruns(void)
{
    uint32_t current=jo_audio_get_music_underruns();
    status.underruns+=current>=last_underruns?current-last_underruns:current;
    last_underruns=current;
}
/* Keep one copy across command, preparation and service entry points. */
static __attribute__((noinline)) void observe_stream_fault(void)
{
    const jo_audio_music_stream_status *error=jo_audio_get_music_stream_status();
    if(!status.fault && error->result){
        stream_command=error->tag;
        fail(AUDIO_ERR_STREAM);
    }
}
static void refresh(void)
{
    uint32_t now=jo_core_get_vblank_count(),elapsed=now-last_vblank;
    unsigned hz=jo_core_get_refresh_rate();
    observe_stream_fault();
    last_vblank=now;status.last_vblank=now;
    if(!hz)hz=60;
    if (!status.ready || status.paused || !elapsed)return;
    /* Bounded chunks avoid multiplication overflow if caller was suspended. */
    while(elapsed){
        unsigned n=elapsed>60?60:elapsed;elapsed-=n;
        for(unsigned c=0;c<VOICE_COUNT;c++){
            audio_voice *v=&voices[c];
            if(!v->active)continue;
            v->phase+=n*v->base->sample_rate;
            v->position+=v->phase/hz;v->phase%=hz;
            if(v->position>=v->samples)stop_voice(c);
        }
        if(voices[FM_CHANNEL].active && status.effect_priority){
            priority_phase+=n*44100u;
            priority_position+=priority_phase/hz;priority_phase%=hz;
            const audio_catalog_command *cmd=&audio_commands[status.effect_command];
            for(unsigned i=0;i<AUDIO_FX_COUNT;i++)
                if(audio_effects[i].asset==cmd->asset && priority_position>=audio_effects[i].priority_samples)
                    status.effect_priority=0;
        }
        fade_phase+=n*2871u;
        while(fade_phase>=hz*50u){
            unsigned period=fade_control&127u;
            fade_phase-=hz*50u;
            if(period && ++fade_count==period){
                fade_count=0;
                if(fade_control&128){if(status.fade_level<15)++status.fade_level;}
                else if(status.fade_level)--status.fade_level;
                set_fade_volume();
            }
        }
    }
    status.oki_mask=0;
    for(unsigned c=1;c<VOICE_COUNT;c++)if(voices[c].active)status.oki_mask|=1u<<(c-1);
}
static void stop_music(void)
{
    observe_underruns();
    jo_audio_stop_music();observe_stream_fault();pending_music=-1;
    status.music_priority=0;status.music_command=0xff;
    status.fade_level=fade_control=fade_count=0;
}
static int start_music(unsigned index)
{
    uint32_t before=jo_core_get_vblank_count();
    bool ok;
    observe_underruns();
    ok=jo_audio_play_prepared_music_tagged(&prepared_music[index],status.music_command);
    last_underruns=jo_audio_get_music_underruns();
    uint32_t waited=jo_core_get_vblank_count()-before;
    if(waited>status.max_music_start_vblanks)status.max_music_start_vblanks=waited;
    if(!ok){observe_stream_fault();fail(AUDIO_ERR_MUSIC);return -1;}
    ++status.music_starts;pending_music=-1;
    set_fade_volume();
    return 0;
}
static int load(jo_sound *sound,const char *name,uint32_t samples,unsigned rate,
                jo_sound_mode mode,unsigned volume,bool resident)
{
    unsigned sample_bytes=(mode&JoSoundMono8Bit)?1u:2u;
    memset(sound,0,sizeof(*sound));
    if(!jo_audio_load_pcm(name,mode,sound))return AUDIO_ERR_LOAD;
    if(sound->data_length<((sample_bytes*(samples+1)+31)&~31u)){jo_audio_free_pcm(sound);return AUDIO_ERR_LOAD;}
    sound->data_length=sample_bytes*(samples+1);sound->sample_rate=(unsigned short)rate;
    sound->volume=(unsigned char)volume;sound->pan=0;sound->reverb=0;
    if(resident && !jo_audio_make_resident(sound)){
        jo_audio_free_pcm(sound);return AUDIO_ERR_RESIDENT;
    }
    if(resident)status.scsp_pcm_bytes+=(sound->data_length+31)&~31u;
    else status.wram_pcm_bytes+=(sound->data_length+31)&~31u;
    return 0;
}
static int music_index(unsigned command)
{
    const audio_catalog_command *c=audio_commands+command;
    if(c->kind!=1)return -1;
    for(unsigned i=0;i<AUDIO_MUSIC_COUNT;++i)if(audio_music[i].asset==c->asset)return (int)i;
    return -1;
}
static int prepare_music(unsigned index,unsigned command)
{
    uint32_t before=jo_core_get_vblank_count();
    prepare_command=command;
    ++status.music_prepare_calls;
    bool ok=jo_audio_prepare_music(audio_music[index].file,audio_music[index].loop_start,
                                   audio_music[index].loop_end,&prepared_music[index]);
    uint32_t elapsed=jo_core_get_vblank_count()-before;
    if(elapsed>status.max_music_prepare_vblanks)status.max_music_prepare_vblanks=elapsed;
    if(!ok){
        ++status.music_prepare_failures;
        /* A file read may have settled the preceding cue's deferred close. */
        jo_audio_service_music();
        observe_stream_fault();
        fail(AUDIO_ERR_PREPARE);return status.fault;
    }
    status.music_prefix_bytes+=prepared_music[index].prefix_size;
    if(status.music_prefix_bytes>status.music_prefix_peak_bytes)
        status.music_prefix_peak_bytes=status.music_prefix_bytes;
    return 0;
}
static void release_music(unsigned index)
{
    status.music_prefix_bytes-=prepared_music[index].prefix_size;
    jo_audio_free_prepared_music(&prepared_music[index]);
}
/* All modes share this bounded cache. Preserve overlapping prefixes, release
 * every obsolete one first, then load the missing set at the stopped boundary.
 * No allocation or file read is reachable from an ordinary audio command. */
static int prepare_cache(unsigned kind,const uint8_t *commands,unsigned count,
                         unsigned stage_index)
{
    int indexes[CACHE_SLOTS];
    unsigned matches=cache_kind==kind;
    if(count>CACHE_SLOTS){++status.invalid_preparations;fail(AUDIO_ERR_PREPARE_STATE);return status.fault;}
    for(unsigned n=0;n<count;++n){
        indexes[n]=music_index(commands[n]);
        if(indexes[n]<0){++status.asset_misses;fail(AUDIO_ERR_ASSET);return status.fault;}
        if(!prepared_music[indexes[n]].prefix)matches=0;
    }
    /* Same-world retries and repeated mode preparations remain harmless even
     * while paused or playing. A changed set requires the explicit STOP. */
    if(matches){status.cached_stage=stage_index;return 0;}
    if(status.music_command!=0xff || status.paused || pending_music>=0){
        ++status.invalid_preparations;fail(AUDIO_ERR_PREPARE_STATE);return status.fault;
    }
    cache_kind=CACHE_NONE;
    status.cached_stage=0xffffffffu;status.cached_music_command=0xff;
    for(unsigned i=0;i<AUDIO_MUSIC_COUNT;++i){
        unsigned retain=0;
        for(unsigned n=0;n<count;++n)if((int)i==indexes[n])retain=1;
        if(prepared_music[i].prefix&&!retain)release_music(i);
    }
    for(unsigned n=0;n<count;++n)
        if(!prepared_music[indexes[n]].prefix&&prepare_music((unsigned)indexes[n],commands[n]))return status.fault;
    cache_kind=(uint8_t)kind;status.cached_stage=stage_index;
    if(kind==CACHE_TOUR)status.cached_music_command=commands[count-1];
    return 0;
}
static int preparation_ready(void)
{
    observe_stream_fault();
    if(status.fault)return status.fault;
    if(!status.ready){++status.invalid_preparations;fail(AUDIO_ERR_PREPARE_STATE);}
    return status.fault;
}
static int prepare_frontend(void)
{
    static const uint8_t commands[]={48};
    return prepare_cache(CACHE_FRONTEND,commands,1,0xffffffffu);
}
int audio_prepare_frontend(void)
{
    if(preparation_ready())return status.fault;
    return prepare_frontend();
}
int audio_prepare_postgame(void)
{
    static const uint8_t commands[]={50,51,52};
    if(preparation_ready())return status.fault;
    return prepare_cache(CACHE_POSTGAME,commands,3,0xffffffffu);
}
int audio_prepare_tutorial(void)
{
    if(preparation_ready())return status.fault;
    return prepare_cache(CACHE_TUTORIAL,0,0,0xffffffffu);
}
int audio_prepare_panic(void)
{
    static const uint8_t commands[]={56,57,54};
    if(preparation_ready())return status.fault;
    return prepare_cache(CACHE_PANIC,commands,3,0xffffffffu);
}
int audio_prepare_ending(void)
{
    static const uint8_t commands[]={53,59,54,55};
    if(preparation_ready())return status.fault;
    return prepare_cache(CACHE_ENDING,commands,4,0xffffffffu);
}
static int prepare_stage(unsigned stage_index)
{
    if(stage_index>=AUDIO_STAGE_COUNT){
        ++status.invalid_preparations;fail(AUDIO_ERR_PREPARE_STATE);return status.fault;
    }
    uint8_t commands[CACHE_SLOTS];
    for(unsigned i=0;i<AUDIO_PERMANENT_MUSIC_COUNT;++i)commands[i]=audio_permanent_music[i];
    commands[AUDIO_PERMANENT_MUSIC_COUNT]=audio_stage_music[stage_index];
    return prepare_cache(CACHE_TOUR,commands,AUDIO_PERMANENT_MUSIC_COUNT+1,stage_index);
}
int audio_prepare_stage(unsigned stage_index)
{
    if(preparation_ready())return status.fault;
    return prepare_stage(stage_index);
}
static int initialize(unsigned frontend)
{
    if(status.ready)return status.fault;
    if(status.fault)return status.fault;
    if(!jo_fs_cd("AUDIO")){fail(AUDIO_ERR_DIRECTORY);return status.fault;}
    for(unsigned i=0;i<AUDIO_FX_COUNT;i++){
        /* Original command 0: 44.1 kHz S8, x4 encoding gain. Volume 103
         * matches the original gun/harpoon ratio at the nearest SCSP step.
         * See audio/r10_r5/manifest.json; resident byte count is unchanged. */
        int gun=audio_effects[i].asset==0;
        int error=load(&effect_sounds[i],audio_effects[i].file,audio_effects[i].samples,
            gun?44100:22050,gun?JoSoundMono8Bit:JoSoundMono16Bit,gun?103:127,audio_effects[i].resident!=0);
        if(error){fail(error);return error;}
        /* F13 remains available for an eliminated player's initials while
         * the partner plays. Lossless glyph storage funds its ordinary WRAM
         * allocation, so no effect is loaded from CD during gameplay. */
    }
    for(unsigned i=0;i<AUDIO_OKI_COUNT;i++){
        int error=load(&oki_sounds[i],audio_oki_assets[i].file,audio_oki_assets[i].samples,7576,JoSoundMono16Bit,127,true);
        if(error){fail(error);return error;}
    }
    status.music_command=status.effect_command=0xff;
    status.cached_stage=0xffffffffu;status.cached_music_command=0xff;
    if(frontend?prepare_frontend():prepare_stage(0))return status.fault;
    jo_audio_set_volume(127);jo_audio_set_music_volume(127);
    status.music_command=status.effect_command=0xff;
    last_vblank=jo_core_get_vblank_count();status.last_vblank=last_vblank;
    status.ready=1;
    return 0;
}
int audio_init(void){return initialize(0);}
int audio_init_frontend(void){return initialize(1);}
void audio_command(uint8_t command,uint8_t parameter)
{
    refresh();++status.requests;status.last_parameter=parameter;
    if(!status.ready || status.fault)return;
    const audio_catalog_command *c=&audio_commands[command];
    switch(c->kind){
    case 1:
        if(status.music_priority>c->priority){++status.ignored_priority;return;}
        for(unsigned i=0;i<AUDIO_MUSIC_COUNT;i++)if(audio_music[i].asset==c->asset){
            /* A missing prefix is a lifecycle error, never permission to read
             * CD synchronously in PLAY or to substitute a different cue. */
            if(!prepared_music[i].prefix){++status.cache_misses;fail(AUDIO_ERR_CACHE);return;}
            /* Close a replaced cue while its command still identifies it.
             * END can become terminal here after a previous WAIT. */
            if(status.music_command!=0xff){
                stop_music();
                if(status.fault)return;
            }
            status.music_priority=c->priority;status.music_command=command;
            status.fade_level=fade_control=fade_count=0;
            if(status.paused)pending_music=(int)i;
            else (void)start_music(i);
            return;
        }
        break;
    case 2:
        if(status.effect_priority>c->priority){++status.ignored_priority;return;}
        for(unsigned i=0;i<AUDIO_FX_COUNT;i++)if(audio_effects[i].asset==c->asset){
            if(!effect_sounds[i].data && !effect_sounds[i].resident){
                ++status.cache_misses;fail(AUDIO_ERR_CACHE);return;
            }
            new_voice(FM_CHANNEL,&effect_sounds[i],audio_effects[i].samples,effect_sounds[i].volume);
            status.effect_command=command;status.effect_priority=c->priority;
            priority_position=priority_phase=0;++status.accepted_effects;return;
        }
        break;
    case 3:stop_music();stop_voice(FM_CHANNEL);return;
    case 4:stop_voice(FM_CHANNEL);return;
    case 5:stop_music();return;
    case 6:
        fade_control=parameter;fade_count=0;
        status.fade_level=(parameter&128)?0:15;set_fade_volume();return;
    default:return;
    }
    ++status.asset_misses;fail(AUDIO_ERR_ASSET);
}
void audio_oki(uint8_t sample,uint8_t control)
{
    static const uint8_t volume[16]={127,123,119,115,111,108,103,100,95,0,0,0,0,0,0,0};
    refresh();++status.oki_requests;status.last_parameter=control;
    if(!status.ready || status.fault)return;
    if(sample>127){++status.asset_misses;fail(AUDIO_ERR_SAMPLE);return;}
    unsigned mask=control>>4;
    if(!mask)return;
    for(unsigned i=0;i<AUDIO_OKI_COUNT;i++)if(audio_oki_assets[i].sample==sample){
        for(unsigned v=0;v<4;v++)if(mask&(1u<<v))
            new_voice(OKI_FIRST_CHANNEL+v,&oki_sounds[i],audio_oki_assets[i].samples,volume[control&15]);
        status.oki_mask|=(uint8_t)mask;
        return;
    }
    ++status.asset_misses;fail(AUDIO_ERR_ASSET);
}
void audio_stop(void)
{
    refresh();++status.stops;
    stop_music();for(unsigned i=0;i<VOICE_COUNT;i++)stop_voice(i);
    status.paused=0;status.oki_mask=0;status.effect_command=0xff;
    priority_position=priority_phase=0;
    last_vblank=jo_core_get_vblank_count();
}
void audio_pause(bool paused)
{
    refresh();
    if(!status.ready || status.fault || status.paused==(unsigned)paused)return;
    status.paused=(uint8_t)paused;
    if(paused){
        ++status.pauses;jo_audio_pause_music();
        for(unsigned i=0;i<VOICE_COUNT;i++){
            jo_audio_stop_channel((unsigned char)i);
            /* WRAM-backed sounds use Jo's aligned DMA copier. Resume
             * at the preceding32-byte boundary (at most15 source samples)
             * so neither an unaligned source nor a padded read past EOF occurs. */
            if(voices[i].active && !voices[i].base->resident)voices[i].position&=~15u;
        }
    }else{
        ++status.resumes;
        if(pending_music>=0)(void)start_music((unsigned)pending_music);
        else jo_audio_resume_music();
        for(unsigned i=0;i<VOICE_COUNT;i++)start_voice(i);
    }
    last_vblank=jo_core_get_vblank_count();
}
void audio_service(unsigned elapsed_vblanks)
{
    (void)elapsed_vblanks;
    ++status.service_calls;refresh();observe_underruns();
    status.max_music_service_vblanks=jo_audio_get_music_max_update_vblanks();
}
const audio_status *audio_get_status(void)
{
    return &status;
}
unsigned audio_get_prepare_command(void)
{
    return prepare_command;
}
unsigned audio_get_stream_command(void)
{
    return stream_command;
}
