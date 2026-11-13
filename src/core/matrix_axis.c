/* Pen Pen port, S17 (b01): two more rotation helpers beside core/transform.c: turn a matrix by the rotation that
 * takes one direction onto another, and read a rotation matrix back as an axis and an angle. Angles are Ninja units
 * (0x10000 a turn); the arcsine and arccosine are the C runtime's (guest code, float in FR4, result in FR0). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

static void njOuterProduct(gaddr a, gaddr b, gaddr out) { CALL3(0x8C0C3D04u, a, b, out); }
static void njRotate(gaddr m, gaddr axis, int32_t angle) { CALL3(0x8C0C4388u, m, axis, angle); }
static f32 rt_asinf(f32 x) { FARGS1(x); CALL0(0x8C062898u); return hw_fret(); } /* func_8C062898 */
static f32 rt_acosf(f32 x) { FARGS1(x); CALL0(0x8C062848u); return hw_fret(); } /* func_8C062848 */

#define ANGLE_UNITS F32(65536.0f)
#define TWO_PI      0x40C90FD7u /* 6.2831855f */

/* radians to Ninja units, as the original truncates them */
static int32_t angle_from_radians(f32 r) { return f32_to_int(fdiv(fmul(r, ANGLE_UNITS), F32_BITS(TWO_PI))); }

/* matrix_turn_toward's frame (the original's layout and depth: two spill words, the three vectors, the saved
 * registers r14, r13, fr15 and PR) */
typedef struct TurnFrame {
  uint32_t spill[2]; /* 0x00 */
  Vec3 axis;         /* 0x08: from x to, normalised */
  Vec3 to;           /* 0x14: normalised copy */
  Vec3 from;         /* 0x20: normalised copy */
  uint32_t saved[4]; /* 0x2C */
} TurnFrame;
HW_OFFSET(TurnFrame, axis, 0x08);
HW_OFFSET(TurnFrame, to, 0x14);
HW_OFFSET(TurnFrame, from, 0x20);
HW_SIZE(TurnFrame, 0x3C);

/* was func_8C010514: turn matrix m by the rotation that takes direction `from` onto direction `to` (about their
 * normalised cross product; nothing when they are parallel: the sine is not above 0) */
void matrix_turn_toward(gaddr m, gaddr from, gaddr to) {
  gaddr frame = hw_stack_alloc(sizeof(TurnFrame));
  TurnFrame *f = (TurnFrame *)hw_ptr(frame);
  memmove(&f->from, hw_ptr(from), sizeof(Vec3));
  njUnitVector(member_addr(frame, TurnFrame, from));
  memmove(&f->to, hw_ptr(to), sizeof(Vec3));
  njUnitVector(member_addr(frame, TurnFrame, to));
  njOuterProduct(member_addr(frame, TurnFrame, from), member_addr(frame, TurnFrame, to),
                 member_addr(frame, TurnFrame, axis));
  f32 sine = njUnitVector(member_addr(frame, TurnFrame, axis));
  if (fgt(sine, F32(0.0f)))
    njRotate(m, member_addr(frame, TurnFrame, axis), angle_from_radians(rt_asinf(sine)));
  hw_stack_free(sizeof(TurnFrame));
}
HW_EXPORT(8c010514, matrix_turn_toward(ARG0, ARG1, ARG2))

/* was func_8C010726: the axis and angle of rotation matrix m: the axis from the differences of the off-diagonal
 * terms, normalised ((0, 1, 0) when they are all 0), the angle (Ninja units, a whole number as a float) from the
 * trace: acos((trace - 1) / 2) */
void matrix_axis_angle(gaddr m, gaddr axis, gaddr angle) {
  const Matrix *mx = (const Matrix *)hw_ptr(m);
  Vec3 *a = vec3_at(axis);
  /* the three differences are read before any is stored (the original's order: the axis may be the matrix) */
  f32 z = fsub(mx->m[4], mx->m[1]);
  f32 y = fsub(mx->m[2], mx->m[8]);
  f32 x = fsub(mx->m[9], mx->m[6]);
  a->z = z;
  a->y = y;
  a->x = x;
  f32 length = njUnitVector(axis);
  f32 trace = fadd(fadd(mx->m[0], mx->m[5]), mx->m[10]);
  int32_t units = angle_from_radians(rt_acosf(fmac(F32(0.5f), trace, F32(-0.5f))));
  F32_AT(angle) = f32_from_int(units);
  if (!fgt(length, F32(0.0f))) {
    a->z = F32(0.0f);
    a->y = F32(1.0f);
    a->x = F32(0.0f);
  }
}
HW_EXPORT(8c010726, matrix_axis_angle(ARG0, ARG1, ARG2))
