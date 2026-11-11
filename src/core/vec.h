/* Pen Pen port, S13: the game's small vector and motion classes (C++ inline methods).
 *
 * The game keeps positions and speeds as three floats (Vec3, Ninja's NJS_VECTOR / NJS_POINT3), and moves things with
 * a tiny damped-motion model: a Body (position, velocity, force, damping: each step the force is added to the
 * velocity, the velocity is multiplied by the damping, the position moves by the velocity, the force is used up)
 * and its one-dimensional twin Body1 (value, velocity, force, damping), which an angle or a fade uses. Things are
 * pulled toward a target by adding force proportional to the distance. A four-float value is initialised to
 * (0, 0, 0, 1) (Quat; Body1 reuses that constructor). Their methods are inline: the compiler put a copy into every
 * unit that uses them, so each function below stands for many identical copies in the binary (port/symbols/copies.tsv; their exports
 * are in src/copies/core/vec.c). All arithmetic is the FPU's (f32, include/fpu.h). */
#ifndef PENPEN_VEC_H
#define PENPEN_VEC_H
#include "hw.h"

typedef struct Vec3 {
  f32 x, y, z;
} Vec3;
HW_SIZE(Vec3, 12);

typedef struct Quat {
  f32 x, y, z, w;
} Quat;
HW_SIZE(Quat, 16);

typedef struct Body1 {
  f32 value, velocity, force, damping;
} Body1;
HW_SIZE(Body1, 16);

typedef struct Body {
  Vec3 position; /* 0x00 */
  Vec3 velocity; /* 0x0C */
  Vec3 force;    /* 0x18: added to the velocity on the next step, then cleared */
  Vec3 damping;  /* 0x24: the velocity is multiplied by it every step (1 = none) */
} Body;
HW_OFFSET(Body, damping, 0x24);
HW_SIZE(Body, 0x30);

/* a 4x4 Ninja matrix (NJS_MATRIX), row-major */
typedef struct Matrix {
  f32 m[16];
} Matrix;
HW_SIZE(Matrix, 64);

static inline Vec3 *vec3_at(gaddr a) { return (Vec3 *)hw_ptr(a); }

/* construction and copies */
gaddr vec3_set(f32 x, f32 y, f32 z, gaddr v);
gaddr vec3_copy(gaddr dst, gaddr src);
void vec3_copy_bytes(gaddr dst, gaddr src);
void vec3_zero(gaddr v);
void vec3_fill(gaddr v, f32 s);
gaddr quat_identity(gaddr q);
gaddr body_init(gaddr t);
void body_reset(gaddr t);
gaddr matrix_identity(gaddr m);
/* arithmetic */
gaddr vec3_diff(gaddr out, gaddr from, gaddr to);
gaddr vec3_sum(gaddr out, gaddr a, gaddr b);
void vec3_add(gaddr v, gaddr w);
void vec3_add_xyz(f32 x, f32 y, f32 z, gaddr v);
void vec3_sub(gaddr v, gaddr w);
void vec3_scale(f32 s, gaddr v);
void vec3_scaled(f32 s, gaddr out, gaddr v);
f32 vec3_cross_y(gaddr a, gaddr b);
f32 vec3_distance(gaddr a, gaddr b);
void body_pull(f32 rate, gaddr t, gaddr target);
void body_step(gaddr b);
/* scalars */
f32 f32_square(f32 x);
f32 f32_abs(f32 x);
f32 f32_reciprocal(f32 x);
f32 angle_wrap(f32 a);
void f32_add_to(f32 x, gaddr p);
void body1_pull(f32 target, f32 rate, gaddr p);
void body1_pull_angle(f32 target, f32 rate, gaddr p);
void body1_step(gaddr s);

#endif
