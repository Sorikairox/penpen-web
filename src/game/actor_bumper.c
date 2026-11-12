/* Pen Pen port, S14 b11: the bumper (type 0x0C) and the swinging flap (type 0x0D) (game/actor.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

/* ---- type 0x0C: the bumper ---- */

#define BUMPER_RESPOND 0x8C05325Au /* bumper_respond below: the contacts' response */

/* was func_8C052FCE: the bumper's base construction: the Actor, the pulse at rest (0, 0, 0, 1) and its contacts
 * (owned by the centre, radius 10, room for 8) */
gaddr bumper_base_construct(gaddr obj) {
  Bumper *b = (Bumper *)hw_ptr(obj);
  actor_construct(obj);
  b->pulse.value = F32(0.0f);
  b->pulse.velocity = F32(0.0f);
  b->pulse.force = F32(0.0f);
  b->pulse.damping = F32(1.0f);
  contacts_construct(member_addr(obj, Bumper, contacts), member_addr(obj, Bumper, center), F32(10.0f), 8);
  return obj;
}
HW_EXPORT(8c052fce, RET(bumper_base_construct(ARG0)))

/* was func_8C053008: type 0x0C's constructor: both vtables, then the base */
gaddr bumper_construct(gaddr obj) {
  Bumper *b = (Bumper *)hw_ptr(obj);
  b->base.vtable = 0x8C23E21Cu;
  b->second_vtable = 0x8C23E23Cu;
  bumper_base_construct(obj);
  return obj;
}
HW_EXPORT(8c053008, RET(bumper_construct(ARG0)))

/* was func_8C05309A: set the bumper up from its placement: a param of 3 or more makes a big one (size 42, its centre
 * 5 higher), else its size is param + 1; the pulse starts at 1 (damping 0.9); its contacts respond with
 * bumper_respond */
void bumper_init(gaddr obj, gaddr placement) {
  Bumper *b = (Bumper *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  b->param = pl->param;
  b->position.z = pl->position.z;
  b->position.y = pl->position.y;
  b->position.x = pl->position.x;
  b->center = b->position;
  if ((int32_t)b->param <= 2) {
    b->size = (int32_t)b->param + 1;
  } else {
    b->size = 42;
    b->center.y = fadd(b->center.y, F32(5.0f));
  }
  b->pulse.value = F32(1.0f);
  b->pulse.damping = F32_BITS(0x3F666666u); /* 0.9 */
  b->contacts.count = 0;
  b->contacts_word = 4;
  b->contacts.mass = F32(1.0f);
  b->contacts.unknown18 = F32(1.0f);
  b->contacts.respond = BUMPER_RESPOND;
}
HW_EXPORT(8c05309a, bumper_init(ARG0, ARG1))

/* was func_8C053126: step the bumper: while touched its pulse is pushed (+0.15) and every view whose camera is within
 * a cell of it plays the bump sound (0x24 small, 0x30 big); the pulse is pulled back toward 1 (0.3 of the distance)
 * and steps; the contacts go into the race scene's slot 8 */
void bumper_update(gaddr obj) {
  Bumper *b = (Bumper *)hw_ptr(obj);
  if (b->contacts.count != 0) {
    b->pulse.force = fadd(b->pulse.force, F32_BITS(0x3E199999u)); /* 0.15 */
    for (int32_t v = 0; v < (int32_t)b->base.word0c; v++) {
      int32_t cell = race_scene_view_cell(g_panel, v);
      if (!fgt(fabs_(f32_from_int(b->base.cell - cell)), F32(1.0f)))
        race_scene_sound(g_panel, v, 0, 8, (int32_t)b->param > 2 ? 0x30 : 0x24);
    }
  }
  b->pulse.force = fmac(F32_BITS(0x3E999999u) /* 0.3 */, fsub(F32(1.0f), b->pulse.value), b->pulse.force);
  body1_step(member_addr(obj, Bumper, pulse));
  race_scene_slot_insert(g_panel, member_addr(obj, Bumper, contacts), 8);
}
HW_EXPORT(8c053126, bumper_update(ARG0))

/* was func_8C05325A: the bumper's contact response to contact i with `body`: push the body out along the contact's
 * normal by the depth, and add 3.7 times the normal to its velocity */
void bumper_respond(gaddr contacts, int32_t i, gaddr body) {
  const Contacts *c = contacts_at(contacts);
  gaddr normal = c->normal + (gaddr)(i * (int32_t)sizeof(Vec3));
  f32 depth = ((const f32 *)hw_ptr(c->depth))[i];
  gaddr push = hw_stack_alloc(sizeof(Vec3));
  vec3_scaled(depth, push, normal);
  vec3_add(member_addr(body, Body, position), push);
  vec3_scaled(F32_BITS(0x406D097Au) /* 3.7037 */, push, normal);
  vec3_add(member_addr(body, Body, velocity), push);
  hw_stack_free(sizeof(Vec3));
}
HW_EXPORT(8c05325a, bumper_respond(ARG0, (int32_t)ARG1, ARG2))

/* ---- type 0x0D: the flap ---- */

#define FLAP_RESPOND 0x8C053A1Cu  /* the flap's contact response (guest code) */
#define FLAP_REST    F32_BITS(0x45638E38u) /* 40 degrees in Ninja units (7281.78) */

/* was func_8C053554: the flap's base construction: the Actor, the pivot and the tip at rest, the swing (0, 0, 0, 1)
 * and the contacts (owned by the tip, radius 7.5, room for 8) */
gaddr flap_base_construct(gaddr obj) {
  Flap *f = (Flap *)hw_ptr(obj);
  actor_construct(obj);
  body_reset(member_addr(obj, Flap, pivot));
  f->swing.value = F32(0.0f);
  f->swing.velocity = F32(0.0f);
  f->swing.force = F32(0.0f);
  f->swing.damping = F32(1.0f);
  body_reset(member_addr(obj, Flap, tip));
  contacts_construct(member_addr(obj, Flap, contacts), member_addr(obj, Flap, tip), F32(7.5f), 8);
  return obj;
}
HW_EXPORT(8c053554, RET(flap_base_construct(ARG0)))

/* was func_8C053600: type 0x0D's constructor: both vtables, then the base */
gaddr flap_construct(gaddr obj) {
  Flap *f = (Flap *)hw_ptr(obj);
  f->base.vtable = 0x8C23E25Cu;
  f->second_vtable = 0x8C23E27Cu;
  flap_base_construct(obj);
  return obj;
}
HW_EXPORT(8c053600, RET(flap_construct(ARG0)))

/* was func_8C0536A0: set the flap up from its placement: the pivot at the position (no damping), the tip with full
 * damping (its velocity is the contacts' owner velocity), the contacts on the actor's cell; a flap with param 0
 * swings (rests at 40 degrees, the flap's response), any other hangs still */
void flap_init(gaddr obj, gaddr placement) {
  Flap *f = (Flap *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  f->param = pl->param;
  f->position = pl->position;
  body_reset(member_addr(obj, Flap, pivot));
  f->pivot.position = f->position;
  f->pivot.damping.z = F32(1.0f);
  f->pivot.damping.y = F32(1.0f);
  f->pivot.damping.x = F32(1.0f);
  body_reset(member_addr(obj, Flap, tip));
  f->tip.damping.z = F32(0.0f);
  f->tip.damping.y = F32(0.0f);
  f->tip.damping.x = F32(0.0f);
  f->contacts.count = 0;
  f->contacts_word = 4;
  f->contacts.mass = F32(1.0f);
  f->contacts.unknown18 = F32(1.0f);
  f->contacts.cell = f->base.cell;
  f->contacts.unknown1c = 0;
  f->swing.value = F32(0.0f);
  f->swing.velocity = F32(0.0f);
  f->swing.force = F32(0.0f);
  f->swing.damping = F32(1.0f);
  if (f->param == 0) {
    f->contacts.owner_velocity = member_addr(obj, Flap, tip.velocity);
    f->contacts.respond = FLAP_RESPOND;
    f->swing.value = FLAP_REST;
    f->swing.damping = F32(1.0f);
  } else {
    f->swing.value = F32(0.0f);
    f->swing.damping = F32(0.0f);
  }
  f->state = 0;
}
HW_EXPORT(8c0536a0, flap_init(ARG0, ARG1))

/* the update's locals that go to Ninja (its frame: the loop counter at 0, the tip at 4, the offset at 0x10, the
 * matrix at 0x1C, the actor at 0x5C) */
typedef struct FlapFrame {
  int32_t view;             /* 0x00 */
  Vec3 tip;                 /* 0x04 */
  Vec3 offset;              /* 0x10 */
  Matrix m;                 /* 0x1C */
  gaddr self;               /* 0x5C */
} FlapFrame;
HW_OFFSET(FlapFrame, offset, 0x10);
HW_OFFSET(FlapFrame, m, 0x1C);
HW_SIZE(FlapFrame, 0x60);

/* was func_8C0537E4: step the flap. Resting (state 0), a touch of a swinging flap stops the swing and drops it
 * (state 1, 120 steps); down, it waits, then swings back damped by 0.8 (state 2, 180 steps); coming back, a touch
 * drops it again (every view on the actor's own cell plays sound 0x27), and when the time is up it rests again. The
 * swing is pulled toward 0 (rate 0.004) while resting or down and toward 40 degrees (rate 0.008) while coming back.
 * Then the tip is placed 55.6 below the pivot turned about z by the swing; the contacts go into slot 8 */
void flap_update(gaddr obj) {
  Flap *f = (Flap *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(FlapFrame));
  FlapFrame *fr = (FlapFrame *)hw_ptr(frame);
  gaddr swing = member_addr(obj, Flap, swing);
  fr->self = obj;
  switch (f->state) {
  case 0:
    if (f->contacts.count != 0 && f->param == 0) {
      f->swing.velocity = F32(0.0f);
      f->countdown = 120;
      f->state++;
    }
    body1_pull_angle(F32(0.0f), F32_BITS(0x3B83126Eu) /* 0.004 */, swing);
    break;
  case 1:
    f->countdown--;
    if (f->countdown <= 0) {
      f->swing.damping = F32_BITS(0x3F4CCCCCu); /* 0.8 */
      f->countdown = 180;
      f->state++;
    }
    body1_pull_angle(F32(0.0f), F32_BITS(0x3B83126Eu) /* 0.004 */, swing);
    break;
  case 2:
    f->countdown--;
    if (f->countdown <= 0) {
      f->swing.damping = F32(1.0f);
      f->state = 0;
    }
    if (f->contacts.count != 0) {
      f->swing.velocity = F32(0.0f);
      f->countdown = 120;
      f->swing.damping = F32(1.0f);
      f->state--;
      for (fr->view = 0; fr->view < (int32_t)f->base.word0c; fr->view++) {
        int32_t cell = race_scene_view_cell(g_panel, fr->view);
        if (!fgt(f32_abs(f32_from_int(f->base.cell - cell)), F32(0.0f)))
          race_scene_sound(g_panel, fr->view, 0, 8, 0x27);
      }
    }
    body1_pull_angle(FLAP_REST, F32_BITS(0x3C03126Eu) /* 0.008 */, swing);
    break;
  default:
    break;
  }
  body1_step(swing);
  gaddr m = member_addr(frame, FlapFrame, m);
  matrix_identity(m);
  njUnitMatrix(m);
  njRotateZ(m, f32_to_int(f->swing.value));
  vec3_set(F32(0.0f), F32_BITS(0xC25E70A3u) /* -55.61 */, F32(0.0f), member_addr(frame, FlapFrame, offset));
  njCalcPoint(m, member_addr(frame, FlapFrame, offset), member_addr(frame, FlapFrame, tip));
  vec3_add(member_addr(frame, FlapFrame, tip), member_addr(obj, Flap, pivot.position));
  vec3_copy_bytes(member_addr(obj, Flap, tip.position), member_addr(frame, FlapFrame, tip));
  hw_stack_free(sizeof(FlapFrame));
  race_scene_slot_insert(g_panel, member_addr(obj, Flap, contacts), 8);
}
HW_EXPORT(8c0537e4, flap_update(ARG0))
