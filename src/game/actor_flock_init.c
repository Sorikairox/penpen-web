/* Pen Pen port, S19 (u): the init method of the flock (type 0x29, Flock in game/actor.h; vtable 0x8C23E31C,
 * slot 1). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "actor.h"
#include "scenery.h"

#define FLOCK_BODY_CTOR 0x8C0551CEu  /* a flock body's constructor (body_reset's work) */
#define FLOCK_SPREAD F32(200.0f)     /* the bodies start within 100 of the start in x and z */
#define FLOCK_HALF_SPREAD F32(-100.0f)
#define FLOCK_RISE F32(20.0f)        /* and up to 20 above it */
#define RAND_SCALE F32(32768.0f)     /* rand() / 32768: 0 to 1 */
#define FLOCK_SPEED F32(10.0f)       /* a speed of up to 10 a second, 60 steps a second */
#define FLOCK_STEPS F32(60.0f)
#define FLOCK_LIFT F32_BITS(0x3E2AAAAAu) /* 0.16666666: the bodies also climb 10 a second */
#define FLOCK_SPRITE_ANIM 0x8C099270u    /* the bodies' NJS_TEXANIM */

/* func_8C062AB0: the C runtime's rand() */
static int32_t rt_rand(void) { return (int32_t)CALL0(0x8C062AB0u); }

/* was func_8C05520E: the init: the start at the placement, waiting, count bodies (param bits 0-7), the trigger three
 * cells back; unless the group re-inits it (flag bit 1), the bodies from operator new[] (48 bytes each, at least one
 * allocated). Each body at rest scattered around the start (x, z within 100, up to 20 above), flying off at a
 * random speed (x, z up to 10 a second, up 10 to 11.25 a second). The sprite: the actor's texlist, the flock's
 * texture animation */
void flock_init(gaddr obj, gaddr placement) {
  Flock *f = (Flock *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  Vec3 start = p->position;
  f->start = start;
  f->state = 0;
  f->count = (int32_t)(p->param & 0xFFu);
  f->trigger_cell = f->base.cell - 3;
  if ((f->base.flags & 2) == 0) {
    int32_t n = f->count != 0 ? f->count : 1;
    gaddr mem = syMalloc((uint32_t)n * sizeof(Body));
    f->bodies = mem != 0 ? array_construct_new(f->count, sizeof(Body), mem, FLOCK_BODY_CTOR) : 0;
  }
  gaddr body = f->bodies;
  for (int32_t i = 0; i < f->count; i++, body += sizeof(Body)) {
    f32 dx = fadd(random_timer_scaled(FLOCK_SPREAD), FLOCK_HALF_SPREAD);
    f32 dy = random_timer_scaled(FLOCK_RISE);
    f32 dz = fadd(random_timer_scaled(FLOCK_SPREAD), FLOCK_HALF_SPREAD);
    body_reset(body);
    Body *b = body_at(body);
    f32 z = fadd(f->start.z, dz), y = fadd(f->start.y, dy), x = fadd(f->start.x, dx);
    b->position.x = x;
    b->position.y = y;
    b->position.z = z;
    vec3_fill(member_addr(body, Body, damping), F32(1.0f));
    f32 sx = fdiv(f32_from_int(rt_rand()), RAND_SCALE);
    f32 sy = fdiv(fdiv(f32_from_int(rt_rand()), RAND_SCALE), F32(8.0f));
    f32 sz = fdiv(f32_from_int(rt_rand()), RAND_SCALE);
    f32 vy = fadd(fdiv(fmul(sy, FLOCK_SPEED), FLOCK_STEPS), FLOCK_LIFT);
    f32 vz = fdiv(fmul(sz, FLOCK_SPEED), FLOCK_STEPS);
    f32 vx = fdiv(fmul(sx, FLOCK_SPEED), FLOCK_STEPS);
    b->velocity.x = vx;
    b->velocity.y = vy;
    b->velocity.z = vz;
  }
  f->sprite.tlist = f->base.texlist;
  f->sprite.tanim = FLOCK_SPRITE_ANIM;
  f->sprite.ang = 0;
  f->sprite.sx = F32(1.0f);
  f->sprite.sy = F32(1.0f);
}
HW_EXPORT(8c05520e, flock_init(ARG0, ARG1))
