#include "include/stack_usage.h"
/* The linker reserves 060FBC00..060FFC00 for the master SH-2 stack.
 * Paint only unused memory below the live SP, leaving a 256-byte guard.
 * This measures a conservative watermark after app entry, not a proof of
 * worst-case usage or a measurement of the earlier BIOS/core bootstrap. */
enum {STACK_BOTTOM=0x060fbc00,STACK_TOP=0x060ffc00};
volatile stack_usage_diagnostics stack_usage_status __attribute__((used,externally_visible));
static unsigned frame_count;
void stack_usage_init(void)
{
 uint32_t sr,sp;
 __asm__ volatile("stc sr,%0\n\tmov r15,%1":"=r"(sr),"=r"(sp));
 if(sp<STACK_BOTTOM+256||sp>STACK_TOP)return;
 uint32_t masked=sr|0xf0u;
 __asm__ volatile("ldc %0,sr"::"r"(masked):"memory");
 uint32_t end=(sp-256)&~3u;
 for(volatile uint32_t *p=(volatile uint32_t *)STACK_BOTTOM;(uintptr_t)p<end;p++)*p=0x51ac7e39;
 stack_usage_status.painted_end=end;stack_usage_status.minimum_free=end-STACK_BOTTOM;
 stack_usage_status.maximum_used=STACK_TOP-end;stack_usage_status.samples=1;
 __asm__ volatile("ldc %0,sr"::"r"(sr):"memory");
}
void stack_usage_sample(void)
{
 if(++frame_count%60||!stack_usage_status.painted_end)return;
 volatile uint32_t *p=(volatile uint32_t *)STACK_BOTTOM;
 while((uintptr_t)p<stack_usage_status.painted_end&&*p==0x51ac7e39)p++;
 uint32_t free=(uint32_t)(uintptr_t)p-STACK_BOTTOM;
 if(free<stack_usage_status.minimum_free)stack_usage_status.minimum_free=free;
 stack_usage_status.maximum_used=STACK_TOP-STACK_BOTTOM-stack_usage_status.minimum_free;
 ++stack_usage_status.samples;
}
