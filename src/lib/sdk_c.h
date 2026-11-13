/* Pen Pen port, S20: the prototypes of the SDK functions that are hand-written C (src/lib/), for sdk.h's wrappers
 * (the game calls them as C). Prototypes only: the libraries' types stay in their own headers. */
#ifndef PENPEN_SDK_C_H
#define PENPEN_SDK_C_H

#include "hw.h"

/* Shinobi (sy/heap.c) */
gaddr sy_malloc(uint32_t size);
void sy_free(gaddr block);
/* Ninja (ninja/matrix.c, transform.c, vector.c, trig.c, render_state.c, chunk.c) */
int32_t nj_push_matrix(gaddr m);
int32_t nj_pop_matrix(int32_t n);
void nj_get_matrix(gaddr dst);
void nj_set_matrix(gaddr m, gaddr src);
void nj_unit_matrix(gaddr m);
void nj_multi_matrix(gaddr m, gaddr m2);
int32_t nj_invert_matrix(gaddr m);
void nj_rotate_x(gaddr m, int32_t angle);
void nj_rotate_y(gaddr m, int32_t angle);
void nj_rotate_z(gaddr m, int32_t angle);
void nj_rotate_xyz(gaddr m, int32_t x, int32_t y, int32_t z);
void nj_rotate_zxy(gaddr m, int32_t x, int32_t y, int32_t z);
void nj_scale(gaddr m, f32 x, f32 y, f32 z);
void nj_translate(gaddr m, f32 x, f32 y, f32 z);
void nj_translate_v(gaddr m, gaddr v);
void nj_calc_point(gaddr m, gaddr src, gaddr dst);
f32 nj_inner_product(gaddr a, gaddr b);
f32 nj_scalor(gaddr v);
f32 nj_scalor2(gaddr v);
f32 nj_unit_vector(gaddr v);
f32 nj_sin(int32_t angle);
f32 nj_cos(int32_t angle);
f32 nj_sqrt(f32 x);
int32_t nj_set_texture(gaddr texlist);
void nj_set_constant_attr(uint32_t and_mask, uint32_t or_mask);
void nj_set_view(gaddr view);
void nj_control_3d(uint32_t flags);
void nj_set_constant_material(gaddr argb);
gaddr nj_read_chunk_file(gaddr data, gaddr offset_io, gaddr type_out);

#endif
