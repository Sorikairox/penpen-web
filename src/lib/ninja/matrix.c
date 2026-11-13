/* Pen Pen port, S20: Ninja's matrix stack and whole-matrix operations (njInitMatrix, njPushMatrix, njPopMatrix,
 * njGetMatrix, njSetMatrix, njUnitMatrix, njMultiMatrix, njInvertMatrix and its determinant helper).
 *
 * The current matrix is XMTRX (the FPU's back bank, ninja_lib.h); a matrix argument of 0 means it. Plain moves of a
 * matrix (get / set / push / pop / unit) copy bits; the arithmetic is the FPU's (fpu.h), in the original's order. */
#include "matrix.h"

#define NJ_MATRIX_BYTES 0x40u

/* the identity matrix of Ninja's .data */
static const f32 *unit_matrix(void) { return nj_floats_at(G_NJ_UNIT_MATRIX); }

/* 8 floats at a time, as the original moves a matrix (all 8 read before any is written) */
static void copy_half(f32 *dst, const f32 *src) {
  f32 t[8];
  memcpy(t, src, sizeof t);
  memcpy(dst, t, sizeof t);
}
static void copy_matrix(f32 *dst, const f32 *src) {
  copy_half(dst, src);
  copy_half(dst + 8, src + 8);
}

/* was func_8C0C325C (njInitMatrix): sets up the matrix stack in the buffer `stack` (room for `count` matrices) and
 * njCalcVector's unit-vector mode; the first slot gets the identity with its X and Y axes negated, which becomes the
 * current matrix */
void nj_init_matrix(gaddr stack, int32_t count, int32_t unit_vectors) {
  g_nj_matrix_depth = 1;
  g_nj_unit_vector_mode = unit_vectors;
  g_nj_current_matrix = stack;
  g_nj_matrix_stack_size = count;
  g_nj_matrix_base = stack;
  const f32 *unit = unit_matrix();
  f32 *slot = nj_floats_at(stack);
  copy_half(slot + 8, unit + 8);
  f32 first[8];
  memcpy(first, unit, sizeof first);
  copy_half(slot, first);
  slot[0] = fneg(first[0]);
  slot[5] = fneg(first[5]);
  memcpy(hw_xmtrx(), slot, NJ_MATRIX_BYTES);
}
HW_EXPORT(8c0c325c, nj_init_matrix(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C0C3D90 (njPushMatrix): saves the current matrix on the stack, then makes `m` current (0: the current
 * matrix stays, and is copied into the new top slot too). 0 when the stack is full, else 1. */
int32_t nj_push_matrix(gaddr m) {
  int32_t depth = g_nj_matrix_depth;
  if (depth >= g_nj_matrix_stack_size) return 0;
  g_nj_matrix_depth = depth + 1;
  gaddr slot = g_nj_current_matrix;
  memcpy(nj_floats_at(slot), hw_xmtrx(), NJ_MATRIX_BYTES);
  slot += NJ_MATRIX_BYTES;
  g_nj_current_matrix = slot;
  if (m)
    memcpy(hw_xmtrx(), nj_floats_at(m), NJ_MATRIX_BYTES);
  else
    memcpy(nj_floats_at(slot), hw_xmtrx(), NJ_MATRIX_BYTES);
  return 1;
}
HW_EXPORT(8c0c3d90, RET(nj_push_matrix(ARG0)))

/* was func_8C0C3D4C (njPopMatrix): drops n matrices and makes the one below them current. The check is unsigned:
 * only a pop that would leave exactly 0 is refused (returns 0; else 1). */
int32_t nj_pop_matrix(int32_t n) {
  uint32_t left = (uint32_t)g_nj_matrix_depth - (uint32_t)n;
  if (left < 1u) return 0;
  g_nj_matrix_depth = (int32_t)left;
  gaddr slot = g_nj_current_matrix - ((uint32_t)n << 6);
  g_nj_current_matrix = slot;
  memcpy(hw_xmtrx(), nj_floats_at(slot), NJ_MATRIX_BYTES);
  return 1;
}
HW_EXPORT(8c0c3d4c, RET(nj_pop_matrix((int32_t)ARG0)))

/* was func_8C0C3230 (njGetMatrix): copies the current matrix to dst */
void nj_get_matrix(gaddr dst) { memcpy(nj_floats_at(dst), hw_xmtrx(), NJ_MATRIX_BYTES); }
HW_EXPORT(8c0c3230, nj_get_matrix(ARG0))

/* was func_8C0C4890 (njSetMatrix): copies src into m (0: into the current matrix) */
void nj_set_matrix(gaddr m, gaddr src) {
  if (m == 0)
    memcpy(hw_xmtrx(), nj_floats_at(src), NJ_MATRIX_BYTES);
  else
    copy_matrix(nj_floats_at(m), nj_floats_at(src));
}
HW_EXPORT(8c0c4890, nj_set_matrix(ARG0, ARG1))

/* was func_8C0C49E0 (njUnitMatrix): sets m (0: the current matrix) to the identity */
void nj_unit_matrix(gaddr m) {
  if (m == 0)
    memcpy(hw_xmtrx(), unit_matrix(), NJ_MATRIX_BYTES);
  else
    copy_matrix(nj_floats_at(m), unit_matrix());
}
HW_EXPORT(8c0c49e0, nj_unit_matrix(ARG0))

/* was func_8C0C3B30 (njMultiMatrix): m = m * m2 (m 0: the current matrix). The current matrix transforms m2's
 * columns with FTRV; a RAM matrix takes each element as the FIPR of m's row and m2's column (the two round alike
 * except for a signed zero, so both are kept). Every element is computed before any is written. */
void nj_multi_matrix(gaddr m, gaddr m2) {
  const f32 *b = nj_floats_at(m2);
  f32 r[16];
  if (m == 0) {
    f32 *x = hw_xmtrx();
    for (int j = 0; j < 4; j++) {
      memcpy(&r[4 * j], &b[4 * j], 4 * sizeof(f32));
      ftrv(x, &r[4 * j]);
    }
    memcpy(x, r, sizeof r);
    return;
  }
  f32 *a = nj_floats_at(m);
  for (int i = 0; i < 4; i++) {
    const f32 row[4] = {a[i], a[i + 4], a[i + 8], a[i + 12]};
    for (int j = 0; j < 4; j++) r[i + 4 * j] = fipr(row, &b[4 * j]);
  }
  memcpy(a, r, sizeof r);
}
HW_EXPORT(8c0c3b30, nj_multi_matrix(ARG0, ARG1))

/* The determinant (expansion along the first two columns, by the 2x2 minors of the last two), in the original's
 * order of operations: a[] is the matrix in column order. */
static f32 determinant(const f32 *a) {
  /* minors of columns 2 and 3 (rows i, j); some appear with both signs, computed as the original does */
  f32 m01 = fsub(fmul(a[8], a[13]), fmul(a[9], a[12]));
  f32 m12 = fsub(fmul(a[9], a[14]), fmul(a[10], a[13]));
  f32 m23 = fsub(fmul(a[10], a[15]), fmul(a[11], a[14]));
  f32 m30 = fsub(fmul(a[11], a[12]), fmul(a[8], a[15]));
  f32 m13 = fsub(fmul(a[13], a[11]), fmul(a[15], a[9]));
  f32 m31 = fsub(fmul(a[15], a[9]), fmul(a[13], a[11]));
  f32 m20 = fsub(fmul(a[12], a[10]), fmul(a[14], a[8]));
  f32 m02 = fsub(fmul(a[14], a[8]), fmul(a[12], a[10]));
  /* the cofactors of column 0's elements, 3x3 determinants over column 1 */
  f32 c0 = fadd(fadd(fmul(m13, a[6]), fmul(m23, a[5])), fmul(m12, a[7]));
  f32 c1 = fadd(fadd(fmul(a[4], m23), fmul(m30, a[6])), fmul(m02, a[7]));
  f32 c2 = fadd(fmul(a[7], m01), fadd(fmul(m31, a[4]), fmul(a[5], m30)));
  f32 c3 = fadd(fadd(fmul(a[5], m20), fmul(a[4], m12)), fmul(a[6], m01));
  return fadd(fsub(fmul(c2, a[2]), fmul(c3, a[3])), fsub(fmul(c0, a[0]), fmul(c1, a[1])));
}

/* was func_8C10C750 (njInvertMatrix's helper): the determinant of m (0: the current matrix) */
f32 nj_matrix_determinant(gaddr m) { return determinant(m ? nj_floats_at(m) : hw_xmtrx()); }
HW_EXPORT(8c10c750, RETF(nj_matrix_determinant(ARG0)))

/* The inverse's elements: each is a cofactor (six triple products a*b*c, the pair a*b first, summed as
 * ((p0 + p1 + p2) - p3 - p4) - p5, negated where `neg` says) times 1 / det, in the original's pairings. */
typedef struct {
  uint8_t neg;
  uint8_t t[6][3];
} InvTerm;
static const InvTerm INV_TERMS[16] = {
    {0, {{10, 15, 5}, {11, 13, 6}, {14, 9, 7}, {10, 13, 7}, {15, 9, 6}, {11, 14, 5}}},
    {1, {{10, 15, 1}, {11, 13, 2}, {14, 9, 3}, {10, 13, 3}, {15, 9, 2}, {11, 14, 1}}},
    {0, {{2, 7, 13}, {1, 6, 15}, {3, 5, 14}, {3, 6, 13}, {2, 5, 15}, {1, 7, 14}}},
    {1, {{1, 6, 11}, {2, 7, 9}, {3, 5, 10}, {3, 6, 9}, {2, 5, 11}, {1, 7, 10}}},
    {1, {{10, 15, 4}, {11, 12, 6}, {14, 8, 7}, {10, 12, 7}, {15, 8, 6}, {11, 14, 4}}},
    {0, {{10, 15, 0}, {11, 12, 2}, {14, 8, 3}, {10, 12, 3}, {15, 8, 2}, {11, 14, 0}}},
    {1, {{2, 7, 12}, {0, 6, 15}, {3, 4, 14}, {3, 6, 12}, {2, 4, 15}, {0, 7, 14}}},
    {0, {{0, 6, 11}, {2, 7, 8}, {3, 4, 10}, {3, 6, 8}, {2, 4, 11}, {0, 7, 10}}},
    {0, {{15, 9, 4}, {11, 12, 5}, {13, 8, 7}, {12, 9, 7}, {15, 8, 5}, {11, 13, 4}}},
    {1, {{15, 9, 0}, {11, 12, 1}, {13, 8, 3}, {12, 9, 3}, {15, 8, 1}, {11, 13, 0}}},
    {0, {{1, 7, 12}, {0, 5, 15}, {3, 4, 13}, {3, 5, 12}, {1, 4, 15}, {0, 7, 13}}},
    {1, {{0, 5, 11}, {1, 7, 8}, {3, 4, 9}, {3, 5, 8}, {1, 4, 11}, {0, 7, 9}}},
    {1, {{14, 9, 4}, {10, 12, 5}, {13, 8, 6}, {12, 9, 6}, {14, 8, 5}, {10, 13, 4}}},
    {0, {{14, 9, 0}, {10, 12, 1}, {13, 8, 2}, {12, 9, 2}, {14, 8, 1}, {10, 13, 0}}},
    {1, {{1, 6, 12}, {0, 5, 14}, {2, 4, 13}, {2, 5, 12}, {1, 4, 14}, {0, 6, 13}}},
    {0, {{0, 5, 10}, {1, 6, 8}, {2, 4, 9}, {2, 5, 8}, {1, 4, 10}, {0, 6, 9}}},
};
static f32 triple(const f32 *a, const uint8_t t[3]) { return fmul(fmul(a[t[0]], a[t[1]]), a[t[2]]); }

/* what a singular matrix becomes (every element; just below FLT_MAX) */
#define NJ_SINGULAR_FILL F32_BITS(0x7F7FC99Eu)

/* was func_8C0C3328 (njInvertMatrix): inverts m (0: the current matrix) in place; 1 on success. A matrix whose
 * determinant is 0 gets NJ_SINGULAR_FILL in every element and 0 is returned. Both bodies of the original compute the
 * same operations. */
int32_t nj_invert_matrix(gaddr m) {
  f32 *a = m ? nj_floats_at(m) : hw_xmtrx();
  f32 det = nj_matrix_determinant(m);
  if (feq(det, F32(0.0f))) {
    for (int i = 0; i < 16; i++) a[i] = NJ_SINGULAR_FILL;
    return 0;
  }
  f32 inv_det = fdiv(F32(1.0f), det);
  f32 r[16];
  for (int i = 0; i < 16; i++) {
    const InvTerm *e = &INV_TERMS[i];
    f32 c = fadd(fadd(triple(a, e->t[0]), triple(a, e->t[1])), triple(a, e->t[2]));
    c = fsub(c, triple(a, e->t[3]));
    c = fsub(c, triple(a, e->t[4]));
    c = fsub(c, triple(a, e->t[5]));
    if (e->neg) c = fneg(c);
    r[i] = fmul(c, inv_det);
  }
  memcpy(a, r, sizeof r);
  return 1;
}
HW_EXPORT(8c0c3328, RET(nj_invert_matrix(ARG0)))
