/* Pen Pen port, S17 b06: more of the scenery actor of placement type 0x19 (game/actor.h: ActorType19): its
 * destructors, its draw and its two half-cosine paths; and the wobbler's (type 0x33) destructors and draw; with
 * the small copies their unit keeps. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"
#include "actor_b06.h"

/* ---- this unit's copies ---- */

/* was func_8C050164: a / b */
f32 f32_quotient(f32 a, f32 b) { return fdiv(a, b); }
HW_EXPORT(8c050164, RETF(f32_quotient(FARG0, FARG1)))

/* was func_8C050466: this unit's copy of race_scene_view_cell (game/camera.c) */
HW_EXPORT(8c050466, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* the model drawn with the shape motion of its motion entry, looping */
static void model_draw_entry(gaddr object, gaddr motions, int32_t index, gaddr frame) {
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(motions))[index];
  shape_motion_draw_l1(object, e->motion, e->shape, frame, 1);
}

/* ---- type 0x33: the wobbler ---- */

/* was func_8C04FAD0: the wobbler's destructor body: its contacts freed */
void wobbler_destroy(gaddr obj) { contacts_free(member_addr(obj, Wobbler, contacts)); }
HW_EXPORT(8c04fad0, wobbler_destroy(ARG0))

/* was func_8C04FAE6: the deleting destructor */
gaddr wobbler_delete(gaddr obj, int32_t free_memory) {
  wobbler_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04fae6, RET(wobbler_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04FF80: draw the wobbler: its texture, its model at the body's position turned about z, y, then x by
 * its angles and scaled by its three wobbles' values, with its motion entry's shape motion at the clock's frame */
void wobbler_draw(gaddr obj) {
  const Wobbler *w = (const Wobbler *)hw_ptr(obj);
  njSetTexture(w->texlist);
  matrix_push();
  matrix_translate_v(member_addr(obj, Wobbler, body.position));
  matrix_rotate_z(w->angle.z);
  matrix_rotate_y(w->angle.y);
  matrix_rotate_x(w->angle.x);
  matrix_scale(w->wobble[0].value, w->wobble[1].value, w->wobble[2].value);
  model_draw_entry(w->object, w->motions, w->motion, member_addr(obj, Wobbler, clock));
  matrix_pop();
}
HW_EXPORT(8c04ff80, wobbler_draw(ARG0))

/* ---- type 0x19 ---- */

/* was func_8C050558: the destructor body: its contacts freed */
void actor_type19_destroy(gaddr obj) { contacts_free(member_addr(obj, ActorType19, contacts)); }
HW_EXPORT(8c050558, actor_type19_destroy(ARG0))

/* was func_8C05056E: the deleting destructor */
gaddr actor_type19_delete(gaddr obj, int32_t free_memory) {
  actor_type19_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05056e, RET(actor_type19_delete(ARG0, (int32_t)ARG1)))

/* was func_8C050592: the destructor body, returning the object */
gaddr actor_type19_destroy_self(gaddr obj) {
  actor_type19_destroy(obj);
  return obj;
}
HW_EXPORT(8c050592, RET(actor_type19_destroy_self(ARG0)))

/* was func_8C050B18: draw it unless it waits: its texture, its model at the body's position turned about z, y, then
 * x by the turns' first values, with the shape motion of motion entry `slow` at the clock's frame */
void actor_type19_draw(gaddr obj) {
  const ActorType19 *a = (const ActorType19 *)hw_ptr(obj);
  if (a->waiting != 0) return;
  njSetTexture(a->texlist);
  matrix_push();
  matrix_translate_v(member_addr(obj, ActorType19, body.position));
  matrix_rotate_z(a->turns[2].x);
  matrix_rotate_y(a->turns[1].x);
  matrix_rotate_x(a->turns[0].x);
  model_draw_entry(a->object, a->motions, a->slow, member_addr(obj, ActorType19, clock));
  matrix_pop();
}
HW_EXPORT(8c050b18, actor_type19_draw(ARG0))

/* was func_8C051A42: start both paths: each half-way vector (to - from) / 2, t at 0 and its step 0x8000 over the
 * length */
void actor_type19_path_start(gaddr obj) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  const f32 half = F32(0.5f);
  a->path_half.x = fmul(fsub(a->path_to.x, a->path_from.x), half);
  a->path_half.y = fmul(fsub(a->path_to.y, a->path_from.y), half);
  a->path_half.z = fmul(fsub(a->path_to.z, a->path_from.z), half);
  a->path_b_half.x = fmul(fsub(a->path_b_to.x, a->path_b_from.x), half);
  a->path_b_half.y = fmul(fsub(a->path_b_to.y, a->path_b_from.y), half);
  a->path_b_half.z = fmul(fsub(a->path_b_to.z, a->path_b_from.z), half);
  a->path_t = F32(0.0f);
  a->path_step = fdiv(F32(32768.0f), a->path_length);
}
HW_EXPORT(8c051a42, actor_type19_path_start(ARG0))

/* one component of a path's point: from + half - cos(t) half */
static f32 path_component(f32 cos_t, f32 from, f32 half) { return fsub(fadd(from, half), fmul(cos_t, half)); }

/* was func_8C051ADC: step both paths: while t is below 0x8000 (half a turn) each point moves along its half cosine
 * and t advances (returns 0); after that both points are their ends, t stays at 0x8000 with no step (returns 1) */
int32_t actor_type19_path_step(gaddr obj) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  const f32 end = F32(32768.0f);
  if (fgt(end, a->path_t)) {
    f32 c = njCos(f32_to_int(a->path_t));
    a->point14c.x = path_component(c, a->path_from.x, a->path_half.x);
    a->point14c.y = path_component(c, a->path_from.y, a->path_half.y);
    a->point14c.z = path_component(c, a->path_from.z, a->path_half.z);
    a->point17c.x = path_component(c, a->path_b_from.x, a->path_b_half.x);
    a->point17c.y = path_component(c, a->path_b_from.y, a->path_b_half.y);
    a->point17c.z = path_component(c, a->path_b_from.z, a->path_b_half.z);
    a->path_t = fadd(a->path_t, a->path_step);
    return 0;
  }
  a->path_t = end;
  a->path_step = F32(0.0f);
  a->point14c.x = a->path_to.x;
  a->point14c.y = a->path_to.y;
  a->point14c.z = a->path_to.z;
  a->point17c.x = a->path_b_to.x;
  a->point17c.y = a->path_b_to.y;
  a->point17c.z = a->path_b_to.z;
  return 1;
}
HW_EXPORT(8c051adc, RET(actor_type19_path_step(ARG0)))
