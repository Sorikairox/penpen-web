/* Pen Pen port, S13 (batch b): the camera's helpers, the current view's lights, and a player's CourseEnv: its fog
 * track, light rig and weather particles (0x8C0407B8-0x8C04381A). S14: the classes are View (core/view.h), CourseEnv
 * and Course (game/course.h), RaceScene (game/race_scene.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "game.h"
#include "course.h"
#include "race_scene.h"

/* njSetConstantAttr (Ninja): the constant attribute words (and-mask, or-mask) */
static inline void njSetConstantAttr(uint32_t and_attr, uint32_t or_attr) { CALL2(0x8C0A9672u, and_attr, or_attr); }

/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

#define g_control3d_top GUEST_VAR(gaddr, 0x8C08A8A0u) /* -> the current flags word (core/ninja.c's stack) */

/* was func_8C0407B8: the address of a view's camera request */
gaddr view_request(gaddr view) { return member_addr(view, View, request); }
HW_EXPORT(8c0407b8, RET(view_request(ARG0)))

/* was func_8C0407CE: the current view's heading: the angle of its camera's direction (x, z) in whole Ninja angle
 * units (0x10000 a turn), as a float */
f32 view_heading(void) {
  const View *view = view_at(g_cur_view);
  f32 radians = rt_atan2f(view->camera_copy.direction.x, view->camera_copy.direction.z);
  f32 units = fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u)); /* 2 pi (6.2831855) */
  return f32_from_int(f32_to_int(units));
}
HW_EXPORT(8c0407ce, RETF(view_heading()))

/* was func_8C040860: add flags to the current njControl3D flags and send them */
void control3d_add(uint32_t flags) {
  gaddr top = g_control3d_top;
  U32_AT(top) |= flags;
  njControl3D(U32_AT(top));
}
HW_EXPORT(8c040860, control3d_add(ARG0))

/* was func_8C040874: a jump to njSetConstantAttr */
HW_EXPORT(8c040874, njSetConstantAttr(ARG0, ARG1))

/* was func_8C040886 / func_8C04089A / func_8C0408AE: a CourseEnv's camera eye / its Vec3s at 0x20 / 0x2C = v (block
 * copy) */
HW_EXPORT(8c040886, vec3_copy_bytes(member_addr(ARG0, CourseEnv, eye), ARG1))
HW_EXPORT(8c04089a, vec3_copy_bytes(member_addr(ARG0, CourseEnv, vec20), ARG1))
HW_EXPORT(8c0408ae, vec3_copy_bytes(member_addr(ARG0, CourseEnv, vec2c), ARG1))

/* was func_8C04094E: the course cell under view i's camera (-1 = the current view) */
int32_t race_scene_view_cell(gaddr scene, int32_t view) {
  RaceScene *s = race_scene_at(scene);
  int32_t i = view == -1 ? view_current_index() : view;
  return s->view_cell[i];
}
HW_EXPORT(8c04094e, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* ---- the current view's lights ---- */

/* was func_8C042388: the current view's light intensities = v */
void view_light_intensity_set(gaddr v) {
  view_at(g_cur_view)->light_intensity.x = vec3_at(v)->x;
  view_at(g_cur_view)->light_intensity.y = vec3_at(v)->y;
  view_at(g_cur_view)->light_intensity.z = vec3_at(v)->z;
}
HW_EXPORT(8c042388, view_light_intensity_set(ARG0))

/* was func_8C0423A8: the current view's light colour = v */
void view_light_color_set(gaddr v) {
  view_at(g_cur_view)->light_color.x = vec3_at(v)->x;
  view_at(g_cur_view)->light_color.y = vec3_at(v)->y;
  view_at(g_cur_view)->light_color.z = vec3_at(v)->z;
}
HW_EXPORT(8c0423a8, view_light_color_set(ARG0))

/* was func_8C042400: the current view's light direction = v (block copy) */
void view_light_dir_set(gaddr v) { vec3_copy_bytes(member_addr(g_cur_view, View, light_dir), v); }
HW_EXPORT(8c042400, view_light_dir_set(ARG0))

/* ---- a view's fog and lights over the course ---- */



/* was func_8C042E28: follow the course's fog track: the first fog key past the camera's cell gives the mode; in
 * modes 0 and 1 the fog is on where the ground's attribute has bit 6 or 7, in modes 2 and 3 while the key's height
 * is above the camera; the result goes to fog_on and the player's View */
void course_env_update_fog(gaddr env) {
  CourseEnv *e = course_env_at(env);
  const FogKey *k = (const FogKey *)hw_ptr(e->fog_track);
  while (e->cell >= k->cell) k++;
  e->fog_mode = k->mode;
  switch (e->fog_mode) {
  case 0:
  case 1: {
    int32_t cell = e->cell;
    uint16_t attribute = (uint16_t)race_scene_cell_attr(panel_get(), cell);
    if (attribute & 0xC0) {
      e->fog_on = 1;
      view_fog_on(e->view);
    } else {
      e->fog_on = 0;
      view_fog_off(e->view);
    }
    break;
  }
  case 2:
  case 3:
    if (fgt(k->height, e->eye.y)) {
      e->fog_on = 1;
      view_fog_on(e->view);
    } else {
      e->fog_on = 0;
      view_fog_off(e->view);
    }
    break;
  default:
    break;
  }
}
HW_EXPORT(8c042e28, course_env_update_fog(ARG0))


/* was func_8C042EF2: set the player's View's lights, then the light rig's three Bodies at the three points of the
 * light track's first key, with damping 0.5 */
void course_env_init_lights(gaddr env, gaddr track) {
  CourseEnv *e = course_env_at(env);
  int32_t view = e->view;
  view_set_light_intensity(F32(1.0f), F32(0.75f), F32_BITS(0x3ECCCCCCu), view); /* 0.39999998 */
  view_set_light_color(F32(1.0f), F32(1.0f), F32(1.0f), e->view);
  view_set_light_dir(F32(0.0f), F32_BITS(0xBF333333u), F32_BITS(0xBF333333u), e->view); /* -0.7 */
  e->light_track = track;
  for (uint32_t k = 0; k < 3; k++) {
    const Vec3 *p = &((const LightKey *)hw_ptr(e->light_track))->points[k];
    vec3_set(p->x, p->y, p->z, member_addr(env, CourseEnv, lights[k])); /* func_8C0424C2: a copy of vec3_set */
  }
  vec3_fill(member_addr(env, CourseEnv, lights[0].damping), F32(0.5f));
  vec3_fill(member_addr(env, CourseEnv, lights[1].damping), F32(0.5f));
  vec3_fill(member_addr(env, CourseEnv, lights[2].damping), F32(0.5f));
}
HW_EXPORT(8c042ef2, course_env_init_lights(ARG0, ARG1))

/* was func_8C042FBE: move the light rig's three Bodies toward the points of the first light key past the camera's
 * cell: rates 0.02, 0.015, 0.02, then one step each (the targets are made with the Vec3 setter on the guest stack) */
void course_env_update_lights(gaddr env) {
  CourseEnv *e = course_env_at(env);
  const LightKey *k = (const LightKey *)hw_ptr(e->light_track);
  while (e->cell >= k->cell) k++;
  const f32 rate = F32_BITS(0x3CA3D70Au); /* 0.02 */
  gaddr t = hw_stack_alloc(3 * sizeof(Vec3));
  gaddr t0 = t, t1 = t + 12, t2 = t + 24;
  vec3_set(k->points[0].x, k->points[0].y, k->points[0].z, t2);
  body_pull(rate, member_addr(env, CourseEnv, lights[0]), t2);
  vec3_set(k->points[1].x, k->points[1].y, k->points[1].z, t1);
  body_pull(F32_BITS(0x3C75C28Fu), member_addr(env, CourseEnv, lights[1]), t1); /* 0.015 */
  vec3_set(k->points[2].x, k->points[2].y, k->points[2].z, t0);
  body_pull(rate, member_addr(env, CourseEnv, lights[2]), t0);
  hw_stack_free(3 * sizeof(Vec3));
  body_step(member_addr(env, CourseEnv, lights[0]));
  body_step(member_addr(env, CourseEnv, lights[1]));
  body_step(member_addr(env, CourseEnv, lights[2]));
}
HW_EXPORT(8c042fbe, course_env_update_lights(ARG0))

/* njColorBlendingMode (Ninja): the source (0) / destination (1) blend mode */
static inline void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* was func_8C04381A: draw the CourseEnv's weather particles (particle_count Bodies, a texture animation number
 * each) as 3D sprites of the shared texture, additive when the weather is 2 */
void course_env_draw_particles(gaddr env) {
  const CourseEnv *e = course_env_at(env);
  gaddr s = hw_stack_alloc(sizeof(NjSprite));
  NjSprite *sp = (NjSprite *)hw_ptr(s);
  sp->tlist = 0x8C09703Cu;
  sp->tanim = 0x8C097044u;
  sp->ang = 0;
  sp->sx = F32(1.0f);
  sp->sy = F32(1.0f);
  sp->p.x = sp->p.y = sp->p.z = F32(0.0f);
  if (e->weather == 2) {
    njColorBlendingMode(0, 9);
    njColorBlendingMode(1, 10);
  }
  gaddr item = e->particles;
  gaddr anim = e->particle_anims;
  for (int32_t i = 0; i < e->particle_count; i++, item += sizeof(Body)) {
    sp->p.x = field_get_f32(item, offsetof(Body, position.x));
    sp->p.y = field_get_f32(item, offsetof(Body, position.y));
    sp->p.z = field_get_f32(item, offsetof(Body, position.z));
    njDrawSprite3D(s, (int32_t)U32_AT(anim), 0x30);
    anim += 4;
  }
  hw_stack_free(sizeof(NjSprite));
}
HW_EXPORT(8c04381a, course_env_draw_particles(ARG0))
