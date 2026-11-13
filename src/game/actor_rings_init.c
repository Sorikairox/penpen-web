/* Pen Pen port, S19 (u): the init method of the type-0x2C actor (the rings, Rings in game/actor.h; vtable
 * 0x8C23E73C, slot 1). */
#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"

#define RINGS_DRIFT_X F32_BITS(0xBF333333u)  /* -0.7 */
#define RINGS_DRIFT_Y F32_BITS(0x3F2B4395u)  /* 0.66899997 */
#define RINGS_DRIFT_Z F32_BITS(0x3E7CED91u)  /* 0.247 */
#define RINGS_DRIFT_SCALE F32_BITS(0x40BAAAAAu) /* 5.8333330 */
#define RINGS_RADIUS F32(40.0f)
#define RINGS_CONTACTS_1C 0xCD
#define RINGS_RESPOND 0x8C05C424u /* contact_take_other_velocity (the contacts' response) */
#define RING_TURN_STEP F32(8192.0f)  /* each ring starts 45 degrees on from the one before */
#define RING_SIZE_STEP F32(10.0f)    /* and 10 smaller (40, 30, ...) */
#define RING_DAMPING F32_BITS(0x3F599999u)      /* 0.84999996 */
#define RING_DAMPING_STEP F32_BITS(0x3C23D70Au) /* 0.01 less per ring */
#define CONTACTS_WORD_INIT 4

/* was func_8C05C054: the init: the body at rest at the placement, the plane's angle, the param (0: a drift of
 * (-0.7, 0.669, 0.247) * 5.8333); the contacts (radius 40) on the actor's cell, owned by this, answering with the
 * other body's velocity; the 40 rings at rest, each a turn of 45 degrees and a size of 10 less than the one before,
 * damped 0.01 less */
void rings_init(gaddr obj, gaddr placement) {
  Rings *r = (Rings *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  gaddr body = member_addr(obj, Rings, body);
  body_reset(body);
  f32 z = p->position.z, y = p->position.y, x = p->position.x;
  r->body.position.x = x;
  r->body.position.y = y;
  r->body.position.z = z;
  vec3_fill(member_addr(body, Body, damping), F32(1.0f));
  r->angle.x = p->angle.x;
  r->angle.y = p->angle.y;
  r->angle.z = p->angle.z;
  r->param = p->param;
  if (p->param == 0) {
    gaddr drift = member_addr(obj, Rings, drift);
    vec3_set(RINGS_DRIFT_X, RINGS_DRIFT_Y, RINGS_DRIFT_Z, drift);
    vec3_scale(RINGS_DRIFT_SCALE, drift);
  }

  Contacts *c = &r->contacts;
  c->count = 0;
  r->contacts_word = CONTACTS_WORD_INIT;
  c->cell = r->base.cell;
  c->radius = RINGS_RADIUS;
  c->unknown1c = RINGS_CONTACTS_1C;
  c->respond = RINGS_RESPOND;
  c->unknown20 = (int32_t)obj;

  f32 turn = F32(0.0f), size = RINGS_RADIUS;
  for (int32_t i = 0; i < RINGS; i++) {
    Body1 *s = &r->sizes[i];
    r->turns[i] = turn;
    s->value = F32(0.0f);
    s->velocity = F32(0.0f);
    s->force = F32(0.0f);
    s->damping = F32(1.0f);
    turn = fadd(turn, RING_TURN_STEP);
    s->value = size;
    s->damping = fsub(RING_DAMPING, fmul(f32_from_int(i), RING_DAMPING_STEP));
    size = fsub(size, RING_SIZE_STEP);
  }
}
HW_EXPORT(8c05c054, rings_init(ARG0, ARG1))
