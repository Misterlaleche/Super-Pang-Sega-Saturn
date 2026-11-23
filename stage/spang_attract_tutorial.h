#ifndef SPANG_ATTRACT_TUTORIAL_H
#define SPANG_ATTRACT_TUTORIAL_H
#include <stdint.h>
typedef struct {
    uint8_t actor[40]; /* Original FA00 actor bytes. */
    uint8_t which,paused,done,error;
} spang_attract_tutorial;
/* Native BANK3:8F08..92D6. which0 is the P1 ball tutorial;1 the P2 items
 * tutorial. Only the first freezes gameplay while instruction boxes are open.
 * The caller invokes this BEFORE gameplay/script input each logical tick.
 * command receives original queue words14xx (116D variant11E8, bank10);
 * zero means none. Geometry writes use original512x256 tile coordinates.
 * No elapsed host time, frame data or CPU interpretation is involved. */
int spang_attract_tutorial_init(spang_attract_tutorial *,unsigned which);
int spang_attract_tutorial_tick(spang_attract_tutorial *,const uint8_t bank3[16384],
                               uint8_t attrs[2048],uint8_t chars[4096],uint16_t *command);
#endif
