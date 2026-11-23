#ifndef R04_FAUNA_TRANSIENTS_H
#define R04_FAUNA_TRANSIENTS_H
#include <stdint.h>
/* Distinct original E680 impacts (4 records, BANK3) and E700 bubbles
 * (8 records, BANK2); never alias the existing F800 ball-burst pool.
 * kind0=impact, kind1=bubble. Advance0 is the original display-only route.
 * Return1 gives the snapshot at20FB; init/retirement return0, errors<0. */
int fauna_transient_step(uint8_t actor[16],const uint8_t bank[16384],
 unsigned kind,int advance,int stopped,uint8_t draw[16]);
int fauna_transient_sprite(const uint8_t draw[16],const uint8_t bank[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
#endif
