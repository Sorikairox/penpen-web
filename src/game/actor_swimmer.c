/* Pen Pen port, S14 b11: the swimmer (type 0x21) (game/actor.h): it goes round a circle, its contact spheres on
 * its body, bubbles rising from it. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

#define SWIMMER_BUBBLE_CONSTRUCTOR 0x8C0567C8u /* swimmer_bubble_construct below */
#define SWIMMER_SPHERE_CONSTRUCTOR 0x8C056940u /* a copy of sphere_construct (contacts: no owner, room for 4) */
#define SWIMMER_POINT_CONSTRUCTOR  0x8C056632u /* the empty Vec3 constructor */

/* func_8C0628E8 (C runtime, float): atan2f(y, x) in radians */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* was func_8C0567C8: a bubble's constructor (the array's): at rest at the origin (body_reset's work) */
HW_EXPORT(8c0567c8, body_reset(ARG0))

/* was func_8C056808: the swimmer's base construction: the Actor, its body and heading at rest, its 8 bubbles; no
 * spheres yet */
gaddr swimmer_base_construct(gaddr obj) {
  Swimmer *s = (Swimmer *)hw_ptr(obj);
  actor_construct(obj);
  body_reset(member_addr(obj, Swimmer, body));
  s->heading.value = F32(0.0f);
  s->heading.velocity = F32(0.0f);
  s->heading.force = F32(0.0f);
  s->heading.damping = F32(1.0f);
  array_construct(SWIMMER_BUBBLES, sizeof(Body), member_addr(obj, Swimmer, bubbles), SWIMMER_BUBBLE_CONSTRUCTOR);
  s->spheres = 0;
  s->points = 0;
  return obj;
}
HW_EXPORT(8c056808, RET(swimmer_base_construct(ARG0)))

/* was func_8C056886: type 0x21's constructor: both vtables, then the base */
gaddr swimmer_construct(gaddr obj) {
  Swimmer *s = (Swimmer *)hw_ptr(obj);
  s->base.vtable = 0x8C23E41Cu;
  s->second_vtable = 0x8C23E43Cu;
  swimmer_base_construct(obj);
  return obj;
}
HW_EXPORT(8c056886, RET(swimmer_construct(ARG0)))

/* operator new[] of n objects of `size` bytes built by ctor (0 when out of memory) */
static gaddr swimmer_new_array(int32_t n, uint32_t size, gaddr ctor) {
  gaddr block = syMalloc((uint32_t)(n != 0 ? n : 1) * size);
  if (block == 0) return 0;
  return array_construct_new(n, size, block, ctor);
}

/* was func_8C056960: set the swimmer up from its placement: kind and stillness from the param, the body at the
 * position (damping 0.8), the heading at the angle about y (damping 0.9), the circle's centre; the big one (kind 0)
 * goes round at half a degree a step, the others at one; the bubbles wait 1, 11, 21 ... steps (damping 0.97). The
 * first time (flags bit 1 clear) its spheres and their points are allocated; every sphere follows the body (cell,
 * velocity); the big one's three weigh 4 (radii 37.4, 40.9, 29.5), the small one's weighs 1 (radius 5) */
void swimmer_init(gaddr obj, gaddr placement) {
  Swimmer *s = (Swimmer *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  s->kind = (int32_t)(pl->param & 0xF);
  s->still = (int32_t)(pl->param & 0xF0) >> 4;
  body_reset(member_addr(obj, Swimmer, body));
  s->body.position.x = pl->position.x;
  s->body.position.y = pl->position.y;
  s->body.position.z = pl->position.z;
  const f32 body_damping = F32_BITS(0x3F4CCCCCu); /* 0.8 */
  s->body.damping.z = body_damping;
  s->body.damping.y = body_damping;
  s->body.damping.x = body_damping;
  s->body.velocity.x = F32(0.0f);
  s->body.velocity.y = F32(0.0f);
  s->body.velocity.z = F32(0.0f);
  s->heading.value = F32(0.0f);
  s->heading.velocity = F32(0.0f);
  s->heading.force = F32(0.0f);
  s->heading.damping = F32(1.0f);
  s->heading.value = pl->angle.y;
  s->heading.velocity = F32(0.0f);
  s->heading.damping = F32_BITS(0x3F666666u); /* 0.9 */
  s->center = s->body.position;
  s->target = s->center;
  s->radius = F32(80.0f);
  s->phase = F32(0.0f);
  if (s->kind != 0) {
    s->phase_step = F32_BITS(0x43360B60u); /* 1 degree */
    s->thrust = F32_BITS(0x3F199999u);     /* 0.6 */
    s->turn_rate = F32_BITS(0x42B60B60u);  /* half a degree */
  } else {
    s->phase_step = F32_BITS(0x42B60B60u); /* half a degree */
    s->thrust = F32_BITS(0x3DCCCCCCu);     /* 0.1 */
    s->turn_rate = F32_BITS(0x4211A2B3u);  /* 0.2 degree */
  }
  s->wag = F32(0.0f);
  const f32 bubble_damping = F32_BITS(0x3F7851EBu); /* 0.97 */
  for (int32_t k = 0; k < SWIMMER_BUBBLES; k++) {
    Body *b = &s->bubbles[k];
    s->bubble_delay[k] = 10 * k + 1;
    s->bubble_state[k] = 0;
    body_reset(member_addr(obj, Swimmer, bubbles[k]));
    b->damping.z = bubble_damping;
    b->damping.y = bubble_damping;
    b->damping.x = bubble_damping;
    b->velocity.x = F32(0.0f);
    b->velocity.y = F32(0.0f);
    b->velocity.z = F32(0.0f);
  }
  s->sphere_count = s->kind != 0 ? 1 : 3;
  if (!(s->base.flags & 2)) {
    s->spheres = swimmer_new_array(s->sphere_count, sizeof(ContactsSlot), SWIMMER_SPHERE_CONSTRUCTOR);
    s->points = swimmer_new_array(s->sphere_count, sizeof(Vec3), SWIMMER_POINT_CONSTRUCTOR);
  }
  gaddr slot = s->spheres;
  gaddr point = s->points;
  for (int32_t k = 0; k < s->sphere_count; k++, slot += sizeof(ContactsSlot), point += sizeof(Vec3)) {
    ContactsSlot *c = (ContactsSlot *)hw_ptr(slot);
    c->contacts.count = 0;
    c->word = 4;
    c->contacts.owner = point;
    c->contacts.owner_velocity = member_addr(obj, Swimmer, body.velocity);
    c->contacts.unknown18 = F32(1.0f);
    c->contacts.cell = s->base.cell;
  }
  ContactsSlot *sp = (ContactsSlot *)hw_ptr(s->spheres);
  if (s->kind != 0) {
    sp[0].contacts.radius = F32(5.0f);
    sp[0].contacts.mass = F32(1.0f);
    sp[0].contacts.unknown1c = 2;
    return;
  }
  for (int32_t k = 0; k < s->sphere_count; k++) {
    sp[k].contacts.mass = F32(4.0f);
    sp[k].contacts.unknown1c = 1;
  }
  sp[0].contacts.radius = F32_BITS(0x42158312u); /* 37.38 */
  sp[1].contacts.radius = F32_BITS(0x4223A1CAu); /* 40.91 */
  sp[2].contacts.radius = F32_BITS(0x41EBF3B6u); /* 29.49 */
}
HW_EXPORT(8c056960, swimmer_init(ARG0, ARG1))

/* the update's frame (0x60 bytes): the matrix and the offset go to Ninja */
typedef struct SwimmerFrame {
  Vec3 offset;              /* 0x00: a sphere's point on the body */
  gaddr point;              /* 0x0C */
  union {
    Matrix m;               /* 0x10 */
    struct {
      uint8_t pad_10[0x38 - 0x10];
      int32_t k;            /* 0x38 */
      gaddr slot;           /* 0x3C */
      f32 aim;              /* 0x40 */
      Vec3 diff;            /* 0x44 */
    } steer;
  };
  int32_t sphere;           /* 0x50 */
  gaddr sphere_slot;        /* 0x54 */
  int32_t bubble;           /* 0x58 */
  gaddr self;               /* 0x5C */
} SwimmerFrame;
HW_OFFSET(SwimmerFrame, m, 0x10);
HW_OFFSET(SwimmerFrame, steer.aim, 0x40);
HW_OFFSET(SwimmerFrame, steer.diff, 0x44);
HW_OFFSET(SwimmerFrame, bubble, 0x58);
HW_SIZE(SwimmerFrame, 0x60);

/* place a sphere's point: the offset turned by the matrix */
static void swimmer_place(gaddr frame, f32 x, f32 y, f32 z, gaddr point) {
  vec3_set(x, y, z, member_addr(frame, SwimmerFrame, offset));
  njCalcPoint(member_addr(frame, SwimmerFrame, m), member_addr(frame, SwimmerFrame, offset), point);
}

/* was func_8C056CAC: step the swimmer. A moving one advances its phase round the circle, turns its heading toward
 * the circle's point (by the turn rate, the short way) and is pushed along the heading by its thrust; it finds its
 * cell and its spheres respond to their contacts and take the cell. The spheres' points: the small one's at the
 * body, the big one's three on its body turned by the heading (half a turn). Heading and body step; the wag turns.
 * Each bubble waits its delay then starts at the body, rises (+0.025 a step) and starts again 50 above it; all
 * spheres go into the race scene's slot 8 */
void swimmer_update(gaddr obj) {
  Swimmer *s = (Swimmer *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(SwimmerFrame));
  SwimmerFrame *f = (SwimmerFrame *)hw_ptr(frame);
  f->self = obj;
  if (s->still == 0) {
    s->phase = fadd(s->phase, s->phase_step);
    f32 r = s->radius;
    s->target.x = fmac(njCos(f32_to_int(s->phase)), r, s->center.x);
    r = s->radius;
    s->target.z = fadd(s->center.z, fneg(fmul(r, njSin(f32_to_int(s->phase)))));
    gaddr diff = member_addr(frame, SwimmerFrame, steer.diff);
    vec3_copy(diff, member_addr(obj, Swimmer, target));
    f->steer.diff.x = fsub(f->steer.diff.x, s->body.position.x);
    f->steer.diff.z = fsub(f->steer.diff.z, s->body.position.z);
    f32 a = rt_atan2f(f->steer.diff.x, f->steer.diff.z);
    a = fdiv(fmul(a, F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 2 pi */);
    f->steer.aim = f32_from_int(f32_to_int(a));
    int16_t off = (int16_t)f32_to_int(fsub(s->heading.value, f->steer.aim));
    f32 turn = s->turn_rate;
    if (fgt(f32_from_int(off), F32(0.0f))) turn = fneg(turn);
    f32_add_to(turn, member_addr(obj, Swimmer, heading.force));
    f32 thrust = s->thrust;
    s->body.force.x = fadd(s->body.force.x, fmul(thrust, njSin(f32_to_int(s->heading.value))));
    thrust = s->thrust;
    s->body.force.z = fadd(s->body.force.z, fmul(thrust, njCos(f32_to_int(s->heading.value))));
    actor_locate(obj, member_addr(obj, Swimmer, body), s->base.cell);
    f->steer.slot = s->spheres;
    for (f->steer.k = 0; f->steer.k < s->sphere_count; f->steer.k++, f->steer.slot += sizeof(ContactsSlot)) {
      contacts_call_handlers(f->steer.slot, member_addr(obj, Swimmer, body), 0, 0);
      contacts_at(f->steer.slot)->cell = s->base.cell;
    }
  }
  if (s->kind != 0) {
    *vec3_at(s->points) = s->body.position;
  } else {
    gaddr m = member_addr(frame, SwimmerFrame, m);
    matrix_identity(m);
    njUnitMatrix(m);
    njTranslateV(m, member_addr(obj, Swimmer, body));
    njRotateY(m, f32_to_int(fadd(s->heading.value, F32(32768.0f))));
    f->point = s->points;
    f->point += sizeof(Vec3);
    swimmer_place(frame, F32(0.0f), F32_BITS(0x41273333u) /* 10.45 */, F32_BITS(0x421D926Eu) /* 39.39 */,
                  f->point - sizeof(Vec3));
    f->point += sizeof(Vec3);
    swimmer_place(frame, F32(0.0f), F32_BITS(0x40E35C28u) /* 7.105 */, F32_BITS(0x3FF66666u) /* 1.925 */,
                  f->point - sizeof(Vec3));
    f->point += sizeof(Vec3);
    swimmer_place(frame, F32_BITS(0x3F0F5C28u) /* 0.56 */, F32_BITS(0xBF8A5E35u) /* -1.081 */,
                  F32_BITS(0xC23A020Cu) /* -46.50 */, f->point - sizeof(Vec3));
  }
  body1_step(member_addr(obj, Swimmer, heading));
  body_step(member_addr(obj, Swimmer, body));
  if (s->still == 0) s->wag = fadd(s->wag, F32_BITS(0x45416C16u) /* 3094.88: 17 degrees */);
  for (f->bubble = 0; f->bubble < SWIMMER_BUBBLES; f->bubble++) {
    int32_t k = f->bubble;
    gaddr bubble = member_addr(obj, Swimmer, bubbles[k]);
    switch (s->bubble_state[k]) {
    case 0:
      s->bubble_delay[k]--;
      if (s->bubble_delay[k] == 0) {
        s->bubble_state[k]++;
        vec3_copy_bytes(bubble, member_addr(obj, Swimmer, body));
      }
      break;
    case 1:
      f32_add_to(F32_BITS(0x3CCCCCCCu) /* 0.025 */, member_addr(obj, Swimmer, bubbles[k].force.y));
      body_step(bubble);
      if (fgt(body_at(bubble)->position.y, fadd(s->body.position.y, F32(50.0f)))) {
        s->bubble_delay[k] = 1;
        s->bubble_state[k] = 0;
      }
      break;
    default:
      break;
    }
  }
  f->sphere_slot = s->spheres;
  for (f->sphere = 0; f->sphere < s->sphere_count; f->sphere++) {
    race_scene_slot_insert(g_panel, f->sphere_slot, 8);
    f->sphere_slot += sizeof(ContactsSlot);
  }
  hw_stack_free(sizeof(SwimmerFrame));
}
HW_EXPORT(8c056cac, swimmer_update(ARG0))

/* was func_8C0574FA: v = (x, y, z) (a copy of the setter that returns nothing; its rts is the next function's) */
HW_EXPORT(8c0574fa, vec3_set(FARG0, FARG1, FARG2, ARG0))
