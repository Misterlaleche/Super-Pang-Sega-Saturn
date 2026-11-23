#ifndef SPANG_MAP_PRESENTATION_H
#define SPANG_MAP_PRESENTATION_H
#include <stdint.h>

/* Read-only presentation counters for the existing inspector layout.
 * Every visible logical tick advances the original map actor. Scene upload
 * and flip ownership remain with main; this module inserts no dwell/skip
 * gate. Legacy hold/skip fields stay zero and do not control gameplay. */
typedef struct {
    uint16_t age,source_steps;
    uint8_t initial_hold,previous_fire,release_seen,skip_requested;
} spang_map_presentation;

void spang_map_presentation_begin(spang_map_presentation *presentation);
/* Always returns 1. The retained state/timer arguments preserve the small
 * monitor API; the original producer receives both players' current and
 * previous input directly from stage and owns all completion decisions. */
int spang_map_presentation_step(spang_map_presentation *presentation,
    unsigned fire_held,unsigned plane_state,unsigned plane_timer);
#endif
