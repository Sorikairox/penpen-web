/* Pen Pen port, S13: building the four views and setting a view's camera (core/view_setup.c; the View structure
 * is core/view.h's).
 *
 * Inside a View (0x18C bytes) the camera code keeps, besides the NJS_CAMERA that Ninja reads (at 0x24) and the
 * NJS_SCREEN (at 0x9C: distance, width, height, ...), the camera as it was asked for at 0xB0 (position, unit
 * direction, roll, field of view: Ninja angle units) and a copy of its first seven words at 0x64 (View, core/view.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/view.h"
#include "../core/inline.h"

/* SDK (Ninja) and the C runtime's array constructor */
static f32 njTan(int32_t angle) { CALL1(0x8C0C4C00u, angle); return hw_fret(); }
static void njSetScreen(gaddr screen) { CALL1(0x8C0D1A7Cu, screen); }
static void nj_SetCamera(gaddr camera) { CALL1(0x8C0D1A94u, camera); }
/* func_8C062800: runs ctor on each of n elements of `size` bytes at array (the C++ runtime's array constructor) */
static uint32_t array_construct(int32_t n, uint32_t size, gaddr array, gaddr ctor) { return CALL4(0x8C062800u, n, size, array, ctor); }

#define VIEW_CTOR    0x8C010BF2u /* view_construct, below */
#define EMPTY_CTOR   0x8C010BEEu /* method_empty */

/* was func_8C010BF2: a View's constructor: its members' (empty) constructors */
void view_construct(gaddr view) {
  method_self(view + (gaddr)__builtin_offsetof(View, light_dir));
  array_construct(3, 12, member_addr(view, View, vec150), EMPTY_CTOR);
}
HW_EXPORT(8c010bf2, view_construct(ARG0))

/* was func_8C01212A: constructs the four views and sets the inverse camera matrix to the identity */
void views_construct(void) {
  array_construct(VIEW_MAX, sizeof(View), hw_addr(g_views), VIEW_CTOR);
  matrix_identity(VIEW_INVERSE_MATRIX);
}
HW_EXPORT(8c01212a, views_construct())

/* was func_8C011F50: a view's camera from a position, a target, a roll and a field of view; with `apply`, the
 * screen distance for the field of view (width / (2 tan(fov / 2))) and the camera go to Ninja */
void view_set_camera(gaddr view, gaddr position, gaddr target, f32 roll, f32 fov, int32_t apply) {
  View *v = view_at(view);
  ViewCamera *c = &v->request;
  const Vec3 *p = vec3_at(position), *t = vec3_at(target);
  c->position.x = p->x;
  c->position.y = p->y;
  c->position.z = p->z;
  c->direction.x = fsub(t->x, p->x);
  c->direction.y = fsub(t->y, p->y);
  c->direction.z = fsub(t->z, p->z);
  c->roll = f32_to_int(roll);
  c->fov = f32_to_int(fov);
  const Vec3 d = c->direction;
  f32 length2 = fadd(fadd(fmul(d.x, d.x), fmul(d.y, d.y)), fmul(d.z, d.z));
  if (!fgt(length2, F32(0.0f))) {
    c->direction.x = F32(0.0f);
    c->direction.y = F32(0.0f);
    c->direction.z = F32(-1.0f);
  }
  /* the copy: position, direction, roll */
  uint32_t *copy = (uint32_t *)&v->camera_copy;
  const uint32_t *words = (const uint32_t *)c;
  for (int i = 0; i < 7; i++)
    copy[i] = words[i];
  if (apply == 0)
    return;
  f32 two = F32(2.0f);
  f32 tangent = fmul(njTan(f32_to_int(fdiv(fov, two))), two);
  v->screen.dist = fdiv(v->screen.w, tangent);
  njSetScreen(member_addr(view, View, screen));
  nj_SetCamera(member_addr(view, View, camera));
}
HW_EXPORT(8c011f50, view_set_camera(ARG0, ARG1, ARG2, FARG0, FARG1, (int32_t)ARG3))

/* was func_8C02286A: the current view's light intensities (unused, diffuse, ambient) */
void view_set_current_light_intensity(f32 a, f32 b, f32 c) {
  ((View *)hw_ptr(g_cur_view))->light_intensity.x = a;
  ((View *)hw_ptr(g_cur_view))->light_intensity.y = b;
  ((View *)hw_ptr(g_cur_view))->light_intensity.z = c;
}
HW_EXPORT(8c02286a, view_set_current_light_intensity(FARG0, FARG1, FARG2))
