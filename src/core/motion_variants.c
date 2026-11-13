/* Pen Pen port, S17 (b01): the other motion drawers of core/motion.c: the same frame keeping (motion_keep_frame:
 * clamped, or wrapped with `loop`), then one of Ninja's motion drawers. Each returns 1 when it moved the frame. */
#include "hw.h"
#include "sdk.h"
#include "../core/motion_frame.h"

/* Ninja's motion drawers (SDK, S2 names): the object and motion (and the shape motion), the frame in FR4 */
static void nj_MotionB3(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0A8DA8u, object, motion); }
static void nj_MotionB1(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0A77A0u, object, motion); }
static void nj_MotionB2(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0A82FCu, object, motion); }
static void nj_MotionL4(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0A7224u, object, motion); }
static void nj_MotionL3(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0CD0A6u, object, motion); }
static void nj_MotionL2(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0CB7FCu, object, motion); }
static void nj_ShapeMotionB3(gaddr object, gaddr motion, gaddr shape, f32 frame) {
  FARGS1(frame);
  CALL3(0x8C0A8EB6u, object, motion, shape);
}
static void nj_ShapeMotionB1(gaddr object, gaddr motion, gaddr shape, f32 frame) {
  FARGS1(frame);
  CALL3(0x8C0A78AEu, object, motion, shape);
}
static void nj_ShapeMotionB2(gaddr object, gaddr motion, gaddr shape, f32 frame) {
  FARGS1(frame);
  CALL3(0x8C0A83F2u, object, motion, shape);
}
static void nj_ShapeMotionL4(gaddr object, gaddr motion, gaddr shape, f32 frame) {
  FARGS1(frame);
  CALL3(0x8C0A72F0u, object, motion, shape);
}
static void nj_ShapeMotionL3b(gaddr object, gaddr motion, gaddr shape, f32 frame) {
  FARGS1(frame);
  CALL3(0x8C0CDAA8u, object, motion, shape);
}
static void nj_ShapeMotionL2(gaddr object, gaddr motion, gaddr shape, f32 frame) {
  FARGS1(frame);
  CALL3(0x8C0CB956u, object, motion, shape);
}

/* was func_8C010F1C: the frame kept in range, then drawn with nj_MotionB3 */
int32_t motion_draw_b3(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionB3(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c010f1c, RET(motion_draw_b3(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C010FE0: the frame kept in range, then drawn with nj_MotionB1 */
int32_t motion_draw_b1(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionB1(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c010fe0, RET(motion_draw_b1(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C011086: the frame kept in range, then drawn with nj_MotionB2 */
int32_t motion_draw_b2(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionB2(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c011086, RET(motion_draw_b2(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C01112C: the frame kept in range, then drawn with nj_MotionL4 */
int32_t motion_draw_l4(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionL4(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c01112c, RET(motion_draw_l4(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C01128A: the frame kept in range, then drawn with nj_MotionL3 */
int32_t motion_draw_l3(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionL3(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c01128a, RET(motion_draw_l3(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C0113E8: the frame kept in range, then drawn with nj_MotionL2 */
int32_t motion_draw_l2(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_MotionL2(object, motion, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c0113e8, RET(motion_draw_l2(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C01148E: the same for a shape motion, with nj_ShapeMotionB3 (the loop flag is the 5th argument) */
int32_t shape_motion_draw_b3(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionB3(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c01148e, RET(shape_motion_draw_b3(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))

/* was func_8C011536: the same for a shape motion, with nj_ShapeMotionB1 (the loop flag is the 5th argument) */
int32_t shape_motion_draw_b1(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionB1(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c011536, RET(shape_motion_draw_b1(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))

/* was func_8C0115F0: the same for a shape motion, with nj_ShapeMotionB2 (the loop flag is the 5th argument) */
int32_t shape_motion_draw_b2(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionB2(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c0115f0, RET(shape_motion_draw_b2(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))

/* was func_8C011698: the same for a shape motion, with nj_ShapeMotionL4 (the loop flag is the 5th argument) */
int32_t shape_motion_draw_l4(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionL4(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c011698, RET(shape_motion_draw_l4(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))

/* was func_8C011740: the same for a shape motion, with nj_ShapeMotionL3b (the loop flag is the 5th argument) */
int32_t shape_motion_draw_l3b(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionL3b(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c011740, RET(shape_motion_draw_l3b(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))

/* was func_8C011948: the same for a shape motion, with nj_ShapeMotionL2 (the loop flag is the 5th argument) */
int32_t shape_motion_draw_l2(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  int32_t moved = motion_keep_frame(motion, frame, loop);
  nj_ShapeMotionL2(object, motion, shape, F32_AT(frame));
  return moved;
}
HW_EXPORT(8c011948, RET(shape_motion_draw_l2(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4)))
