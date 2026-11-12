/* Pen Pen port, S15 b08: constructors, destructors and set-ups of the actor classes of placement types 1, 2, 3,
 * 0x20 and 0x2D (game/actor.h). A class's constructor stores its two vtables and calls its body constructor, which
 * runs the Actor base constructor and then initialises the class's members; the set-up (the init method) reads the
 * placement. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "../core/view.h"
#include "game.h"
#include "actor.h"
#include "course.h"
#include "race_scene.h"
#include "scenery.h"

/* a Body1 at rest: value 0, velocity 0, force 0, damping 1 (the inline constructor; a Quat (0, 0, 0, 1) too) */
static void body1_at_rest(Body1 *b) {
  b->value = F32(0.0f);
  b->velocity = F32(0.0f);
  b->force = F32(0.0f);
  b->damping = F32(1.0f);
}

/* was func_8C04572A (and its copy func_8C0617E6): a Body's damping = (1, 1, 1), no damping */
void body_damping_none(gaddr body) {
  Body *b = body_at(body);
  b->damping.z = F32(1.0f);
  b->damping.y = F32(1.0f);
  b->damping.x = F32(1.0f);
}
HW_EXPORT(8c04572a, body_damping_none(ARG0))
HW_EXPORT(8c0617e6, body_damping_none(ARG0))

/* ---- type 1 (ActorType01) ---- */

#define TYPE01_VTABLE        0x8C23DBCCu
#define TYPE01_SECOND_VTABLE 0x8C23DBECu

/* was func_8C044F54: the body constructor: the Actor base, the spin at rest, contacts of radius 10 owned by the
 * position, the four direction vectors, the position at the origin; returns the actor */
gaddr actor_type01_construct_body(gaddr obj) {
  ActorType01 *a = (ActorType01 *)hw_ptr(obj);
  actor_construct(obj);
  body1_at_rest(&a->spin);
  contacts_construct(member_addr(obj, ActorType01, contacts), member_addr(obj, ActorType01, position), F32(10.0f), 0);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(obj, ActorType01, dir[0]));
  vec3_set(F32(0.0f), F32(0.0f), F32(-1.0f), member_addr(obj, ActorType01, dir[1]));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(obj, ActorType01, dir[2]));
  vec3_set(F32(0.0f), F32(0.0f), F32(-1.0f), member_addr(obj, ActorType01, dir[3]));
  a->position.z = F32(0.0f);
  a->position.y = F32(0.0f);
  a->position.x = F32(0.0f);
  return obj;
}
HW_EXPORT(8c044f54, RET(actor_type01_construct_body(ARG0)))

/* was func_8C045020: the constructor */
gaddr actor_type01_construct(gaddr obj) {
  ActorType01 *a = (ActorType01 *)hw_ptr(obj);
  a->base.vtable = TYPE01_VTABLE;
  a->second_vtable = TYPE01_SECOND_VTABLE;
  actor_type01_construct_body(obj);
  return obj;
}
HW_EXPORT(8c045020, RET(actor_type01_construct(ARG0)))

#define TYPE01_WIND_CELL_FIRST 0x3A /* the course cells where the camera feels its wind */
#define TYPE01_WIND_CELL_LAST  0x3D
#define TYPE01_MODEL           10   /* its model in the actor's model list */

/* was func_8C045514: the per-view draw: while the current view's camera is on cells 0x3A-0x3D, push that view's
 * wind by 40 times the push vector; then, when the view's course row has the actor's cell, its model 10 at its
 * position, turned by its heading (about y) and word38 (about z) */
void actor_type01_draw_view(gaddr obj) {
  ActorType01 *a = (ActorType01 *)hw_ptr(obj);
  int32_t cell = race_scene_view_cell(panel_get(), -1);
  if (cell >= TYPE01_WIND_CELL_FIRST && cell <= TYPE01_WIND_CELL_LAST) {
    gaddr push = hw_stack_alloc(sizeof(Vec3));
    vec3_copy(push, member_addr(obj, ActorType01, push));
    vec3_scale(F32(40.0f), push);
    int32_t view = view_current_index();
    race_scene_env_push(panel_get(), view, push);
    hw_stack_free(sizeof(Vec3));
  }
  if (race_scene_view_bit(panel_get(), a->base.cell)) {
    matrix_push();
    matrix_translate_v(member_addr(obj, ActorType01, position));
    matrix_rotate_y(a->heading);
    matrix_rotate_z(a->word38);
    nj_ModelL1(actor_model(&a->base, TYPE01_MODEL));
    matrix_pop();
  }
}
HW_EXPORT(8c045514, actor_type01_draw_view(ARG0))

/* ---- type 2 (ActorType02) ---- */

#define TYPE02_VTABLE        0x8C23DC0Cu
#define TYPE02_SECOND_VTABLE 0x8C23DC2Cu
#define TYPE02_BURST_PARTICLES 10

/* was func_8C045876: the body constructor: the Actor base, the Body at rest, contacts of radius 5 (two slots) owned
 * by the body, and a Burst of ten particles (0 when out of memory); returns the actor */
gaddr actor_type02_construct_body(gaddr obj) {
  ActorType02 *a = (ActorType02 *)hw_ptr(obj);
  actor_construct(obj);
  body_reset_set3(member_addr(obj, ActorType02, body));
  contacts_construct(member_addr(obj, ActorType02, contacts), member_addr(obj, ActorType02, body), F32(5.0f), 2);
  gaddr block = syMalloc(sizeof(Burst));
  gaddr burst = 0;
  if (block != 0) burst = burst_construct(block, TYPE02_BURST_PARTICLES);
  a->burst = burst;
  return obj;
}
HW_EXPORT(8c045876, RET(actor_type02_construct_body(ARG0)))

/* was func_8C0458FC: the constructor */
gaddr actor_type02_construct(gaddr obj) {
  ActorType02 *a = (ActorType02 *)hw_ptr(obj);
  a->base.vtable = TYPE02_VTABLE;
  a->second_vtable = TYPE02_SECOND_VTABLE;
  actor_type02_construct_body(obj);
  return obj;
}
HW_EXPORT(8c0458fc, RET(actor_type02_construct(ARG0)))

/* was func_8C04594C: the destructor body: the Burst deleted (its virtual destructor, "free" = 1), the contacts
 * freed */
void actor_type02_destroy(gaddr obj) {
  gaddr burst = ((ActorType02 *)hw_ptr(obj))->burst;
  if (burst != 0) {
    const VMethod *destroy = (const VMethod *)hw_ptr(((const Burst *)hw_ptr(burst))->vtable);
    hw_call(destroy->fn, 2, burst + (uint32_t)destroy->this_adjust, 1, 0, 0);
  }
  contacts_free(member_addr(obj, ActorType02, contacts));
}
HW_EXPORT(8c04594c, actor_type02_destroy(ARG0))

/* was func_8C045984: the deleting destructor */
gaddr actor_type02_delete(gaddr obj, int32_t free_memory) {
  actor_type02_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c045984, RET(actor_type02_delete(ARG0, (int32_t)ARG1)))

/* was func_8C0459BA: the set-up from the placement: the Burst's defaults, the body at the placement's position (at
 * rest, no damping), its angles, the target point of its kind (param bits 0-5: kinds 0-3), its model (param bit 7:
 * the list's first, else the second), param bit 6, the delay (param bits 8-15, times 5), and the contacts (mass 0.5,
 * the body's velocity, its cell) */
void actor_type02_setup(gaddr obj, gaddr placement) {
  ActorType02 *a = (ActorType02 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  effect_defaults(a->burst);
  a->state = 0;
  body_reset(member_addr(obj, ActorType02, body));
  a->body.position.x = p->position.x;
  a->body.position.y = p->position.y;
  a->body.position.z = p->position.z;
  a->body.velocity.x = F32(0.0f);
  a->body.velocity.y = F32(0.0f);
  a->body.velocity.z = F32(0.0f);
  a->angle_x = p->angle.x;
  a->angle_y = p->angle.y;
  a->wordd4 = 0;
  uint32_t param = p->param;
  switch (param & 0x3F) {
  case 0:
    a->target.x = F32(-3240.0f);
    a->target.y = F32(-300.0f);
    a->target.z = F32(-7420.0f);
    break;
  case 1:
    a->target.x = F32(-3270.0f);
    a->target.y = F32(-300.0f);
    a->target.z = F32(-7430.0f);
    break;
  case 2:
    a->target.x = F32(-3240.0f);
    a->target.y = F32(-300.0f);
    a->target.z = F32(-7425.0f);
    break;
  case 3:
    a->target.x = F32(-3255.0f);
    a->target.y = F32(-300.0f);
    a->target.z = F32_BITS(0xC5E83FFFu); /* -7431.9995 */
    break;
  default:
    break;
  }
  a->model = (param & 0x80) ? actor_model(&a->base, 0) : actor_model(&a->base, 1);
  a->flag40 = (param & 0x40) ? 1 : 0;
  a->delay = ((int32_t)(param & 0xFF00u) >> 8) * 5;
  a->contacts.count = 0;
  a->contacts_word = 4;
  a->contacts.owner_velocity = member_addr(obj, ActorType02, body.velocity);
  a->contacts.unknown18 = F32(1.0f);
  a->contacts.mass = F32(0.5f);
  a->contacts.cell = a->base.cell;
  a->contacts.unknown1c = 3;
}
HW_EXPORT(8c0459ba, actor_type02_setup(ARG0, ARG1))

/* ---- type 3 (ActorPair) ---- */

#define ACTOR_PAIR_VTABLE        0x8C23DC4Cu
#define ACTOR_PAIR_SECOND_VTABLE 0x8C23DC6Cu

/* was func_8C045FEE: the body constructor: the Actor base, both turns at rest; returns the actor */
gaddr actor_pair_construct_body(gaddr obj) {
  ActorPair *a = (ActorPair *)hw_ptr(obj);
  actor_construct(obj);
  body1_at_rest(&a->turn_a);
  body1_at_rest(&a->angle_b);
  return obj;
}
HW_EXPORT(8c045fee, RET(actor_pair_construct_body(ARG0)))

/* was func_8C046028: the constructor */
gaddr actor_pair_construct(gaddr obj) {
  ActorPair *a = (ActorPair *)hw_ptr(obj);
  a->base.vtable = ACTOR_PAIR_VTABLE;
  a->second_vtable = ACTOR_PAIR_SECOND_VTABLE;
  actor_pair_construct_body(obj);
  return obj;
}
HW_EXPORT(8c046028, RET(actor_pair_construct(ARG0)))

/* ---- type 0x2D (ActorType2D) ---- */

#define TYPE2D_VTABLE        0x8C23E77Cu
#define TYPE2D_SECOND_VTABLE 0x8C23E79Cu
#define TYPE2D_RESPOND       0x8C05CAA8u /* the class's contact response */
#define CONTACTS_KIND_ACTOR  0xCD        /* Contacts.unknown1c of the actors with their own response */

/* was func_8C05C670: the body constructor: the Actor base, the turn at rest, contacts of radius 20 owned by the
 * position; returns the actor */
gaddr actor_type2d_construct_body(gaddr obj) {
  ActorType2D *a = (ActorType2D *)hw_ptr(obj);
  actor_construct(obj);
  body1_at_rest(&a->turn);
  contacts_construct(member_addr(obj, ActorType2D, contacts), member_addr(obj, ActorType2D, position), F32(20.0f), 0);
  return obj;
}
HW_EXPORT(8c05c670, RET(actor_type2d_construct_body(ARG0)))

/* was func_8C05C6AA: the constructor */
gaddr actor_type2d_construct(gaddr obj) {
  ActorType2D *a = (ActorType2D *)hw_ptr(obj);
  a->base.vtable = TYPE2D_VTABLE;
  a->second_vtable = TYPE2D_SECOND_VTABLE;
  actor_type2d_construct_body(obj);
  return obj;
}
HW_EXPORT(8c05c6aa, RET(actor_type2d_construct(ARG0)))

/* was func_8C05C738: the set-up from the placement: its position and heading; the turn at rest at the heading;
 * the wait (param bits 0-3, times 144), the direction (param bit 4), state 4 with param bit 5; the contacts with the
 * class's response (this as its argument) and the actor's cell */
void actor_type2d_setup(gaddr obj, gaddr placement) {
  ActorType2D *a = (ActorType2D *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  Vec3 pos = p->position;
  memmove(&a->position, &pos, sizeof(Vec3));
  a->heading = p->angle.y;
  a->state = 0;
  body1_at_rest(&a->turn);
  a->turn.value = a->heading;
  a->turn.velocity = F32(0.0f);
  a->turn.damping = F32(1.0f);
  a->timer = F32(0.0f);
  a->wait = fmul(f32_from_int((int32_t)(p->param & 0xF)), F32_BITS(0x430FFFFFu) /* 143.99998 */);
  if (p->param & 0x10) {
    a->speed = F32_BITS(0xC3AAAAABu); /* -341.33334 */
    a->reverse = 1;
  } else {
    a->speed = F32_BITS(0x43AAAAABu); /* 341.33334 */
    a->reverse = 0;
  }
  if (p->param & 0x20) a->state = 4;
  a->contacts.count = 0;
  a->contacts_word = 4;
  a->contacts.cell = a->base.cell;
  a->contacts.respond = TYPE2D_RESPOND;
  a->contacts.unknown20 = (int32_t)obj;
  a->contacts.unknown1c = CONTACTS_KIND_ACTOR;
}
HW_EXPORT(8c05c738, actor_type2d_setup(ARG0, ARG1))

/* func_8C05C63A: the course cell of view i of the scene (still translated) */
static int32_t view_cell_of(gaddr scene, int32_t i) { return (int32_t)CALL2(0x8C05C63Au, scene, i); }

enum {
  TYPE2D_WAITING = 0,   /* the timer runs to the wait, then it turns away (and sounds near the views) */
  TYPE2D_TURNING = 1,   /* until a quarter turn from the heading */
  TYPE2D_PAUSED = 2,    /* 48 steps */
  TYPE2D_RETURNING = 3, /* back to the heading */
  TYPE2D_STILL = 4      /* param bit 5: never moves */
};
#define TYPE2D_QUARTER_TURN F32(16384.0f) /* Ninja units */
#define TYPE2D_FULL_TURN    F32(65536.0f)
#define TYPE2D_SOUND_BANK   8
#define TYPE2D_CONTACT_SLOT 8

/* was func_8C05C81E: one step: the state machine above; the turn steps and wraps below a full turn; the contacts go
 * into slot 8 of the scene */
void actor_type2d_step(gaddr obj) {
  ActorType2D *a = (ActorType2D *)hw_ptr(obj);
  int32_t reached;
  switch (a->state) {
  case TYPE2D_WAITING:
    a->timer = fadd(a->timer, F32(1.0f));
    if (fgt(a->timer, a->wait)) {
      a->timer = F32(0.0f);
      a->state++;
      a->turn.velocity = fmul(a->speed, F32(3.0f));
      int32_t sound = (a->base.index & 1) ? 0x28 : 0x2A;
      for (int32_t v = 0; v < (int32_t)a->base.word0c; v++) {
        int32_t cell = a->base.cell;
        int32_t distance = cell - view_cell_of(panel_get(), v);
        if (!fgt(f32_abs(f32_from_int(distance)), F32(2.0f)))
          race_scene_sound(panel_get(), v, 0, TYPE2D_SOUND_BANK, sound);
      }
    }
    break;
  case TYPE2D_TURNING:
    reached = 0;
    if (a->reverse) {
      if (!fgt(a->turn.value, fadd(a->heading, fneg(TYPE2D_QUARTER_TURN)))) reached = 1;
    } else {
      if (!fgt(fadd(a->heading, TYPE2D_QUARTER_TURN), a->turn.value)) reached = 1;
    }
    if (reached) {
      a->state++;
      a->turn.velocity = F32(0.0f);
    }
    break;
  case TYPE2D_PAUSED:
    a->timer = fadd(a->timer, F32(1.0f));
    if (fgt(a->timer, F32_BITS(0x423FFFFFu) /* 47.999996 */)) {
      a->timer = F32(0.0f);
      a->state++;
      a->turn.velocity = fneg(a->speed);
    }
    break;
  case TYPE2D_RETURNING:
    reached = 0;
    if (a->reverse) {
      if (!fgt(a->heading, a->turn.value)) reached = 1;
    } else {
      if (!fgt(a->turn.value, a->heading)) reached = 1;
    }
    if (reached) {
      a->turn.value = a->heading;
      a->state = TYPE2D_WAITING;
      a->turn.velocity = F32(0.0f);
    }
    break;
  default: /* TYPE2D_STILL */
    break;
  }
  body1_step(member_addr(obj, ActorType2D, turn));
  if (fgt(a->turn.value, TYPE2D_FULL_TURN)) a->turn.value = fadd(a->turn.value, fneg(TYPE2D_FULL_TURN));
  race_scene_slot_insert(panel_get(), member_addr(obj, ActorType2D, contacts), TYPE2D_CONTACT_SLOT);
}
HW_EXPORT(8c05c81e, actor_type2d_step(ARG0))

/* ---- type 0x20 (ActorType20) ---- */

#define TYPE20_VTABLE        0x8C23E87Cu
#define TYPE20_SECOND_VTABLE 0x8C23E89Cu
#define TYPE20_TABLE         0x8C09975Cu /* static data the class reads */

/* was func_8C0618EC: the body constructor: the Actor base, the Body and both Body1s at rest; returns the actor */
gaddr actor_type20_construct_body(gaddr obj) {
  ActorType20 *a = (ActorType20 *)hw_ptr(obj);
  actor_construct(obj);
  body_reset_set3(member_addr(obj, ActorType20, body));
  body1_at_rest(&a->c4);
  body1_at_rest(&a->d4);
  return obj;
}
HW_EXPORT(8c0618ec, RET(actor_type20_construct_body(ARG0)))

/* was func_8C061964: the constructor */
gaddr actor_type20_construct(gaddr obj) {
  ActorType20 *a = (ActorType20 *)hw_ptr(obj);
  a->base.vtable = TYPE20_VTABLE;
  a->second_vtable = TYPE20_SECOND_VTABLE;
  actor_type20_construct_body(obj);
  return obj;
}
HW_EXPORT(8c061964, RET(actor_type20_construct(ARG0)))

/* was func_8C0619DC: the set-up from the placement: its kind (param bits 0-3); the body at rest at the placement's
 * position, which is also its home and its start; a random frame (0-15) and two random phases; by kind, a random
 * rate, a rate of a random sign and a radius */
void actor_type20_setup(gaddr obj, gaddr placement) {
  ActorType20 *a = (ActorType20 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->kind = (int32_t)(p->param & 0xF);
  body_reset(member_addr(obj, ActorType20, body));
  Vec3 pos = p->position;
  memmove(&a->body.position, &pos, sizeof(Vec3));
  a->tex_word = ((const gaddr *)hw_ptr(a->base.texlist))[1];
  a->home.x = a->body.position.x;
  a->home.y = a->body.position.y;
  a->home.z = a->body.position.z;
  a->word4c = F32(0.625f);
  a->word50 = F32(0.625f);
  a->word54 = 0;
  a->word58 = a->tex_word;
  a->table = TYPE20_TABLE;
  a->frame = f32_to_int(random_timer_scaled(F32(16.0f)));
  a->pull = F32_BITS(0x3D4CCCCCu);      /* 0.049999997 */
  a->damping94 = F32_BITS(0x3F4CCCCCu); /* 0.79999995 */
  body1_at_rest(&a->c4);
  body1_at_rest(&a->d4);
  memmove(&a->start, &a->body.position, sizeof(Vec3));
  a->phase_a = random_timer_scaled(F32(65536.0f));
  a->phase_b = random_timer_scaled(F32(65536.0f));
  int32_t positive = f32_to_int(random_timer_scaled(F32(1.0f)));
  const f32 rate_base = F32_BITS(0x4211A2B3u);  /* 36.408886 */
  const f32 rate_range = F32_BITS(0x4191A2B3u); /* 18.204443 */
  switch (a->kind) {
  case 0:
    a->rate_b = positive ? F32_BITS(0x44088888u) : F32_BITS(0xC4088888u); /* +-546.1333 */
    a->rate_a = fadd(random_timer_scaled(rate_range), rate_base);
    a->radius = F32(20.0f);
    break;
  case 1:
    a->rate_b = positive ? F32_BITS(0x43888888u) : F32_BITS(0xC3888888u); /* +-273.06665 */
    a->rate_a = fadd(random_timer_scaled(rate_range), rate_base);
    a->radius = F32(5.0f);
    break;
  default:
    break;
  }
}
HW_EXPORT(8c0619dc, actor_type20_setup(ARG0, ARG1))
