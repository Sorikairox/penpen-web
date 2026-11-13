/* Pen Pen port, S14 b10: the type-0x1A actor (Effect, game/actor.h): a model on a Body pulled toward its home,
 * pulsing when something fast touches it, with a particle effect (game/actor.c: effect_reset, effect_update). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/file.h"
#include "../core/inline.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

void effect_reset(gaddr fx);  /* game/actor.c */
void effect_update(gaddr fx); /* game/actor.c */

#define g_effect_radius     GUEST_VAR(f32, 0x8C0881A4u) /* 5.9999995 */
#define g_effect_push       GUEST_VAR(f32, 0x8C0881A8u) /* 0.15 */
#define g_effect_pulse_damp GUEST_VAR(f32, 0x8C0881ACu) /* 0.95 */
#define EFFECT_PARTICLES    32
#define EFFECT_BODY_CONSTRUCTOR 0x8C04AEEEu /* body_reset_set3 (game/actor.c) */

/* func_8C04B78C: start a burst of particles at `pos`, turned by `angle` (still translated) */

static void body1_rest(Body1 *b) {
  b->value = F32(0.0f);
  b->velocity = F32(0.0f);
  b->force = F32(0.0f);
  b->damping = F32(1.0f);
}

/* was func_8C04AF40: the base construction: the Actor, the pulses and angles at rest, the Body at rest at the
 * origin, its contacts (radius 6, room for 2), and the per-particle arrays (32 each; the Bodies built in place) */
gaddr effect_base_construct(gaddr obj) {
  Effect *e = (Effect *)hw_ptr(obj);
  actor_construct(obj);
  for (int32_t k = 0; k < 3; k++) body1_rest(&e->pulse[k]);
  for (int32_t k = 0; k < 3; k++) body1_rest(&e->angle[k]);
  gaddr body = member_addr(obj, Effect, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  contacts_construct(member_addr(obj, Effect, contacts), body, g_effect_radius, 2);
  const uint32_t words = EFFECT_PARTICLES * 4;
  e->frames = syMalloc(words);
  e->countdowns = syMalloc(words);
  e->states = syMalloc(words);
  gaddr block = syMalloc(EFFECT_PARTICLES * sizeof(Body));
  e->bodies = block != 0 ? array_construct_new(EFFECT_PARTICLES, sizeof(Body), block, EFFECT_BODY_CONSTRUCTOR) : 0;
  e->values = syMalloc(words);
  e->extra = syMalloc(words);
  return obj;
}
HW_EXPORT(8c04af40, RET(effect_base_construct(ARG0)))

/* was func_8C04B072: type 0x1A's constructor: both vtables, then the base */
gaddr effect_construct(gaddr obj) {
  Effect *e = (Effect *)hw_ptr(obj);
  e->base.vtable = 0x8C23DF14u;
  e->second_vtable = 0x8C23DF34u;
  effect_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04b072, RET(effect_construct(ARG0)))

/* was func_8C04B118: set the effect up from its placement: its kind (param bits 8-11) picks the model and the
 * motion, its body sits at the position (damping 0.9) which is its home, the motion starts on a random frame, the
 * angles are the placement's (damping 0.8), the pulses at 1 (damping 0.95); the contacts take part in others'
 * (radius 6, mass 0.4, kind 3, the actor's cell); then the particles are reset */
void effect_init(gaddr obj, gaddr placement) {
  Effect *e = (Effect *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  int32_t kind = (int32_t)(pl->param & 0xF00) / 256;
  e->kind = kind;
  switch (kind) {
  case 0: case 1: e->motion = 0; break;
  case 2: e->motion = 1; break;
  case 3: e->motion = 2; break;
  default: break;
  }
  e->subkind = (int32_t)(pl->param & 0xF);
  gaddr body = member_addr(obj, Effect, body);
  body_reset(body);
  vec3_set(pl->position.x, pl->position.y, pl->position.z, member_addr(body, Body, position));
  vec3_fill(member_addr(body, Body, damping), F32_BITS(0x3F666666u)); /* 0.9 */
  e->home = *vec3_at(method_self(body));
  const ActorList *models = (const ActorList *)hw_ptr(e->base.model_set);
  e->model = ((const gaddr *)hw_ptr(models->list))[e->kind];
  e->word_ec = ((const ActorList *)hw_ptr(e->base.texlist))->list;
  e->motions = ((const ActorList *)hw_ptr(e->base.word1c))->list;
  const MotionEntry *m = &((const MotionEntry *)hw_ptr(e->motions))[e->motion];
  if (m->motion != 0)
    e->frame_count = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(m->motion))->nb_frames - 1));
  e->frame = f32_from_int(f32_to_int(random_timer_scaled(e->frame_count)));
  const f32 *angle = &pl->angle.x;
  for (int32_t k = 0; k < 3; k++) {
    quat_identity(member_addr(obj, Effect, angle[k]));
    e->angle[k].value = angle[k];
    e->angle[k].damping = F32_BITS(0x3F4CCCCCu); /* 0.8 */
  }
  for (int32_t k = 0; k < 3; k++) {
    quat_identity(member_addr(obj, Effect, pulse[k]));
    e->pulse[k].damping = g_effect_pulse_damp;
    e->pulse[k].value = F32(1.0f);
  }
  field_reset_2c(member_addr(obj, Effect, contacts));
  e->contacts.unknown18 = F32(1.0f);
  e->contacts.radius = g_effect_radius;
  e->contacts.mass = F32_BITS(0x3ECCCCCCu); /* 0.4 */
  e->contacts.unknown1c = 3;
  e->contacts.cell = e->base.cell;
  effect_reset(obj);
}
HW_EXPORT(8c04b118, effect_init(ARG0, ARG1))

/* was func_8C04B302: step the effect: every contact with a body of kind 7 or less moving faster than 0.463 pushes
 * the pulses (0, 2, 1) and starts a burst at the body turned by the y angle; the pulses are pulled back to 1 and
 * step; the motion's frame counts round; the contacts' handlers run; the body is pulled home (rate 0.01, damping
 * 0.9) and steps; the contacts go into the race scene's slot 8; then the particles step */
void effect_step(gaddr obj) {
  Effect *e = (Effect *)hw_ptr(obj);
  for (int32_t i = 0; i < e->contacts.count; i++) {
    if (((const int32_t *)hw_ptr(e->contacts.other))[i] > 7) continue;
    if (!fgt(njScalor(e->contacts.velocity + (gaddr)i * sizeof(Vec3)), F32_BITS(0x3EED097Au) /* 0.463 */)) continue;
    f32_add_to(g_effect_push, member_addr(obj, Effect, pulse[0].force));
    f32_add_to(g_effect_push, member_addr(obj, Effect, pulse[2].force));
    f32_add_to(g_effect_push, member_addr(obj, Effect, pulse[1].force));
    f32 angle = e->angle[1].value;
    effect_burst(obj, method_self(member_addr(obj, Effect, body)), angle);
  }
  body1_pull(F32(1.0f), g_effect_push, member_addr(obj, Effect, pulse[0]));
  body1_pull(F32(1.0f), g_effect_push, member_addr(obj, Effect, pulse[2]));
  body1_pull(F32(1.0f), g_effect_push, member_addr(obj, Effect, pulse[1]));
  body1_step(member_addr(obj, Effect, pulse[0]));
  body1_step(member_addr(obj, Effect, pulse[1]));
  body1_step(member_addr(obj, Effect, pulse[2]));
  e->frame = fadd(e->frame, F32(1.0f));
  if (fgt(e->frame, e->frame_count)) e->frame = F32(0.0f);
  contacts_call_handlers(member_addr(obj, Effect, contacts), member_addr(obj, Effect, body), 0, 0);
  body_pull(F32_BITS(0x3C23D70Au) /* 0.01 */, member_addr(obj, Effect, body), member_addr(obj, Effect, home));
  vec3_fill(member_addr(obj, Effect, body.damping), F32_BITS(0x3F666666u)); /* 0.9 */
  body_step(member_addr(obj, Effect, body));
  race_scene_slot_insert(panel_get(), member_addr(obj, Effect, contacts), 8);
  effect_update(obj);
}
HW_EXPORT(8c04b302, effect_step(ARG0))
