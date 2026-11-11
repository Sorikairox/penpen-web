/* Pen Pen port, S3: semantics of the SH-4 instructions that are not control transfers.
 *
 * One op_<NAME>(c, op, pc) per OP entry of sh4_insns.def; op is the instruction word, pc its address
 * (used by the PC-relative loads). The interpreter calls them with run-time values, the recompiled C with
 * constants (so the field extraction folds away). They never touch c->pc. Written from the SH-4 Software
 * Manual's descriptions; tested by port/sh4/tests/test_sh4.c.
 *
 * Deviations from the hardware, deliberate: floating point uses the host's IEEE round-to-nearest (FPSCR.RM
 * round-to-zero and DN are not modelled), no FPU exceptions/flags; fsca and fsrra are computed exactly then
 * rounded to single (the hardware's are table approximations); cache and prefetch operations are no-ops,
 * except a prefetch of a store-queue address, which flushes that queue through sh4_io_pref.
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
SH4_INLINE uint32_t *fpair(SH4 *c, int n) { return (n & 1) ? &c->xf.u[n & 14] : &c->fr.u[n]; }
#define PR_ON (c->fpscr & FPSCR_PR)
#define SZ_ON (c->fpscr & FPSCR_SZ)

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
#define FBIN(name, expr)                                                                       \
  OPF(name) {                                                                                  \
    UNUSED;                                                                                    \
    if (PR_ON) { int n = OPN & 14, m = OPM & 14; double x = fgetd(&c->fr, n), y = fgetd(&c->fr, m); \
      fsetd(&c->fr, n, expr); }                                                                \
    else { float x = c->fr.f[OPN], y = c->fr.f[OPM]; c->fr.f[OPN] = expr; }                    \
  }
FBIN(FADD, x + y)
FBIN(FSUB, x - y)
FBIN(FMUL, x * y)
FBIN(FDIV, x / y)
OPF(FCMPEQ) {
  UNUSED;
  if (PR_ON) c->t = fgetd(&c->fr, OPN & 14) == fgetd(&c->fr, OPM & 14);
  else c->t = c->fr.f[OPN] == c->fr.f[OPM];
}
OPF(FCMPGT) {
  UNUSED;
  if (PR_ON) c->t = fgetd(&c->fr, OPN & 14) > fgetd(&c->fr, OPM & 14);
  else c->t = c->fr.f[OPN] > c->fr.f[OPM];
}
OPF(FLOAT) {
  UNUSED;
  if (PR_ON) fsetd(&c->fr, OPN & 14, (double)(int32_t)c->fpul);
  else c->fr.f[OPN] = (float)(int32_t)c->fpul;
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
OPF(FMAC) { UNUSED; c->fr.f[OPN] = c->fr.f[0] * c->fr.f[OPM] + c->fr.f[OPN]; }
OPF(FSQRT) {
  UNUSED;
  if (PR_ON) fsetd(&c->fr, OPN & 14, sqrt(fgetd(&c->fr, OPN & 14)));
  else c->fr.f[OPN] = sqrtf(c->fr.f[OPN]);
}
OPF(FSRRA) { UNUSED; c->fr.f[OPN] = (float)(1.0 / sqrt((double)c->fr.f[OPN])); }
OPF(FCNVDS) {
  UNUSED;
  float f = (float)fgetd(&c->fr, OPN & 14);
  memcpy(&c->fpul, &f, 4);
}
OPF(FCNVSD) {
  UNUSED;
  float f; memcpy(&f, &c->fpul, 4);
  fsetd(&c->fr, OPN & 14, (double)f);
}
OPF(FIPR) {
  UNUSED;
  int n = ((op >> 10) & 3) * 4, m = ((op >> 8) & 3) * 4;
  float *f = c->fr.f;
  f[n + 3] = f[m] * f[n] + f[m + 1] * f[n + 1] + f[m + 2] * f[n + 2] + f[m + 3] * f[n + 3];
}
OPF(FTRV) {
  UNUSED;
  int n = ((op >> 10) & 3) * 4;
  float *f = c->fr.f, *x = c->xf.f;
  float v0 = f[n], v1 = f[n + 1], v2 = f[n + 2], v3 = f[n + 3];
  for (int i = 0; i < 4; i++) f[n + i] = x[i] * v0 + x[i + 4] * v1 + x[i + 8] * v2 + x[i + 12] * v3;
}
OPF(FSCA) {
  UNUSED;
  int n = ((op >> 9) & 7) * 2;
  double a = (double)(c->fpul & 0xFFFF) * (6.283185307179586476925 / 65536.0);
  c->fr.f[n] = (float)sin(a);
  c->fr.f[n + 1] = (float)cos(a);
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
