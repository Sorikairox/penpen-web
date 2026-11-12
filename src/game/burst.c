/* Pen Pen port, S14 b10: a burst of particles (Burst, game/actor.h): when triggered, `count` Bodies start at the
 * origin with random velocities, fall under gravity and die below the origin's depth; when all are dead the burst
 * frees them and is over. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "actor.h"
#include "scenery.h"

#define BURST_VTABLE           0x8C23DD8Cu
#define BURST_BODY_CONSTRUCTOR 0x8C047FD8u /* a Body at rest */

/* func_8C048222: every particle at rest at the origin with the burst's damping, alive (still translated) */
static void burst_reset(gaddr obj) { CALL1(0x8C048222u, obj); }

/* was func_8C047EB6: the burst's constructor: its vtable, `count` particles, no blocks yet */
gaddr burst_construct(gaddr obj, uint32_t count) {
  ((Burst *)hw_ptr(obj))->vtable = BURST_VTABLE;
  buffer_pair_init(obj, count);
  return obj;
}
HW_EXPORT(8c047eb6, RET(burst_construct(ARG0, ARG1)))

/* was func_8C047F02: the deleting destructor: the blocks freed */
gaddr burst_delete(gaddr obj, int32_t free_memory) {
  buffer_pair_free(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c047f02, RET(burst_delete(ARG0, (int32_t)ARG1)))

/* was func_8C048018: step the burst. Idle: once triggered, allocate the particles' Bodies and alive flags, reset
 * them and give each a random velocity between the two limits. Flying: each live particle falls by the gravity and
 * moves, and dies below origin.y + depth; once none is alive the blocks are freed and the burst is over */
void burst_update(gaddr obj) {
  Burst *b = (Burst *)hw_ptr(obj);
  switch (b->state) {
  case 0: {
    if (b->trigger == 0) return;
    b->state++;
    int32_t n = b->count != 0 ? b->count : 1;
    gaddr block = syMalloc((uint32_t)n * sizeof(Body));
    b->bodies = block != 0 ? array_construct_new(b->count, sizeof(Body), block, BURST_BODY_CONSTRUCTOR) : 0;
    n = b->count != 0 ? b->count : 1;
    b->alive = syMalloc((uint32_t)n * 4);
    burst_reset(obj);
    b->trigger = 0;
    Vec3 range;
    range.x = fsub(b->velocity_max.x, b->velocity_min.x);
    range.y = fsub(b->velocity_max.y, b->velocity_min.y);
    range.z = fsub(b->velocity_max.z, b->velocity_min.z);
    for (int32_t i = 0; i < b->count; i++) {
      Vec3 v;
      v.x = fadd(random_timer_scaled(range.x), b->velocity_min.x);
      v.y = fadd(random_timer_scaled(range.y), b->velocity_min.y);
      v.z = fadd(random_timer_scaled(range.z), b->velocity_min.z);
      body_at(b->bodies + (gaddr)i * sizeof(Body))->velocity = v;
    }
    break;
  }
  case 1: {
    f32 floor = fadd(b->depth, b->origin.y);
    int32_t none_alive = 1;
    for (int32_t i = 0; i < b->count; i++) {
      int32_t *alive = &((int32_t *)hw_ptr(b->alive))[i];
      if (*alive == 0) continue;
      gaddr body = b->bodies + (gaddr)i * sizeof(Body);
      Body *p = body_at(body);
      p->velocity.y = fadd(p->velocity.y, b->gravity);
      body_step(body);
      none_alive = 0;
      if (fgt(floor, body_at(b->bodies + (gaddr)i * sizeof(Body))->position.y))
        ((int32_t *)hw_ptr(b->alive))[i] = 0;
    }
    if (!none_alive) break;
    if (b->bodies != 0) syFree(b->bodies);
    if (b->alive != 0) syFree(b->alive);
    b->bodies = 0;
    b->alive = 0;
    b->state++;
    break;
  }
  default:
    break;
  }
}
HW_EXPORT(8c048018, burst_update(ARG0))
