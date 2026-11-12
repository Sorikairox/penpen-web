/* Pen Pen port, S13: drawing a model in a motion at a frame kept in range (core/motion.c).
 *
 * Each of the four keeps the caller's frame number (a float it holds) inside the motion, then draws through one of
 * Ninja's motion drawers. Without `loop` the frame is clamped to [0, frames - 1]; with it, it wraps: from `frames`
 * or more back to 0, and below 0 to `frames`. They return 1 when they moved the frame. */
#include "hw.h"
#include "sdk.h"
#include "../core/file.h"

/* Ninja's motion drawers (SDK, S2 names): the object and motion, the frame in FR4 */
static void nj_MotionL3b(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0CD94Eu, object, motion); }
static void nj_MotionL1(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0CC84Eu, object, motion); }
static void nj_ShapeMotionL3(gaddr object, gaddr motion, gaddr shape, f32 frame) {
  FARGS1(frame);
  CALL3(0x8C0CD1F4u, object, motion, shape);
}

/* the frame count as a float, as SHC converts it (unsigned) */
static f32 frame_count(int32_t n) { return f32_from_uint((uint32_t)n); }

/* keeps *frame in the motion (see above); 1 when it changed it */
int32_t motion_keep_frame(gaddr motion, gaddr frame, int32_t loop) {
  int32_t frames = ((const NjMotion *)hw_ptr(motion))->nb_frames;
  f32 *f = &F32_AT(frame);
  if (loop == 0) {
    if (fgt(*f, frame_count(frames - 1))) {
      *f = frame_count(frames - 1);
      return 1;
    }
    if (fgt(F32(0.0f), *f)) {
      *f = F32(0.0f);
      return 1;
    }
  } else {
    if (!fgt(frame_count(frames), *f)) {
      *f = F32(0.0f);
      return 1;
    }
    if (fgt(F32(0.0f), *f)) {
      *f = frame_count(frames);
      return 1;
    }
  }
  return 0;
}

/* was func_8C0111E4: the frame kept in range, then drawn with nj_MotionL3b */
int32_t motion_draw_l3b(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionL3b(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c0111e4, RET(motion_draw_l3b(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C011330: the same with nj_MotionL1 */
int32_t motion_draw_l1(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionL1(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c011330, RET(motion_draw_l1(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C0117F8: the same for a shape motion, with nj_ShapeMotionL3 (the loop flag is the 5th argument) */
int32_t shape_motion_draw_l3(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionL3(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c0117f8, RET(shape_motion_draw_l3(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))

/* was func_8C0118A0: the same with nj_ShapeMotionL1 */
int32_t shape_motion_draw_l1(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionL1(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c0118a0, RET(shape_motion_draw_l1(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))
