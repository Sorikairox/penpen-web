/* Pen Pen port, S20: two Ninja curve evaluators over four 3D points (12 floats). The census names
 * (port/symbols/sdk_calls.tsv: njProjectScreen, njCalcScreen) were guesses: the code evaluates a cubic Bezier
 * curve and a cubic Hermite curve. Each output component sums its four terms in the original's order (the order
 * differs per component: the sums are rounded at each step). */
#include "hw.h"
#include "ninja_lib.h"

#define THREE F32(3.0f)

/* was func_8C0C4A80 (census name njProjectScreen): out = the cubic Bezier curve of control points P0..P3 (p[0..2],
 * p[3..5], p[6..8], p[9..11]) at t: (1-t)^3 P0 + 3t(1-t)^2 P1 + 3t^2(1-t) P2 + t^3 P3 */
void nj_bezier3(gaddr points, gaddr out, f32 t) {
  const f32 *p = (const f32 *)hw_ptr(points);
  f32 *o = (f32 *)hw_ptr(out);
  f32 u = fsub(F32(1.0f), t);
  f32 t2 = fmul(t, t);
  f32 b0 = fmul(fmul(u, u), u);              /* (1-t)^3 */
  f32 b1 = fmul(fmul(fmul(t, u), u), THREE); /* 3t(1-t)^2 */
  f32 b2 = fmul(fmul(t2, u), THREE);         /* 3t^2(1-t) */
  f32 b3 = fmul(t2, t);                      /* t^3 */
  o[0] = fmac(b0, p[0], fmac(b3, p[9], fmac(b1, p[3], fmul(p[6], b2))));
  o[1] = fmac(b3, p[10], fmac(b2, p[7], fmac(b0, p[1], fmul(p[4], b1))));
  o[2] = fmac(b3, p[11], fmac(b2, p[8], fmac(b0, p[2], fmul(p[5], b1))));
}
HW_EXPORT(8c0c4a80, nj_bezier3(ARG0, ARG1, FARG0))

/* was func_8C0C4B28 (census name njCalcScreen): out = the cubic Hermite curve at t from point P0 (p[0..2]) to P1
 * (p[3..5]) with tangents T0 (p[6..8]) and T1 (p[9..11]):
 * (2t^3 - 3t^2 + 1) P0 + (-2t^3 + 3t^2) P1 + (t^3 - 2t^2 + t) T0 + (t^3 - t^2) T1 */
void nj_hermite3(gaddr points, gaddr out, f32 t) {
  const f32 *p = (const f32 *)hw_ptr(points);
  f32 *o = (f32 *)hw_ptr(out);
  f32 two = fadd(F32(1.0f), F32(1.0f));
  f32 t2 = fmul(t, t);
  f32 t3 = fmul(t2, t);
  f32 t2x3 = fmul(t2, THREE);
  f32 t2x2 = fmul(t2, two);
  f32 t3x2 = fmul(t3, two);
  f32 h_t0 = fadd(fsub(t, t2x2), t3);               /* t^3 - 2t^2 + t, computed as (t - 2t^2) + t^3 */
  f32 h_p0 = fadd(fsub(t3x2, t2x3), F32(1.0f));     /* (2t^3 - 3t^2) + 1 */
  f32 h_p1 = fsub(t2x3, t3x2);                      /* 3t^2 - 2t^3 */
  f32 h_t1 = fsub(t3, t2);                          /* t^3 - t^2 */
  o[0] = fmac(h_p0, p[0], fmac(h_t1, p[9], fmac(h_p1, p[3], fmul(p[6], h_t0))));
  o[1] = fmac(h_t1, p[10], fmac(h_t0, p[7], fmac(h_p0, p[1], fmul(p[4], h_p1))));
  o[2] = fmac(h_t1, p[11], fmac(h_t0, p[8], fmac(h_p0, p[2], fmul(p[5], h_p1))));
}
HW_EXPORT(8c0c4b28, nj_hermite3(ARG0, ARG1, FARG0))
