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

/* the axis and sine of the turn from up to dir, in s (guest stack) */
static f32 align_axis(gaddr s, gaddr dir) {
  AlignScratch *p = (AlignScratch *)hw_ptr(s);
  p->up.z = F32(0.0f);
  p->up.y = F32(1.0f);
  p->up.x = F32(0.0f);
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
  gaddr s = hw_stack_alloc(sizeof(AlignScratch));
  f32 sine = align_axis(s, dir);
  if (fgt(sine, F32_BITS(PARALLEL_SINE)))
    njRotate(m, s, align_angle(sine));
  hw_stack_free(sizeof(AlignScratch));
}
HW_EXPORT(8c010594, matrix_align_up(ARG0, ARG1))

/* was func_8C010610: scales matrix m by (sx, sy, sz) in the frame whose up axis is dir (turn to that frame, scale,
 * turn back), keeping m's translation out of the turns */
void matrix_scale_along(gaddr m, gaddr dir, f32 sx, f32 sy, f32 sz) {
  gaddr s = hw_stack_alloc(sizeof(AlignScratch));
  f32 sine = align_axis(s, dir);
  if (fgt(sine, F32_BITS(PARALLEL_SINE))) {
    int32_t angle = align_angle(sine);
    Matrix *p = (Matrix *)hw_ptr(m);
    f32 tx = p->m[12], ty = p->m[13], tz = p->m[14];
    p->m[14] = p->m[13] = p->m[12] = F32(0.0f);
    /* the angle goes through a float and back, as the original */
    njRotate(m, s, f32_to_int(f32_from_int(-angle)));
    njScale(m, sx, sy, sz);
    njRotate(m, s, f32_to_int(f32_from_int(angle)));
    p->m[12] = tx;
    p->m[13] = ty;
    p->m[14] = tz;
  } else {
    njScale(m, sx, sy, sz);
  }
  hw_stack_free(sizeof(AlignScratch));
}
/* not exported (S13 difftest: differs from the original, to redo): HW_EXPORT(8c010610, matrix_scale_along(ARG0, ARG1, FARG0, FARG1, FARG2)) */
