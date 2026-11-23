#ifndef SPANG_PANIC_H
#define SPANG_PANIC_H
#include <stdint.h>

enum { SPANG_PANIC_FM=1,SPANG_PANIC_QUEUE=2,SPANG_PANIC_PALETTE=3 };
/* Original E1A0 scheduler; all globals use E080..E0FF indices. Events are
 * ordered triples(kind,command,parameter), consumed after the current tick.
 * FM stores command in byte1. QUEUE stores original D/E. PALETTE stores
 * source bank/request; A383's (13,20) request uses E188=2. */
typedef struct spang_panic {
 uint8_t scheduler[32],difficulty,events[48],event_count;
} spang_panic;
void spang_panic_events_reset(spang_panic *s);
int spang_panic_init(spang_panic *s,uint8_t g[128],const uint8_t fixed[32768],unsigned difficulty);
int spang_panic_restart(spang_panic *s,uint8_t g[128],const uint8_t fixed[32768]);
int spang_panic_level(uint8_t g[128],const uint8_t fixed[32768],unsigned difficulty);
/* Progress is per normal popped balloon, including bomb victims. Reaching
 * level100 does not stop the scheduler until the following progress call;
 * callers must inspect E0D6/g56, not infer completion from the return value. */
int spang_panic_progress(spang_panic *s,uint8_t g[128],const uint8_t fixed[32768]);
int spang_panic_schedule(spang_panic *s,uint8_t g[128],uint8_t balls[1024],
 const uint8_t p1[128],const uint8_t p2[128],uint8_t input1,uint8_t input2,
 const uint8_t bank2[16384],uint16_t *seed);
int spang_panic_services(spang_panic *s,uint8_t g[128],const uint8_t p1[128],
 const uint8_t p2[128],const uint8_t fixed[32768]);
int spang_panic_death_reset(uint8_t g[128],const uint8_t fixed[32768]);
/* Original primitives exposed to the owner and independent ROM probes. */
int spang_panic_spawn(spang_panic *s,uint8_t g[128],uint8_t balls[1024],
 const uint8_t bank2[16384],unsigned selector,uint16_t *seed);
int spang_panic_enter(uint8_t ball[32],const uint8_t bank1[16384],uint8_t delay);
int spang_panic_fall(uint8_t ball[32],uint8_t delay,uint8_t blocked);
int spang_panic_hit(uint8_t weapon[32],uint8_t ball[32],uint8_t p1[128],
 uint8_t p2[128],uint8_t g[128],int bullet);
#endif
