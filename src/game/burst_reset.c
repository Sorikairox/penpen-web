/* Pen Pen port, S17 b05: the burst's particles put back at its origin (Burst, game/actor.h; game/burst.c calls it
 * through its guest address as burst_reset). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "actor.h"

/* was func_8C048222: every particle's Body reset, moved to the origin with the burst's damping, and alive (the
 * bodies' array read again at each use, as the original does) */
void burst_reset_particles(gaddr obj) {
  Burst *b = (Burst *)hw_ptr(obj);
  for (int32_t i = 0; i < b->count; i++) {
    gaddr body = b->bodies + (uint32_t)i * sizeof(Body);
    vec3_zero(member_addr(body, Body, position));
    vec3_zero(member_addr(body, Body, velocity));
    vec3_zero(member_addr(body, Body, force));
    vec3_fill(member_addr(body, Body, damping), F32(1.0f));
    Body *p = body_at(b->bodies + (uint32_t)i * sizeof(Body));
    memmove(&p->position, &b->origin, sizeof(Vec3));
    p = body_at(b->bodies + (uint32_t)i * sizeof(Body));
    f32 d = b->damping;
    p->damping.z = d;
    p->damping.y = d;
    p->damping.x = d;
    ((int32_t *)hw_ptr(b->alive))[i] = 1;
  }
}
HW_EXPORT(8c048222, burst_reset_particles(ARG0))
