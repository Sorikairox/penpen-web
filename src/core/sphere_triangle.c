/* Pen Pen port, S17 b09 (b01's rest): two tests of a sphere (a centre and a radius) against a triangle (three
 * Vec3s), on the distance func_8C012678 measures between them. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

/* a sphere as these read it (the layout of core/draw2d.c's and game/actor_b05.h's Sphere) */
typedef struct Sphere {
  Vec3 center; /* 0x00 */
  f32 radius;  /* 0x0C */
} Sphere;
HW_OFFSET(Sphere, radius, 0x0C);

/* func_8C012678 (still translated): the distance between the sphere's centre and the triangle (a float in fr0) */
static f32 sphere_triangle_distance(gaddr sphere, gaddr triangle) {
  CALL2(0x8C012678u, sphere, triangle);
  return hw_fret();
}

/* was func_8C0128AA: how far the sphere's surface is from the triangle (negative: it reaches into it) */
f32 sphere_triangle_gap(gaddr sphere, gaddr triangle) {
  f32 d = sphere_triangle_distance(sphere, triangle);
  return fsub(d, ((const Sphere *)hw_ptr(sphere))->radius);
}
HW_EXPORT(8c0128aa, RETF(sphere_triangle_gap(ARG0, ARG1)))

/* was func_8C0128C0: 1 when the sphere reaches the triangle (the distance not above the radius), else 0 */
int32_t sphere_triangle_touches(gaddr sphere, gaddr triangle) {
  f32 d = sphere_triangle_distance(sphere, triangle);
  return !fgt(d, ((const Sphere *)hw_ptr(sphere))->radius);
}
HW_EXPORT(8c0128c0, RET(sphere_triangle_touches(ARG0, ARG1)))
