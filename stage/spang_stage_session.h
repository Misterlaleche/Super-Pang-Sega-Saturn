#ifndef SPANG_STAGE_SESSION_H
#define SPANG_STAGE_SESSION_H
#include <stdint.h>
/* BANK5:89DE. Reset only the original stage-local bytes; do not clear the
 * Lucky/weapon-use flags E0C7/C8/C9/E0E4 or the session count E0CC here. */
int spang_stage_session_reset(uint8_t globals[128]);
/* BANK5:896E..89DB, including 89DE, 13BF and FIXED2F06. Tour only.
 * E780 first slot becomes the original delayed music owner A263. The caller
 * must have a context-capable score/music scheduler before enabling this.
 * No writes on invalid stage, rank or table; seed consumes precisely the
 * entry's conditional13BF call. This is not the whole80C9/bootstrap/video FSM. */
int spang_stage_session_begin(uint8_t globals[128],uint8_t scores[128],
 uint16_t *seed,const uint8_t bank5[16384],const uint8_t fixed[32768]);
#endif
