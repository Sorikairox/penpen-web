/* Pen Pen port, S14 b10: rising things: the bubbles (type 7: three bubbles that rise from the origin one after
 * the other) and the spring (type 8: it waits, then sends a set of motes up, again and again), with the mote set
 * they use (MemberArray, game/actor.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "actor.h"
#include "scenery.h"

/* ---- type 7: the bubbles ---- */

#define BUBBLE_BODY_CONSTRUCTOR 0x8C04C188u /* body_reset_set3's copy below */

/* was func_8C04C188: this unit's copy of body_reset_set3 (game/actor.c): a Body at rest, through the setters */
HW_EXPORT(8c04c188, RET(body_reset_set3(ARG0)))

/* was func_8C04C1C8: the bubbles' base construction: the Actor and the three Bodies */
gaddr bubbles_base_construct(gaddr obj) {
  actor_construct(obj);
  array_construct(BUBBLE_COUNT, sizeof(Body), member_addr(obj, Bubbles, bodies), BUBBLE_BODY_CONSTRUCTOR);
  return obj;
}
HW_EXPORT(8c04c1c8, RET(bubbles_base_construct(ARG0)))

/* was func_8C04C1E8: type 7's constructor: both vtables, then the base */
gaddr bubbles_construct(gaddr obj) {
  Bubbles *b = (Bubbles *)hw_ptr(obj);
  b->base.vtable = 0x8C23DF94u;
  b->second_vtable = 0x8C23DFB4u;
  bubbles_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04c1e8, RET(bubbles_construct(ARG0)))

/* was func_8C04C2CC: start bubble i again: its Body at rest at the origin (damping 0.9), its timer at -60 i, hidden,
 * its sprite 8x8 centred on (4, 4) over the whole texture */
void bubble_start(gaddr obj, int32_t i) {
  Bubbles *b = (Bubbles *)hw_ptr(obj);
  gaddr body = member_addr(obj, Bubbles, bodies[i]);
  vec3_zero(member_addr(body, Body, position));
  vec3_zero(member_addr(body, Body, velocity));
  vec3_zero(member_addr(body, Body, force));
  vec3_fill(member_addr(body, Body, damping), F32(1.0f));
  Body *p = &b->bodies[i];
  const f32 damping = F32_BITS(0x3F666666u); /* 0.9 */
  p->damping.z = damping;
  p->damping.y = damping;
  p->damping.x = damping;
  p->position = b->origin;
  b->timer[i] = -(60 * i);
  b->shown[i] = 0;
  BubbleSprite *s = &b->sprites[i];
  s->w = 8;
  s->h = 8;
  s->cx = 4;
  s->cy = 4;
  s->texid = 0;
  s->v1 = 0;
  s->u1 = 0;
  s->v2 = 0xFF;
  s->u2 = 0xFF;
  s->attr = 0;
}
HW_EXPORT(8c04c2cc, bubble_start(ARG0, (int32_t)ARG1))

/* was func_8C04C260: set the bubbles up from their placement: the origin, the three bubbles started, each 20 param
 * steps later */
void bubbles_init(gaddr obj, gaddr placement) {
  Bubbles *b = (Bubbles *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  b->origin = pl->position;
  for (int32_t i = 0; i < BUBBLE_COUNT; i++) {
    bubble_start(obj, i);
    b->timer[i] -= (int32_t)(pl->param * 20);
  }
}
HW_EXPORT(8c04c260, bubbles_init(ARG0, ARG1))

/* was func_8C04C3BA: step the bubbles: each counts its timer up; once past 0 it is shown, pushed up (and a little
 * along x and back along z) and moves; past 240 it starts again */
void bubbles_update(gaddr obj) {
  Bubbles *b = (Bubbles *)hw_ptr(obj);
  gaddr push = hw_stack_alloc(sizeof(Vec3));
  for (int32_t i = 0; i < BUBBLE_COUNT; i++) {
    b->timer[i]++;
    if (b->timer[i] <= 0) continue;
    b->shown[i] = 1;
    Vec3 *v = vec3_at(push);
    v->z = F32_BITS(0xBB360B60u); /* -0.0027777 */
    v->y = F32_BITS(0x3D2AAAAAu); /* 0.041666 */
    v->x = F32_BITS(0x3B360B60u); /* 0.0027777 */
    vec3_add(member_addr(obj, Bubbles, bodies[i].force), push);
    body_step(member_addr(obj, Bubbles, bodies[i]));
    if (b->timer[i] > 240) bubble_start(obj, i);
  }
  hw_stack_free(sizeof(Vec3));
}
HW_EXPORT(8c04c3ba, bubbles_update(ARG0))

/* ---- the mote set (MemberArray) ---- */

#define MOTES_VTABLE 0x8C23DFD4u
#define MOTE_DRIFT ((const Vec3 *)hw_ptr(0x8C0881D4u)) /* by sprite frame: the push of a rising mote */

/* was func_8C04C76A: the mote set's constructor: its vtable, then `count` motes (member_array_init) */
gaddr motes_construct(gaddr obj, int32_t count) {
  ((MemberArray *)hw_ptr(obj))->vtable = MOTES_VTABLE;
  member_array_init(obj, count);
  return obj;
}
HW_EXPORT(8c04c76a, RET(motes_construct(ARG0, (int32_t)ARG1)))

/* was func_8C04C7CA: the mote set's destructor body (the motes freed) */
gaddr motes_destroy(gaddr obj) {
  member_array_free(obj);
  return obj;
}
HW_EXPORT(8c04c7ca, RET(motes_destroy(ARG0)))

/* a mote at rest at the set's origin, slowed by 0.95, waiting 5 i + 1 steps */
static void mote_reset(gaddr set, gaddr mote) {
  vec3_zero(member_addr(mote, Body, position));
  vec3_zero(member_addr(mote, Body, velocity));
  vec3_zero(member_addr(mote, Body, force));
  vec3_fill(member_addr(mote, Body, damping), F32(1.0f));
  ((Mote *)hw_ptr(mote))->body.position = ((const MemberArray *)hw_ptr(set))->origin;
}

/* was func_8C04C7DC: set the motes up at `origin`: each at rest there (damping 0.95), its frame i % 10 (looping back
 * to 6 - i % 5), waiting 5 i + 1 steps, its kind by the mode; the fade at 0.3, the top, the shared texture */
void motes_setup(gaddr obj, gaddr origin, int32_t mode, f32 top) {
  MemberArray *a = (MemberArray *)hw_ptr(obj);
  gaddr mote = a->array;
  a->origin = *vec3_at(origin);
  a->mode = mode;
  const f32 damping = F32_BITS(0x3F733333u); /* 0.95 */
  for (int32_t i = 0; i < a->count; i++, mote += sizeof(Mote)) {
    Mote *m = (Mote *)hw_ptr(mote);
    mote_reset(obj, mote);
    m->body.damping.z = damping;
    m->body.damping.y = damping;
    m->body.damping.x = damping;
    m->frame_timer = 0;
    m->frame = i % 10;
    m->loop_frame = 6 - i % 5;
    m->state = 0;
    m->delay = 5 * i + 1;
    if (mode == 0)
      m->kind = i == 0 ? 1 : 3;
    else if (mode == 1)
      m->kind = i & 1;
  }
  a->fade.value = F32(0.0f);
  a->fade.velocity = F32(0.0f);
  a->fade.force = F32(0.0f);
  a->fade.damping = F32(1.0f);
  a->fade.value = F32_BITS(0x3E999999u); /* 0.3 */
  a->fade.damping = F32(1.0f);
  a->top = top;
  a->texlist = 0x8C09703Cu; /* the shared texture list (static) and its texture */
  a->texanim = 0x8C097044u;
}
HW_EXPORT(8c04c7dc, motes_setup(ARG0, ARG1, (int32_t)ARG2, FARG0))

/* was func_8C04C924: start the motes again: each at rest at the origin, waiting 5 i + 1 steps */
void motes_restart(gaddr obj) {
  const MemberArray *a = (const MemberArray *)hw_ptr(obj);
  gaddr mote = a->array;
  for (int32_t i = 0; i < a->count; i++, mote += sizeof(Mote)) {
    Mote *m = (Mote *)hw_ptr(mote);
    mote_reset(obj, mote);
    m->state = 0;
    m->frame_timer = 0;
    m->delay = 5 * i + 1;
  }
}
HW_EXPORT(8c04c924, motes_restart(ARG0))

/* was func_8C04C986: step the motes: a waiting one counts down its delay; a rising one steps its sprite frame
 * (every i + 6 steps), is pushed by its frame's drift and up by 0.0833 - 0.00278 i, moves, and is done above the
 * top. The fade is pulled toward 0 (rate 0.08). Returns 1 when no mote waits or rises any more */
int32_t motes_update(gaddr obj) {
  const MemberArray *a = (const MemberArray *)hw_ptr(obj);
  gaddr mote = a->array;
  int32_t all_done = 1;
  gaddr push = hw_stack_alloc(sizeof(Vec3));
  for (int32_t i = 0; i < a->count; i++, mote += sizeof(Mote)) {
    Mote *m = (Mote *)hw_ptr(mote);
    switch (m->state) {
    case 0:
      if (--m->delay == 0) m->state++;
      all_done = 0;
      break;
    case 1: {
      if (++m->frame_timer > i + 5) {
        m->frame_timer = 0;
        if (++m->frame >= 10) m->frame = m->loop_frame;
      }
      const Vec3 *d = &MOTE_DRIFT[m->frame];
      Vec3 *v = vec3_at(push);
      v->z = d->z;
      v->y = d->y;
      v->x = d->x;
      vec3_add(member_addr(mote, Body, force), push);
      f32 lift = fsub(F32_BITS(0x3DAAAAAAu) /* 0.0833 */, fmul(f32_from_int(i), F32_BITS(0x3B360B60u) /* 0.00278 */));
      m->body.force.y = fadd(m->body.force.y, lift);
      body_step(mote);
      if (fgt(m->body.position.y, a->top)) m->state++;
      all_done = 0;
      break;
    }
    default:
      break;
    }
  }
  hw_stack_free(sizeof(Vec3));
  gaddr fade = member_addr(obj, MemberArray, fade);
  body1_pull(F32(0.0f), F32_BITS(0x3DA3D70Au) /* 0.08 */, fade);
  body1_step(fade);
  return all_done;
}
HW_EXPORT(8c04c986, RET(motes_update(ARG0)))

/* ---- type 8: the spring ---- */

/* was func_8C04CB0C: the spring's base construction: the Actor and a set of 5 motes */
gaddr spring_base_construct(gaddr obj) {
  actor_construct(obj);
  motes_construct(member_addr(obj, Spring, motes), 5);
  return obj;
}
HW_EXPORT(8c04cb0c, RET(spring_base_construct(ARG0)))

/* was func_8C04CB26: type 8's constructor: both vtables, then the base */
gaddr spring_construct(gaddr obj) {
  Spring *s = (Spring *)hw_ptr(obj);
  s->base.vtable = 0x8C23DFDCu;
  s->second_vtable = 0x8C23DFFCu;
  spring_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04cb26, RET(spring_construct(ARG0)))

/* was func_8C04CBC6: set the spring up from its placement: waiting `param` steps first, at its position; the motes
 * set up there (mode 0, done below -550) */
void spring_init(gaddr obj, gaddr placement) {
  Spring *s = (Spring *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  s->state = 0;
  s->countdown = (int32_t)pl->param;
  s->position = pl->position;
  motes_setup(member_addr(obj, Spring, motes), member_addr(obj, Spring, position), 0, F32(-550.0f));
}
HW_EXPORT(8c04cbc6, spring_init(ARG0, ARG1))

/* was func_8C04CC10: step the spring: wait out the countdown, then let the motes rise until they are all done,
 * then start them again */
void spring_update(gaddr obj) {
  Spring *s = (Spring *)hw_ptr(obj);
  switch (s->state) {
  case 0:
    if (--s->countdown <= 0) s->state++;
    break;
  case 1:
    if (motes_update(member_addr(obj, Spring, motes))) s->state++;
    break;
  case 2:
    s->state = 0;
    motes_restart(member_addr(obj, Spring, motes));
    break;
  default:
    break;
  }
}
HW_EXPORT(8c04cc10, spring_update(ARG0))
