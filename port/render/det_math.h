/* Pen Pen port, S22: the draw HLE's transcendental maths as our own code, so that a native build and the wasm module
 * draw bit-identical frames (the host libraries' powf / sinf / cosf may differ in the last place: S21 saw boot's draw
 * digest differ on two frames). Only IEEE-exact operations (+ - * /, conversions, bit moves) in double, no libm, no
 * fused multiply-add (the builds use -ffp-contract=off), so every platform computes the same bits. */
#ifndef PENPEN_DET_MATH_H
#define PENPEN_DET_MATH_H

#include <stdint.h>
#include <string.h>

#define DET_PI_2 1.57079632679489661923
#define DET_LN2 0.693147180559945309417
#define DET_SQRT2 1.41421356237309504880

/* sin and cos of x in [0, pi/2] (Taylor series to x^19 / x^18: below 1e-13 there) */
static inline void det_sincos_q(double x, double *s, double *c) {
  double x2 = x * x, ts = x, tc = 1.0, ss = x, cc = 1.0;
  for (int k = 1; k <= 9; k++) {
    ts = -ts * x2 / (double)((2 * k) * (2 * k + 1));
    tc = -tc * x2 / (double)((2 * k - 1) * (2 * k));
    ss += ts;
    cc += tc;
  }
  *s = ss;
  *c = cc;
}

/* sin and cos of a Ninja angle (0x10000 = one turn: only the low 16 bits count) */
static inline void det_sincos_angle(int32_t angle, float *s, float *c) {
  uint32_t u = (uint32_t)angle & 0xFFFFu, q = u >> 14;
  double x = (double)(u & 0x3FFFu) * (DET_PI_2 / 16384.0), ds, dc;
  det_sincos_q(x, &ds, &dc);
  switch (q) {
  case 0: *s = (float)ds; *c = (float)dc; break;
  case 1: *s = (float)dc; *c = (float)-ds; break;
  case 2: *s = (float)-ds; *c = (float)-dc; break;
  default: *s = (float)-dc; *c = (float)ds; break;
  }
}

/* 2^k as a double, k in [-1022, 1023] */
static inline double det_pow2i(int k) {
  uint64_t b = (uint64_t)(k + 1023) << 52;
  double d;
  memcpy(&d, &b, sizeof d);
  return d;
}

/* the natural logarithm of a finite x > 0: x = m 2^k with m in [sqrt(1/2), sqrt(2)), ln m = 2 atanh((m - 1) / (m + 1)) */
static inline double det_log(double x) {
  int k = 0;
  uint64_t b;
  memcpy(&b, &x, sizeof b);
  if (((b >> 52) & 0x7FF) == 0) { /* subnormal */
    x *= det_pow2i(54);
    k -= 54;
    memcpy(&b, &x, sizeof b);
  }
  k += (int)((b >> 52) & 0x7FF) - 1023;
  b = (b & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
  double m;
  memcpy(&m, &b, sizeof m);
  if (m > DET_SQRT2) {
    m *= 0.5;
    k++;
  }
  double s = (m - 1.0) / (m + 1.0), s2 = s * s, t = s, sum = s;
  for (int n = 3; n <= 25; n += 2) {
    t *= s2;
    sum += t / (double)n;
  }
  return (double)k * DET_LN2 + 2.0 * sum;
}

/* e^y: y = k ln 2 + r, |r| <= ln 2 / 2, Taylor series of e^r to r^18 */
static inline double det_exp(double y) {
  if (y < -744.0) return 0.0;
  if (y > 709.0) y = 709.0;
  double kf = y / DET_LN2;
  int k = (int)(kf < 0 ? kf - 0.5 : kf + 0.5);
  double r = y - (double)k * DET_LN2, t = 1.0, sum = 1.0;
  for (int n = 1; n <= 18; n++) {
    t = t * r / (double)n;
    sum += t;
  }
  if (k < -1022) {
    sum *= det_pow2i(-1022);
    k += 1022;
    if (k < -1022) return 0.0;
  }
  return sum * det_pow2i(k);
}

/* x^e for x > 0 (0 for x <= 0) */
static inline float det_powf(float x, float e) {
  if (!(x > 0.0f)) return 0.0f;
  if (x == 1.0f) return 1.0f;
  return (float)det_exp((double)e * det_log((double)x));
}

#endif
