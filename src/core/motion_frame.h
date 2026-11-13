/* Pen Pen port, S17 (b01): core/motion.c's frame keeping and the motion drawers (core/motion.c,
 * core/motion_variants.c). */
#ifndef PENPEN_MOTION_FRAME_H
#define PENPEN_MOTION_FRAME_H
#include "hw.h"

/* keeps the float at `frame` inside the motion (clamped, or wrapped with `loop`); 1 when it changed it */
int32_t motion_keep_frame(gaddr motion, gaddr frame, int32_t loop);
int32_t motion_draw_l3b(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t motion_draw_l1(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t shape_motion_draw_l3(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);
int32_t shape_motion_draw_l1(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);
int32_t motion_draw_b3(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t motion_draw_b1(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t motion_draw_b2(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t motion_draw_l4(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t motion_draw_l3(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t motion_draw_l2(gaddr object, gaddr motion, gaddr frame, int32_t loop);
int32_t shape_motion_draw_b3(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);
int32_t shape_motion_draw_b1(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);
int32_t shape_motion_draw_b2(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);
int32_t shape_motion_draw_l4(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);
int32_t shape_motion_draw_l3b(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);
int32_t shape_motion_draw_l2(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop);

#endif
