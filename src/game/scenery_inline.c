/* Pen Pen port, S17 b05: small inline methods of the race's classes that SHC emitted in the scenery units (each
 * unit keeps its own copy): the CourseEnv's push setter, the Burst's and the Body's, the race scene's course
 * accessors, two colour/vector helpers and the static initialiser of the motion draw's attach matrices. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "game.h"
#include "course.h"
#include "race_scene.h"
#include "actor.h"
#include "scenery.h"
#include "actor_b05.h"

/* ---- CourseEnv ---- */

/* was func_8C03C5E8, func_8C03DA38, func_8C0408C6, func_8C0425D4, func_8C043CD2, func_8C044A08, func_8C044E44,
 * func_8C0457A8, func_8C045F92, func_8C046666, func_8C046B1C, func_8C047506, func_8C04783C, func_8C0483EE,
 * func_8C04873E, func_8C048B66, func_8C04921A, func_8C049B1A, func_8C04AE82, func_8C0165E8 (S17 b01): the env's push
 * = v (a block copy) */
void course_env_set_push(gaddr env, gaddr v) {
  memmove(&course_env_at(env)->push, hw_ptr(v), sizeof(Vec3));
}
HW_EXPORT(8c0165e8, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c03c5e8, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c03da38, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c0408c6, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c0425d4, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c043cd2, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c044a08, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c044e44, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c0457a8, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c045f92, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c046666, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c046b1c, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c047506, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c04783c, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c0483ee, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c04873e, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c048b66, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c04921a, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c049b1a, course_env_set_push(ARG0, ARG1))
HW_EXPORT(8c04ae82, course_env_set_push(ARG0, ARG1))

/* ---- Burst (game/actor.h) ---- */

/* was func_8C045788: the burst's origin = v (a block copy) */
void burst_set_origin(gaddr burst, gaddr v) {
  memmove(&((Burst *)hw_ptr(burst))->origin, hw_ptr(v), sizeof(Vec3));
}
HW_EXPORT(8c045788, burst_set_origin(ARG0, ARG1))

/* was func_8C04821A: the burst idle and not triggered */
void burst_clear(gaddr burst) {
  Burst *b = (Burst *)hw_ptr(burst);
  b->state = 0;
  b->trigger = 0;
}
HW_EXPORT(8c04821a, burst_clear(ARG0))

/* was func_8C04829A: start the burst on its next step */
void burst_trigger(gaddr burst) { ((Burst *)hw_ptr(burst))->trigger = 1; }
HW_EXPORT(8c04829a, burst_trigger(ARG0))

/* ---- Body (core/vec.h) ---- */

/* was func_8C0456EC and func_8C046A56: the body's vertical velocity */
f32 body_velocity_y(gaddr body) { return body_at(body)->velocity.y; }
HW_EXPORT(8c0456ec, RETF(body_velocity_y(ARG0)))
HW_EXPORT(8c046a56, RETF(body_velocity_y(ARG0)))

/* was func_8C038912: the body's damping in y = d */
void body_set_damping_y(gaddr body, f32 d) { body_at(body)->damping.y = d; }
HW_EXPORT(8c038912, body_set_damping_y(ARG0, FARG0))

/* ---- vectors and colours ---- */

/* was func_8C048A64: c = (r, g, b, a), alpha stored first */
void argb_set(f32 r, f32 g, f32 b, f32 a, gaddr c) {
  NjArgb *p = (NjArgb *)hw_ptr(c);
  p->a = a;
  p->r = r;
  p->g = g;
  p->b = b;
}
HW_EXPORT(8c048a64, argb_set(FARG0, FARG1, FARG2, FARG3, ARG0))

/* was func_8C0497FC: out = a * b component by component (both read before out is written; z, y, x stored in that
 * order) */
void vec3_mul(gaddr out, gaddr a, gaddr b) {
  const Vec3 *u = vec3_at(a), *v = vec3_at(b);
  f32 x = fmul(u->x, v->x);
  f32 y = fmul(u->y, v->y);
  f32 z = fmul(u->z, v->z);
  Vec3 *o = vec3_at(out);
  o->z = z;
  o->y = y;
  o->x = x;
}
HW_EXPORT(8c0497fc, vec3_mul(ARG0, ARG1, ARG2))

/* ---- two objects drawn one after the other ---- */

typedef struct ObjectPair {
  gaddr first;   /* 0x00: NJS_OBJECT */
  gaddr second;  /* 0x04: NJS_OBJECT */
} ObjectPair;
HW_SIZE(ObjectPair, 8);

/* func_8C038A40: draw an object tree (a thunk to nj_ObjectB, core/ninja_thunks.c) */
static void object_draw(gaddr object) { CALL1(0x8C038A40u, object); }

/* was func_8C03ADB8: draw both objects of the pair (the pair read again after the first draw) */
void object_pair_draw(gaddr pair) {
  object_draw(((const ObjectPair *)hw_ptr(pair))->first);
  object_draw(((const ObjectPair *)hw_ptr(pair))->second);
}
HW_EXPORT(8c03adb8, object_pair_draw(ARG0))

/* ---- the race scene's course ---- */

/* was func_8C03DB56: the course's static words (Course.course_words) */
gaddr race_scene_course_words(gaddr scene) {
  return member_addr(race_scene_at(scene)->course, Course, course_words);
}
HW_EXPORT(8c03db56, RET(race_scene_course_words(ARG0)))

/* was func_8C03C67A: the scene's course value at the course's finish segment (race_scene_start_value's
 * counterpart) */
f32 race_scene_finish_value(gaddr scene) {
  gaddr course = race_scene_at(scene)->course;
  return course_value(course, course_at(course)->finish_segment);
}
HW_EXPORT(8c03c67a, RETF(race_scene_finish_value(ARG0)))

/* was func_8C042648: the course's variant (0, 1 or 2) */
int32_t race_scene_course_variant(gaddr scene) { return course_at(race_scene_at(scene)->course)->variant; }
HW_EXPORT(8c042648, RET(race_scene_course_variant(ARG0)))

/* was func_8C041350: the course's word28 = 1 (still at most 1: its next update sets the CourseEnvs up,
 * course_update_views) */
void course_set_word28_one(gaddr course) { course_at(course)->word28 = 1; }
HW_EXPORT(8c041350, course_set_word28_one(ARG0))

/* was func_8C0417C0: the course row bit `bit` of the current view (course_row_bit) */
int32_t course_current_row_bit(gaddr course, int32_t bit) {
  return course_row_bit(course, g_cur_view_index, bit);
}
HW_EXPORT(8c0417c0, RET(course_current_row_bit(ARG0, (int32_t)ARG1)))

/* ---- views (core/view.h) ---- */

/* was func_8C04241E: fog on for view i (view_fog_on, core/view.c, where its export is commented out) */
HW_EXPORT(8c04241e, view_fog_on((int32_t)ARG0))

/* was func_8C045F6E: a copy of view_current_index */
HW_EXPORT(8c045f6e, RET(view_current_index()))

/* ---- the motion draw's attach matrices ---- */

#define ATTACH_MATRICES   0x8C1BAB18u /* four Ninja matrices (0x40 bytes) game/motion_draw.c's callback fills */
#define MATRIX_IDENTITY_CTOR 0x8C037CAEu /* a matrix's constructor: the identity (its address is data) */

/* was func_8C038750: the static initialiser of the attach matrices: four identities */
void attach_matrices_init(void) { array_construct(4, 0x40, ATTACH_MATRICES, MATRIX_IDENTITY_CTOR); }
HW_EXPORT(8c038750, attach_matrices_init())

/* was func_8C045840: a copy of race_scene_view_cell (the course cell under view i's camera; -1: the current
 * view) */
HW_EXPORT(8c045840, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* was func_8C041848: the kind of the course segment `cell` falls in: 4 past the last segment, else the segment's
 * unknown2 (sign-extended), with bit 7 set when the segment starts at that very cell */
int32_t course_segment_kind_marked(gaddr course, int32_t cell) {
  const CourseSegment *s = (const CourseSegment *)hw_ptr(course_find_segment(course, cell));
  if (s->first_cell == -1) return 4;
  int32_t kind = s->unknown2;
  if (s->first_cell == cell) return kind | 0x80;
  return kind;
}
HW_EXPORT(8c041848, RET(course_segment_kind_marked(ARG0, (int32_t)ARG1)))

/* was func_8C047FD8: the body at rest at the origin with no damping (four Vec3 sets) */
void body_rest(gaddr body) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
}
HW_EXPORT(8c047fd8, body_rest(ARG0))
