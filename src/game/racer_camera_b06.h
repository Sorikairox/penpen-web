/* Pen Pen port, S18 b06: game/racer_camera_b06.c's camera modes, declared for game/racer_camera_update.c (they
 * belong beside racer.h's racer_camera_intro / _follow / _high). */
#ifndef PENPEN_RACER_CAMERA_B06_H
#define PENPEN_RACER_CAMERA_B06_H
#include "hw.h"

/* was func_8C03C99C: the camera of racer state 1 (over the finish) */
void racer_camera_finish(gaddr racer);
/* was func_8C03D2C4: the camera of racer state 5 (swimming) */
void racer_camera_swim(gaddr racer);

#endif
