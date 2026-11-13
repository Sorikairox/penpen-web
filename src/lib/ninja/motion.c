/* Pen Pen port, S22: Ninja's step-by-step motion reader (nj_MotionSetup, nj_MotionStepA / B / C, nj_MotionEnd and the
 * key interpolators they jump to). Until S22 these five were HLE stubs that wrote nothing, so a caller's position,
 * angles and scale stayed as its frame held them (type 0x30's posed parts: DECOMP.md 22.2).
 *
 * The reader keeps its state in Ninja's globals (0x8C1EFB00...): the motion's current node (an NJS_MDATA entry: n key
 * pointers, then n key counts, n = the motion's element count), the frame, and which elements the motion has (type
 * bits 1 position, 2 angles, 4 scale). A step finds the key pair around the frame by a binary search on the keys'
 * frame numbers (as unsigned integers) and jumps through a table by its interpolation argument: 0 linear between the
 * key and the next, 1 the Catmull-Rom spline through the keys before, at, after and after that (indices wrap around
 * the key count). A key is {frame, three values} (16 bytes): floats for position / scale, Ninja angles for angles.
 * End moves to the next node (and multiplies the current matrix by a matrix the reader's other entries set; Setup
 * clears it). The arithmetic is the FPU's (fpu.h), in the original's order.
 *
 * functions.tsv splits each interpolator after its first call (0x8C0A5068, 0x8C0A5116, 0x8C0A51E2, 0x8C0A535A): those
 * parts are written with their heads here; no code enters them on its own (the trimmed translations drop them, so
 * they are not declared as fragments). */
#include "hw.h"
#include "matrix.h"
#include "../lib.h"

/* the reader's state (0x8C1EFB00) */
typedef struct NjMotionState {
  gaddr mdata;        /* 0x00: the current node's NJS_MDATA entry */
  gaddr keys;         /* 0x04: its key pointers (the entry) */
  gaddr counts;       /* 0x08: its key counts (after the pointers) */
  f32 frame;          /* 0x0C */
  uint32_t nb_frames; /* 0x10 */
  int32_t element;    /* 0x14: the other entries' element index (Setup: 0) */
  gaddr matrix;       /* 0x18: multiplied in by End when not 0 (Setup: 0) */
} NjMotionState;
HW_OFFSET(NjMotionState, frame, 0x0C);
HW_OFFSET(NjMotionState, matrix, 0x18);
HW_SIZE(NjMotionState, 0x1C);

/* NJS_MOTION */
typedef struct NjMotionHeader {
  gaddr mdata;        /* 0x00 */
  uint32_t nb_frames; /* 0x04 */
  uint16_t type;      /* 0x08: the elements present (1 pos, 2 ang, 4 scl, ...) */
  uint16_t inp_fn;    /* 0x0A: the element count (low bits), 0xC0 the interpolation */
} NjMotionHeader;
HW_SIZE(NjMotionHeader, 0x0C);

/* a key of three values */
typedef struct NjKey3 {
  uint32_t frame;
  uint32_t v[3]; /* floats or Ninja angles */
} NjKey3;
HW_SIZE(NjKey3, 0x10);

#define g_motion          GUEST_VAR(NjMotionState, 0x8C1EFB00u)
#define g_motion_elements GUEST_VAR(uint16_t, 0x8C1EFB38u) /* inp_fn & 0xFF3F: entries of an NJS_MDATA (1-4) */
#define g_motion_type     GUEST_VAR(uint16_t, 0x8C1EFB3Au)
#define g_motion_spline   GUEST_VAR(uint16_t, 0x8C1EFCE8u) /* inp_fn & 0xC0 != 0 */
/* the reader's other entries (the whole-object motion draws) call through these; Setup picks them by the type bits */
#define g_motion_fn_pos   GUEST_VAR(gaddr, 0x8C1EFAECu)
#define g_motion_fn_ang   GUEST_VAR(gaddr, 0x8C1EFAF0u)
#define g_motion_fn_scl   GUEST_VAR(gaddr, 0x8C1EFAF4u)
#define g_motion_fn_ang2  GUEST_VAR(gaddr, 0x8C1EFAF8u)
#define G_MOTION_STEP_V   0x8C181B6Cu /* the interpolators of position / scale keys, by the step's argument */
#define G_MOTION_STEP_A   0x8C181B74u /*   and of angle keys */
#define G_ONES_LINEAR_V   0x8C17A57Cu /* two 1.0f: the divisor when the next key's frame is not after the key's */
#define G_ONES_LINEAR_A   0x8C17A584u
#define G_ONES_SPLINE_V   0x8C17A58Cu
#define G_ONES_SPLINE_A   0x8C17A594u
#define F_LINEAR_V        0x8C0A5054u
#define F_SPLINE_V        0x8C0A51CAu
#define F_LINEAR_A        0x8C0A5102u
#define F_SPLINE_A        0x8C0A5340u
#define g_errno           GUEST_VAR(int32_t, 0x8C1BAC68u)
#define EDIVZERO          0x44E

#define TWO_POW_31 F32(2147483648.0f)
#define TWO_POW_32 F32(4294967296.0f)

static NjKey3 *key_at(gaddr keys, uint32_t i) { return (NjKey3 *)hw_ptr(keys + i * sizeof(NjKey3)); }

/* was func_8C06320C: the C runtime's unsigned remainder (r1 % r0); a divisor of 0 sets errno and gives 0 */
static uint32_t rt_urem(uint32_t a, uint32_t b) {
  if (b == 0) {
    g_errno = EDIVZERO;
    return 0;
  }
  return a % b;
}

/* was func_8C0A484C (nj_MotionSetup): starts reading `motion` at `frame`; a frame not before the motion's end changes
 * nothing; motion 0 clears the element count, type and spline flag (the node pointer stays) */
void nj_motion_setup(gaddr motion, f32 frame) {
  if (motion != 0) {
    const NjMotionHeader *m = (const NjMotionHeader *)hw_ptr(motion);
    if (!fgt(f32_from_uint(m->nb_frames), frame)) return;
    g_motion_elements = (uint16_t)(m->inp_fn & 0xFF3Fu);
    g_motion_spline = (m->inp_fn & 0xC0u) != 0;
    g_motion_type = m->type;
    g_motion.mdata = m->mdata;
    g_motion.nb_frames = m->nb_frames;
  } else {
    g_motion_elements = 0;
    g_motion_spline = 0;
    g_motion_type = 0;
  }
  g_motion.frame = frame;
  g_motion.element = 0;
  g_motion.matrix = 0;
  g_motion_fn_pos = (g_motion_type & 1) ? 0x8C0A54C6u : 0x8C0A6168u;
  if (g_motion_type & 2) {
    g_motion_fn_ang2 = 0x8C0A56CAu;
    g_motion_fn_ang = 0x8C0A562Cu;
  } else {
    g_motion_fn_ang2 = 0x8C0A61A0u;
    g_motion_fn_ang = 0x8C0A6186u;
  }
  g_motion_fn_scl = (g_motion_type & 4) ? 0x8C0A555Cu : 0x8C0A61BAu;
}
HW_EXPORT(8c0a484c, nj_motion_setup(ARG0, FARG0))

/* the current node's key pointers and counts (an element count other than 1-4 leaves them as they were) */
static void motion_node(void) {
  uint32_t n = g_motion_elements;
  if (n >= 1 && n <= 4) {
    g_motion.keys = g_motion.mdata;
    g_motion.counts = g_motion.mdata + 4 * n;
  }
}

/* the key at or before the frame: a binary search on the keys' frame numbers (unsigned) over [0, count) */
static uint32_t motion_find(gaddr keys, uint32_t count) {
  f32 f = g_motion.frame;
  if (fgt(f, TWO_POW_31)) f = fsub(f, TWO_POW_32); /* fadd of -2^32 */
  uint32_t frame = (uint32_t)f32_to_int(f);
  uint32_t lo = 0, hi = count;
  while (hi - lo > 1) {
    uint32_t mid = (lo + hi) >> 1;
    if (frame >= key_at(keys, mid)->frame) lo = mid; else hi = mid;
  }
  return lo;
}

/* the blend factor between key k0 and the next key k1: (frame - k0's) / (k1's - k0's), or / ones[0] (1.0) when k1 is
 * not after k0 */
static f32 motion_t(const NjKey3 *k0, const NjKey3 *k1, gaddr ones) {
  f32 d[2];
  d[0] = ((const f32 *)hw_ptr(ones))[0];
  d[1] = f32_from_uint(k1->frame - k0->frame);
  f32 from = fsub(g_motion.frame, f32_from_uint(k0->frame));
  return fdiv(from, d[(int32_t)(k1->frame - k0->frame) > 0 ? 1 : 0]);
}

/* was func_8C0A5054 (with its part at 0x8C0A5068): position / scale keys, linear */
static void motion_linear_v(gaddr keys, uint32_t lo, uint32_t count, gaddr out) {
  const NjKey3 *k0 = key_at(keys, lo), *k1 = key_at(keys, rt_urem(lo + 1, count));
  f32 t = motion_t(k0, k1, G_ONES_LINEAR_V);
  f32 u = fsub(F32(1.0f), t);
  f32 *o = (f32 *)hw_ptr(out);
  for (int i = 0; i < 3; i++) o[i] = fmac(u, F32_BITS(k0->v[i]), fmul(t, F32_BITS(k1->v[i])));
}
HW_EXPORT(8c0a5054, motion_linear_v(ARG0, ARG1, ARG2, ARG3))

/* was func_8C0A5102 (with its part at 0x8C0A5116): angle keys, linear (through floats, truncated) */
static void motion_linear_a(gaddr keys, uint32_t lo, uint32_t count, gaddr out) {
  const NjKey3 *k0 = key_at(keys, lo), *k1 = key_at(keys, rt_urem(lo + 1, count));
  f32 t = motion_t(k0, k1, G_ONES_LINEAR_A);
  f32 u = fsub(F32(1.0f), t);
  int32_t *o = (int32_t *)hw_ptr(out);
  for (int i = 0; i < 3; i++)
    o[i] = f32_to_int(fmac(u, f32_from_int((int32_t)k0->v[i]), fmul(t, f32_from_int((int32_t)k1->v[i]))));
}
HW_EXPORT(8c0a5102, motion_linear_a(ARG0, ARG1, ARG2, ARG3))

/* the Catmull-Rom weights of the keys before (m), at (0), after (1) and after that (2) */
typedef struct SplineWeights {
  f32 m, k0, k1, k2;
} SplineWeights;
static SplineWeights spline_weights(f32 t) {
  const f32 half = F32(0.5f);
  f32 t2 = fmul(t, t);
  f32 t3 = fmul(t, t2);
  f32 two_t2 = fmul(t2, fadd(F32(1.0f), F32(1.0f)));
  SplineWeights w;
  w.m = fmul(fsub(fsub(two_t2, t), t3), half);                 /* (2t^2 - t - t^3) / 2 */
  f32 a = fmul(t3, F32(1.5f));
  f32 b = fsub(fmul(t, half), a);                               /* t / 2 - 3t^3 / 2 */
  w.k0 = fadd(fsub(a, fmul(t2, F32(2.5f))), F32(1.0f));        /* 3t^3 / 2 - 5t^2 / 2 + 1 */
  w.k1 = fadd(b, two_t2);                                       /* t / 2 - 3t^3 / 2 + 2t^2 */
  w.k2 = fmul(fsub(t3, t2), half);                              /* (t^3 - t^2) / 2 */
  return w;
}
static void spline_keys(gaddr keys, uint32_t lo, uint32_t count, const NjKey3 *k[4]) {
  k[0] = key_at(keys, rt_urem(lo + count - 1, count));
  k[1] = key_at(keys, lo);
  k[2] = key_at(keys, rt_urem(lo + 1, count));
  k[3] = key_at(keys, rt_urem(lo + 2, count));
}
static f32 spline_sum(const SplineWeights *w, f32 pm, f32 p0, f32 p1, f32 p2) {
  return fmac(w->k2, p2, fmac(w->k1, p1, fmac(w->m, pm, fmul(w->k0, p0))));
}

/* was func_8C0A51CA (with its part at 0x8C0A51E2): position / scale keys, spline */
static void motion_spline_v(gaddr keys, uint32_t lo, uint32_t count, gaddr out) {
  const NjKey3 *k[4];
  spline_keys(keys, lo, count, k);
  SplineWeights w = spline_weights(motion_t(k[1], k[2], G_ONES_SPLINE_V));
  f32 *o = (f32 *)hw_ptr(out);
  for (int i = 0; i < 3; i++)
    o[i] = spline_sum(&w, F32_BITS(k[0]->v[i]), F32_BITS(k[1]->v[i]), F32_BITS(k[2]->v[i]), F32_BITS(k[3]->v[i]));
}
HW_EXPORT(8c0a51ca, motion_spline_v(ARG0, ARG1, ARG2, ARG3))

/* was func_8C0A5340 (with its part at 0x8C0A535A): angle keys, spline (through floats, truncated) */
static void motion_spline_a(gaddr keys, uint32_t lo, uint32_t count, gaddr out) {
  const NjKey3 *k[4];
  spline_keys(keys, lo, count, k);
  SplineWeights w = spline_weights(motion_t(k[1], k[2], G_ONES_SPLINE_A));
  int32_t *o = (int32_t *)hw_ptr(out);
  for (int i = 0; i < 3; i++)
    o[i] = f32_to_int(spline_sum(&w, f32_from_int((int32_t)k[0]->v[i]), f32_from_int((int32_t)k[1]->v[i]),
                                 f32_from_int((int32_t)k[2]->v[i]), f32_from_int((int32_t)k[3]->v[i])));
}
HW_EXPORT(8c0a5340, motion_spline_a(ARG0, ARG1, ARG2, ARG3))

/* a step's jump through its table (the two interpolators the tables hold; another entry is called as guest code with
 * the registers the original passes) */
static void motion_interpolate(gaddr table, int32_t how, gaddr keys, uint32_t lo, uint32_t count, gaddr out) {
  gaddr fn = GUEST_VAR(gaddr, table + 4u * (uint32_t)how);
  switch (fn) {
  case F_LINEAR_V: motion_linear_v(keys, lo, count, out); break;
  case F_SPLINE_V: motion_spline_v(keys, lo, count, out); break;
  case F_LINEAR_A: motion_linear_a(keys, lo, count, out); break;
  case F_SPLINE_A: motion_spline_a(keys, lo, count, out); break;
  default: CALL4(fn, keys, lo, count, out); break;
  }
}
/* element `slot` of the current node: its keys interpolated at the frame into out */
static void motion_step(uint32_t slot, gaddr table, gaddr out, int32_t how) {
  motion_node();
  gaddr keys = GUEST_VAR(gaddr, g_motion.keys + 4 * slot);
  uint32_t count = GUEST_VAR(uint32_t, g_motion.counts + 4 * slot);
  motion_interpolate(table, how, keys, motion_find(keys, count), count, out);
}

/* was func_8C0A4D98 (nj_MotionStepA): the node's position (element 0) into out (three floats); how: 0 linear,
 * 1 spline */
void nj_motion_step_a(gaddr out, int32_t how) { motion_step(0, G_MOTION_STEP_V, out, how); }
HW_EXPORT(8c0a4d98, nj_motion_step_a(ARG0, (int32_t)ARG1))

/* was func_8C0A4E44 (nj_MotionStepB): the node's angles (the element after the position, if the motion has one) into
 * out (three Ninja angles) */
void nj_motion_step_b(gaddr out, int32_t how) {
  uint32_t slot = (g_motion_type & 1) != 0;
  motion_step(slot, G_MOTION_STEP_A, out, how);
}
HW_EXPORT(8c0a4e44, nj_motion_step_b(ARG0, (int32_t)ARG1))

/* was func_8C0A4F2C (nj_MotionStepC): the node's scale (after the position and the angles the motion has) */
void nj_motion_step_c(gaddr out, int32_t how) {
  uint32_t slot = (g_motion_type & 1) != 0;
  if (g_motion_type & 2) slot++;
  motion_step(slot, G_MOTION_STEP_V, out, how);
}
HW_EXPORT(8c0a4f2c, nj_motion_step_c(ARG0, (int32_t)ARG1))

/* was func_8C0A4FF8 (nj_MotionEnd): the next node (an NJS_MDATA entry is 8 bytes per element); the reader's matrix,
 * if one is set, multiplied into the current matrix */
void nj_motion_end(void) {
  uint32_t n = g_motion_elements;
  if (n >= 1 && n <= 4) g_motion.mdata += 8 * n;
  if (g_motion.matrix != 0) nj_multi_matrix(0, g_motion.matrix);
}
HW_EXPORT(8c0a4ff8, nj_motion_end())
