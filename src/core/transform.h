/* Pen Pen port, S14 (b05): core/transform.c's matrix helpers (exported there; declared for their callers). */
#ifndef PENPEN_TRANSFORM_H
#define PENPEN_TRANSFORM_H
#include "hw.h"

/* was func_8C010594: turn matrix m so that its up axis points along dir */
void matrix_align_up(gaddr m, gaddr dir);
/* was func_8C010610: scale matrix m by (sx, sy, sz) in the frame whose up axis is dir */
void matrix_scale_along(gaddr m, gaddr dir, f32 sx, f32 sy, f32 sz);

#endif
