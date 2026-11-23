#include "spang_map_presentation.h"

void spang_map_presentation_begin(spang_map_presentation *p)
{
    p->age=p->source_steps=0;
    p->initial_hold=p->previous_fire=p->release_seen=p->skip_requested=0;
}

int spang_map_presentation_step(spang_map_presentation *p,
    unsigned fire_held,unsigned plane_state,unsigned plane_timer)
{
    (void)plane_state;(void)plane_timer;
    /* One visible logical tick is one original actor call. Completion and
     * FIRE edges belong to BANK3, including its distinct 1P/2P tail. */
    if(p->age<65535u)++p->age;
    if(p->source_steps<65535u)++p->source_steps;
    p->previous_fire=(uint8_t)(fire_held!=0);
    if(!fire_held)p->release_seen=1;
    return 1;
}
