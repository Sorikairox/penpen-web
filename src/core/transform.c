/* Pen Pen port, S13: matrix helpers that turn a model toward a direction (core/transform.c).
 *
 * Both take a direction, normalise a copy of it and find the axis that turns the model's up vector (0, 1, 0) onto
 * it (up x dir, normalised: its length before is the sine of the angle). When the two are not already parallel
 * (sine above 1e-4), the angle comes from the C runtime's arcsine and turns into Ninja angle units (0x10000 a
 * turn). The vectors live on the guest stack: Ninja's vector functions take their addresses. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

/* SDK calls (Ninja) and the C runtime's arcsine (float in FR4, result in FR0; it works in doubles: guest code) */
static void njOuterProduct(gaddr a, gaddr b, gaddr out) { CALL3(0x8C0C3D04u, a, b, out); }
static void njRotate(gaddr m, gaddr axis, int32_t angle) { CALL3(0x8C0C4388u, m, axis, angle); }
static f32 rt_asinf(f32 x) { FARGS1(x); CALL0(0x8C062898u); return hw_fret(); } /* func_8C062898 */

#define PARALLEL_SINE   0x38D1B717u /* 1e-4f: below it the direction counts as straight up */
#define ANGLE_UNITS     F32(65536.0f)
#define TWO_PI          0x40C90FD7u /* 6.2831855f */

/* the scratch vectors, in the order of the original's frame */
typedef struct AlignScratch {
  Vec3 axis; /* up x dir, normalised */
  Vec3 dir;  /* the direction, normalised */
  Vec3 up;   /* (0, 1, 0) */
} AlignScratch;
HW_SIZE(AlignScratch, 36);

/* the axis and sine of the turn from `up` (the model's y axis, or its z axis) to dir, in s (guest stack) */
static f32 align_axis(gaddr s, gaddr dir, const Vec3 *up) {
  AlignScratch *p = (AlignScratch *)hw_ptr(s);
  p->up.z = up->z;
  p->up.y = up->y;
  p->up.x = up->x;
  memmove(&p->dir, hw_ptr(dir), sizeof(Vec3));
  njUnitVector(s + 12);
  njOuterProduct(s + 24, s + 12, s);
  return njUnitVector(s);
}

/* the turn's angle in Ninja units (arcsine of the sine, scaled) */
static int32_t align_angle(f32 sine) {
  return f32_to_int(fdiv(fmul(rt_asinf(sine), ANGLE_UNITS), F32_BITS(TWO_PI)));
}

/* was func_8C010594: turns matrix m so that its up axis points along dir */
void matrix_align_up(gaddr m, gaddr dir) {
  const Vec3 axis_y = {F32(0.0f), F32(1.0f), F32(0.0f)};
  gaddr s = hw_stack_alloc(sizeof(AlignScratch));
  f32 sine = align_axis(s, dir, &axis_y);
  if (fgt(sine, F32_BITS(PARALLEL_SINE)))
    njRotate(m, s, align_angle(sine));
  hw_stack_free(sizeof(AlignScratch));
}
HW_EXPORT(8c010594, matrix_align_up(ARG0, ARG1))

/* was func_8C010610: scales matrix m by (sx, sy, sz) in the frame whose z axis is dir (turn to that frame, scale,
 * turn back), keeping m's translation out of the turns. Its frame holds the saved translation at 8 and the scratch
 * vectors at 0x14, 0x54 bytes deep with its saved registers. (S13 turned from the y axis, as matrix_align_up does:
 * this one turns from the z axis; that was its difftest failure) */
typedef struct ScaleAlongFrame {
  uint32_t spill[2];      /* 0x00 */
  Vec3 translation;       /* 0x08 */
  AlignScratch scratch;   /* 0x14 */
  uint32_t saved[7];      /* 0x38: the original's saved registers (r14, r13, fr15-fr12, PR): the frame keeps the
                           * original's depth, so that Ninja's writes into the caller's matrix (on the caller's stack),
                           * which the difftest replays relative to r15, land on the same words */
} ScaleAlongFrame;
HW_OFFSET(ScaleAlongFrame, scratch, 0x14);
HW_SIZE(ScaleAlongFrame, 0x54);
void matrix_scale_along(gaddr m, gaddr dir, f32 sx, f32 sy, f32 sz) {
  gaddr frame = hw_stack_alloc(sizeof(ScaleAlongFrame));
  gaddr s = member_addr(frame, ScaleAlongFrame, scratch);
  const Vec3 axis_z = {F32(0.0f), F32(0.0f), F32(1.0f)};
  f32 sine = align_axis(s, dir, &axis_z);
  if (fgt(sine, F32_BITS(PARALLEL_SINE))) {
    int32_t angle = align_angle(sine);
    Matrix *p = (Matrix *)hw_ptr(m);
    ScaleAlongFrame *f = (ScaleAlongFrame *)hw_ptr(frame);
    /* the translation is kept in the frame, as the original keeps it */
    f->translation.z = p->m[14];
    f->translation.y = p->m[13];
    f->translation.x = p->m[12];
    p->m[14] = p->m[13] = p->m[12] = F32(0.0f);
    /* the angle goes through a float and back, as the original */
    njRotate(m, s, f32_to_int(f32_from_int(-angle)));
    njScale(m, sx, sy, sz);
    njRotate(m, s, f32_to_int(f32_from_int(angle)));
    p->m[12] = f->translation.x;
    p->m[13] = f->translation.y;
    p->m[14] = f->translation.z;
  } else {
    njScale(m, sx, sy, sz);
  }
  hw_stack_free(sizeof(ScaleAlongFrame));
}
HW_EXPORT(8c010610, matrix_scale_along(ARG0, ARG1, FARG0, FARG1, FARG2))
