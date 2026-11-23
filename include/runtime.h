#ifndef HK320_RUNTIME_H
#define HK320_RUNTIME_H
#include "stage/stage.h"
enum {RT_FAULT_ASSET=1,RT_FAULT_SIMULATION=2,RT_FAULT_AUDIO=4,
      RT_FAULT_OBJECT=8,RT_FAULT_GLYPH=16,RT_FAULT_TEXTURE=32,RT_FAULT_VIDEO=64};
int rt_init_art(const stage_view *view);
void rt_draw_stage(const stage_view *view);
void rt_set_screen_offset(int x,int y);
/* Application pause overlay; no simulation/palette/scene ownership changes. */
void rt_draw_pause(void);
/* Frontend and gameplay revisions have independent counters. Their handoff
 * always needs a complete hidden repaint and matched VDP1 scene commit. */
void rt_invalidate_scene(void);
/* A new complete scene is waiting for its matching VDP1 displayed frame. */
int rt_scene_pending(void);
/* Last completed scene flip; false until the first queued scene is shown. */
int rt_scene_presented_vblank(unsigned *field);
void rt_report_fault(unsigned flags);
/* Preserve the first causal code, including a legitimate zero, with its sign. */
void rt_report_fault_code(unsigned flags,int code);
unsigned rt_fault_first_flags(void);
int rt_fault_code(void);
int rt_video_ready(void);
void rt_draw_fault(void);
unsigned rt_fault_flags(void);
unsigned rt_fault_detail(void);
#endif
