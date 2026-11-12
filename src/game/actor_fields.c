/* Pen Pen port, S14 b11: scenery laid out from static tables (game/actor.h): the row of 53 posts (type 0x12) and
 * the wave field (type 0x13). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "course.h"
#include "race_scene.h"
#include "scenery.h"

/* func_8C041A7C: the course cell a position is on (searched from the start) */
static int32_t course_cell_at(gaddr course, gaddr pos) { return (int32_t)CALL2(0x8C041A7Cu, course, pos); }

/* ---- type 0x12: the row of posts ---- */

#define g_fence_positions ((const Vec3 *)hw_ptr(0x8C088F88u)) /* per post (static) */
#define g_fence_angles    ((const Vec3 *)hw_ptr(0x8C089204u)) /* per post: about x, y, z (static) */

/* was func_8C05A72E: type 0x12's constructor: both vtables, then the Actor */
gaddr fence_construct(gaddr obj) {
  Fence *f = (Fence *)hw_ptr(obj);
  f->base.vtable = 0x8C23E5FCu;
  f->second_vtable = 0x8C23E61Cu;
  actor_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05a72e, RET(fence_construct(ARG0)))

/* was func_8C05A7AC: set the row up: the placement's position and param, flags 1, and each post's course cell */
void fence_init(gaddr obj, gaddr placement) {
  Fence *f = (Fence *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  f->position = pl->position;
  f->param = pl->param;
  f->base.flags = 1;
  gaddr pos = hw_stack_alloc(0x18); /* the original's frame: the post's position at its bottom */
  for (int32_t k = 0; k < FENCE_POSTS; k++) {
    *vec3_at(pos) = g_fence_positions[k];
    f->cells[k] = course_cell_at(race_scene_at(f->base.scene)->course, pos);
  }
  hw_stack_free(0x18);
}
HW_EXPORT(8c05a7ac, fence_init(ARG0, ARG1))

/* was func_8C05A834: draw the posts the current view's course row shows: each at its place, turned x, y, z; posts
 * 0-11 are model 12, 12-30 model 3, the rest model 27; posts 2 to 28 scaled by (1.6643, 1.6732, 1.6732) */
void fence_draw(gaddr obj) {
  const Fence *f = (const Fence *)hw_ptr(obj);
  for (int32_t k = 0; k < FENCE_POSTS; k++) {
    int32_t cell = f->cells[k];
    gaddr course = race_scene_at(f->base.scene)->course;
    if (!course_row_bit(course, view_current_index(), cell)) continue;
    const Vec3 *p = &g_fence_positions[k];
    const Vec3 *a = &g_fence_angles[k];
    njPushMatrix(0);
    njTranslate(0, p->x, p->y, p->z);
    njRotateX(0, f32_to_int(a->x));
    njRotateY(0, f32_to_int(a->y));
    njRotateZ(0, f32_to_int(a->z));
    int32_t model = k < 12 ? 12 : 3;
    if (k >= 31) model = 27;
    if (k >= 2 && k <= 28) {
      const f32 s = F32_BITS(0x3FD62B6Au); /* 1.6732 */
      njScale(0, F32_BITS(0x3FD507C8u) /* 1.6643 */, s, s);
    }
    nj_ModelL1(actor_model(&f->base, model));
    njPopMatrix(1);
  }
}
HW_EXPORT(8c05a834, fence_draw(ARG0))

/* ---- type 0x13: the wave field ---- */

#define WAVE_CONSTRUCTOR 0x8C05AA2Eu /* a Body1's constructor (0, 0, 0, 1) */
#define g_wave_items   ((const f32 *)hw_ptr(0x8C089480u)) /* per placed model: kind, position, angles (7 floats) */
#define g_waves_a      ((const f32 *)hw_ptr(0x8C089D78u)) /* per wave: target, rate */
#define g_waves_b      ((const f32 *)hw_ptr(0x8C089DA0u))
#define g_wave_scales  ((const f32 *)hw_ptr(0x8C089DC8u)) /* per kind: x and y scale */
#define WAVE_FIRST_MESH 15 /* the meshes are models 15-20 of the list */

/* was func_8C05AA42: the wave field's base construction: the Actor and its two sets of 5 waves */
gaddr wave_field_base_construct(gaddr obj) {
  actor_construct(obj);
  array_construct(WAVE_COUNT, sizeof(Body1), member_addr(obj, WaveField, waves_a), WAVE_CONSTRUCTOR);
  array_construct(WAVE_COUNT, sizeof(Body1), member_addr(obj, WaveField, waves_b), WAVE_CONSTRUCTOR);
  return obj;
}
HW_EXPORT(8c05aa42, RET(wave_field_base_construct(ARG0)))

/* was func_8C05AA76: type 0x13's constructor: both vtables, then the base */
gaddr wave_field_construct(gaddr obj) {
  WaveField *w = (WaveField *)hw_ptr(obj);
  w->base.vtable = 0x8C23E63Cu;
  w->second_vtable = 0x8C23E65Cu;
  wave_field_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05aa76, RET(wave_field_construct(ARG0)))

/* the first vertex of a chunk model's vertex list (after its 8-byte chunk header) */
static gaddr mesh_first_vertex(gaddr object) {
  gaddr model = ((const NjObjectPose *)hw_ptr(object))->model;
  return *(const gaddr *)hw_ptr(model) + 8;
}

/* was func_8C05AB00: set the wave field up: its 82 placed models from the table (each locating the actor to find its
 * cell), its six meshes (models 15-20) with their first vertices and rest x, the waves at rest; flags 1 */
void wave_field_init(gaddr obj) {
  WaveField *w = (WaveField *)hw_ptr(obj);
  const f32 *t = g_wave_items;
  w->count = 0;
  gaddr pos = hw_stack_alloc(sizeof(Vec3));
  for (uint32_t i = 0; i < WAVE_ITEMS; i++, t += 7) {
    WaveItem *it = &w->items[i];
    it->kind = f32_to_int(t[0]);
    it->pos.x = t[1];
    it->pos.y = t[2];
    it->pos.z = t[3];
    it->angle.x = t[4];
    it->angle.y = t[5];
    it->angle.z = t[6];
    *vec3_at(pos) = it->pos;
    actor_locate(obj, pos, 0);
    it->cell = w->base.cell;
    w->count++;
  }
  hw_stack_free(sizeof(Vec3));
  for (int32_t j = 0; j < WAVE_MESHES; j++) {
    w->meshes[j] = actor_model(&w->base, WAVE_FIRST_MESH + j);
    gaddr v = mesh_first_vertex(w->meshes[j]);
    w->vertices[j] = v;
    for (uint32_t i = 0; i < WAVE_MESH_VERTICES; i++, v += sizeof(WaveVertex))
      w->rest_x[j][i] = ((const WaveVertex *)hw_ptr(v))->x;
  }
  for (uint32_t k = 0; k < WAVE_COUNT; k++) {
    w->waves_a[k].damping = F32(1.0f);
    w->waves_a[k].velocity = F32(0.0f);
    w->waves_a[k].value = F32(0.0f);
    w->waves_b[k].damping = F32(1.0f);
    w->waves_b[k].velocity = F32(0.0f);
    w->waves_b[k].value = F32(0.0f);
  }
  w->base.flags = 1;
}
HW_EXPORT(8c05ab00, wave_field_init(ARG0))

/* was func_8C05AC58: step the waves (each pulled toward its target by its rate from the tables) and move the meshes'
 * vertices: vertex i's x = its rest x + wave (i mod 5), its z = wave (i mod 4) / 2 (meshes 0, 1 by the first set,
 * the others by the second) */
void wave_field_update(gaddr obj) {
  WaveField *w = (WaveField *)hw_ptr(obj);
  const f32 *ta = g_waves_a;
  const f32 *tb = g_waves_b;
  for (uint32_t k = 0; k < WAVE_COUNT; k++, ta += 2, tb += 2) {
    Body1 *a = &w->waves_a[k];
    a->force = fmac(ta[1], fsub(ta[0], a->value), a->force);
    body1_step(member_addr(obj, WaveField, waves_a[k]));
    Body1 *b = &w->waves_b[k];
    b->force = fmac(tb[1], fsub(tb[0], b->value), b->force);
    body1_step(member_addr(obj, WaveField, waves_b[k]));
  }
  for (int32_t j = 0; j < WAVE_MESHES; j++) {
    WaveVertex *v = (WaveVertex *)hw_ptr(w->vertices[j]);
    const Body1 *waves = j < 2 ? w->waves_a : w->waves_b;
    int32_t a = 0, b = 0;
    for (int32_t i = 0; i < WAVE_MESH_VERTICES; i++, v++) {
      v->x = fadd(waves[a].value, w->rest_x[j][i]);
      v->z = fdiv(waves[b].value, F32(2.0f));
      if (++a >= 5) a = 0;
      if (++b >= 4) b = 0;
    }
  }
}
HW_EXPORT(8c05ac58, wave_field_update(ARG0))

/* was func_8C05AD58: draw the placed models the current view's course row shows: each at its place, turned x, y, z,
 * scaled by its kind's (x, y, 1), model kind / 2 of the meshes' list */
void wave_field_draw(gaddr obj) {
  const WaveField *w = (const WaveField *)hw_ptr(obj);
  for (int32_t i = 0; i < w->count; i++) {
    const WaveItem *it = &w->items[i];
    int32_t cell = it->cell;
    gaddr course = race_scene_at(w->base.scene)->course;
    if (!course_row_bit(course, view_current_index(), cell)) continue;
    njPushMatrix(0);
    njTranslate(0, it->pos.x, it->pos.y, it->pos.z);
    njRotateX(0, f32_to_int(it->angle.x));
    njRotateY(0, f32_to_int(it->angle.y));
    njRotateZ(0, f32_to_int(it->angle.z));
    const f32 *s = &g_wave_scales[2 * it->kind];
    njScale(0, s[0], s[1], F32(1.0f));
    nj_ModelL1(w->meshes[it->kind >> 1]);
    njPopMatrix(1);
  }
}
HW_EXPORT(8c05ad58, wave_field_draw(ARG0))
