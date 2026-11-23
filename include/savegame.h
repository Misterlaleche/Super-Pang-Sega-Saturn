#ifndef SPANG_SAVEGAME_H
#define SPANG_SAVEGAME_H
#include <stdint.h>
typedef struct {uint32_t loaded,writes,generation,slot,attempted_revision;int32_t error;} savegame_diagnostics;
extern volatile savegame_diagnostics savegame_status;
void savegame_load(void);
/* Called only at a stopped-audio frontend transition. Errors are non-fatal. */
void savegame_flush(void);
#endif
