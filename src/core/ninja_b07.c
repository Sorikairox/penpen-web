/* Pen Pen port, S17 b07: small Ninja matrix thunks and a vector helper that the scenery actor units of
 * 0x8C052700-0x8C05A600 keep their own copies of (C++ inline methods). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../game/actor_b07.h"

/* was func_8C0599F4: the current matrix = m */
void matrix_load(gaddr m) { njSetMatrix(0, m); }
HW_EXPORT(8c0599f4, matrix_load(ARG0))

/* was func_8C05467C and func_8C05991E: a Matrix assignment: dst = src, returning dst */
gaddr matrix_assign(gaddr dst, gaddr src) {
  njSetMatrix(dst, src);
  return dst;
}
HW_EXPORT(8c05467c, RET(matrix_assign(ARG0, ARG1)))
HW_EXPORT(8c05991e, RET(matrix_assign(ARG0, ARG1)))

/* was func_8C053B98: scale the matrix m (0 = the current one) by (x, y, z) */
void matrix_scale_on(gaddr m, f32 x, f32 y, f32 z) { njScale(m, x, y, z); }
HW_EXPORT(8c053b98, matrix_scale_on(ARG0, FARG0, FARG1, FARG2))

/* was func_8C059932: m = m times `by` (m 0 = the current matrix) */
void matrix_multiply_on(gaddr m, gaddr by) { njMultiMatrix(m, by); }
HW_EXPORT(8c059932, matrix_multiply_on(ARG0, ARG1))

/* was func_8C0527A6: the squared distance from `from` to `to` (the difference on the guest stack, Ninja's
 * njScalor2) */
f32 vec3_distance2(gaddr from, gaddr to) {
  gaddr d = hw_stack_alloc(sizeof(Vec3));
  vec3_diff(d, from, to);
  f32 r = njScalor2(d);
  hw_stack_free(sizeof(Vec3));
  return r;
}
HW_EXPORT(8c0527a6, RETF(vec3_distance2(ARG0, ARG1)))

/* was func_8C059AA6: this unit's copy of vec3_set (core/vec.c) */
HW_EXPORT(8c059aa6, vec3_set(FARG0, FARG1, FARG2, ARG0))

/* was func_8C059ACE: a Body's velocity += (x, y, z) */
HW_EXPORT(8c059ace, vec3_add_xyz(FARG0, FARG1, FARG2, member_addr(ARG0, Body, velocity)))

/* was func_8C058A1A and func_8C059A6C: copies of body_reset (core/vec.c) */
HW_EXPORT(8c058a1a, body_reset(ARG0))
HW_EXPORT(8c059a6c, body_reset(ARG0))
