#ifndef HK320_AUDIO_H
#define HK320_AUDIO_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t fault;
    uint32_t requests,oki_requests,stops,accepted_effects,ignored_priority;
    uint32_t music_starts,pauses,resumes,asset_misses,underruns;
    uint32_t scsp_pcm_bytes,wram_pcm_bytes,max_music_start_vblanks;
    uint32_t service_calls,last_vblank;
    uint8_t ready,paused,music_command,effect_command;
    uint8_t music_priority,effect_priority,oki_mask,last_parameter;
    uint8_t fade_level,reserved[3];
    uint32_t music_prefix_bytes,max_music_service_vblanks;
    /* R07 tail only: preserve the preceding84-byte diagnostic ABI. */
    uint32_t music_prepare_calls,music_prepare_failures,invalid_preparations,cache_misses;
    uint32_t music_prefix_peak_bytes,cached_stage,cached_music_command,max_music_prepare_vblanks;
} audio_status;

/* Call after jo_core_init, registering LowRAM and completing graphics loading.
 * Current Jo FS directory MUST be CD root. On success it remains AUDIO: Jo's
 * stock file API uses the current directory; gameplay must not change it.
 * 0 succeeds, negative is a latched diagnostic. Safe to call again after success.
 * Required make flags: JO_COMPILE_WITH_AUDIO_MODULE=1, FS_MODULE=1.
 * Compile audio/audio.c only; audio/vendor and test C files are host tools.
 */
int audio_init(void);
/* Levels 0..10; defaults 10/10. Effects are mono PCM. The music mixer can
 * fold its two channels to both speakers without changing decoded samples. */
void audio_set_options(unsigned music,unsigned effects,bool mono,bool muted);
/* Boot into the original frontend with only selector music48 prepared.
 * Both initializers load the same resident effects/OKI once. Repeating either
 * initializer after success preserves the current cache and playback. */
int audio_init_frontend(void);
/* Loading-boundary APIs. Drain STOP before changing mode: music must be
 * stopped, unpaused and have no deferred command. Repeating the current mode
 * is read-free, including while its music is playing or paused.
 * Frontend keeps48; Panic keeps56/57/54 for read-free speed and death cues.
 * Obsolete prefixes are released before loading replacements. Every mode
 * shares one cache of at most four64KiB prefixes (262144 bytes). */
int audio_prepare_frontend(void);
/* Stopped whole-game continue/name-entry boundary: original50/51/52 and the
 * name confirmation effect13. The temporary40288-byte effect is released
 * before preparing any gameplay cache; starts themselves never read CD. */
int audio_prepare_postgame(void);
/* Original demo scenes, including tutorials40/41, issue effects but no BGM. */
int audio_prepare_tutorial(void);
int audio_prepare_panic(void);
/* Prepare the original BGM for stage0..39 at a new READY scene boundary.
 * Drain that boundary's STOP event first. A cached world needs no file I/O,
 * including same-world advances/retries. A changed world requires stopped,
 * unpaused music with no pending command; misuse latches error-208.
 * Init prepares stage0. Warning/clear/tally prefixes stay resident in stages.
 * All preparation functions are loading-boundary APIs. Never call them from
 * PLAY or an audio command dispatch. Every cached start remains nonblocking.
 */
int audio_prepare_stage(unsigned stage_index);
/* Prepare original final cues54/55 at the stopped ending-entry boundary.
 * Ending retains53/59 with54/55; stage preparation restores49 and the world.
 * Requests during ending and restart remain read-free; peak stays262144. */
int audio_prepare_ending(void);
/* Preserve the stage FIFO order; no coalescing. Catalog request priorities,
 * reset commands and fade parameters follow original C13 semantics.
 */
void audio_command(uint8_t command,uint8_t parameter);
void audio_oki(uint8_t sample,uint8_t control);
void audio_stop(void);
void audio_pause(bool paused);
/* Once each presentation iteration. Jo itself services streaming/CD refill;
 * this updates the catalog's priorities/fades and exports hardware underruns.
 * Actual Jo VBlank count drives sample lifetime, so call order within a frame
 * cannot age a newly dispatched effect by the caller's elapsed-frame count.
 */
void audio_service(unsigned elapsed_vblanks);
const audio_status *audio_get_status(void);
/* Target of the most recent loading preparation; pair with Jo's detailed
 * preparation status when reporting -207. Does not alter audio_status ABI. */
unsigned audio_get_prepare_command(void);
/* First failed streaming cue, retained after STOP. */
#define AUDIO_ERR_STREAM (-210)
unsigned audio_get_stream_command(void);
#endif
