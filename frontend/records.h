#ifndef R10_FRONTEND_RECORDS_H
#define R10_FRONTEND_RECORDS_H
#include <stdint.h>
/* BANK0:96BA..9A74. Records retain the original16-byte RAM schema. */
typedef struct {
 uint8_t phase,seconds,prescale,selected,count,repeat_left,repeat_right;
 uint8_t blink_timer,blink_phase,mode,world,stage;
 uint16_t name[3];
} spang_record_entry;
typedef struct {
 uint8_t rows[2][80],total[2][16];
 spang_record_entry entry[2];
 uint32_t revision;
} spang_records;
enum {SPANG_RECORD_CHANGED=1,SPANG_RECORD_SOUND=2,SPANG_RECORD_FINISHED=4};
void spang_records_init(spang_records *,const uint8_t bank0[16384]);
/* owner0/1. Equal scores qualify ahead, as at BANK0:96F4. Returns1 if the
 * original name editor is entered,0 if below the five-row table. */
int spang_records_submit(spang_records *,unsigned mode,const uint8_t player[128],
                         unsigned owner,const uint8_t fixed[32768]);
/* Raw original input bits left20/right10/fire08. One call per arcade tick. */
unsigned spang_records_tick(spang_records *,unsigned owner,uint8_t held,
                            uint8_t pressed,const uint8_t bank0[16384]);
uint16_t spang_records_glyph(const uint8_t bank0[16384],unsigned index);
#endif
