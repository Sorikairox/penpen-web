/* Pen Pen port, S18 b04: small methods of the scenery units: the wind actor's contact response (type 1), the
 * Burst's draw, a unit's copy of the constant-material setter and of race_scene_view_cell, and a Vec3 blend. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/view.h"
#include "../core/motion_frame.h"
#include "actor.h"
#include "actor_b05.h"     /* NjArgb */
#include "race_scene.h"

/* njColorBlendingMode (Ninja): the blending factor `mode` for the source (0) or the destination (1) */
static inline void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* was func_8C042D04: blend: out is scaled by (1 - s) and from * s added to that in a local, s = (t + 1) / 2 (t from
 * -1 to 1); out keeps only the scaling, the local is lost: the original returns its (stack) address */
gaddr vec3_blend_lost(f32 t, gaddr from, gaddr out) {
  gaddr frame = hw_stack_alloc(0x14); /* the original's frame: out at 0, the local Vec3 at 4 */
  gaddr local = frame + 4;
  U32_AT(frame) = out;
  f32 s = fmul(fadd(t, F32(1.0f)), F32(0.5f));
  Vec3 *v = vec3_at(local);
  *v = *vec3_at(from);
  v->x = fmul(v->x, s);
  v->y = fmul(v->y, s);
  v->z = fmul(v->z, s);
  vec3_scale(fsub(F32(1.0f), s), out);
  vec3_add(local, out);
  hw_stack_free(0x14);
  return local;
}
/* r4 is not read (a method's `this`); `from` is r5 and `out` r6 (S18: exported with r4 / r5 first: a synthetic
 * sample with distinct registers showed it) */
HW_EXPORT(8c042d04, RET(vec3_blend_lost(FARG0, ARG1, ARG2)))

/* was func_8C0455B0: type 1's contact response (Contacts.respond) to contact i with `body`: the wind actor (the
 * contact's word at 0x4C) pushes the body by its wind when the body is in front of both its first and its second
 * direction pair (dir[0] facing dir[1], dir[2] facing dir[3]) */
void actor_type01_respond(gaddr contacts, int32_t i, gaddr body) {
  gaddr owner = ((const gaddr *)hw_ptr(contacts_at(contacts)->unknown4c))[i];
  gaddr frame = hw_stack_alloc(0x18); /* the original's frame: two Vec3 */
  vec3_diff(frame + 12, member_addr(owner, ActorType01, dir[0]), body);
  vec3_diff(frame, member_addr(owner, ActorType01, dir[2]), body);
  if (!fgt(F32(0.0f), njInnerProduct(frame + 12, member_addr(owner, ActorType01, dir[1]))) &&
      !fgt(F32(0.0f), njInnerProduct(frame, member_addr(owner, ActorType01, dir[3]))))
    vec3_add(member_addr(body, Body, force), member_addr(owner, ActorType01, push));
  hw_stack_free(0x18);
}
HW_EXPORT(8c0455b0, actor_type01_respond(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C046BA4: this unit's copy of race_scene_view_cell (game/camera.c) */
HW_EXPORT(8c046ba4, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* was func_8C0482A0: draw a Burst's particles while they fly (state 1): additive-like blending (source 8,
 * destination 6), each live particle as its sprite (the list's next number, or sprite_n) at its Body, size 1 */
void burst_draw(gaddr obj) {
  Burst *b = (Burst *)hw_ptr(obj);
  if (b->state != 1) return;
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  b->sprite.tlist = b->texlist;
  b->sprite.ang = 0;
  b->sprite.tanim = b->texture;
  b->sprite.sx = F32(1.0f);
  b->sprite.sy = F32(1.0f);
  gaddr list = 0;
  if (b->sprite_list != 0) list = b->sprite_list;
  for (int32_t i = 0; i < b->count; i++) {
    if (((const int32_t *)hw_ptr(b->alive))[i] == 0) continue;
    const Body *body = &((const Body *)hw_ptr(b->bodies))[i];
    b->sprite.p.x = body->position.x;
    b->sprite.p.y = body->position.y;
    b->sprite.p.z = body->position.z;
    int32_t n = b->sprite_n;
    if (b->sprite_list != 0) {
      n = (int32_t)U32_AT(list);
      list += 4;
    }
    njDrawSprite3D(member_addr(obj, Burst, sprite), n, 0x30);
  }
}
HW_EXPORT(8c0482a0, burst_draw(ARG0))

#define MATERIAL_DEFAULT_AC6A 0x8C098F18u /* the unit's default NJS_ARGB */

/* was func_8C04AC6A (its code runs on through func_8C04AC88, a fragment): njSetConstantMaterial of {a, r, g, b} =
 * the four floats (r, g, b, a): a local NJS_ARGB first copied from the unit's default, then filled (this unit's
 * copy of material_set_e028, game/actor_type28.c, with its own default) */
void material_set_ac6a(f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(20); /* the original's local, 16 bytes below its saved PR */
  memmove(hw_ptr(argb), hw_ptr(MATERIAL_DEFAULT_AC6A), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(20);
}
HW_EXPORT(8c04ac6a, material_set_ac6a(FARG0, FARG1, FARG2, FARG3))

/* nj_ModelL3 (Ninja): draw a chunk model (another light mode than nj_ModelL1) */
static inline void nj_ModelL3(gaddr model) { CALL1(0x8C0D1996u, model); }

/* was func_8C0493DC: type 0x1C's draw in the other light mode (L3): as type1c_draw (game/actor_misc.c) */
void type1c_draw_l3(gaddr obj) {
  const ActorType1C *a = (const ActorType1C *)hw_ptr(obj);
  njSetTexture(a->texlist);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, ActorType1C, position));
  njRotateZ(0, f32_to_int(a->angle.z));
  njRotateX(0, f32_to_int(a->angle.x));
  njRotateY(0, f32_to_int(a->angle.y));
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  if (e->shape != 0)
    shape_motion_draw_l3(a->object, e->motion, e->shape, member_addr(obj, ActorType1C, frame), 1);
  else if (e->motion != 0)
    motion_draw_l3(a->object, e->motion, member_addr(obj, ActorType1C, frame), 1);
  else
    nj_ModelL3(a->object);
  njPopMatrix(1);
}
HW_EXPORT(8c0493dc, type1c_draw_l3(ARG0))

/* was func_8C04AA28: a contact response (Contacts.respond; by its address type 0x2B's unit) to contact i with
 * `body`: pushed out along the contact's normal by its depth, its velocity multiplied by the normal component by
 * component */
void contact_respond_slide(gaddr contacts, int32_t i, gaddr body) {
  const Contacts *c = contacts_at(contacts);
  gaddr normal = c->normal + (gaddr)(i * (int32_t)sizeof(Vec3));
  gaddr frame = hw_stack_alloc(0x20); /* the original's frame: the push at 0 */
  vec3_scaled(((const f32 *)hw_ptr(contacts_at(contacts)->depth))[i], frame, normal);
  vec3_add(body, frame);
  vec3_mul(frame, normal, member_addr(body, Body, velocity));
  vec3_copy_bytes(member_addr(body, Body, velocity), frame);
  hw_stack_free(0x20);
}
HW_EXPORT(8c04aa28, contact_respond_slide(ARG0, (int32_t)ARG1, ARG2))

/* nj_ShapeMotionL1b: the blend of two motions and two shape motions (links) at `rate` */
static inline void nj_ShapeMotionL1b(gaddr object, gaddr motion_link, gaddr shape_link, f32 rate) {
  FARGS1(rate);
  CALL3(0x8C0C94CAu, object, motion_link, shape_link);
}

/* was func_8C04A8FC: type 0x2B's draw: at its body, turned about z, y, x by values[5], [4], [3], scaled by
 * values[0..2]; while two motions blend, the blend at blend_time / blend_length, else the shape motion of its motion
 * entry at its frame */
void actor_type2b_draw(gaddr obj) {
  const ActorType2B *a = (const ActorType2B *)hw_ptr(obj);
  njSetTexture(a->texlist);
  matrix_push();
  matrix_translate_v(member_addr(obj, ActorType2B, body));
  matrix_rotate_z(a->values[5].value);
  matrix_rotate_y(a->values[4].value);
  matrix_rotate_x(a->values[3].value);
  f32 sz = a->values[2].value;
  f32 sy = a->values[1].value;
  matrix_scale(a->values[0].value, sy, sz);
  if (a->blending != 0) {
    nj_ShapeMotionL1b(a->object, member_addr(obj, ActorType2B, blend_motion), member_addr(obj, ActorType2B, blend_word),
                      fdiv(a->blend_time, a->blend_length));
  } else {
    const Type2BMotion *m = &((const Type2BMotion *)hw_ptr(a->motions))[a->motion_index];
    shape_motion_draw_l1(a->object, m->motion, m->word4, member_addr(obj, ActorType2B, frame), 1);
  }
  matrix_pop();
}
HW_EXPORT(8c04a8fc, actor_type2b_draw(ARG0))

/* was func_8C045E20: type 2's draw: while its state is below 3 its model at the body, turned about y then x by the
 * placement's angles; then its Burst (a tail call) */
void actor_type02_draw(gaddr obj) {
  const ActorType02 *a = (const ActorType02 *)hw_ptr(obj);
  if (a->state < 3) {
    njPushMatrix(0);
    njTranslateV(0, member_addr(obj, ActorType02, body));
    njRotateY(0, f32_to_int(a->angle_y));
    njRotateX(0, f32_to_int(a->angle_x));
    nj_ModelL1(a->model);
    njPopMatrix(1);
  }
  burst_draw(a->burst);
}
HW_EXPORT(8c045e20, actor_type02_draw(ARG0))

/* was func_8C053A1C: the flap's contact response (FLAP_RESPOND, game/actor_bumper.c) to contact i with `body`:
 * pushed out along the contact's normal by its depth, then flung by 1.852 along (0, sin 10, cos 10 degrees), its z
 * turned to the normal's side */
void flap_respond(gaddr contacts, int32_t i, gaddr body) {
  const Contacts *c = contacts_at(contacts);
  gaddr normal = c->normal + (gaddr)(i * (int32_t)sizeof(Vec3));
  gaddr frame = hw_stack_alloc(0x2C); /* the original's frame: Vec3s at 0 and 12 */
  gaddr push = frame + 12, fling = frame;
  vec3_scaled(((const f32 *)hw_ptr(contacts_at(contacts)->depth))[i], push, normal);
  vec3_add(body, push);
  vec3_set(F32(0.0f), F32_BITS(0x3E31C432u) /* 0.17365 */, F32_BITS(0x3F7C1BDAu) /* 0.98481 */, fling);
  if (fgt(F32(0.0f), vec3_at(normal)->z)) vec3_at(fling)->z = fneg(vec3_at(fling)->z);
  vec3_scaled(F32_BITS(0x3FED097Au) /* 1.852 */, push, fling);
  vec3_add(member_addr(body, Body, velocity), push);
  hw_stack_free(0x2C);
}
HW_EXPORT(8c053a1c, flap_respond(ARG0, (int32_t)ARG1, ARG2))
