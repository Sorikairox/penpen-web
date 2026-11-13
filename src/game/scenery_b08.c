/* Pen Pen port, S17 b08: small methods of the scenery actor classes of 0x8C05A614-0x8C060284 (game/actor.h,
 * game/actor_b08.h): the deleting destructors of placement types 0x12-0x16, 0x2C and 0x2D, the destructor bodies of
 * types 0x2C and 0x2D, type 0x2D's draw, a contact handler of type 0x2C, and this unit's copies of small helpers
 * (Body resets, Vec3 setters, the view's course cell, Ninja thunks, constant-material setters). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/ninja.h"
#include "game.h"
#include "race_scene.h"
#include "actor.h"
#include "actor_b05.h"
#include "scenery.h"
#include "actor_b07.h"
#include "actor_b08.h"

/* func_8C0D02FE (nj_ObjectL1 in port/symbols/sdk_calls.tsv): draw a model of an object (the unit passes the
 * NJS_OBJECT's model) */
static inline void nj_ObjectL1(gaddr model) { CALL1(0x8C0D02FEu, model); }

/* ---- deleting destructors of the classes whose destructor body is empty (empty_delete, game/actor.c; and
 * empty_destroy_self, game/actor_lifetimes.c) ---- */

/* was func_8C05A74E / func_8C05A772: type 0x12 (the row of posts, Fence) */
HW_EXPORT(8c05a74e, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c05a772, RET(empty_destroy_self(ARG0)))
/* was func_8C05AA96 / func_8C05AABA: type 0x13 (the wave field, WaveField) */
HW_EXPORT(8c05aa96, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c05aaba, RET(empty_destroy_self(ARG0)))
/* was func_8C05B964 / func_8C05B988: type 0x15 (Swinger) */
HW_EXPORT(8c05b964, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c05b988, RET(empty_destroy_self(ARG0)))
/* was func_8C05BC80 / func_8C05BCA4: type 0x16 (Sparkles) */
HW_EXPORT(8c05bc80, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c05bca4, RET(empty_destroy_self(ARG0)))

/* ---- type 0x14: the ornament ---- */

/* its destructor body is func_8C05B1B0, a copy of flap_destroy: the contacts freed */
static void ornament_destroy(gaddr obj) { contacts_free(member_addr(obj, Ornament, contacts)); }

/* was func_8C05B1C6: the ornament's deleting destructor */
gaddr ornament_delete(gaddr obj, int32_t free_memory) {
  ornament_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05b1c6, RET(ornament_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05B1EA */
gaddr ornament_destroy_self(gaddr obj) {
  ornament_destroy(obj);
  return obj;
}
HW_EXPORT(8c05b1ea, RET(ornament_destroy_self(ARG0)))

/* ---- type 0x2C: the rings ---- */

/* was func_8C05BFE2: the destructor body: the contacts freed */
void rings_destroy(gaddr obj) { contacts_free(member_addr(obj, Rings, contacts)); }
HW_EXPORT(8c05bfe2, rings_destroy(ARG0))

/* was func_8C05BFF6 */
gaddr rings_delete(gaddr obj, int32_t free_memory) {
  rings_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05bff6, RET(rings_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05C01A */
gaddr rings_destroy_self(gaddr obj) {
  rings_destroy(obj);
  return obj;
}
HW_EXPORT(8c05c01a, RET(rings_destroy_self(ARG0)))

/* was func_8C05C424: a contact handler (Contacts.handler: self, index, a, ...): the body `body`'s velocity = the
 * Vec3 at 0xC0 of contact i's object (the Contacts' unknown4c word of the contact) */
void contact_take_other_velocity(gaddr contacts, int32_t i, gaddr body) {
  gaddr other = ((const gaddr *)hw_ptr(contacts_at(contacts)->unknown4c))[i];
  vec3_copy_bytes(member_addr(body, Body, velocity), member_addr(other, ContactOther, velocity));
}
HW_EXPORT(8c05c424, contact_take_other_velocity(ARG0, (int32_t)ARG1, ARG2))

/* ---- type 0x2D ---- */

/* was func_8C05C6C6: the destructor body: the contacts freed */
void actor_type2d_destroy(gaddr obj) { contacts_free(member_addr(obj, ActorType2D, contacts)); }
HW_EXPORT(8c05c6c6, actor_type2d_destroy(ARG0))

/* was func_8C05C6DA */
gaddr actor_type2d_delete(gaddr obj, int32_t free_memory) {
  actor_type2d_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05c6da, RET(actor_type2d_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05C6FE */
gaddr actor_type2d_destroy_self(gaddr obj) {
  actor_type2d_destroy(obj);
  return obj;
}
HW_EXPORT(8c05c6fe, RET(actor_type2d_destroy_self(ARG0)))

/* was func_8C05CA34: draw it: model 5 of the list at its position, turned about y by its turn */
void actor_type2d_draw(gaddr obj) {
  ActorType2D *a = (ActorType2D *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, ActorType2D, position));
  njRotateY(0, f32_to_int(a->turn.value));
  nj_ModelL1(actor_model(&a->base, 5));
  njPopMatrix(1);
}
HW_EXPORT(8c05ca34, actor_type2d_draw(ARG0))

/* ---- this unit's copies of small helpers ---- */

/* was func_8C05C63A, func_8C05D002, func_8C0601FA: copies of race_scene_view_cell (game/camera.c): the course cell
 * under view i's camera (-1 = the current view) */
HW_EXPORT(8c05c63a, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c05d002, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c0601fa, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* was func_8C05CE98, func_8C05EC14, func_8C060080: copies of body_reset (core/vec.c): at rest at the origin through
 * the Vec3 setters */
HW_EXPORT(8c05ce98, body_reset(ARG0))
HW_EXPORT(8c05ec14, body_reset(ARG0))
HW_EXPORT(8c060080, body_reset(ARG0))

/* was func_8C05ED9C, func_8C060230: copies of body_reset_set3 (game/actor.c): the same through the setters that
 * return the vector (the damping's address is left in r0) */
HW_EXPORT(8c05ed9c, RET(body_reset_set3(ARG0)))
HW_EXPORT(8c060230, RET(body_reset_set3(ARG0)))

/* was func_8C05EC4E, func_8C0600BA: v = (x, y, z) (copies of vec3_set; nothing left in r0) */
HW_EXPORT(8c05ec4e, vec3_set(FARG0, FARG1, FARG2, ARG0))
HW_EXPORT(8c0600ba, vec3_set(FARG0, FARG1, FARG2, ARG0))

/* was func_8C05CD46: a jump to njGetMatrix */
HW_EXPORT(8c05cd46, njGetMatrix(ARG0))

/* was func_8C05EBAE: a jump to nj_ObjectL1 */
HW_EXPORT(8c05ebae, nj_ObjectL1(ARG0))

/* was func_8C05E1F0: draw an object's model (its NJS_OBJECT.model) with nj_ObjectL1 (`self` is not read) */
void object_draw_model_l1(gaddr self, gaddr object) { nj_ObjectL1(((const NjObject *)hw_ptr(object))->model); }
HW_EXPORT(8c05e1f0, object_draw_model_l1(ARG0, ARG1))

/* njSetConstantMaterial of {a, r, g, b} = the four floats (r, g, b, a): a local NJS_ARGB first copied from the
 * unit's default (16 bytes below the saved PR, as in the original's frame), then filled */
static void material_set_from(gaddr defaults, f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(20);
  memmove(hw_ptr(argb), hw_ptr(defaults), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(20);
}

/* was func_8C05CD7E (its code runs on through func_8C05CD8C, a fragment): the constant material = (r, g, b, a), from
 * the default at 0x8C099670 */
void material_set_cd7e(f32 r, f32 g, f32 b, f32 a) { material_set_from(0x8C099670u, r, g, b, a); }
HW_EXPORT(8c05cd7e, material_set_cd7e(FARG0, FARG1, FARG2, FARG3))

/* was func_8C05FF24 (on through func_8C05FF32, a fragment): the same from the default at 0x8C0996D4 */
void material_set_ff24(f32 r, f32 g, f32 b, f32 a) { material_set_from(0x8C0996D4u, r, g, b, a); }
HW_EXPORT(8c05ff24, material_set_ff24(FARG0, FARG1, FARG2, FARG3))

/* ---- type 0x14: the ornament's draw ---- */

/* was func_8C05B7C2: draw it: its model at the body's position, turned
 * about x, y and z by its angles (the unit's matrix helpers) */
void ornament_draw_model(gaddr obj) {
  Ornament *o = (Ornament *)hw_ptr(obj);
  matrix_push();
  matrix_translate_v(method_self(member_addr(obj, Ornament, body))); /* func_8C05AFB2: the Body's position */
  matrix_rotate_x(o->angle.x);
  matrix_rotate_y(o->angle.y);
  matrix_rotate_z(o->angle.z);
  nj_ModelL1(actor_model(&o->base, o->model));
  matrix_pop();
}
HW_EXPORT(8c05b7c2, ornament_draw_model(ARG0))

/* ---- type 0x15: the swinger's draw ---- */

#define g_swinger_models ((const int32_t *)hw_ptr(0x8C089E28u)) /* per kind (0-6): its model in the list (static) */

/* was func_8C05BB14: draw it: at its position, turned about x, y and z by its angles, the swing added to the axes
 * its style's bits 0, 1, 2 pick; the model of its kind */
void swinger_draw(gaddr obj) {
  Swinger *s = (Swinger *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Swinger, position));
  f32 x = s->angle.x;
  if (s->style & 1) x = fadd(x, s->swing.value);
  njRotateX(0, f32_to_int(x));
  f32 y = s->angle.y;
  if (s->style & 2) y = fadd(y, s->swing.value);
  njRotateY(0, f32_to_int(y));
  f32 z = s->angle.z;
  if (s->style & 4) z = fadd(z, s->swing.value);
  njRotateZ(0, f32_to_int(z));
  nj_ModelL1(actor_model(&s->base, g_swinger_models[s->kind]));
  njPopMatrix(1);
}
HW_EXPORT(8c05bb14, swinger_draw(ARG0))

/* ---- contact handlers of type 0x30 (Contacts.handler: self, index, a, ...) ---- */

#define g_push_bounce_a GUEST_VAR(f32, 0x8C08A1B4u) /* 0.46296296 */
#define g_push_bounce_b GUEST_VAR(f32, 0x8C08A1B8u) /* 0.92592591 */

/* push the body out of contact i: its position moved along the contact's normal by the depth, its velocity given the
 * normal times `bounce`; the step vector is a local of the original's frame (0x0C above its bottom, the frame under the saved
 * r14 and PR) */
static void contact_push_out(gaddr contacts, int32_t i, gaddr body, f32 bounce) {
  const Contacts *c = contacts_at(contacts);
  gaddr normal = c->normal + (uint32_t)i * sizeof(Vec3);
  gaddr frame = hw_stack_alloc(0x20); /* the frame below the saved r14 and PR */
  gaddr step = frame + 0x0C;
  method_self(step); /* func_8C05CBD0: the Vec3's (empty) constructor */
  vec3_scaled(((const f32 *)hw_ptr(c->depth))[i], step, normal);
  vec3_add(body, step);
  vec3_scaled(bounce, step, normal);
  vec3_add(member_addr(body, Body, velocity), step);
  hw_stack_free(0x20);
}

/* was func_8C05E920: push out with a soft bounce (0.463) */
void contact_push_out_soft(gaddr contacts, int32_t i, gaddr body) {
  contact_push_out(contacts, i, body, g_push_bounce_a);
}
HW_EXPORT(8c05e920, contact_push_out_soft(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C05E986: push out with a hard bounce (0.926) */
void contact_push_out_hard(gaddr contacts, int32_t i, gaddr body) {
  contact_push_out(contacts, i, body, g_push_bounce_b);
}
HW_EXPORT(8c05e986, contact_push_out_hard(ARG0, (int32_t)ARG1, ARG2))
