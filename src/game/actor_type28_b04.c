/* Pen Pen port, S18 b04: the scenery actor of placement type 0x28 (game/actor_b06.h), its behaviour: the init from
 * its placement, the update (a rocking thing that shoots a little spray at fixed moments of its timer and of its
 * motion; roused by a player touching its second contacts it runs about its home), the spray's two emitters and its
 * draw. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "../core/file.h"
#include "actor.h"
#include "game.h"
#include "race_scene.h"
#include "scenery.h"
#include "actor_b06.h"

/* statics of the binary the unit reads */
#define g_type28_radius_a    GUEST_VAR(f32, 0x8C0882C8u)     /* 10: the first contacts' radius */
#define g_type28_radius_b    GUEST_VAR(f32, 0x8C0882CCu)     /* 10: the second contacts' radius */
#define g_type28_particles   GUEST_VAR(int32_t, 0x8C0882D4u) /* 8: the spray's particles */
#define g_type28_countdown   GUEST_VAR(int32_t, 0x8C0882D8u) /* 20: a new particle's countdown */
#define g_type28_sprite_attr GUEST_VAR(uint32_t, 0x8C0882DCu) /* 0x32: njDrawSprite3D's attributes */
#define g_type28_value_limit GUEST_VAR(f32, 0x8C0882E0u)     /* 20: a growing particle's value stops there */
#define g_type28_spin_kind0  GUEST_VAR(f32, 0x8C0882E4u)     /* 0.2273: a kind-0 spray particle's spin */
#define g_type28_spin_kind1  GUEST_VAR(f32, 0x8C0882E8u)     /* 0.15625: a kind-1 spray particle's spin */
#define g_type28_spin_puff   GUEST_VAR(f32, 0x8C0882ECu)     /* 0.1136: a puff particle's spin */
#define g_type28_push        GUEST_VAR(f32, 0x8C0882F4u)     /* 0.7: a particle's push */
#define g_type28_tip         GUEST_VAR(f32, 0x8C0882F8u)     /* 18204.4: 100 degrees (Ninja units) */
#define g_type28_roll_tip    GUEST_VAR(f32, 0x8C0882FCu)     /* 1820.4: 10 degrees */
#define g_type28_turn_speed  GUEST_VAR(f32, 0x8C088300u)     /* 145.6: the heading's turn per update */
#define g_type28_scale_pull  GUEST_VAR(f32, 0x8C088304u)     /* 0.15: the scales' pull and kick */
#define g_type28_damping     GUEST_VAR(f32, 0x8C088308u)     /* 0.8: the scales' damping */
#define g_type28_texlist     GUEST_VAR(uint32_t, 0x8C096D10u) /* the particles' texture list */
#define TYPE28_RESPOND_NONE  0x8C04E2BEu                      /* an empty method: the second contacts' response */

/* game/race_sound_ports.c (exported there, no header yet) */
void race_channel_sound_play(gaddr scene, int32_t i, int32_t channel, int32_t priority, int32_t sound);

static inline ActorType28 *type28_at(gaddr a) { return (ActorType28 *)hw_ptr(a); }
static inline const ActorList *actor_list_at(gaddr a) { return (const ActorList *)hw_ptr(a); }

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* the Ninja angle of the way from `from` to `to` on the ground: atan2(dx, dz) in units of 65536 a turn, whole */
static f32 ground_angle(gaddr to, gaddr from) {
  const Vec3 *t = vec3_at(to), *f = vec3_at(from);
  f32 dz = fsub(t->z, f->z);
  f32 dx = fsub(t->x, f->x);
  f32 a = fdiv(fmul(rt_atan2f(dx, dz), F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 2 pi */);
  return f32_from_int(f32_to_int(a));
}

/* was func_8C04E5B0: the init from its placement: kind and motion from param bits 8-11, the body at the placement,
 * its point above it, the home Body at the same place, its model, textures and motions from the group's file, a
 * random frame of its motion, the angles from the placement, the scales at 1; a kind-0 one also gets both contact
 * sets, a speed of 2, a random heading and the timer of state 0, and its spray reset (others: a speed of 1) */
void actor_type28_init(gaddr obj, gaddr placement) {
  ActorType28 *a = type28_at(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  int32_t kind = (int32_t)(p->param & 0xF00u) / 256;
  a->word28 = kind;
  a->motion = kind;
  a->param_low = (int32_t)(p->param & 0xFu);
  gaddr body = member_addr(obj, ActorType28, body);
  body_reset(body);
  vec3_set(p->position.x, p->position.y, p->position.z, body);
  vec3_fill(member_addr(obj, ActorType28, body.damping), F32_BITS(0x3F333333u) /* 0.7 */);
  vec3_copy_bytes(member_addr(obj, ActorType28, point), body);
  a->point.y = fadd(a->point.y, F32(10.0f));
  gaddr home = member_addr(obj, ActorType28, body_c);
  body_reset(home);
  vec3_copy_bytes(home, body);
  vec3_fill(member_addr(obj, ActorType28, body_c.damping), F32_BITS(0x3F4CCCCCu) /* 0.8 */);

  /* the group file's entries: each one's word at 4 is its list */
  a->object = ((const gaddr *)hw_ptr(actor_list_at(a->base.model_set)->list))[a->word28];
  a->texlist = actor_list_at(a->base.texlist)->list;
  a->motions = actor_list_at(a->base.word1c)->list;
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  if (e->motion != 0) {
    const NjMotion *m = (const NjMotion *)hw_ptr(e->motion);
    a->last_frame = f32_from_uint((uint32_t)(m->nb_frames - 1));
  }
  a->frame = f32_from_int(f32_to_int(random_timer_scaled(a->last_frame)));
  if (fgt(F32(30.0f), a->frame)) {
    a->first_half = 1;
    a->second_half = 0;
  } else {
    a->first_half = 0;
    a->second_half = 1;
  }

  /* the angles about x, y, z from the placement */
  quat_identity(member_addr(obj, ActorType28, pulls[4]));
  a->pulls[4].value = p->angle.x;
  a->pulls[4].damping = F32_BITS(0x3F4CCCCCu) /* 0.8 */;
  quat_identity(member_addr(obj, ActorType28, yaw));
  a->yaw.value = p->angle.y;
  a->yaw.damping = F32_BITS(0x3F4CCCCCu);
  quat_identity(member_addr(obj, ActorType28, roll));
  a->roll.value = p->angle.z;
  a->roll.damping = F32_BITS(0x3F4CCCCCu);
  /* the scales at 1 */
  for (int32_t i = 0; i < 3; i++) {
    quat_identity(member_addr(obj, ActorType28, pulls[i]));
    a->pulls[i].damping = g_type28_damping;
    a->pulls[i].value = F32(1.0f);
  }
  quat_identity(member_addr(obj, ActorType28, pulls[3]));
  a->pulls[3].damping = F32_BITS(0x3F4CCCCCu);
  if (a->word28 != 0) {
    a->pulls[3].value = F32(1.0f);
    return;
  }

  /* kind 0: the contacts */
  Contacts *c = &a->contacts;
  c->unknown18 = F32(0.0f);
  c->radius = g_type28_radius_a;
  c->mass = F32(1.0f);
  c->unknown1c = 2;
  c->cell = a->base.cell;
  field_reset_2c(member_addr(obj, ActorType28, contacts));
  field_reset_2c(member_addr(obj, ActorType28, contacts_b));
  Contacts *cb = &a->contacts_b;
  cb->unknown18 = F32(0.0f);
  cb->radius = g_type28_radius_b;
  cb->mass = F32(0.0f);
  cb->unknown1c = 0xCD;
  cb->cell = a->base.cell;
  cb->respond = TYPE28_RESPOND_NONE;
  a->pulls[3].value = F32(2.0f);
  quat_identity(member_addr(obj, ActorType28, heading));
  a->heading.damping = F32_BITS(0x3F4CCCCCu);
  a->heading.value = random_scaled(F32(65536.0f));
  a->state = 0;
  a->turning = 0;
  switch (a->state) {
  case 0:
    a->timer = f32_to_int(fadd(random_scaled(F32(360.0f)), F32(180.0f)));
    break;
  case 1:
    a->timer = f32_to_int(fadd(random_scaled(F32(120.0f)), F32(120.0f)));
    break;
  case 2:
    a->timer = f32_to_int(fadd(random_scaled(F32(120.0f)), F32(120.0f)));
    break;
  default:
    break;
  }
  actor_type28_reset(obj);
}
HW_EXPORT(8c04e5b0, actor_type28_init(ARG0, ARG1))

/* was func_8C04F498: shoot a spray particle of `kind` from pos: the first free particle starts counting down, its
 * sprite and spin by the kind, its Body at pos with a push away from the angle */
void actor_type28_spray(gaddr obj, gaddr pos, int32_t kind, f32 angle) {
  const ActorType28 *a = type28_at(obj);
  int32_t *kinds = (int32_t *)hw_ptr(a->words25c);
  int32_t *countdowns = (int32_t *)hw_ptr(a->countdowns);
  int32_t *states = (int32_t *)hw_ptr(a->states);
  f32 *values = (f32 *)hw_ptr(a->values);
  f32 *spins = (f32 *)hw_ptr(a->spins);
  f32 *alphas = (f32 *)hw_ptr(a->words270);
  gaddr body = a->bodies;
  for (int32_t i = 0; i < g_type28_particles; i++, body += sizeof(Body)) {
    if (states[i] != 0) continue;
    kinds[i] = kind;
    spins[i] = kind == 0 ? g_type28_spin_kind0 : g_type28_spin_kind1;
    countdowns[i] = g_type28_countdown;
    values[i] = F32(0.0f);
    alphas[i] = F32(1.0f);
    states[i] = 1;
    body_reset(body);
    vec3_fill(member_addr(body, Body, damping), F32_BITS(0x3F733333u) /* 0.95 */);
    vec3_copy_bytes(body, pos);
    int32_t ang = f32_to_int(angle);
    f32_add_to(fneg(fmul(njSin(ang), g_type28_push)), member_addr(body, Body, force.x));
    f32_add_to(fneg(fmul(njCos(ang), g_type28_push)), member_addr(body, Body, force.z));
    return;
  }
}
HW_EXPORT(8c04f498, actor_type28_spray(ARG0, ARG1, (int32_t)ARG2, FARG0))

/* was func_8C04F5B4: blow a puff particle (sprite 0) from pos: the first free particle, pushed toward the angle
 * and up (half as hard again while roused) */
void actor_type28_puff(gaddr obj, gaddr pos, f32 angle) {
  const ActorType28 *a = type28_at(obj);
  int32_t *kinds = (int32_t *)hw_ptr(a->words25c);
  int32_t *countdowns = (int32_t *)hw_ptr(a->countdowns);
  int32_t *states = (int32_t *)hw_ptr(a->states);
  f32 *values = (f32 *)hw_ptr(a->values);
  f32 *spins = (f32 *)hw_ptr(a->spins);
  f32 *alphas = (f32 *)hw_ptr(a->words270);
  gaddr body = a->bodies;
  for (int32_t i = 0; i < g_type28_particles; i++, body += sizeof(Body)) {
    if (states[i] != 0) continue;
    kinds[i] = 0;
    spins[i] = g_type28_spin_puff;
    countdowns[i] = g_type28_countdown;
    values[i] = F32(0.0f);
    alphas[i] = F32(1.0f);
    states[i] = 1;
    body_reset(body);
    vec3_fill(member_addr(body, Body, damping), F32_BITS(0x3F733333u) /* 0.95 */);
    vec3_copy_bytes(body, pos);
    int32_t ang = f32_to_int(angle);
    if (a->state >= 3) {
      f32_add_to(fmul(fmul(njSin(ang), g_type28_push), F32(1.5f)), member_addr(body, Body, force.x));
      f32_add_to(fmul(fmul(njCos(ang), g_type28_push), F32(1.5f)), member_addr(body, Body, force.z));
      f32_add_to(fneg(fmul(g_type28_push, F32_BITS(0x3FA66666u) /* 1.3 */)), member_addr(body, Body, force.y));
    } else {
      f32_add_to(fmul(njSin(ang), g_type28_push), member_addr(body, Body, force.x));
      f32_add_to(fmul(njCos(ang), g_type28_push), member_addr(body, Body, force.z));
      f32_add_to(fneg(g_type28_push), member_addr(body, Body, force.y));
    }
    return;
  }
}
HW_EXPORT(8c04f5b4, actor_type28_puff(ARG0, ARG1, FARG0))

/* the puff's mouth: 6.551 out along the yaw, 13.83 up, from the body */
static void type28_puff_from_mouth(gaddr obj, gaddr local) {
  ActorType28 *a = type28_at(obj);
  f32 z = fmul(njCos(f32_to_int(a->yaw.value)), F32_BITS(0x40D1A1CAu) /* 6.551 */);
  f32 x = fmul(njSin(f32_to_int(a->yaw.value)), F32_BITS(0x40D1A1CAu));
  vec3_set(x, F32_BITS(0x415D4FDFu) /* 13.83 */, z, local);
  vec3_add(local, member_addr(obj, ActorType28, body.position));
  actor_type28_puff(obj, local, a->yaw.value);
}

/* the views whose player stands within one cell of it hear the spray */
static void type28_spray_sound(gaddr obj) {
  const ActorType28 *a = type28_at(obj);
  for (int32_t i = 0; i < (int32_t)a->base.word0c; i++) {
    int32_t cell = race_scene_view_cell(panel_get(), i);
    if (!fgt(f32_abs(f32_from_int(a->base.cell - cell)), F32(1.0f)))
      race_channel_sound_play(panel_get(), i, 0, 8, 0x2A);
  }
}

/* the kick of the scales at a spray: wider and flatter */
static void type28_kick_scales(ActorType28 *a, gaddr obj) {
  f32_add_to(g_type28_scale_pull, member_addr(obj, ActorType28, pulls[0].force));
  f32_add_to(g_type28_scale_pull, member_addr(obj, ActorType28, pulls[2].force));
  f32_add_to(fneg(g_type28_scale_pull), member_addr(obj, ActorType28, pulls[1].force));
}

/* was func_8C04E8CC: the update. A kind other than 0 only plays its motion. Kind 0: a player on its second contacts
 * (other < 8) rouses it (state 3: its home moved to where it is now, heading away from it); then by state: 0 rocks
 * (speed 1) and shoots a puff at fixed timer values, switching to 1 when its timer runs out, 1 rocks faster (speed 2)
 * back to 0, 3 runs to its target and, once there, picks a new random one 40 x 80 about its home. Then it turns, its
 * yaw follows the heading, its body follows the home (or is placed about it), its contacts are offered to the
 * scene, its motion advances, and at its frame's halves it shoots sprays, tips and squashes */
void actor_type28_update(gaddr obj) {
  ActorType28 *a = type28_at(obj);
  if (a->word28 != 0) {
    gaddr frame = member_addr(obj, ActorType28, frame);
    F32_AT(frame) = fadd(F32_AT(frame), a->pulls[3].value);
    return;
  }
  gaddr local = hw_stack_alloc(0x18); /* the original's frame: a Vec3 at 12 */
  gaddr spot = local + 12;
  gaddr home = member_addr(obj, ActorType28, body_c);
  gaddr target = member_addr(obj, ActorType28, point100);
  gaddr body = member_addr(obj, ActorType28, body);
  gaddr speed = member_addr(obj, ActorType28, pulls[3]);

  /* roused by a player on the second contacts */
  if (a->state < 3) {
    for (int32_t i = 0; i < a->contacts_b.count; i++) {
      if (((const int32_t *)hw_ptr(a->contacts_b.other))[i] > 7) continue;
      a->state = 3;
      vec3_copy_bytes(target, home);
      a->turning = 0;
      a->heading.value = ground_angle(body, target);
    }
  }

  switch (a->state) {
  case 0:
    body1_pull(F32(1.0f), F32_BITS(0x3D4CCCCCu) /* 0.05 */, speed);
    body1_step(speed);
    if (a->timer-- <= 0) {
      a->timer = f32_to_int(fadd(random_scaled(F32(120.0f)), F32(120.0f)));
      a->state = 1;
      a->turning = 1;
    }
    switch (a->timer) {
    case 180: case 160: case 120: case 100: case 60: case 55: case 50: case 45: case 40: case 35: case 30: case 25:
      type28_puff_from_mouth(obj, spot);
      break;
    default:
      break;
    }
    break;
  case 1:
    body1_pull(F32(2.0f), F32_BITS(0x3D4CCCCCu), speed);
    body1_step(speed);
    if (a->timer-- <= 0) {
      a->timer = f32_to_int(fadd(random_scaled(F32(360.0f)), F32(180.0f)));
      a->state = 0;
      a->turning = 0;
    }
    break;
  case 3:
    body1_pull(F32(2.0f), F32_BITS(0x3D4CCCCCu), speed);
    body1_step(speed);
    vec3_copy(spot, body);
    vec3_sub(spot, target);
    if (fgt(F32(10.0f), njScalor(spot))) {
      /* at the target: a new random target about home */
      f32 r = random_scaled(F32(65536.0f));
      vec3_copy_bytes(target, home);
      int32_t ang = f32_to_int(r);
      Vec3 *t = vec3_at(target);
      t->x = fmac(F32(40.0f), njSin(ang), t->x);
      t->z = fmac(F32(80.0f), njCos(ang), t->z);
      a->heading.value = ground_angle(body, target);
    }
    break;
  default: /* 2, 4: still */
    break;
  }

  /* the heading turns, the yaw follows it */
  if (a->turning == 1)
    f32_add_to(g_type28_turn_speed, member_addr(obj, ActorType28, heading.value));
  else if (a->turning == 2)
    f32_add_to(fneg(g_type28_turn_speed), member_addr(obj, ActorType28, heading.value));
  body1_step(member_addr(obj, ActorType28, heading));
  if (a->turning == 1)
    body1_pull_angle(fadd(a->heading.value, g_type28_tip), F32_BITS(0x3D4CCCCCu), member_addr(obj, ActorType28, yaw));
  else if (a->turning == 2)
    body1_pull_angle(fsub(a->heading.value, g_type28_tip), F32_BITS(0x3D4CCCCCu), member_addr(obj, ActorType28, yaw));
  else if (a->turning == 0)
    body1_pull_angle(fadd(a->heading.value, F32(-32768.0f)), F32_BITS(0x3CA3D70Au) /* 0.02 */,
                     member_addr(obj, ActorType28, yaw));
  body1_step(member_addr(obj, ActorType28, yaw));

  if (a->state < 3) {
    /* placed on its circle about home */
    gaddr circle = member_addr(obj, ActorType28, body_b);
    Body *b = body_at(circle);
    b->position.x = fmul(njSin(f32_to_int(a->heading.value)), F32(40.0f));
    b->position.z = fmul(njCos(f32_to_int(a->heading.value)), F32(80.0f));
    vec3_fill(member_addr(obj, ActorType28, body_b.damping), F32_BITS(0x3F666666u) /* 0.9 */);
    body_step(circle);
    body_step(home);
    Body *h = body_at(home), *m = body_at(body);
    m->position.x = fadd(h->position.x, b->position.x);
    m->position.y = fadd(h->position.y, b->position.y);
    m->position.z = fadd(h->position.z, b->position.z);
  } else {
    /* running to the target */
    body_pull(F32_BITS(0x3C23D70Au) /* 0.01 */, body, target);
    vec3_fill(member_addr(obj, ActorType28, body.damping), F32_BITS(0x3F199999u) /* 0.6 */);
    body_step(body);
  }
  vec3_copy_bytes(member_addr(obj, ActorType28, point), body);
  a->point.y = fadd(a->point.y, F32(10.0f));
  race_scene_slot_insert(panel_get(), member_addr(obj, ActorType28, contacts), 8);
  if (a->state < 3) race_scene_slot_insert(panel_get(), member_addr(obj, ActorType28, contacts_b), 8);

  /* the motion, and a spray at each half of it */
  gaddr frame = member_addr(obj, ActorType28, frame);
  F32_AT(frame) = fadd(F32_AT(frame), a->pulls[3].value);
  if (fgt(F32(30.0f), a->frame)) {
    if (a->first_half == 0) {
      actor_type28_spray(obj, body, 0, a->yaw.value);
      a->first_half = 1;
      a->second_half = 0;
      if (a->turning != 0)
        body1_pull_angle(fneg(g_type28_roll_tip), F32_BITS(0x3E999999u) /* 0.3 */, member_addr(obj, ActorType28, roll));
      type28_kick_scales(a, obj);
      if (a->state >= 3) type28_puff_from_mouth(obj, spot);
      type28_spray_sound(obj);
    }
  } else if (a->second_half == 0) {
    actor_type28_spray(obj, body, 1, a->yaw.value);
    a->second_half = 1;
    a->first_half = 0;
    if (a->turning != 0)
      body1_pull_angle(g_type28_roll_tip, F32_BITS(0x3E999999u), member_addr(obj, ActorType28, roll));
    type28_kick_scales(a, obj);
    if (a->state >= 3) type28_puff_from_mouth(obj, spot);
    type28_spray_sound(obj);
  }

  /* the roll and the scales settle */
  if (a->turning == 0)
    body1_pull_angle(F32(0.0f), F32_BITS(0x3DCCCCCCu) /* 0.1 */, member_addr(obj, ActorType28, roll));
  body1_pull(F32(1.0f), g_type28_scale_pull, member_addr(obj, ActorType28, pulls[0]));
  body1_pull(F32(1.0f), g_type28_scale_pull, member_addr(obj, ActorType28, pulls[2]));
  body1_pull(F32(1.0f), g_type28_scale_pull, member_addr(obj, ActorType28, pulls[1]));
  body1_step(member_addr(obj, ActorType28, pulls[0]));
  body1_step(member_addr(obj, ActorType28, pulls[1]));
  body1_step(member_addr(obj, ActorType28, pulls[2]));
  body1_step(member_addr(obj, ActorType28, roll));
  actor_type28_step_particles(obj);
  hw_stack_free(0x18);
}
HW_EXPORT(8c04e8cc, actor_type28_update(ARG0))

/* was func_8C04F3A2: draw the spray's particles: each live one as its sprite at its Body, its size its spin, faded
 * by its value (its alpha kept) */
void actor_type28_draw_particles(gaddr obj) {
  ActorType28 *a = type28_at(obj);
  control3d_push();
  control3d_set(0x10);
  const int32_t *kinds = (const int32_t *)hw_ptr(a->words25c);
  const int32_t *states = (const int32_t *)hw_ptr(a->states);
  const f32 *values = (const f32 *)hw_ptr(a->values);
  const f32 *spins = (const f32 *)hw_ptr(a->spins);
  f32 *alphas = (f32 *)hw_ptr(a->words270);
  gaddr body = a->bodies;
  njSetTexture(g_type28_texlist);
  for (int32_t i = 0; i < g_type28_particles; i++, body += sizeof(Body)) {
    if (states[i] == 0) continue;
    f32 alpha = fsub(F32(1.0f), fdiv(values[i], g_type28_value_limit));
    alphas[i] = alpha;
    material_set_e028(F32(1.0f), F32(1.0f), F32(1.0f), alpha);
    a->sprite.sx = spins[i];
    a->sprite.sy = spins[i];
    const Body *b = body_at(body);
    a->sprite.p.x = b->position.x;
    a->sprite.p.y = b->position.y;
    a->sprite.p.z = b->position.z;
    njDrawSprite3D(member_addr(obj, ActorType28, sprite), kinds[i], g_type28_sprite_attr);
  }
  control3d_pop();
}
HW_EXPORT(8c04f3a2, actor_type28_draw_particles(ARG0))
