/* Pen Pen port, S13: the game's wrappers around Ninja's matrix and render-state functions (core/ninja.c). */
#ifndef PENPEN_NINJA_H
#define PENPEN_NINJA_H
#include "vec.h"

/* NJS_SPRITE (S14: moved here from game/camera.c for its users) */
typedef struct NjSprite {
  Vec3 p;        /* 0x00 */
  f32 sx, sy;    /* 0x0C */
  int32_t ang;   /* 0x14 */
  gaddr tlist;   /* 0x18: NJS_TEXLIST */
  gaddr tanim;   /* 0x1C: NJS_TEXANIM */
} NjSprite;
HW_SIZE(NjSprite, 0x20);

void matrix_push(void);
void matrix_pop(void);
void matrix_reset(void);
void matrix_translate_v(gaddr v);
void matrix_translate(f32 x, f32 y, f32 z);
void matrix_rotate_x(f32 a);
void matrix_rotate_y(f32 a);
void matrix_rotate_z(f32 a);
void matrix_rotate_xyz(f32 x, f32 y, f32 z);
void matrix_scale(f32 x, f32 y, f32 z);
void matrix_scale_all(f32 s);
void matrix_multiply(gaddr m);
f32 vec3_normalized(gaddr v, gaddr out);
void control3d_push(void);
void control3d_pop(void);
void control3d_set(uint32_t flags);
void fog_enable(int32_t on);

#endif
