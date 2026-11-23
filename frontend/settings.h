#ifndef SPANG_SETTINGS_H
#define SPANG_SETTINGS_H
#include <stdint.h>
typedef struct {uint8_t difficulty,lives,demo_sounds,mono,music,effects;int8_t x,y;} spang_settings;
void spang_settings_reset(void);
const spang_settings *spang_settings_get(void);
unsigned spang_settings_revision(void);
int spang_settings_valid(const uint8_t data[8]);
int spang_settings_import(const uint8_t data[8]);
void spang_settings_export(uint8_t data[8]);
int spang_settings_adjust(unsigned row,int direction);
int spang_settings_move(int dx,int dy);
void spang_settings_center(void);
#endif
