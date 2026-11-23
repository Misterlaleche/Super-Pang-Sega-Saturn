#ifndef SPANG_BOOT_DIAG_H
#define SPANG_BOOT_DIAG_H
/* Internal startup trace; loading message is visible before assets/audio exist. */
void boot_diag_mark(unsigned phase);
void boot_diag_finish(void);
#endif
