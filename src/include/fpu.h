/* Pen Pen port, S13: single-precision arithmetic for hand-written C, bit for bit as the game's FPU computes it.
 *
 * The game and its SDK run the SH-4 FPU with FPSCR = 0x00040001: round toward zero (RM = 01), denormals are zero
 * (DN), single precision (PR = 0), single moves (SZ = 0). The host rounds to nearest, so a plain C `a + b` on floats
 * can differ from the original in the last bit, and the game's logic sees those bits (RECOMP.md 2: with
 * round-to-nearest the first logo's fade ended a frame late and the attract demo picked another course).
 *
 * So hand-written code never does host float arithmetic. Its float values have the type f32, a struct holding the
 * float's bits: `a + b` on two f32 does not compile, every operation goes through the functions below, and each of
 * them is exactly the instruction the original compiler emitted, computed by the same code as the interpreter and
 * the translation (port/sh4/sh4_ops.h: op_FADD and the others call rz_fadd, f_rz, f_dz, fq):
 *
 *   fadd fsub fmul fdiv   FADD FSUB FMUL FDIV           (denormal operands and results flushed to signed zero, the
 *   fsqrt fmac            FSQRT FMAC (fr0 * m + n)       product of fmac rounded on its own, then the sum)
 *   fneg fabs_            FNEG FABS (sign bit only)
 *   fgt feq               FCMP/GT FCMP/EQ (denormals compare as zero; NaN never compares)
 *   f32_from_int          FLOAT (int -> float, rounded toward zero)
 *   f32_from_uint         SHC's unsigned conversion: FLOAT, then + 2^32 when the int was negative
 *   f32_to_int            FTRC (toward zero, saturating; NaN -> 0x80000000)
 *   fsca                  FSCA (sine and cosine of a 16-bit angle unit: 0x10000 = one turn)
 *   fsrra fipr            FSRRA (1/sqrt), FIPR (4-element dot product)
 *
 * Constants: F32(1.5f) for a value that is a float exactly, F32_BITS(0x3F19999Au) for one the binary stores as a
 * pattern (a decimal literal would be parsed with the host's rounding: the original's constants were not always
 * rounded to nearest). Comparisons the compiler builds from FCMP: a < b is fgt(b, a); a <= b is !fgt(a, b) (as
 * SHC emits it: true for NaN, which the game never produces).
 *
 * The game keeps that FPU mode for its whole run (RECOMP.md 2; the difftest compares FPSCR after every sample); the
 * C runtime's double helpers (PR = 1) are not modelled here: a function that uses doubles calls them as guest code.
 * Tested against the interpreter by port/sh4/tests/test_fpu.c (tools/s3.sh fputest), mutated by Mull with that test
 * as the gate (tools/s3.sh fpu-mull).
 */
#ifndef PENPEN_FPU_H
#define PENPEN_FPU_H

#include "../../port/sh4/sh4_ops.h"

typedef struct f32 {
  float f; /* only fpu.h reads this: everything else goes through the operations */
} f32;
_Static_assert(sizeof(f32) == 4, "f32 is a guest float");

#define F32(x) ((f32){(x)})
static inline f32 F32_BITS(uint32_t bits) { return (f32){u2f(bits)}; }
static inline uint32_t f32_bits(f32 x) { return f2u(x.f); }

/* the FPU's view of an operand or a result: denormals as signed zero (DN), NaN as the SH-4's own qNaN */
static inline float fpu_in(float x) { return f_dz(x); }
static inline f32 fpu_out(float r) { return (f32){fq(f_dz(r))}; }

static inline f32 fadd(f32 a, f32 b) { return fpu_out(rz_fadd(fpu_in(a.f), fpu_in(b.f))); }
static inline f32 fsub(f32 a, f32 b) { return fpu_out(rz_fadd(fpu_in(a.f), -fpu_in(b.f))); }
static inline f32 fmul(f32 a, f32 b) { return fpu_out(rz_fmul(fpu_in(a.f), fpu_in(b.f))); }
static inline f32 fdiv(f32 a, f32 b) { return fpu_out(rz_fdiv(fpu_in(a.f), fpu_in(b.f))); }
static inline f32 fsqrt(f32 a) { return fpu_out(rz_fsqrt(fpu_in(a.f))); }
/* FMAC FR0, FRm, FRn: n + fr0 * m, the product rounded first (as sh4_ops.h models it) */
static inline f32 fmac(f32 fr0, f32 m, f32 n) {
  float p = f_dz(rz_fmul(fpu_in(fr0.f), fpu_in(m.f)));
  return fpu_out(rz_fadd(p, fpu_in(n.f)));
}
static inline f32 fneg(f32 a) { return F32_BITS(f32_bits(a) ^ 0x80000000u); }
static inline f32 fabs_(f32 a) { return F32_BITS(f32_bits(a) & 0x7FFFFFFFu); }

static inline int fgt(f32 a, f32 b) { return fpu_in(a.f) > fpu_in(b.f); }
static inline int feq(f32 a, f32 b) { return fpu_in(a.f) == fpu_in(b.f); }

static inline f32 f32_from_int(int32_t i) { return fpu_out(f_rz((double)i, 0)); }
static inline f32 f32_from_uint(uint32_t u) {
  f32 r = f32_from_int((int32_t)u);
  if ((int32_t)u < 0) r = fadd(r, F32(4294967296.0f));
  return r;
}
static inline int32_t f32_to_int(f32 a) { return (int32_t)sh4_ftrc((double)a.f); }

/* FSCA FPUL, DRn: sin and cos of angle * 2 pi / 0x10000 (only the low 16 bits count) */
static inline void fsca(uint32_t angle, f32 *s, f32 *c) {
  double a = (double)(angle & 0xFFFF) * (6.283185307179586476925 / 65536.0);
  *s = (f32){f_dz(f_rz(sin(a), 0))};
  *c = (f32){f_dz(f_rz(cos(a), 0))};
}
static inline f32 fsrra(f32 a) { return (f32){fq(f_dz(f_rz(1.0 / sqrt((double)fpu_in(a.f)), 0)))}; }
/* FIPR FVm, FVn: the four products and their sum in double, one rounding */
static inline f32 fipr(const f32 a[4], const f32 b[4]) {
  double s = 0;
  for (int i = 0; i < 4; i++) s += (double)fpu_in(b[i].f) * (double)fpu_in(a[i].f);
  return (f32){fq(f_dz(f_rz(s, 0)))};
}

#endif
