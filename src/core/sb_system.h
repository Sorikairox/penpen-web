/* Pen Pen port, S18 b01: the SDK sample's start-up and exit as linked into the game (core/sb_system.c). */
#ifndef PENPEN_SB_SYSTEM_H
#define PENPEN_SB_SYSTEM_H
#include "hw.h"

/* was func_8C010184: sbInitSystem(mode, frame, count) */
void sb_init_system(int32_t mode, int32_t frame, int32_t count);
/* was func_8C01022C: sbExitSystem() */
void sb_exit_system(void);

#endif
