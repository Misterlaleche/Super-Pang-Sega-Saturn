#ifndef SPANG_STACK_USAGE_H
#define SPANG_STACK_USAGE_H
#include <stdint.h>
typedef struct {uint32_t painted_end,minimum_free,maximum_used,samples;} stack_usage_diagnostics;
extern volatile stack_usage_diagnostics stack_usage_status;
void stack_usage_init(void);
void stack_usage_sample(void);
#endif
