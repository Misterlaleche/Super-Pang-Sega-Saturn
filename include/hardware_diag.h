#ifndef HK320_HARDWARE_DIAG_H
#define HK320_HARDWARE_DIAG_H
#include <stdint.h>

/* Optional physical-console monitor. Every field is a naturally aligned word;
 * this block is for read-only inspection, not a control interface. */
typedef struct {
    uint32_t irq_started, irq_completed, main_started, main_completed, vblank;
    uint32_t video_ready, enabled, edsr, copr, lopr;
    uint32_t cd_commands, cd_waits, cd_rejects, cd_timeouts;
    uint32_t cd_phase, cd_command, cd_status, cd_wait_mask, cd_wait_age;
    uint32_t cd_max_wait_vblanks;
    uint32_t error_latched, error_command, error_status, error_phase;
    uint32_t error_mask, error_hirq, error_vblank;
    uint32_t repaint_generation, words_last, words_max;
} rt_hw_diag_status;

extern volatile rt_hw_diag_status rt_hw_diag_state;

#define RT_FAULT_REPORT_VERSION 2
#define RT_FAULT_REPORT_ROWS 12u
#define RT_FAULT_REPORT_COLS 24u
typedef struct {
    uint32_t flags,first_flags;
    int32_t code;
    uint32_t in_game,mode,stage,scene,phase,tick,vdp_free;
} rt_fault_report;
extern volatile rt_fault_report rt_hw_diag_fault_state;
/* Main-only, first call wins. IRQ paints without allocating or reading assets. */
void rt_hw_diag_latch_fault(const rt_fault_report *report);

/* Call once before the core begins dispatching these hooks. Starts OFF. */
void rt_hw_diag_init(void);
/* Main only: call after rt_init_art has reserved both NBG1 bitmaps. */
void rt_hw_diag_enable_video(void);
/* IRQ: after gun latch handling, before __jo_vdp1_vblank; samples only. */
void rt_hw_diag_vblank_begin(void);
/* IRQ: after __jo_vdp2_vblank, before the audio decoder. Reads the last
 * completed controller report; Z is edge-detected independently of main. */
void rt_hw_diag_vblank_draw(void);
/* IRQ: after audio/callbacks/SMPC, before restoring the SCU mask. */
void rt_hw_diag_vblank_end(void);
/* Main only: bracket the main work whose completion is to be observed. */
void rt_hw_diag_frame_begin(void);
void rt_hw_diag_frame_end(void);
/* Main only: consumes an OFF request. If true, call rt_invalidate_scene().
 * IRQ never restores gameplay pixels or waits for this acknowledgement. */
int rt_hw_diag_repaint_needed(void);

#endif
