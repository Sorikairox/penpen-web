/* Pen Pen port, S20: Ninja's matrix and vector functions (batch N1: matrix.c, transform.c, vector.c), over the
 * globals and types of ninja_lib.h.
 *
 * Ninja's current matrix is not in RAM: it is the FPU's back register bank XF0..XF15 (XMTRX, hw_xmtrx(), column
 * order: element (row i, column j) is m[i + 4 * j]; a RAM matrix keeps the same order). Every matrix function takes
 * the matrix to work on, 0 meaning the current one; most have two bodies (XMTRX through FTRV, a RAM matrix through
 * FIPR or scalar operations) that round differently, so both are kept as the original wrote them.
 *
 * The matrix stack lives in RAM (njInitMatrix's buffer): g_nj_current_matrix points at the slot where the next
 * njPushMatrix saves XMTRX (the draw HLE reads that pointer: port/render/nj.h NJG_CURMATRIX). */
#ifndef PENPEN_NINJA_MATRIX_H
#define PENPEN_NINJA_MATRIX_H

#include "ninja_lib.h"

/* g_nj_matrix_depth (ninja_lib.h): njInitMatrix sets it to 1, push adds 1, pop subtracts n */
#define g_nj_matrix_stack_size GUEST_VAR(int32_t, 0x8C1EF358u) /* how many matrices the stack holds */
#define g_nj_unit_vector_mode  GUEST_VAR(int32_t, 0x8C1EFD30u) /* non-zero: njCalcVector normalises its result */

typedef struct NjVector { f32 x, y, z; } NjVector;
HW_SIZE(NjVector, 12);
static inline NjVector *nj_vector_at(gaddr v) { return (NjVector *)hw_ptr(v); }
/* a RAM matrix's 16 floats */
static inline f32 *nj_floats_at(gaddr m) { return nj_matrix_at(m)->m; }

/* matrix.c */
void nj_init_matrix(gaddr stack, int32_t count, int32_t unit_vectors);
int32_t nj_push_matrix(gaddr m);
int32_t nj_pop_matrix(int32_t n);
void nj_get_matrix(gaddr dst);
void nj_set_matrix(gaddr m, gaddr src);
void nj_unit_matrix(gaddr m);
void nj_multi_matrix(gaddr m, gaddr m2);
f32 nj_matrix_determinant(gaddr m);
int32_t nj_invert_matrix(gaddr m);

/* transform.c */
void nj_rotate_x(gaddr m, int32_t angle);
void nj_rotate_y(gaddr m, int32_t angle);
void nj_rotate_z(gaddr m, int32_t angle);
void nj_rotate_xyz(gaddr m, int32_t x, int32_t y, int32_t z);
void nj_rotate_zxy(gaddr m, int32_t x, int32_t y, int32_t z);
void nj_rotate(gaddr m, gaddr axis, int32_t angle);
void nj_scale(gaddr m, f32 x, f32 y, f32 z);
void nj_scale_v(gaddr m, gaddr v);
void nj_translate(gaddr m, f32 x, f32 y, f32 z);
void nj_translate_v(gaddr m, gaddr v);
void nj_calc_point(gaddr m, gaddr src, gaddr dst);
void nj_calc_vector(gaddr m, gaddr src, gaddr dst);

/* vector.c */
f32 nj_inner_product(gaddr a, gaddr b);
f32 nj_outer_product(gaddr a, gaddr b, gaddr out);
f32 nj_scalor(gaddr v);
f32 nj_scalor2(gaddr v);
f32 nj_unit_vector(gaddr v);

#endif
