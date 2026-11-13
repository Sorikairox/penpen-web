/* Pen Pen port, S14 b10: scenery actors that others bump into (game/actor.h): the leaning post (type 0x2E), the
 * big body with eight small spheres (type 0x1E), the wobbler (type 0x33), type 0x19's construction. */
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

/* the radius the big body and the post's contacts get (a static float of the binary) */
#define BALLS_RADIUS F32_BITS(0x42F4B22Cu) /* 122.348 */

/* ---- type 0x2E: the post ---- */

/* was func_8C04D1FC: the post's base construction: the Actor and its contacts (owned by the position, radius 25,
 * no room) */
gaddr post_base_construct(gaddr obj) {
  actor_construct(obj);
  contacts_construct(member_addr(obj, Post, contacts), member_addr(obj, Post, position), F32(25.0f), 0);
  return obj;
}
HW_EXPORT(8c04d1fc, RET(post_base_construct(ARG0)))

/* was func_8C04D220: type 0x2E's constructor: both vtables, then the base */
gaddr post_construct(gaddr obj) {
  Post *p = (Post *)hw_ptr(obj);
  p->base.vtable = 0x8C23E05Cu;
  p->second_vtable = 0x8C23E07Cu;
  post_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04d220, RET(post_construct(ARG0)))

/* the set-up's locals that go to Ninja (its frame: the offset at 4, the matrix at 0x10, a copy of the position at
 * 0x50) */
typedef struct PostFrame {
  uint8_t pad_00[4];
  Vec3 offset;              /* 0x04 */
  Matrix m;                 /* 0x10 */
  Vec3 position;            /* 0x50 */
} PostFrame;
HW_OFFSET(PostFrame, m, 0x10);
HW_SIZE(PostFrame, 0x5C);

/* was func_8C04D2A4: set the post up from its placement: position and angles; its tip is (0.2333, 0, 0) turned
 * about y by its angle and 20 degrees down about z; its contacts take part in others' (cell, response func_8C04D468,
 * the post as their object, kind 0xCD) */
void post_init(gaddr obj, gaddr placement) {
  Post *p = (Post *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  gaddr frame = hw_stack_alloc(sizeof(PostFrame));
  PostFrame *f = (PostFrame *)hw_ptr(frame);
  f->position = pl->position;
  p->position = f->position;
  p->angle.x = pl->angle.x;
  p->angle.y = pl->angle.y;
  p->angle.z = pl->angle.z;
  p->word4c = 0;
  p->f50 = F32(0.0f);
  gaddr m = member_addr(frame, PostFrame, m);
  matrix_identity(m);
  njUnitMatrix(m);
  njRotateY(m, f32_to_int(p->angle.y));
  njRotateZ(m, -0x0E38); /* -20 degrees */
  f->offset.z = F32(0.0f);
  f->offset.y = F32(0.0f);
  f->offset.x = F32_BITS(0x3E6EEEEDu); /* 0.2333 */
  njCalcPoint(m, member_addr(frame, PostFrame, offset), member_addr(obj, Post, tip));
  hw_stack_free(sizeof(PostFrame));
  p->contacts.count = 0;
  p->contacts_word = 4;
  p->contacts.cell = p->base.cell;
  p->contacts.respond = 0x8C04D468u;
  p->contacts.unknown20 = (int32_t)obj;
  p->contacts.unknown1c = 0xCD;
}
HW_EXPORT(8c04d2a4, post_init(ARG0, ARG1))

/* was func_8C04D3B2: put the post's contacts into the race scene's slot 8 */
int32_t post_update(gaddr obj) { return race_scene_slot_insert(g_panel, member_addr(obj, Post, contacts), 8); }
HW_EXPORT(8c04d3b2, RET(post_update(ARG0)))

/* ---- type 0x1E: the big body and its spheres ---- */

#define SPHERE_CONSTRUCTOR 0x8C04D810u

/* was func_8C04D810: a sphere's constructor: contacts with no owner yet, radius 0, room for 4 */
gaddr sphere_construct(gaddr slot) { return contacts_construct(slot, 0, F32(0.0f), 4); }
HW_EXPORT(8c04d810, RET(sphere_construct(ARG0)))

/* was func_8C04D81A: the base construction: the Actor, the swing at rest, the body's contacts (owned by the
 * position, radius 122.35, no room), the 8 spheres and their points; each sphere is owned by its point, radius 5.5
 * (even) or 4.5 (odd) */
gaddr balls_base_construct(gaddr obj) {
  Balls *b = (Balls *)hw_ptr(obj);
  actor_construct(obj);
  b->swing.value = F32(0.0f);
  b->swing.velocity = F32(0.0f);
  b->swing.force = F32(0.0f);
  b->swing.damping = F32(1.0f);
  contacts_construct(member_addr(obj, Balls, contacts), member_addr(obj, Balls, position), BALLS_RADIUS, 0);
  array_construct(BALLS_POINTS, sizeof(ContactsSlot), member_addr(obj, Balls, spheres), SPHERE_CONSTRUCTOR);
  array_construct(BALLS_POINTS, sizeof(Vec3), member_addr(obj, Balls, points), CTOR_EMPTY_VEC3_BALLS);
  b->position.z = F32(0.0f);
  b->position.y = F32(0.0f);
  b->position.x = F32(0.0f);
  for (int32_t k = 0; k < BALLS_POINTS; k++) {
    Contacts *c = &b->spheres[k].contacts;
    c->owner = member_addr(obj, Balls, points[k]);
    c->radius = (k & 1) ? F32(4.5f) : F32(5.5f);
    c->mass = F32(1.0f);
    c->unknown18 = F32(1.0f);
  }
  return obj;
}
HW_EXPORT(8c04d81a, RET(balls_base_construct(ARG0)))

/* was func_8C04D924: type 0x1E's constructor: both vtables, then the base */
gaddr balls_construct(gaddr obj) {
  Balls *b = (Balls *)hw_ptr(obj);
  b->base.vtable = 0x8C23E09Cu;
  b->second_vtable = 0x8C23E0BCu;
  balls_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04d924, RET(balls_construct(ARG0)))

/* was func_8C04D99C: set the body up from its placement: position, param; param 0 swings (state 1, damping 0.95,
 * the static table, 0x77); the contacts take part in others' (cell, response func_8C04DD94, the body as their
 * object, kind 0xCD), every sphere on the body's cell; model 3 of the list loses evalflags bits 0-1 */
void balls_init(gaddr obj, gaddr placement) {
  Balls *b = (Balls *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  b->param = pl->param;
  b->position.z = pl->position.z;
  b->position.y = pl->position.y;
  b->position.x = pl->position.x;
  b->f34 = F32(0.0f);
  b->swing.value = F32(0.0f);
  b->swing.velocity = F32(0.0f);
  b->swing.force = F32(0.0f);
  b->swing.damping = F32(1.0f);
  b->swing.value = F32(0.0f);
  if (b->param == 0) {
    b->state = 1;
    b->swing.velocity = F32(0.0f);
    b->swing.damping = F32_BITS(0x3F733333u); /* 0.95 */
    b->f48 = F32(0.0f);
    b->table = 0x8C08824Cu; /* a static table */
    b->word44 = 0x77;
  } else {
    b->state = 0;
    b->f48 = F32(0.0f);
    b->swing.velocity = F32(0.0f);
    b->swing.damping = F32(0.0f);
  }
  b->contacts.count = 0;
  b->contacts_word = 4;
  b->contacts.cell = b->base.cell;
  b->contacts.owner = member_addr(obj, Balls, position);
  b->contacts.radius = BALLS_RADIUS;
  b->contacts.respond = 0x8C04DD94u;
  b->contacts.unknown20 = (int32_t)obj;
  b->contacts.unknown1c = 0xCD;
  for (int32_t k = 0; k < BALLS_POINTS; k++) {
    b->cells[k] = b->base.cell;
    b->spheres[k].contacts.cell = b->base.cell;
  }
  NjObject *model = (NjObject *)hw_ptr(actor_model(&b->base, 3));
  model->evalflags &= ~3u;
}
HW_EXPORT(8c04d99c, balls_init(ARG0, ARG1))

/* ---- type 0x33: the wobbler ---- */

#define g_wobble_rate   GUEST_VAR(f32, 0x8C088310u) /* 0.1 */
#define g_wobble_radius GUEST_VAR(f32, 0x8C08830Cu) /* 5.9999995 */

/* was func_8C04F8FC: this unit's copy of body_reset (core/vec.c) */
HW_EXPORT(8c04f8fc, body_reset(ARG0))

/* was func_8C04FA18: the base construction: the Actor, the three wobbles at rest, the Body at rest at the origin,
 * the contacts owned by the position (radius 6, room for 2) */
gaddr wobbler_base_construct(gaddr obj) {
  Wobbler *w = (Wobbler *)hw_ptr(obj);
  actor_construct(obj);
  for (int32_t k = 0; k < 3; k++) {
    w->wobble[k].value = F32(0.0f);
    w->wobble[k].velocity = F32(0.0f);
    w->wobble[k].force = F32(0.0f);
    w->wobble[k].damping = F32(1.0f);
  }
  gaddr body = member_addr(obj, Wobbler, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  contacts_construct(member_addr(obj, Wobbler, contacts), member_addr(obj, Wobbler, position), g_wobble_radius, 2);
  return obj;
}
HW_EXPORT(8c04fa18, RET(wobbler_base_construct(ARG0)))

/* was func_8C04FAB4: type 0x33's constructor: both vtables, then the base */
gaddr wobbler_construct(gaddr obj) {
  Wobbler *w = (Wobbler *)hw_ptr(obj);
  w->base.vtable = 0x8C23E11Cu;
  w->second_vtable = 0x8C23E13Cu;
  wobbler_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04fab4, RET(wobbler_construct(ARG0)))

/* was func_8C04FE68: every contact with a body of kind 7 or less moving faster than 0.463 pushes the three wobbles;
 * they are pulled back to rest and step; the clock counts; the contacts go into the race scene's slot 8 */
void wobbler_update(gaddr obj) {
  Wobbler *w = (Wobbler *)hw_ptr(obj);
  for (int32_t i = 0; i < w->contacts.count; i++) {
    if (((const int32_t *)hw_ptr(w->contacts.other))[i] > 7) continue;
    if (!fgt(njScalor(w->contacts.velocity + (gaddr)i * sizeof(Vec3)), F32_BITS(0x3EED097Au) /* 0.463 */)) continue;
    f32_add_to(g_wobble_rate, member_addr(obj, Wobbler, wobble[0].force));
    f32_add_to(g_wobble_rate, member_addr(obj, Wobbler, wobble[2].force));
    f32_add_to(g_wobble_rate, member_addr(obj, Wobbler, wobble[1].force));
  }
  body1_pull(w->rest, g_wobble_rate, member_addr(obj, Wobbler, wobble[0]));
  body1_pull(w->rest, g_wobble_rate, member_addr(obj, Wobbler, wobble[2]));
  body1_pull(w->rest, g_wobble_rate, member_addr(obj, Wobbler, wobble[1]));
  body1_step(member_addr(obj, Wobbler, wobble[0]));
  body1_step(member_addr(obj, Wobbler, wobble[1]));
  body1_step(member_addr(obj, Wobbler, wobble[2]));
  w->clock = fadd(w->clock, F32(1.0f));
  race_scene_slot_insert(panel_get(), member_addr(obj, Wobbler, contacts), 8);
}
HW_EXPORT(8c04fe68, wobbler_update(ARG0))

/* ---- type 0x19 ---- */

/* was func_8C05049C: the base construction: the Actor, three points (empty), three turns at identity, the Body at
 * rest, its contacts (radius 1.9999999, room for 2) and the point at 0x14C (empty) */
gaddr type19_base_construct(gaddr obj) {
  actor_construct(obj);
  for (int32_t k = 0; k < 3; k++) method_self(member_addr(obj, ActorType19, points[k])); /* empty constructors */
  for (int32_t k = 0; k < 3; k++) quat_identity(member_addr(obj, ActorType19, turns[k]));
  body_init(member_addr(obj, ActorType19, body));
  gaddr owner = method_self(member_addr(obj, ActorType19, body));
  contacts_construct(member_addr(obj, ActorType19, contacts), owner, GUEST_VAR(f32, 0x8C088318u) /* 1.9999999 */, 2);
  method_self(member_addr(obj, ActorType19, point14c));
  return obj;
}
HW_EXPORT(8c05049c, RET(type19_base_construct(ARG0)))

/* was func_8C050504: type 0x19's constructor: both vtables, then the base */
gaddr type19_construct(gaddr obj) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  a->base.vtable = 0x8C23E15Cu;
  a->second_vtable = 0x8C23E17Cu;
  type19_base_construct(obj);
  return obj;
}
HW_EXPORT(8c050504, RET(type19_construct(ARG0)))

/* ---- type 0x24 ---- */

#define CTOR_EMPTY_VEC3_24 0x8C051C5Eu

/* was func_8C051D26: this unit's copy of body_reset (core/vec.c) */
HW_EXPORT(8c051d26, body_reset(ARG0))

/* a Body at rest at the origin, through the setters */
static void body_set_rest(gaddr body) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
}

/* was func_8C051EA4: type 0x24's base construction: the Actor, two sets of four (empty) points, four Body1s at
 * rest, both Bodies at rest at the origin and the contacts owned by the first (radius 2.9999998, room for 2) */
gaddr type24_base_construct(gaddr obj) {
  ActorType24 *a = (ActorType24 *)hw_ptr(obj);
  actor_construct(obj);
  array_construct(4, sizeof(Vec3), member_addr(obj, ActorType24, points_a), CTOR_EMPTY_VEC3_24);
  array_construct(4, sizeof(Vec3), member_addr(obj, ActorType24, points_b), CTOR_EMPTY_VEC3_24);
  for (int32_t k = 0; k < 4; k++) {
    a->values[k].value = F32(0.0f);
    a->values[k].velocity = F32(0.0f);
    a->values[k].force = F32(0.0f);
    a->values[k].damping = F32(1.0f);
  }
  body_set_rest(member_addr(obj, ActorType24, body));
  body_set_rest(member_addr(obj, ActorType24, body2));
  contacts_construct(member_addr(obj, ActorType24, contacts), member_addr(obj, ActorType24, body),
                     GUEST_VAR(f32, 0x8C08842Cu) /* 2.9999998 */, 2);
  return obj;
}
HW_EXPORT(8c051ea4, RET(type24_base_construct(ARG0)))

/* was func_8C051FE0: type 0x24's constructor: both vtables, then the base */
gaddr type24_construct(gaddr obj) {
  ActorType24 *a = (ActorType24 *)hw_ptr(obj);
  a->base.vtable = 0x8C23E19Cu;
  a->second_vtable = 0x8C23E1BCu;
  type24_base_construct(obj);
  return obj;
}
HW_EXPORT(8c051fe0, RET(type24_construct(ARG0)))

/* was func_8C05205C: set type 0x24 up from its placement: its kind (param bits 0-3) picks the model, the motion
 * and the spread; its home and body at the position (damping 0.9); the motion starts on a random frame; the
 * placement's angles; three Body1s at 1 (damping 0.95) and one at 0.9 (damping 0.9); four random points within
 * the spread (20, or 6) around 0, copied; the contacts take part in others' (radius 3, mass 0.2, kind 3, the
 * body's velocity, the actor's cell) */
void type24_init(gaddr obj, gaddr placement) {
  ActorType24 *a = (ActorType24 *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  a->kind = (int32_t)(pl->param & 0xF);
  a->motion = a->kind;
  a->size = (int32_t)(pl->param & 0xF);
  vec3_set(pl->position.x, pl->position.y, pl->position.z, member_addr(obj, ActorType24, home));
  gaddr body = member_addr(obj, ActorType24, body);
  body_reset(body);
  vec3_copy_bytes(body, member_addr(obj, ActorType24, home));
  vec3_fill(member_addr(body, Body, damping), F32_BITS(0x3F666666u)); /* 0.9 */
  gaddr models = ((const ActorList *)hw_ptr(a->base.model_set))->list;
  a->model = ((const gaddr *)hw_ptr(models))[a->kind];
  a->word184 = ((const ActorList *)hw_ptr(a->base.texlist))->list;
  a->motions = ((const ActorList *)hw_ptr(a->base.word1c))->list;
  const MotionEntry *m = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  a->frame_count = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(m->motion))->nb_frames - 1));
  a->frame = f32_from_int(f32_to_int(random_timer_scaled(a->frame_count)));
  a->angle.x = pl->angle.x;
  a->angle.y = pl->angle.y;
  a->angle.z = pl->angle.z;
  for (int32_t k = 0; k < 3; k++) {
    quat_identity(member_addr(obj, ActorType24, values[k]));
    a->values[k].damping = GUEST_VAR(f32, 0x8C088434u); /* 0.95 */
    a->values[k].value = F32(1.0f);
  }
  quat_identity(member_addr(obj, ActorType24, values[3]));
  a->values[3].damping = F32_BITS(0x3F666666u); /* 0.9 */
  a->values[3].value = F32_BITS(0x3F666666u);
  f32 spread = a->size == 0 ? GUEST_VAR(f32, 0x8C088424u) /* 20.0 */ : GUEST_VAR(f32, 0x8C088428u) /* 5.9999995 */;
  for (int32_t i = 0; i < 4; i++) {
    Vec3 *p = &a->points_a[i];
    p->x = fsub(random_scaled(spread), fdiv(spread, fadd(F32(1.0f), F32(1.0f))));
    p->y = fsub(random_scaled(spread), fdiv(spread, fadd(F32(1.0f), F32(1.0f))));
    p->z = fsub(random_scaled(spread), fdiv(spread, fadd(F32(1.0f), F32(1.0f))));
  }
  a->f54 = F32(0.0f);
  a->word44 = 0;
  for (int32_t j = 0; j < 4; j++) a->points_b[j] = a->points_a[j];
  a->word48 = 0;
  a->contacts.owner_velocity = member_addr(body, Body, velocity);
  a->contacts.unknown18 = F32(1.0f);
  a->contacts.radius = GUEST_VAR(f32, 0x8C08842Cu); /* 2.9999998 */
  a->contacts.mass = F32_BITS(0x3E4CCCCCu);          /* 0.2 */
  a->contacts.unknown1c = 3;
  a->contacts.cell = a->base.cell;
}
HW_EXPORT(8c05205c, type24_init(ARG0, ARG1))

/* type 0x19's steps by state (still translated) */
static void type19_step2(gaddr obj) { CALL1(0x8C0512A4u, obj); } /* func_8C0512A4 */

/* was func_8C050A74: step type 0x19: its clock counts by 1 (2 when not slow), the step of its state runs, its
 * three turns step and its contacts go into the race scene's slot 8 */
void type19_update(gaddr obj) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  a->clock = fadd(a->clock, F32(1.0f));
  if (a->slow == 0) a->clock = fadd(a->clock, F32(1.0f));
  switch (a->state) {
  case 0: type19_step0(obj); break;
  case 1: type19_step1(obj); break;
  case 2: type19_step2(obj); break;
  case 3: type19_step3(obj); break;
  default: break;
  }
  for (int32_t k = 0; k < 3; k++) body1_step(member_addr(obj, ActorType19, turns[k]));
  race_scene_slot_insert(g_panel, member_addr(obj, ActorType19, contacts), 8);
}
HW_EXPORT(8c050a74, type19_update(ARG0))

/* ---- an actor resting where it is placed ----
 * The init of an actor class (its type is not mapped yet) that keeps a Body after the Actor base, the placement's
 * kind bits and its three angles. Ghidra cut the function short at a literal-pool-like word, so the recompiler
 * translated only its first part and let the interpreter run the rest with a stale return address: written whole
 * here (S14: the cup4 route stopped there in the hand build). */
typedef struct RestingActor {
  Actor base;          /* 0x00 */
  Body body;           /* 0x28 */
  int32_t kind;        /* 0x58: the placement's param, low 4 bits */
  uint32_t word5c;     /* 0x5C: 0 */
  int32_t level;       /* 0x60: the placement's param >> 4 (signed) */
  Vec3 angle;          /* 0x64: the placement's angles */
} RestingActor;
HW_OFFSET(RestingActor, body, 0x28);
HW_OFFSET(RestingActor, kind, 0x58);
HW_OFFSET(RestingActor, angle, 0x64);

/* was func_8C059CAC (with its tail at 0x8C059CD0): the Body at rest at the placement's position, no damping and no
 * velocity, then the placement's angles and param */
void resting_actor_init(gaddr actor, gaddr placement) {
  RestingActor *a = (RestingActor *)hw_ptr(actor);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  gaddr body = member_addr(actor, RestingActor, body);
  vec3_zero(member_addr(body, Body, position));
  vec3_zero(member_addr(body, Body, velocity));
  vec3_zero(member_addr(body, Body, force));
  vec3_fill(member_addr(body, Body, damping), F32(1.0f));
  f32 z = p->position.z, y = p->position.y, x = p->position.x;
  a->body.position.x = x;
  a->body.position.y = y;
  a->body.position.z = z;
  a->body.damping.z = a->body.damping.y = a->body.damping.x = F32(0.0f);
  a->body.velocity.x = a->body.velocity.y = a->body.velocity.z = F32(0.0f);
  a->angle.x = p->angle.x;
  a->angle.y = p->angle.y;
  a->angle.z = p->angle.z;
  a->kind = (int32_t)(p->param & 0xF);
  a->level = (int32_t)p->param >> 4;
  a->word5c = 0;
}
HW_EXPORT(8c059cac, resting_actor_init(ARG0, ARG1))
