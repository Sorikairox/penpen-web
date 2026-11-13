/* Pen Pen port, S17 b06: the scenery actor of placement type 0x28 (game/actor_b06.h): its construction and
 * destruction, the reset and the step of its particle spray, and the small copies its unit keeps. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"
#include "actor_b05.h"
#include "race_scene.h"
#include "scenery.h"
#include "actor_b06.h"

#define TYPE28_VTABLE        0x8C23E0DCu
#define TYPE28_SECOND_VTABLE 0x8C23E0FCu
#define TYPE28_BODY_CONSTRUCTOR 0x8C04E35Eu /* body_reset_set3's copy below: the particles' Bodies */
#define TYPE28_BODIES        8
#define TYPE28_BLOCK         0x20u           /* bytes of each per-particle array */

/* statics of the binary the unit reads (not folded into the code) */
#define g_type28_radius_a    GUEST_VAR(f32, 0x8C0882C8u)     /* 10: the first contacts' radius */
#define g_type28_radius_b    GUEST_VAR(f32, 0x8C0882CCu)     /* 10: the second contacts' radius */
#define g_type28_particles   GUEST_VAR(int32_t, 0x8C0882D4u) /* 8: the spray's particles */
#define g_type28_value_limit GUEST_VAR(f32, 0x8C0882E0u)     /* 20: a growing particle's value stops there */
#define g_type28_spin_step   GUEST_VAR(f32, 0x8C0882F0u)     /* 0.017 */
#define g_type28_reset_word  GUEST_VAR(uint32_t, 0x8C096D10u)
#define TYPE28_STATIC_TABLE  0x8C0990E0u
#define MATERIAL_DEFAULT_E028 0x8C0990D0u /* the unit's default NJS_ARGB */

/* ---- this unit's copies ---- */

/* was func_8C04E028: njSetConstantMaterial of {a, r, g, b} = the four floats (r, g, b, a): a local NJS_ARGB first
 * copied from the unit's default, then filled (its code runs on through func_8C04E036, a fragment) */
void material_set_e028(f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(20); /* the original's local, 16 bytes below its saved PR */
  memmove(hw_ptr(argb), hw_ptr(MATERIAL_DEFAULT_E028), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(20);
}
HW_EXPORT(8c04e028, material_set_e028(FARG0, FARG1, FARG2, FARG3))

/* was func_8C04E15E: this unit's copy of body_reset (core/vec.c) */
HW_EXPORT(8c04e15e, body_reset(ARG0))

/* was func_8C04E328: this unit's copy of race_scene_view_cell (game/camera.c) */
HW_EXPORT(8c04e328, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* was func_8C04E35E: this unit's copy of body_reset_set3 (game/actor.c) */
HW_EXPORT(8c04e35e, RET(body_reset_set3(ARG0)))

/* ---- type 0x28 ---- */

/* was func_8C04E3B4: the base construction: the Actor, five Body1s at (0, 0, 0, 1), three turns at (0, 0, 0, 1), the
 * Bodies at rest, both contact sets (room for 2: the first owned by the point, the second by the third Body), and
 * the spray's arrays (a block of 0x20 bytes each, and eight Bodies from operator new[]: 0 when out of memory) */
gaddr actor_type28_base_construct(gaddr obj) {
  ActorType28 *a = (ActorType28 *)hw_ptr(obj);
  actor_construct(obj);
  for (int32_t i = 0; i < 5; i++) {
    a->pulls[i].value = F32(0.0f);
    a->pulls[i].velocity = F32(0.0f);
    a->pulls[i].force = F32(0.0f);
    a->pulls[i].damping = F32(1.0f);
  }
  quat_identity(member_addr(obj, ActorType28, turn_a));
  quat_identity(member_addr(obj, ActorType28, turn_b));
  method_self(member_addr(obj, ActorType28, point));
  body_init(member_addr(obj, ActorType28, body));
  method_self(member_addr(obj, ActorType28, point100));
  body_init(member_addr(obj, ActorType28, body_b));
  body_init(member_addr(obj, ActorType28, body_c));
  quat_identity(member_addr(obj, ActorType28, turn_c));
  contacts_construct(member_addr(obj, ActorType28, contacts), member_addr(obj, ActorType28, point),
                     g_type28_radius_a, 2);
  gaddr owner = method_self(member_addr(obj, ActorType28, body_c));
  contacts_construct(member_addr(obj, ActorType28, contacts_b), owner, g_type28_radius_b, 2);
  a->words25c = syMalloc(TYPE28_BLOCK);
  a->countdowns = syMalloc(TYPE28_BLOCK);
  a->states = syMalloc(TYPE28_BLOCK);
  gaddr block = syMalloc(TYPE28_BODIES * sizeof(Body));
  gaddr bodies = 0;
  if (block != 0) bodies = array_construct_new(TYPE28_BODIES, sizeof(Body), block, TYPE28_BODY_CONSTRUCTOR);
  a->bodies = bodies;
  a->values = syMalloc(TYPE28_BLOCK);
  a->spins = syMalloc(TYPE28_BLOCK);
  a->words270 = syMalloc(TYPE28_BLOCK);
  return obj;
}
HW_EXPORT(8c04e3b4, RET(actor_type28_base_construct(ARG0)))

/* was func_8C04E528: type 0x28's constructor: both vtables, then the base */
gaddr actor_type28_construct(gaddr obj) {
  ActorType28 *a = (ActorType28 *)hw_ptr(obj);
  a->base.vtable = TYPE28_VTABLE;
  a->second_vtable = TYPE28_SECOND_VTABLE;
  actor_type28_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04e528, RET(actor_type28_construct(ARG0)))

/* was func_8C04F21C: the spray's blocks freed (each that was allocated; the Bodies' block with syFree too) */
void actor_type28_free_blocks(gaddr obj) {
  const ActorType28 *a = (const ActorType28 *)hw_ptr(obj);
  gaddr p;
  if ((p = a->words25c) != 0) syFree(p);
  if ((p = a->countdowns) != 0) syFree(p);
  if ((p = a->states) != 0) syFree(p);
  if ((p = a->bodies) != 0) syFree(p);
  if ((p = a->values) != 0) syFree(p);
  if ((p = a->spins) != 0) syFree(p);
  if ((p = a->words270) != 0) syFree(p);
}
HW_EXPORT(8c04f21c, actor_type28_free_blocks(ARG0))

/* was func_8C04E544: the destructor body: the blocks freed, then both contact sets */
void actor_type28_destroy(gaddr obj) {
  actor_type28_free_blocks(obj);
  contacts_free(member_addr(obj, ActorType28, contacts_b));
  contacts_free(member_addr(obj, ActorType28, contacts));
}
HW_EXPORT(8c04e544, actor_type28_destroy(ARG0))

/* was func_8C04E562: the deleting destructor */
gaddr actor_type28_delete(gaddr obj, int32_t free_memory) {
  actor_type28_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04e562, RET(actor_type28_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04E586: the destructor body, returning the object */
gaddr actor_type28_destroy_self(gaddr obj) {
  actor_type28_destroy(obj);
  return obj;
}
HW_EXPORT(8c04e586, RET(actor_type28_destroy_self(ARG0)))

/* was func_8C04F198: reset the spray: both scales and f244 0, the counter cleared, word254 from the global, the
 * static table, every particle's state off */
void actor_type28_reset(gaddr obj) {
  ActorType28 *a = (ActorType28 *)hw_ptr(obj);
  a->scale_a = F32(0.0f);
  a->scale_b = F32(0.0f);
  a->f244 = F32(0.0f);
  a->counter = 0;
  a->word254 = g_type28_reset_word;
  a->table = TYPE28_STATIC_TABLE;
  gaddr state = a->states;
  for (int32_t n = g_type28_particles; n > 0; n--) {
    U32_AT(state) = 0;
    state += 4;
  }
}
HW_EXPORT(8c04f198, actor_type28_reset(ARG0))

/* was func_8C04F29C: step the spray's particles: a counting-down one (state 1) starts growing once its countdown
 * has run out, a growing one (2) grows its value by 1 up to the limit, then ends; both spin and move; an ending one
 * (3) goes off */
void actor_type28_step_particles(gaddr obj) {
  const ActorType28 *a = (const ActorType28 *)hw_ptr(obj);
  gaddr countdown = a->countdowns, state = a->states, value = a->values, spin = a->spins, body = a->bodies;
  for (int32_t n = g_type28_particles; n > 0; n--) {
    int32_t *s = (int32_t *)hw_ptr(state);
    switch (*s) {
    case 0:
      break;
    case 1: {
      int32_t *c = (int32_t *)hw_ptr(countdown);
      int32_t left = (*c)--;
      if (left < 0) (*s)++;
      F32_AT(spin) = fadd(F32_AT(spin), g_type28_spin_step);
      body_step(body);
      break;
    }
    case 2:
      if (fgt(g_type28_value_limit, F32_AT(value)))
        F32_AT(value) = fadd(F32_AT(value), F32(1.0f));
      else
        (*s)++;
      F32_AT(spin) = fadd(F32_AT(spin), g_type28_spin_step);
      body_step(body);
      break;
    case 3:
      *s = 0;
      break;
    default:
      break;
    }
    countdown += 4;
    state += 4;
    value += 4;
    spin += 4;
    body += sizeof(Body);
  }
}
HW_EXPORT(8c04f29c, actor_type28_step_particles(ARG0))

/* func_8C04F3A2: draw the spray's particles (still translated) */

/* was func_8C04F0E0: draw it: its texture, its model at the body's position turned about x by the fifth pull's
 * value, about y and z by the first two turns' first values, scaled by the first three pulls' values, with the shape
 * motion of its motion entry at its frame; then, while word28 is 0, the spray's particles */
void actor_type28_draw(gaddr obj) {
  const ActorType28 *a = (const ActorType28 *)hw_ptr(obj);
  njSetTexture(a->texlist);
  matrix_push();
  matrix_translate_v(member_addr(obj, ActorType28, body.position));
  matrix_rotate_x(a->pulls[4].value);
  matrix_rotate_y(a->turn_a.x);
  matrix_rotate_z(a->turn_b.x);
  matrix_scale(a->pulls[0].value, a->pulls[1].value, a->pulls[2].value);
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  shape_motion_draw_l1(a->object, e->motion, e->shape, member_addr(obj, ActorType28, frame), 1);
  matrix_pop();
  if (a->word28 == 0) actor_type28_draw_particles(obj);
}
HW_EXPORT(8c04f0e0, actor_type28_draw(ARG0))
