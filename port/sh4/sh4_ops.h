/* Pen Pen port, S3: semantics of the SH-4 instructions that are not control transfers.
 *
 * One op_<NAME>(c, op, pc) per OP entry of sh4_insns.def; op is the instruction word, pc its address
 * (used by the PC-relative loads). The interpreter calls them with run-time values, the recompiled C with
 * constants (so the field extraction folds away). They never touch c->pc. Written from the SH-4 Software
 * Manual's descriptions; tested by port/sh4/tests/test_sh4.c.
 *
 * Floating point (S4): FPSCR.RM = 01 (round to zero, the mode the game and its SDK run in) and FPSCR.DN
 * (denormals are zero) are modelled in software: each result is computed exactly, or with its exact error
 * term (two-sum, Dekker two-product, residuals), in IEEE double on the host, then rounded toward zero; the
 * host's round-to-nearest is used only when RM = 00 (wasm has no rounding modes). Deliberate deviations:
 * no FPU exceptions/flags (NaN results are the SH-4's own qNaN); fsca, fsrra, fipr and ftrv are computed in
 * double then rounded to single (the hardware's are approximations); fmac rounds the product, then the
 * sum; cache and prefetch operations are no-ops, except a prefetch of a store-queue address, which flushes
 * that queue through sh4_io_pref.
 */
#ifndef SH4_OPS_H
#define SH4_OPS_H

#include <math.h>
#include "sh4.h"

void sh4_io_pref(uint32_t a);

#define OPN ((op >> 8) & 15)
#define OPM ((op >> 4) & 15)
#define OPI8 (op & 0xFF)
#define OPSI8 ((uint32_t)(int32_t)(int8_t)(op & 0xFF))
#define OPD4 (op & 15)
#define RN c->r[OPN]
#define RM c->r[OPM]
#define R0 c->r[0]
#define OPF(name) SH4_INLINE void op_##name(SH4 *c, uint32_t op, uint32_t pc)
#define UNUSED (void)op; (void)pc

SH4_INLINE uint32_t sx8(uint32_t v) { return (uint32_t)(int32_t)(int8_t)v; }
SH4_INLINE uint32_t sx16(uint32_t v) { return (uint32_t)(int32_t)(int16_t)v; }

/* ---- data transfer ---- */
OPF(MOV_RR) { UNUSED; RN = RM; }
OPF(MOV_I) { (void)pc; RN = OPSI8; }
OPF(MOVW_PC) { RN = sx16(rd16(pc + 4 + OPI8 * 2)); }
OPF(MOVL_PC) { RN = rd32((pc & ~3u) + 4 + OPI8 * 4); }
OPF(MOVB_S) { UNUSED; wr8(RN, (uint8_t)RM); }
OPF(MOVW_S) { UNUSED; wr16(RN, (uint16_t)RM); }
OPF(MOVL_S) { UNUSED; wr32(RN, RM); }
OPF(MOVB_L) { UNUSED; RN = sx8(rd8(RM)); }
OPF(MOVW_L) { UNUSED; RN = sx16(rd16(RM)); }
OPF(MOVL_L) { UNUSED; RN = rd32(RM); }
OPF(MOVB_SDEC) { UNUSED; uint32_t v = RM; wr8(RN - 1, (uint8_t)v); RN -= 1; }
OPF(MOVW_SDEC) { UNUSED; uint32_t v = RM; wr16(RN - 2, (uint16_t)v); RN -= 2; }
OPF(MOVL_SDEC) { UNUSED; uint32_t v = RM; wr32(RN - 4, v); RN -= 4; }
OPF(MOVB_LINC) { UNUSED; uint32_t a = RM; uint32_t v = sx8(rd8(a)); if (OPN != OPM) RM = a + 1; RN = v; }
OPF(MOVW_LINC) { UNUSED; uint32_t a = RM; uint32_t v = sx16(rd16(a)); if (OPN != OPM) RM = a + 2; RN = v; }
OPF(MOVL_LINC) { UNUSED; uint32_t a = RM; uint32_t v = rd32(a); if (OPN != OPM) RM = a + 4; RN = v; }
OPF(MOVB_S4) { UNUSED; wr8(c->r[(op >> 4) & 15] + OPD4, (uint8_t)R0); }
OPF(MOVW_S4) { UNUSED; wr16(c->r[(op >> 4) & 15] + OPD4 * 2, (uint16_t)R0); }
OPF(MOVL_S4) { UNUSED; wr32(RN + OPD4 * 4, RM); }
OPF(MOVB_L4) { UNUSED; R0 = sx8(rd8(c->r[(op >> 4) & 15] + OPD4)); }
OPF(MOVW_L4) { UNUSED; R0 = sx16(rd16(c->r[(op >> 4) & 15] + OPD4 * 2)); }
OPF(MOVL_L4) { UNUSED; RN = rd32(RM + OPD4 * 4); }
OPF(MOVB_S0) { UNUSED; wr8(RN + R0, (uint8_t)RM); }
OPF(MOVW_S0) { UNUSED; wr16(RN + R0, (uint16_t)RM); }
OPF(MOVL_S0) { UNUSED; wr32(RN + R0, RM); }
OPF(MOVB_L0) { UNUSED; RN = sx8(rd8(RM + R0)); }
OPF(MOVW_L0) { UNUSED; RN = sx16(rd16(RM + R0)); }
OPF(MOVL_L0) { UNUSED; RN = rd32(RM + R0); }
OPF(MOVB_SG) { UNUSED; wr8(c->gbr + OPI8, (uint8_t)R0); }
OPF(MOVW_SG) { UNUSED; wr16(c->gbr + OPI8 * 2, (uint16_t)R0); }
OPF(MOVL_SG) { UNUSED; wr32(c->gbr + OPI8 * 4, R0); }
OPF(MOVB_LG) { UNUSED; R0 = sx8(rd8(c->gbr + OPI8)); }
OPF(MOVW_LG) { UNUSED; R0 = sx16(rd16(c->gbr + OPI8 * 2)); }
OPF(MOVL_LG) { UNUSED; R0 = rd32(c->gbr + OPI8 * 4); }
OPF(MOVA) { R0 = (pc & ~3u) + 4 + OPI8 * 4; }
OPF(MOVT) { UNUSED; RN = c->t; }
OPF(SWAPB) { UNUSED; uint32_t v = RM; RN = (v & 0xFFFF0000u) | ((v & 0xFF) << 8) | ((v >> 8) & 0xFF); }
OPF(SWAPW) { UNUSED; uint32_t v = RM; RN = (v >> 16) | (v << 16); }
OPF(XTRCT) { UNUSED; RN = (RN >> 16) | (RM << 16); }
OPF(MOVCAL) { UNUSED; wr32(RN, R0); }

/* ---- arithmetic ---- */
OPF(ADD) { UNUSED; RN += RM; }
OPF(ADD_I) { (void)pc; RN += OPSI8; }
OPF(ADDC) {
  UNUSED;
  uint32_t n0 = RN, t1 = n0 + RM, r = t1 + c->t;
  c->t = (n0 > t1) | (t1 > r);
  RN = r;
}
OPF(ADDV) {
  UNUSED;
  uint32_t n0 = RN, m0 = RM, r = n0 + m0;
  c->t = ((~(n0 ^ m0) & (n0 ^ r)) >> 31) & 1;
  RN = r;
}
OPF(CMPEQ_I) { (void)pc; c->t = R0 == OPSI8; }
OPF(CMPEQ) { UNUSED; c->t = RN == RM; }
OPF(CMPHS) { UNUSED; c->t = RN >= RM; }
OPF(CMPGE) { UNUSED; c->t = (int32_t)RN >= (int32_t)RM; }
OPF(CMPHI) { UNUSED; c->t = RN > RM; }
OPF(CMPGT) { UNUSED; c->t = (int32_t)RN > (int32_t)RM; }
OPF(CMPPZ) { UNUSED; c->t = (int32_t)RN >= 0; }
OPF(CMPPL) { UNUSED; c->t = (int32_t)RN > 0; }
OPF(CMPSTR) {
  UNUSED;
  uint32_t x = RN ^ RM;
  c->t = !(x & 0xFF000000u) || !(x & 0xFF0000u) || !(x & 0xFF00u) || !(x & 0xFFu);
}
OPF(DIV1) {
  UNUSED;
  /* The manual's four-way table (old Q x M) reduces to: subtract when old Q == M, else add; then
   * Q = Q' ^ carry/borrow ^ M, where Q' is the bit shifted out of Rn. */
  uint32_t oq = c->q, rm = RM, rn = RN, t0, t1;
  uint32_t q = rn >> 31;
  rn = (rn << 1) | c->t;
  t0 = rn;
  if (oq == c->m) { rn -= rm; t1 = rn > t0; }
  else { rn += rm; t1 = rn < t0; }
  c->q = q ^ t1 ^ c->m;
  c->t = c->q == c->m;
  RN = rn;
}
OPF(DIV0S) { UNUSED; c->q = RN >> 31; c->m = RM >> 31; c->t = c->q ^ c->m; }
OPF(DIV0U) { UNUSED; c->m = c->q = c->t = 0; }
OPF(DMULS) {
  UNUSED;
  int64_t p = (int64_t)(int32_t)RN * (int64_t)(int32_t)RM;
  c->mach = (uint32_t)((uint64_t)p >> 32); c->macl = (uint32_t)p;
}
OPF(DMULU) {
  UNUSED;
  uint64_t p = (uint64_t)RN * (uint64_t)RM;
  c->mach = (uint32_t)(p >> 32); c->macl = (uint32_t)p;
}
OPF(DT) { UNUSED; RN -= 1; c->t = RN == 0; }
OPF(EXTSB) { UNUSED; RN = sx8(RM); }
OPF(EXTSW) { UNUSED; RN = sx16(RM); }
OPF(EXTUB) { UNUSED; RN = RM & 0xFF; }
OPF(EXTUW) { UNUSED; RN = RM & 0xFFFF; }
OPF(MACL) {
  UNUSED;
  int32_t vn = (int32_t)rd32(RN); RN += 4;
  int32_t vm = (int32_t)rd32(RM); RM += 4;
  int64_t acc = (int64_t)(((uint64_t)c->mach << 32) | c->macl) + (int64_t)vn * vm;
  if (c->s) {
    if (acc > 0x00007FFFFFFFFFFFLL) acc = 0x00007FFFFFFFFFFFLL;
    if (acc < -0x0000800000000000LL) acc = -0x0000800000000000LL;
  }
  c->mach = (uint32_t)((uint64_t)acc >> 32); c->macl = (uint32_t)acc;
}
OPF(MACW) {
  UNUSED;
  int32_t vn = (int16_t)rd16(RN); RN += 2;
  int32_t vm = (int16_t)rd16(RM); RM += 2;
  int64_t p = (int64_t)vn * vm;
  if (c->s) {
    int64_t s = (int64_t)(int32_t)c->macl + p;
    if (s > 0x7FFFFFFF) { s = 0x7FFFFFFF; c->mach = 1; }
    else if (s < -0x7FFFFFFF - 1) { s = -0x7FFFFFFF - 1; c->mach = 1; }
    c->macl = (uint32_t)s;
  } else {
    int64_t acc = (int64_t)(((uint64_t)c->mach << 32) | c->macl) + p;
    c->mach = (uint32_t)((uint64_t)acc >> 32); c->macl = (uint32_t)acc;
  }
}
OPF(MULL) { UNUSED; c->macl = RN * RM; }
OPF(MULSW) { UNUSED; c->macl = (uint32_t)((int32_t)(int16_t)RN * (int32_t)(int16_t)RM); }
OPF(MULUW) { UNUSED; c->macl = (RN & 0xFFFF) * (RM & 0xFFFF); }
OPF(NEG) { UNUSED; RN = 0 - RM; }
OPF(NEGC) {
  UNUSED;
  uint32_t t0 = 0 - RM, r = t0 - c->t;
  c->t = (0 < t0) | (t0 < r);
  RN = r;
}
OPF(SUB) { UNUSED; RN -= RM; }
OPF(SUBC) {
  UNUSED;
  uint32_t n0 = RN, t1 = n0 - RM, r = t1 - c->t;
  c->t = (n0 < t1) | (t1 < r);
  RN = r;
}
OPF(SUBV) {
  UNUSED;
  uint32_t n0 = RN, m0 = RM, r = n0 - m0;
  c->t = (((n0 ^ m0) & (n0 ^ r)) >> 31) & 1;
  RN = r;
}

/* ---- logic ---- */
OPF(AND) { UNUSED; RN &= RM; }
OPF(AND_I) { (void)pc; R0 &= OPI8; }
OPF(ANDB) { (void)pc; uint32_t a = c->gbr + R0; wr8(a, rd8(a) & OPI8); }
OPF(NOT) { UNUSED; RN = ~RM; }
OPF(OR) { UNUSED; RN |= RM; }
OPF(OR_I) { (void)pc; R0 |= OPI8; }
OPF(ORB) { (void)pc; uint32_t a = c->gbr + R0; wr8(a, rd8(a) | OPI8); }
OPF(TASB) { UNUSED; uint32_t a = RN; uint8_t v = rd8(a); c->t = v == 0; wr8(a, v | 0x80); }
OPF(TST) { UNUSED; c->t = (RN & RM) == 0; }
OPF(TST_I) { (void)pc; c->t = (R0 & OPI8) == 0; }
OPF(TSTB) { (void)pc; c->t = (rd8(c->gbr + R0) & OPI8) == 0; }
OPF(XOR) { UNUSED; RN ^= RM; }
OPF(XOR_I) { (void)pc; R0 ^= OPI8; }
OPF(XORB) { (void)pc; uint32_t a = c->gbr + R0; wr8(a, rd8(a) ^ OPI8); }

/* ---- shifts ---- */
OPF(ROTL) { UNUSED; uint32_t v = RN; c->t = v >> 31; RN = (v << 1) | c->t; }
OPF(ROTR) { UNUSED; uint32_t v = RN; c->t = v & 1; RN = (v >> 1) | (c->t << 31); }
OPF(ROTCL) { UNUSED; uint32_t v = RN, o = v >> 31; RN = (v << 1) | c->t; c->t = o; }
OPF(ROTCR) { UNUSED; uint32_t v = RN, o = v & 1; RN = (v >> 1) | (c->t << 31); c->t = o; }
OPF(SHAD) {
  UNUSED;
  int32_t s = (int32_t)RM;
  uint32_t v = RN;
  if (s >= 0) RN = v << (s & 31);
  else if ((s & 31) == 0) RN = ((int32_t)v < 0) ? 0xFFFFFFFFu : 0;
  else RN = (uint32_t)((int32_t)v >> ((~s & 31) + 1));
}
OPF(SHLD) {
  UNUSED;
  int32_t s = (int32_t)RM;
  uint32_t v = RN;
  if (s >= 0) RN = v << (s & 31);
  else if ((s & 31) == 0) RN = 0;
  else RN = v >> ((~s & 31) + 1);
}
OPF(SHAL) { UNUSED; c->t = RN >> 31; RN <<= 1; }
OPF(SHAR) { UNUSED; c->t = RN & 1; RN = (uint32_t)((int32_t)RN >> 1); }
OPF(SHLL) { UNUSED; c->t = RN >> 31; RN <<= 1; }
OPF(SHLR) { UNUSED; c->t = RN & 1; RN >>= 1; }
OPF(SHLL2) { UNUSED; RN <<= 2; }
OPF(SHLR2) { UNUSED; RN >>= 2; }
OPF(SHLL8) { UNUSED; RN <<= 8; }
OPF(SHLR8) { UNUSED; RN >>= 8; }
OPF(SHLL16) { UNUSED; RN <<= 16; }
OPF(SHLR16) { UNUSED; RN >>= 16; }

/* ---- system ---- */
OPF(CLRMAC) { UNUSED; c->mach = c->macl = 0; }
OPF(CLRS) { UNUSED; c->s = 0; }
OPF(CLRT) { UNUSED; c->t = 0; }
OPF(SETS) { UNUSED; c->s = 1; }
OPF(SETT) { UNUSED; c->t = 1; }
OPF(NOP) { UNUSED; }
OPF(OCBI) { UNUSED; }
OPF(OCBP) { UNUSED; }
OPF(OCBWB) { UNUSED; }
OPF(PREF) { UNUSED; uint32_t a = RN; if ((a >> 26) == 0x38) sh4_io_pref(a); }
OPF(LDC_SR) { UNUSED; sh4_set_sr(c, RN & SR_MASK); }
OPF(LDC_GBR) { UNUSED; c->gbr = RN; }
OPF(LDC_VBR) { UNUSED; c->vbr = RN; }
OPF(LDC_SSR) { UNUSED; c->ssr = RN; }
OPF(LDC_SPC) { UNUSED; c->spc = RN; }
OPF(LDC_DBR) { UNUSED; c->dbr = RN; }
OPF(LDC_BANK) { UNUSED; c->rbank[(op >> 4) & 7] = RN; }
#define LDCL(field) uint32_t a = RN; uint32_t v = rd32(a); RN = a + 4; field = v
OPF(LDCL_SR) { UNUSED; uint32_t a = RN; uint32_t v = rd32(a); RN = a + 4; sh4_set_sr(c, v & SR_MASK); }
OPF(LDCL_GBR) { UNUSED; LDCL(c->gbr); }
OPF(LDCL_VBR) { UNUSED; LDCL(c->vbr); }
OPF(LDCL_SSR) { UNUSED; LDCL(c->ssr); }
OPF(LDCL_SPC) { UNUSED; LDCL(c->spc); }
OPF(LDCL_DBR) { UNUSED; LDCL(c->dbr); }
OPF(LDCL_BANK) { UNUSED; LDCL(c->rbank[(op >> 4) & 7]); }
OPF(LDS_MACH) { UNUSED; c->mach = RN; }
OPF(LDS_MACL) { UNUSED; c->macl = RN; }
OPF(LDS_PR) { UNUSED; c->pr = RN; }
OPF(LDSL_MACH) { UNUSED; LDCL(c->mach); }
OPF(LDSL_MACL) { UNUSED; LDCL(c->macl); }
OPF(LDSL_PR) { UNUSED; LDCL(c->pr); }
OPF(STC_SR) { UNUSED; RN = sh4_get_sr(c); }
OPF(STC_GBR) { UNUSED; RN = c->gbr; }
OPF(STC_VBR) { UNUSED; RN = c->vbr; }
OPF(STC_SSR) { UNUSED; RN = c->ssr; }
OPF(STC_SPC) { UNUSED; RN = c->spc; }
OPF(STC_SGR) { UNUSED; RN = c->sgr; }
OPF(STC_DBR) { UNUSED; RN = c->dbr; }
OPF(STC_BANK) { UNUSED; RN = c->rbank[(op >> 4) & 7]; }
#define STCL(val) uint32_t v = (val); uint32_t a = RN - 4; wr32(a, v); RN = a
OPF(STCL_SR) { UNUSED; STCL(sh4_get_sr(c)); }
OPF(STCL_GBR) { UNUSED; STCL(c->gbr); }
OPF(STCL_VBR) { UNUSED; STCL(c->vbr); }
OPF(STCL_SSR) { UNUSED; STCL(c->ssr); }
OPF(STCL_SPC) { UNUSED; STCL(c->spc); }
OPF(STCL_SGR) { UNUSED; STCL(c->sgr); }
OPF(STCL_DBR) { UNUSED; STCL(c->dbr); }
OPF(STCL_BANK) { UNUSED; STCL(c->rbank[(op >> 4) & 7]); }
OPF(STS_MACH) { UNUSED; RN = c->mach; }
OPF(STS_MACL) { UNUSED; RN = c->macl; }
OPF(STS_PR) { UNUSED; RN = c->pr; }
OPF(STSL_MACH) { UNUSED; STCL(c->mach); }
OPF(STSL_MACL) { UNUSED; STCL(c->macl); }
OPF(STSL_PR) { UNUSED; STCL(c->pr); }

/* ---- FPU ---- */
/* Double DRn = FR[n] (upper word) : FR[n+1] (lower word). With FPSCR.SZ = 1 the fmov forms move register
 * pairs; an odd register field selects the XD pair of the other bank, and memory holds FR[n] at the lower
 * address. */
SH4_INLINE double fgetd(const sh4_fbank *b, int n) {
  uint64_t v = ((uint64_t)b->u[n] << 32) | b->u[n + 1];
  double d; memcpy(&d, &v, 8); return d;
}
SH4_INLINE void fsetd(sh4_fbank *b, int n, double d) {
  uint64_t v; memcpy(&v, &d, 8);
  b->u[n] = (uint32_t)(v >> 32); b->u[n + 1] = (uint32_t)v;
}
/* NaN results: the SH-4 FPU returns its own quiet NaN (0x7FBFFFFF / 0x7FF7FFFF_FFFFFFFF; on the SH-4 a
 * mantissa MSB of 0 means quiet), whatever the operands' payloads; the host would propagate a payload that
 * depends on operand order. */
SH4_INLINE float fq(float x) { if (x != x) { uint32_t u = 0x7FBFFFFFu; memcpy(&x, &u, 4); } return x; }
SH4_INLINE double dq(double x) { if (x != x) { uint64_t u = 0x7FF7FFFFFFFFFFFFull; memcpy(&x, &u, 8); } return x; }
SH4_INLINE uint32_t *fpair(SH4 *c, int n) { return (n & 1) ? &c->xf.u[n & 14] : &c->fr.u[n]; }
#define PR_ON (c->fpscr & FPSCR_PR)
#define SZ_ON (c->fpscr & FPSCR_SZ)
#define RZ_ON ((c->fpscr & FPSCR_RM) == 1)
#define DN_ON (c->fpscr & FPSCR_DN)

/* ---- rounding toward zero and denormals-as-zero, in software ---- */
SH4_INLINE uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
SH4_INLINE float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
SH4_INLINE uint64_t d2u(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
SH4_INLINE double u2d(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
SH4_INLINE float f_dz(float x) { uint32_t u = f2u(x); return (u & 0x7F800000u) ? x : u2f(u & 0x80000000u); }
SH4_INLINE double d_dz(double x) {
  uint64_t u = d2u(x);
  return (u & 0x7FF0000000000000ull) ? x : u2d(u & 0x8000000000000000ull);
}
/* the value d + e (e: the exact error of d, |e| <= half an ulp of d; 0 when d is exact) rounded to single
 * toward zero. (float)d rounds to nearest: step back one ulp when that went away from zero, or when d is a
 * single exactly and the error points toward zero. Overflow gives the largest finite value. */
SH4_INLINE float f_rz(double d, double e) {
  float f = (float)d;
  if (f != f || d == 0) return f;
  double fd = (double)f;
  if (fabs(d) != INFINITY && (fabs(fd) > fabs(d) || (fd == d && e != 0 && ((e < 0) != (d < 0)))))
    f = u2f(f2u(f) - 1);
  return f;
}
/* d with exact error e, toward zero (doubles) */
SH4_INLINE double d_rz(double d, double e) {
  if (d != d || d == 0) return d;
  if (fabs(d) == INFINITY) return d; /* overflow: the callers' rz_ovf */
  if (e != 0 && ((e < 0) != (d < 0))) d = u2d(d2u(d) - 1);
  return d;
}
SH4_INLINE double two_sum_err(double a, double b, double s) {
  double bb = s - a;
  return (a - (s - bb)) + (b - bb);
}
SH4_INLINE void dsplit(double a, double *hi, double *lo) {
  double t = 134217729.0 * a; /* 2^27 + 1 */
  *hi = t - (t - a); *lo = a - *hi;
}
SH4_INLINE double two_prod_err(double a, double b, double p) {
  double ah, al, bh, bl;
  dsplit(a, &ah, &al); dsplit(b, &bh, &bl);
  return ((ah * bh - p) + ah * bl + al * bh) + al * bl;
}
/* single precision, rounded toward zero, denormals as zero (when DN) */
SH4_INLINE float rz_fadd(float x, float y) { double a = x, b = y, s = a + b; return f_rz(s, two_sum_err(a, b, s)); }
SH4_INLINE float rz_fmul(float x, float y) { return f_rz((double)x * (double)y, 0); } /* 24x24 bits: exact */
/* a quotient or root that is a single exactly is the exact result (it cannot be within 2^-53 of a single
 * without being one), so the double result rounded toward zero is the answer */
SH4_INLINE float rz_fdiv(float x, float y) { return f_rz((double)x / (double)y, 0); }
SH4_INLINE float rz_fsqrt(float x) { return f_rz(sqrt((double)x), 0); }
/* double precision: overflow toward zero is the largest finite value */
SH4_INLINE double rz_ovf(double r, double x, double y) {
  if (fabs(r) == INFINITY && fabs(x) != INFINITY && fabs(y) != INFINITY) return u2d(d2u(r) - 1);
  return r;
}
SH4_INLINE double rz_dadd(double x, double y) {
  double s = x + y;
  if (fabs(s) == INFINITY) return rz_ovf(s, x, y);
  return d_rz(s, two_sum_err(x, y, s));
}
SH4_INLINE double rz_dmul(double x, double y) {
  double p = x * y;
  if (fabs(p) == INFINITY) return rz_ovf(p, x, y);
  if (p == 0 || fabs(p) < 0x1p-969 || fabs(x) > 0x1p995 || fabs(y) > 0x1p995) return p; /* outside Dekker's range */
  return d_rz(p, two_prod_err(x, y, p));
}
SH4_INLINE double rz_ddiv(double x, double y) {
  double q = x / y;
  if (fabs(q) == INFINITY) return y != 0 ? rz_ovf(q, x, 1) : q;
  if (q != q || q == 0 || fabs(q) < 0x1p-969 || fabs(q) > 0x1p995 || fabs(y) > 0x1p995 || fabs(y) < 0x1p-969) return q;
  double p = q * y, r = (x - p) - two_prod_err(q, y, p); /* x - q*y; the exact quotient is q + r/y */
  if (r != 0 && ((r < 0) != (y < 0)) != (q < 0)) q = u2d(d2u(q) - 1);
  return q;
}
SH4_INLINE double rz_dsqrt(double x) {
  double q = sqrt(x);
  if (q != q || q == 0 || q == INFINITY || q < 0x1p-480) return q;
  double p = q * q, r = (x - p) - two_prod_err(q, q, p);
  if (r < 0) q = u2d(d2u(q) - 1);
  return q;
}

OPF(FMOV) {
  UNUSED;
  if (SZ_ON) { uint32_t *s = fpair(c, OPM), *d = fpair(c, OPN); uint32_t a = s[0], b = s[1]; d[0] = a; d[1] = b; }
  else c->fr.u[OPN] = c->fr.u[OPM];
}
OPF(FMOV_L) {
  UNUSED;
  uint32_t a = RM;
  if (SZ_ON) { uint32_t *d = fpair(c, OPN); d[0] = rd32(a); d[1] = rd32(a + 4); }
  else c->fr.u[OPN] = rd32(a);
}
OPF(FMOV_S) {
  UNUSED;
  uint32_t a = RN;
  if (SZ_ON) { uint32_t *s = fpair(c, OPM); wr32(a, s[0]); wr32(a + 4, s[1]); }
  else wr32(a, c->fr.u[OPM]);
}
OPF(FMOV_LINC) {
  UNUSED;
  uint32_t a = RM;
  if (SZ_ON) { uint32_t *d = fpair(c, OPN); d[0] = rd32(a); d[1] = rd32(a + 4); RM = a + 8; }
  else { c->fr.u[OPN] = rd32(a); RM = a + 4; }
}
OPF(FMOV_SDEC) {
  UNUSED;
  if (SZ_ON) { uint32_t *s = fpair(c, OPM); uint32_t a = RN - 8; wr32(a, s[0]); wr32(a + 4, s[1]); RN = a; }
  else { uint32_t a = RN - 4; wr32(a, c->fr.u[OPM]); RN = a; }
}
OPF(FMOV_L0) {
  UNUSED;
  uint32_t a = RM + R0;
  if (SZ_ON) { uint32_t *d = fpair(c, OPN); d[0] = rd32(a); d[1] = rd32(a + 4); }
  else c->fr.u[OPN] = rd32(a);
}
OPF(FMOV_S0) {
  UNUSED;
  uint32_t a = RN + R0;
  if (SZ_ON) { uint32_t *s = fpair(c, OPM); wr32(a, s[0]); wr32(a + 4, s[1]); }
  else wr32(a, c->fr.u[OPM]);
}
OPF(FLDI0) { UNUSED; c->fr.u[OPN] = 0x00000000u; }
OPF(FLDI1) { UNUSED; c->fr.u[OPN] = 0x3F800000u; }
OPF(FLDS) { UNUSED; c->fpul = c->fr.u[OPN]; }
OPF(FSTS) { UNUSED; c->fr.u[OPN] = c->fpul; }
OPF(FABS) { UNUSED; c->fr.u[PR_ON ? (OPN & 14) : OPN] &= 0x7FFFFFFFu; }
OPF(FNEG) { UNUSED; c->fr.u[PR_ON ? (OPN & 14) : OPN] ^= 0x80000000u; }
/* x op y: single or double by PR; RM = 01: toward zero (software), else the host's nearest; DN: denormal
 * operands and results are zero */
#define FBIN(name, expr, rzs, rzd)                                                                     \
  OPF(name) {                                                                                          \
    UNUSED;                                                                                            \
    if (PR_ON) {                                                                                       \
      int n = OPN & 14, m = OPM & 14;                                                                  \
      double x = fgetd(&c->fr, n), y = fgetd(&c->fr, m), r;                                            \
      if (DN_ON) { x = d_dz(x); y = d_dz(y); }                                                         \
      r = RZ_ON ? rzd : (expr);                                                                        \
      if (DN_ON) r = d_dz(r);                                                                          \
      fsetd(&c->fr, n, dq(r));                                                                         \
    } else {                                                                                           \
      float x = c->fr.f[OPN], y = c->fr.f[OPM], r;                                                     \
      if (DN_ON) { x = f_dz(x); y = f_dz(y); }                                                         \
      r = RZ_ON ? rzs : (expr);                                                                        \
      if (DN_ON) r = f_dz(r);                                                                          \
      c->fr.f[OPN] = fq(r);                                                                            \
    }                                                                                                  \
  }
FBIN(FADD, x + y, rz_fadd(x, y), rz_dadd(x, y))
FBIN(FSUB, x - y, rz_fadd(x, -y), rz_dadd(x, -y))
FBIN(FMUL, x * y, rz_fmul(x, y), rz_dmul(x, y))
FBIN(FDIV, x / y, rz_fdiv(x, y), rz_ddiv(x, y))
/* single results of the other operations: toward zero by RM, denormals flushed by DN */
#define FRES(c, d) (DN_ON ? f_dz(RZ_ON ? f_rz((d), 0) : (float)(d)) : (RZ_ON ? f_rz((d), 0) : (float)(d)))
#define FIN(c, x) (DN_ON ? f_dz(x) : (x))
#define DIN(c, x) (DN_ON ? d_dz(x) : (x))
OPF(FCMPEQ) {
  UNUSED;
  if (PR_ON) c->t = DIN(c, fgetd(&c->fr, OPN & 14)) == DIN(c, fgetd(&c->fr, OPM & 14));
  else c->t = FIN(c, c->fr.f[OPN]) == FIN(c, c->fr.f[OPM]);
}
OPF(FCMPGT) {
  UNUSED;
  if (PR_ON) c->t = DIN(c, fgetd(&c->fr, OPN & 14)) > DIN(c, fgetd(&c->fr, OPM & 14));
  else c->t = FIN(c, c->fr.f[OPN]) > FIN(c, c->fr.f[OPM]);
}
OPF(FLOAT) {
  UNUSED;
  if (PR_ON) fsetd(&c->fr, OPN & 14, (double)(int32_t)c->fpul); /* exact */
  else c->fr.f[OPN] = FRES(c, (double)(int32_t)c->fpul);
}
SH4_INLINE uint32_t sh4_ftrc(double x) {
  if (x != x) return 0x80000000u;
  if (x >= 2147483648.0) return 0x7FFFFFFFu;
  if (x <= -2147483649.0) return 0x80000000u;
  return (uint32_t)(int32_t)x;
}
OPF(FTRC) {
  UNUSED;
  if (PR_ON) c->fpul = sh4_ftrc(fgetd(&c->fr, OPN & 14));
  else c->fpul = sh4_ftrc((double)c->fr.f[OPN]);
}
OPF(FMAC) {
  UNUSED;
  float a = FIN(c, c->fr.f[0]), b = FIN(c, c->fr.f[OPM]), d = FIN(c, c->fr.f[OPN]), p, r;
  if (RZ_ON) { p = rz_fmul(a, b); if (DN_ON) p = f_dz(p); r = rz_fadd(p, d); }
  else { p = a * b; if (DN_ON) p = f_dz(p); r = p + d; }
  c->fr.f[OPN] = fq(FIN(c, r));
}
OPF(FSQRT) {
  UNUSED;
  if (PR_ON) { double x = DIN(c, fgetd(&c->fr, OPN & 14)); fsetd(&c->fr, OPN & 14, dq(DIN(c, RZ_ON ? rz_dsqrt(x) : sqrt(x)))); }
  else { float x = FIN(c, c->fr.f[OPN]); c->fr.f[OPN] = fq(FIN(c, RZ_ON ? rz_fsqrt(x) : sqrtf(x))); }
}
OPF(FSRRA) { UNUSED; c->fr.f[OPN] = fq(FRES(c, 1.0 / sqrt((double)FIN(c, c->fr.f[OPN])))); }
OPF(FCNVDS) {
  UNUSED;
  float f = fq(FRES(c, DIN(c, fgetd(&c->fr, OPN & 14))));
  memcpy(&c->fpul, &f, 4);
}
OPF(FCNVSD) {
  UNUSED;
  float f; memcpy(&f, &c->fpul, 4);
  fsetd(&c->fr, OPN & 14, dq((double)FIN(c, f))); /* exact */
}
/* fipr / ftrv: products exact in double, sums in double, one rounding to single */
OPF(FIPR) {
  UNUSED;
  int n = ((op >> 10) & 3) * 4, m = ((op >> 8) & 3) * 4;
  float *f = c->fr.f;
  double s = 0;
  for (int i = 0; i < 4; i++) s += (double)FIN(c, f[m + i]) * (double)FIN(c, f[n + i]);
  f[n + 3] = fq(FRES(c, s));
}
OPF(FTRV) {
  UNUSED;
  int n = ((op >> 10) & 3) * 4;
  float *f = c->fr.f, *x = c->xf.f;
  double v[4];
  for (int i = 0; i < 4; i++) v[i] = FIN(c, f[n + i]);
  for (int i = 0; i < 4; i++) {
    double s = (double)FIN(c, x[i]) * v[0] + (double)FIN(c, x[i + 4]) * v[1] + (double)FIN(c, x[i + 8]) * v[2] +
               (double)FIN(c, x[i + 12]) * v[3];
    f[n + i] = fq(FRES(c, s));
  }
}
OPF(FSCA) {
  UNUSED;
  int n = ((op >> 9) & 7) * 2;
  double a = (double)(c->fpul & 0xFFFF) * (6.283185307179586476925 / 65536.0);
  c->fr.f[n] = FRES(c, sin(a));
  c->fr.f[n + 1] = FRES(c, cos(a));
}
OPF(FRCHG) { UNUSED; sh4_set_fpscr(c, c->fpscr ^ FPSCR_FR); }
OPF(FSCHG) { UNUSED; c->fpscr ^= FPSCR_SZ; }
OPF(LDS_FPSCR) { UNUSED; sh4_set_fpscr(c, RN & FPSCR_MASK); }
OPF(LDS_FPUL) { UNUSED; c->fpul = RN; }
OPF(LDSL_FPSCR) { UNUSED; uint32_t a = RN; uint32_t v = rd32(a); RN = a + 4; sh4_set_fpscr(c, v & FPSCR_MASK); }
OPF(LDSL_FPUL) { UNUSED; LDCL(c->fpul); }
OPF(STS_FPSCR) { UNUSED; RN = c->fpscr; }
OPF(STS_FPUL) { UNUSED; RN = c->fpul; }
OPF(STSL_FPSCR) { UNUSED; STCL(c->fpscr); }
OPF(STSL_FPUL) { UNUSED; STCL(c->fpul); }

#endif
