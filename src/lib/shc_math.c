/* Pen Pen port, S19: the C runtime's single-precision maths (the SHC library linked after the game code, region
 * shc_rt of port/symbols/functions.tsv): atan2f and the pieces it is built from. The first library code outside the
 * game region written by hand (DECOMP.md 20), with asinf, acosf and floorf: every operation is the FPU instruction the original runs (fpu.h:
 * round toward zero, denormals as zero), and the library's error reporting is kept: errno (0x8C1BAC68) is set to
 * EDOM when a result is a NaN and to ERANGE when it is infinite, as the float wrappers do. */
#include "hw.h"

#define g_errno         GUEST_VAR(int32_t, 0x8C1BAC68u)
#define EDOM            0x44D
#define ERANGE          0x44C
/* the library's constants (its .data) */
#define RT_NAN          F32_BITS(0x7F800001u) /* the NaN its functions return (a signalling pattern, moved as is) */
#define RT_INF          F32_BITS(0x7F800000u)
#define RT_PI           F32_BITS(0x40490FDBu)
#define RT_HALF_PI      F32_BITS(0x3FC90FDBu)
#define RT_QUARTER_PI   F32_BITS(0x3F490FDBu)
#define RT_ATAN_QUARTER F32_BITS(0x3E7ADBB0u) /* atan(1/4) */
#define RT_ATAN_HALF    F32_BITS(0x3EED6338u) /* atan(1/2) */
#define RT_ATAN_3_4     F32_BITS(0x3F24BC7Du) /* atan(3/4) */
/* atan's odd polynomial on [-1/8, 1/8]: six coefficients, highest power first */
#define g_atan_poly     ((const f32 *)hw_ptr(0x8C08A500u))
#define ATAN_POLY_N     6

#define EXP_MASK  0x7F800000u
#define MANT_MASK 0x007FFFFFu
#define SIGN_BIT  0x80000000u

static int is_inf_or_nan(f32 x) { return (f32_bits(x) & EXP_MASK) == EXP_MASK; }
static int is_nan(f32 x) { return is_inf_or_nan(x) && (f32_bits(x) & MANT_MASK) != 0; }

/* the float wrappers' check of a result: EDOM for a NaN, ERANGE for an infinity (errno is left alone otherwise) */
static f32 rt_check(f32 r) {
  if (is_inf_or_nan(r)) g_errno = (f32_bits(r) & MANT_MASK) != 0 ? EDOM : ERANGE;
  return r;
}

/* was func_8C064994: x divided by y in whole steps: *steps = |x| / |y| truncated (as an unsigned count: the C
 * library's conversion through 2^31), the remainder |x| - steps * |y| with x's sign. NaN (steps 0) when x is not
 * finite, y is a NaN or zero; x itself (steps 0) when y is infinite. */
static f32 fdivstep(f32 x, f32 y, int32_t *steps) {
  if (is_inf_or_nan(x) || is_nan(y) || feq(y, F32(0.0f))) {
    *steps = 0;
    return RT_NAN;
  }
  if (is_inf_or_nan(y)) {
    *steps = 0;
    return x;
  }
  uint32_t sign = f32_bits(x) & SIGN_BIT;
  f32 ax = fabs_(x), ay = fabs_(y);
  f32 q = fdiv(ax, ay);
  uint32_t q_sign = f32_bits(q) & SIGN_BIT;
  if ((int32_t)(f32_bits(q) & EXP_MASK) < (int32_t)0x4F800000u) /* below 2^32: drop the fraction */
    q = f32_from_int(f32_to_int(q));
  q = F32_BITS(f32_bits(q) | q_sign);
  f32 qs = fgt(q, F32(2147483648.0f)) ? fadd(q, F32(-4294967296.0f)) : q; /* (unsigned) q */
  int32_t n = f32_to_int(qs);
  *steps = n;
  f32 whole = f32_from_int(n);
  if (n < 0) whole = fadd(whole, F32(4294967296.0f));
  f32 rest = fsub(ax, fmul(whole, ay));
  return F32_BITS(f32_bits(rest) | sign);
}
f32 rt_fdivstep(f32 x, f32 y, gaddr steps_out) { return fdivstep(x, y, (int32_t *)hw_ptr(steps_out)); }
HW_EXPORT(8c064994, RETF(rt_fdivstep(FARG0, FARG1, ARG0)))

/* was func_8C064A98: ldexpf(x, n): x * 2^n by its exponent field; infinities and NaN (the library's) as they come,
 * a result out of range is a signed infinity (n >= 0) or a signed zero, and a zero or denormal x a signed zero */
f32 rt_ldexpf(f32 x, int32_t n) {
  uint32_t b = f32_bits(x);
  uint32_t e = b & EXP_MASK;
  int neg = (b & SIGN_BIT) != 0;
  if (e == EXP_MASK) {
    if (b & MANT_MASK) return RT_NAN;
    return neg ? F32_BITS(0xFF800000u) : RT_INF;
  }
  if ((int32_t)e > 0) {
    int32_t biased = (int32_t)(e >> 23) + n;
    if (biased > 0 && biased < 0xFF) return F32_BITS((b & SIGN_BIT) | ((uint32_t)biased << 23) | (b & MANT_MASK));
    if (n >= 0) return neg ? F32_BITS(0xFF800000u) : RT_INF;
  }
  return neg ? F32_BITS(SIGN_BIT) : F32(0.0f);
}
HW_EXPORT(8c064a98, RETF(rt_ldexpf(FARG0, (int32_t)ARG0)))

/* was func_8C064D98: atan's polynomial for small x: x * P(x^2), P by Horner (each step an fmac) */
f32 rt_atan_poly(f32 x) {
  f32 x2 = fmul(x, x);
  f32 p = g_atan_poly[0];
  for (int32_t i = 1; i < ATAN_POLY_N; i++) p = fmac(x2, p, g_atan_poly[i]);
  return fmul(x, p);
}
HW_EXPORT(8c064d98, RETF(rt_atan_poly(FARG0)))

/* was func_8C0647B4: atanf. |x| >= 2^32 gives +-pi/2, |x| = 1 +-pi/4; below 1 the argument is x, above it
 * (|x| - 1) / (|x| + 1) with pi/4 added; that argument's eighth (0..7) picks a reduction about 0, 1/4, 1/2, 3/4 or
 * 1 (whose atan is added), and the polynomial does the rest. NaN in: the library's NaN out. */
f32 rt_atanf(f32 x) {
  int neg = (f32_bits(x) & SIGN_BIT) != 0;
  if (feq(F32(0.0f), x)) return x;
  if (!feq(x, x)) return RT_NAN;
  f32 ax = neg ? fneg(x) : x;
  int32_t exponent = (int32_t)((f32_bits(ax) & EXP_MASK) >> 23) - 127;
  if (exponent >= 32) return neg ? fneg(RT_HALF_PI) : RT_HALF_PI;
  f32 base, t;
  if (fgt(F32(1.0f), ax)) {
    base = F32(0.0f);
    t = ax;
  } else if (feq(ax, F32(1.0f))) {
    return neg ? fneg(RT_QUARTER_PI) : RT_QUARTER_PI;
  } else {
    base = RT_QUARTER_PI;
    t = fdiv(fadd(F32(-1.0f), ax), fadd(F32(1.0f), ax));
  }
  int32_t eighth;
  fdivstep(t, F32(0.125f), &eighth);
  f32 u, r;
  switch (eighth) {
  case 0:
    u = t;
    break;
  case 1:
  case 2: /* about 1/4: (t - 1/4) / (1 + t / 4) */
    base = fadd(base, RT_ATAN_QUARTER);
    u = fdiv(fadd(F32(-0.25f), t), fadd(F32(1.0f), rt_ldexpf(t, -2)));
    break;
  case 3:
  case 4: /* about 1/2 */
    base = fadd(base, RT_ATAN_HALF);
    u = fdiv(fadd(F32(-0.5f), t), fadd(F32(1.0f), rt_ldexpf(t, -1)));
    break;
  case 5:
  case 6: /* about 3/4 */
    base = fadd(base, RT_ATAN_3_4);
    u = fdiv(fadd(t, F32(-0.75f)), fmac(F32(0.75f), t, F32(1.0f)));
    break;
  case 7: /* about 1 */
    base = fadd(base, RT_QUARTER_PI);
    u = fdiv(fadd(F32(-1.0f), t), fadd(F32(1.0f), t));
    break;
  default: /* not reached (t < 1): the original returns the step it passed, 1/8 */
    return neg ? fneg(F32(0.125f)) : F32(0.125f);
  }
  r = fadd(base, rt_atan_poly(u));
  return neg ? fneg(r) : r;
}
HW_EXPORT(8c0647b4, RETF(rt_atanf(FARG0)))

/* was func_8C0635A4: atan2f's body: atan(y / x), moved by pi (toward y's side) when x is negative */
f32 rt_atan2f_body(f32 y, f32 x) {
  f32 r = rt_atanf(fdiv(y, x));
  if (f32_bits(x) & SIGN_BIT) r = fadd((f32_bits(y) & SIGN_BIT) ? fneg(RT_PI) : RT_PI, r);
  return r;
}
HW_EXPORT(8c0635a4, RETF(rt_atan2f_body(FARG0, FARG1)))

/* was func_8C0628E8: atan2f(y, x), errno set as the library's float wrappers do */
f32 rt_atan2f(f32 y, f32 x) { return rt_check(rt_atan2f_body(y, x)); }
HW_EXPORT(8c0628e8, RETF(rt_atan2f(FARG0, FARG1)))

/* was func_8C0634B0: asinf. |x| > 1 or NaN: the library's NaN; |x| = 1: +-pi/2; zero: x; else 2 atan(t), t the
 * half-angle tangent: x / (1 + sqrt(1 - x^2)) below 1/2, (1 - sqrt(1 - x^2)) / x from there */
f32 rt_asinf(f32 x) {
  int neg = (f32_bits(x) & SIGN_BIT) != 0;
  if (!feq(x, x)) return RT_NAN;
  f32 ax = neg ? fneg(x) : x;
  if (fgt(ax, F32(1.0f))) return RT_NAN;
  if (feq(ax, F32(1.0f))) return neg ? fneg(RT_HALF_PI) : RT_HALF_PI;
  if (feq(F32(0.0f), x)) return x;
  f32 root = fsqrt(fsub(F32(1.0f), fmul(x, x)));
  f32 t = fgt(F32(0.5f), ax) ? fdiv(x, fadd(F32(1.0f), root)) : fdiv(fsub(F32(1.0f), root), x);
  return rt_ldexpf(rt_atanf(t), 1);
}
HW_EXPORT(8c0634b0, RETF(rt_asinf(FARG0)))

/* was func_8C063494: acosf = pi/2 - asinf(x) */
f32 rt_acosf(f32 x) { return fsub(RT_HALF_PI, rt_asinf(x)); }
HW_EXPORT(8c063494, RETF(rt_acosf(FARG0)))

/* was func_8C0635FC: floorf: below 2^32 in size the fraction is dropped (toward zero, the sign kept), then a negative
 * x that had a fraction goes down by one */
f32 rt_floorf(f32 x) {
  uint32_t sign = f32_bits(x) & SIGN_BIT;
  f32 t = x;
  if ((int32_t)(f32_bits(x) & EXP_MASK) < (int32_t)0x4F800000u) t = f32_from_int(f32_to_int(x));
  t = F32_BITS(f32_bits(t) | sign);
  if (sign && !feq(x, t)) t = fadd(F32(-1.0f), t);
  return t;
}
HW_EXPORT(8c0635fc, RETF(rt_floorf(FARG0)))

/* was func_8C062898 / func_8C062848 / func_8C062988: asinf, acosf and floorf with errno set as atan2f's wrapper does */
f32 rt_asinf_checked(f32 x) { return rt_check(rt_asinf(x)); }
HW_EXPORT(8c062898, RETF(rt_asinf_checked(FARG0)))
f32 rt_acosf_checked(f32 x) { return rt_check(rt_acosf(x)); }
HW_EXPORT(8c062848, RETF(rt_acosf_checked(FARG0)))
f32 rt_floorf_checked(f32 x) { return rt_check(rt_floorf(x)); }
HW_EXPORT(8c062988, RETF(rt_floorf_checked(FARG0)))

/* S20 */
/* was func_8C06384C: frexpf: x = m * 2^e with 0.5 <= |m| < 1. Zero (and a denormal, which the FPU reads as zero):
 * *e = 0 and x itself; an infinity or NaN: x itself, *e left alone, errno ERANGE / EDOM; otherwise m is x's sign and
 * mantissa under the exponent of 0.5 */
f32 rt_frexpf(f32 x, int32_t *e) {
  uint32_t b = f32_bits(x);
  if (feq(x, F32(0.0f))) {
    *e = 0;
    return x;
  }
  if ((b & EXP_MASK) == EXP_MASK) {
    g_errno = (b & MANT_MASK) != 0 ? EDOM : ERANGE;
    return x;
  }
  *e = (int32_t)((b & EXP_MASK) >> 23) - 0x7E;
  return F32_BITS((b & SIGN_BIT) | (b & MANT_MASK) | 0x3F000000u);
}
HW_EXPORT(8c06384c, RETF(rt_frexpf(FARG0, (int32_t *)hw_ptr(ARG0))))

/* was func_8C064B58: modff's kernel: the whole part (below 2^32 in size the fraction is dropped toward zero, the sign
 * kept) into *whole, the fraction x - whole returned */
f32 rt_modf_kernel(f32 x, f32 *whole) {
  uint32_t sign = f32_bits(x) & SIGN_BIT;
  f32 t = x;
  if ((int32_t)(f32_bits(x) & EXP_MASK) < (int32_t)0x4F800000u) t = f32_from_int(f32_to_int(x));
  t = F32_BITS(f32_bits(t) | sign);
  *whole = t;
  return fsub(x, t);
}
HW_EXPORT(8c064b58, RETF(rt_modf_kernel(FARG0, (f32 *)hw_ptr(ARG0))))

/* was func_8C0638F4: modff: the kernel, with errno set on the fraction as the float wrappers do */
f32 rt_modff(f32 x, f32 *whole) {
  f32 w;
  f32 frac = rt_modf_kernel(x, &w);
  *whole = w;
  return rt_check(frac);
}
HW_EXPORT(8c0638f4, RETF(rt_modff(FARG0, (f32 *)hw_ptr(ARG0))))

/* ---- S20: sinf and cosf (reached from the C: no route runs them) ---- */
#define g_cos_poly ((const f32 *)hw_ptr(0x8C08A44Cu)) /* seven coefficients in x^2, highest power first */
#define g_sin_poly ((const f32 *)hw_ptr(0x8C08A468u))
#define TRIG_POLY_N 7
#define RT_MINUS_QUARTER_PI F32_BITS(0xBF490FDBu)

/* a polynomial in x2 by Horner's rule, each step one fmac */
static f32 poly_x2(const f32 *c, f32 x2) {
  f32 p = c[0];
  for (int k = 1; k < TRIG_POLY_N; k++) p = fmac(x2, p, c[k]);
  return p;
}
/* was func_8C063DA4: cos(x) on [-pi/4, pi/4]: its polynomial in x^2 */
f32 rt_cos_kernel(f32 x) { return poly_x2(g_cos_poly, fmul(x, x)); }
HW_EXPORT(8c063da4, RETF(rt_cos_kernel(FARG0)))
/* was func_8C063DC4: sin(x) on [-pi/4, pi/4]: x times its polynomial in x^2 */
f32 rt_sin_kernel(f32 x) { return fmul(x, poly_x2(g_sin_poly, fmul(x, x))); }
HW_EXPORT(8c063dc4, RETF(rt_sin_kernel(FARG0)))

/* the argument reduction both share: |x| (negative tells whether x was), reduced to [-pi/4, pi/4] by whole
 * quarter turns; returns the octant (0-7) and the reduced value in *r. 1 when x is an infinity or NaN. */
static int trig_reduce(f32 x, int *negative, int *octant, f32 *r) {
  uint32_t b = f32_bits(x);
  *negative = (b & SIGN_BIT) != 0;
  if (!feq(x, F32(0.0f)) && ((b & 0x7FFFFFFFu) == EXP_MASK || !feq(x, x))) return 1;
  if (*negative) x = fneg(x);
  int32_t n;
  f32 t = fdivstep(x, RT_QUARTER_PI, &n);
  n &= 7;
  if (n & 1) t = fadd(RT_MINUS_QUARTER_PI, t);
  *octant = n;
  *r = t;
  return 0;
}
/* was func_8C06365C: cosf (the library's NaN for an infinity or NaN) */
f32 rt_cosf(f32 x) {
  int negative, octant;
  f32 r;
  if (trig_reduce(x, &negative, &octant, &r)) return RT_NAN;
  switch (octant) {
  case 0: case 7: return rt_cos_kernel(r);
  case 1: case 2: return fneg(rt_sin_kernel(r));
  case 3: case 4: return fneg(rt_cos_kernel(r));
  default: return rt_sin_kernel(r); /* 5, 6 */
  }
}
HW_EXPORT(8c06365c, RETF(rt_cosf(FARG0)))
/* was func_8C06374C: sinf (odd: the sign of x applied at the end) */
f32 rt_sinf(f32 x) {
  int negative, octant;
  f32 r, s;
  if (trig_reduce(x, &negative, &octant, &r)) return RT_NAN;
  switch (octant) {
  case 0: case 7: s = rt_sin_kernel(r); break;
  case 1: case 2: s = rt_cos_kernel(r); break;
  case 3: case 4: s = fneg(rt_sin_kernel(r)); break;
  default: s = fneg(rt_cos_kernel(r)); break; /* 5, 6 */
  }
  return negative ? fneg(s) : s;
}
HW_EXPORT(8c06374c, RETF(rt_sinf(FARG0)))
/* was func_8C062938 / func_8C062ADC: cosf and sinf with errno set as the float wrappers do */
f32 rt_cosf_checked(f32 x) { return rt_check(rt_cosf(x)); }
HW_EXPORT(8c062938, RETF(rt_cosf_checked(FARG0)))
f32 rt_sinf_checked(f32 x) { return rt_check(rt_sinf(x)); }
HW_EXPORT(8c062adc, RETF(rt_sinf_checked(FARG0)))
