#ifndef SPANG_FRAME_CLOCK_H
#define SPANG_FRAME_CLOCK_H
/* Original57.42/60 accumulator, charged by elapsed presentation VBlanks.
 * Bound catch-up work, retaining all fractional/deferred ticks. Paused callers
 * do not charge time. Scene transitions clear phase at their existing boundary. */
static inline unsigned spang_frame_clock_due(unsigned *phase,unsigned elapsed){
 *phase+=elapsed*2871u;
 unsigned due=*phase/3000u;if(due>8)due=8;
 *phase-=due*3000u;return due;
}
#endif
