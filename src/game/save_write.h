/* Pen Pen port, S18 b01: game/save_write.c */
#ifndef PENPEN_SAVE_WRITE_H
#define PENPEN_SAVE_WRITE_H
#include "hw.h"

/* was func_8C018F7C: writes the save file to the memory card; 1 = done (or a message dismissed with Start) */
int32_t save_write(void);

#endif
