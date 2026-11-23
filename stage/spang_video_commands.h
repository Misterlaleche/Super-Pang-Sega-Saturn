#ifndef SPANG_VIDEO_COMMANDS_H
#define SPANG_VIDEO_COMMANDS_H
#include <stdint.h>
typedef void (*spang_video_write)(void *context, uint16_t address, uint8_t value);
/* Translate 116D/11E8 (text tile banks 0/2), 11A6 (tile words), and
 * 1496 (2x2 glyphs), after selecting bank 10.
 * bank10 contains 16384 original decoded DATA bytes. parameter is E144.
 * Writes are ordered arcade bus addresses; the caller owns bank routing and
 * rendering. INC L/INC C wrap within their 256-byte pages, as on the ROM.
 * Return 0 on original stream terminator; -1 on unsupported handler, source
 * outside bank 10, or destination outside C800..DFFF. On malformed streams
 * earlier callbacks may have happened; caller must discard that command's
 * output. No CPU register/flag, bank-switch or IRQ side effects are modeled.
 * ROM identifiers select native routines, never interpreted product code. */
int spang_video_command(uint16_t handler, uint8_t parameter,
                        const uint8_t bank10[16384],
                        spang_video_write write, void *context);
/* Same native tile/text writers with an already-selected original DATA
 * stream. Used by BANK0 per-player WAIT/CONTINUE, without a table lookup. */
int spang_video_stream(uint16_t handler,uint16_t source,const uint8_t bank[16384],
                      spang_video_write write,void *context);
#endif
