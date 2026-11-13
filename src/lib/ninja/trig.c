/* Pen Pen port, S20: Ninja's scalar maths. Angles are Ninja units (0x10000 = one turn: only the low 16 bits count);
 * sine and cosine are the FPU's FSCA (fpu.h fsca). */
#include "hw.h"
#include "ninja_lib.h"
#include "../lib.h"

/* was func_8C0C4BC4 (njSin) */
f32 nj_sin(int32_t angle) {
  f32 s, c;
  fsca((uint16_t)angle, &s, &c);
  return s;
}
HW_EXPORT(8c0c4bc4, RETF(nj_sin((int32_t)ARG0)))

/* was func_8C0C4A70 (njCos) */
f32 nj_cos(int32_t angle) {
  f32 s, c;
  fsca((uint16_t)angle, &s, &c);
  return c;
}
HW_EXPORT(8c0c4a70, RETF(nj_cos((int32_t)ARG0)))

/* was func_8C0C4C00 (njTan): sine over cosine */
f32 nj_tan(int32_t angle) {
  f32 s, c;
  fsca((uint16_t)angle, &s, &c);
  return fdiv(s, c);
}
HW_EXPORT(8c0c4c00, RETF(nj_tan((int32_t)ARG0)))

/* was func_8C0C4BD0 (njSqrt): the square root, odd for negative x (-sqrt(|x|)) */
f32 nj_sqrt(f32 x) {
  if (fgt(F32(0.0f), x)) return fneg(fsqrt(fabs_(x)));
  return fsqrt(x);
}
HW_EXPORT(8c0c4bd0, RETF(nj_sqrt(FARG0)))


/* was func_8C0C4B14 (census name njRandom: the code is a thunk to modff): the fraction of x (fr4 as it came), the
 * whole part dropped into an SDK scratch word */
f32 nj_fraction(f32 x) { return rt_modff(x, (f32 *)hw_ptr(G_NJ_MODF_INT_PART)); /* the C library's modff */ }
HW_EXPORT(8c0c4b14, RETF(nj_fraction(FARG0)))
