/* Pen Pen port, S16 (w4): a view's camera, screen and lights go to Ninja (func_8C011C7C; view_select calls it).
 *
 * The camera asked for (View.request) is noted in camera_copy, the screen distance is computed from the field of
 * view and the screen's width, the screen, the Ninja camera and the 2D clip go to Ninja, the fog is switched, then
 * the lights: lights 1, 3 and 4 shine along the view's light direction turned by the current matrix, with its
 * intensity and colour; the ambient colour is its colour times the ambient intensity; light 2's extra sources
 * (1-3) take the view's per-source direction, range and colour. */
#include "hw.h"
#include "sdk.h"
#include "vec.h"
#include "view.h"

/* Ninja (SDK) calls this file makes; the names are port/symbols/sdk_calls.tsv's */
static inline f32 njTan(int32_t angle) { CALL1(0x8C0C4C00u, angle); return hw_fret(); }
static inline void njSetScreen(gaddr screen) { CALL1(0x8C0D1A7Cu, screen); }
static inline void nj_SetCamera(gaddr camera) { CALL1(0x8C0D1A94u, camera); }
/* njCalcVector (Ninja): dst = m * src without the translation (m = 0: the current matrix) */
static inline void njCalcVector(gaddr m, gaddr src, gaddr dst) { CALL3(0x8C0C31C4u, m, src, dst); }
static inline void nj_LightL1Dir(f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL0(0x8C0CFA00u); }
static inline void nj_LightL1Intensity(f32 d, f32 a) { FARGS2(d, a); CALL0(0x8C0CFA38u); }
static inline void nj_LightL1Color(f32 r, f32 g, f32 b) { FARGS3(r, g, b); CALL0(0x8C0CFA46u); }
static inline void nj_LightL3Dir(f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL0(0x8C0D0F34u); }
static inline void nj_LightL3Intensity(f32 d, f32 a) { FARGS2(d, a); CALL0(0x8C0D0F6Cu); }
static inline void nj_LightL3Color(f32 r, f32 g, f32 b) { FARGS3(r, g, b); CALL0(0x8C0D0F7Au); }
static inline void nj_LightL4Dir(f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL0(0x8C0AB584u); }
static inline void nj_LightL4Intensity(f32 d, f32 a) { FARGS2(d, a); CALL0(0x8C0AB5BCu); }
static inline void nj_LightL4Color(f32 r, f32 g, f32 b) { FARGS3(r, g, b); CALL0(0x8C0AB5CAu); }
static inline void nj_SetColor3(f32 r, f32 g, f32 b) { FARGS3(r, g, b); CALL0(0x8C0C542Cu); }
static inline void nj_LightReset(int32_t mode) { CALL1(0x8C0D0348u, mode); }
static inline void nj_LightL2Dir(f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL0(0x8C0D03C8u); }
static inline void nj_LightL2DirN(int32_t n, f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL1(0x8C0D03DCu, n); }
static inline void nj_LightL2IntensityN(int32_t n, f32 a, f32 b) { FARGS2(a, b); CALL1(0x8C0D03F6u, n); }
static inline void nj_LightL2ColorN(int32_t n, f32 r, f32 g, f32 b) { FARGS3(r, g, b); CALL1(0x8C0D03B0u, n); }
static inline void nj_LightL2Color(f32 r, f32 g, f32 b) { FARGS3(r, g, b); CALL0(0x8C0D03A2u); }
static inline void nj_LightL2Update(void) { CALL0(0x8C0D040Eu); }

#define CLIP_MODE 2
#define LIGHT2_SOURCES 3

/* was func_8C011C7C: the view's camera, screen and lights to Ninja (see the top of the file) */
void view_apply_camera(View *view) {
  gaddr v = hw_addr(view);
  const f32 two = fadd(F32(1.0f), F32(1.0f));
  view->camera_copy.position.x = view->request.position.x;
  view->camera_copy.position.y = view->request.position.y;
  view->camera_copy.position.z = view->request.position.z;
  view->camera_copy.direction.x = view->request.direction.x;
  view->camera_copy.direction.y = view->request.direction.y;
  view->camera_copy.direction.z = view->request.direction.z;
  view->camera_copy.roll = view->request.roll;
  f32 half_width = fdiv(view->screen.w, two);
  f32 half_fov = fdiv(f32_from_int(view->request.fov), two);
  view->screen.dist = fdiv(half_width, njTan(f32_to_int(half_fov)));
  njSetScreen(member_addr(v, View, screen));
  nj_SetCamera(member_addr(v, View, camera));
  njClip2D(CLIP_MODE, member_addr(v, View, clip));
  if (view->fog != 0)
    njFogEnable();
  else
    njFogDisable();

  gaddr dir = hw_stack_alloc(sizeof(Vec3));
  njCalcVector(0, member_addr(v, View, light_dir), dir);
  const Vec3 *d = vec3_at(dir);
  const Vec3 *in = &view->light_intensity;
  const Vec3 *c = &view->light_color;
  nj_LightL3Dir(d->x, d->y, d->z);
  nj_LightL3Intensity(in->y, in->z);
  nj_LightL3Color(c->x, c->y, c->z);
  nj_LightL4Dir(d->x, d->y, d->z);
  nj_LightL4Intensity(in->y, in->z);
  nj_LightL4Color(c->x, c->y, c->z);
  nj_LightL1Dir(d->x, d->y, d->z);
  nj_LightL1Intensity(in->y, in->z);
  nj_LightL1Color(c->x, c->y, c->z);
  nj_SetColor3(fmul(c->x, in->z), fmul(c->y, in->z), fmul(c->z, in->z));
  nj_LightReset(view->word11c);
  nj_LightL2Dir(view->light_dir.x, view->light_dir.y, view->light_dir.z);
  for (int32_t n = 1; n <= LIGHT2_SOURCES; n++) {
    const Vec3 *p = &view->vec150[n - 1];
    nj_LightL2DirN(n, p->x, p->y, p->z);
  }
  for (int32_t n = 1; n <= LIGHT2_SOURCES; n++)
    nj_LightL2IntensityN(n, view->ranges174[n - 1].a, view->ranges174[n - 1].b);
  nj_LightL2ColorN(0, c->x, c->y, c->z);
  for (int32_t n = 1; n <= LIGHT2_SOURCES; n++) {
    const f32 *rgb = view->colors120[n - 1].rgb;
    nj_LightL2ColorN(n, rgb[0], rgb[1], rgb[2]);
  }
  nj_LightL2Color(fmul(c->x, in->z), fmul(c->y, in->z), fmul(c->z, in->z));
  nj_LightL2Update();
  hw_stack_free(sizeof(Vec3));
}
HW_EXPORT(8c011c7c, view_apply_camera(view_at(ARG0)))
