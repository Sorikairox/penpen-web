/* Pen Pen port, S18 (b03): a particle set of falling models (game/effects_drops.c), beside the racer's effect sets
 * (game/effects.h) but owned by none of RacerEffects' slots: its constructor (func_8C03A4C8) and its methods have no
 * direct caller in the game code (only data references).
 *
 * The set keeps g_drop_count (16) particles as per-particle arrays, like the other sets; each particle is a model
 * of the shared container (entry g_drop_object_entry) thrown up from the racer's frame (RibbonFrame.vec58, a little
 * lower, then sideways by a random angle), turned to face its velocity, growing while it rises and falls, and
 * switched off once it has fallen below the height it started from. */
#include "hw.h"
#include "sdk.h"
#include "effects.h"
#include "scenery.h" /* random_timer_scaled, array_construct_new */
#include "../core/vec.h"
#include "../core/ninja.h"

#define DROP_SLOTS 16 /* the arrays' size: fixed at construction (g_drop_count may not exceed it) */

typedef struct EffectDrops {
  gaddr object;        /* 0x00: the model drawn (the shared container's entry g_drop_object_entry) */
  gaddr counters;      /* 0x04: int32: g_drop_counter_start when a drop starts (not read here) */
  gaddr states;        /* 0x08: int32: 0 off, 1 flying, 2 landed (off at the next update) */
  gaddr sizes;         /* 0x0C: f32: the model's scale */
  gaddr array10;       /* 0x10: f32: 0 at construction (not read here) */
  gaddr floors;        /* 0x14: f32: the height the drop started from (it lands below it) */
  gaddr bodies;        /* 0x18: a Body per drop */
  gaddr pitches;       /* 0x1C: a Quat per drop (its first word: the pitch, Ninja angle units) */
  gaddr headings;      /* 0x20: a Quat per drop (its first word: the heading) */
} EffectDrops;
HW_OFFSET(EffectDrops, states, 0x08);
HW_OFFSET(EffectDrops, floors, 0x14);
HW_OFFSET(EffectDrops, bodies, 0x18);
HW_OFFSET(EffectDrops, headings, 0x20);
HW_SIZE(EffectDrops, 0x24);

#define QUAT_SIZE 16
enum { DROP_OFF = 0, DROP_FLYING = 1, DROP_LANDED = 2 };

#define g_shared_entry0       GUEST_VAR(gaddr, 0x8C096D14u)   /* model.c's: the shared container's table of models */
#define g_drop_counter_start  GUEST_VAR(int32_t, 0x8C083078u) /* 10 */
#define g_drop_size_start     GUEST_VAR(f32, 0x8C08307Cu)     /* 0.2 */
#define g_drop_size_limit     GUEST_VAR(f32, 0x8C083080u)     /* 0.6: grown toward over 30 updates */
#define g_drop_damping        GUEST_VAR(f32, 0x8C083084u)     /* 0.95 */
#define g_drop_min_speed      GUEST_VAR(f32, 0x8C083088u)     /* 0.1: the racer's speed (RibbonFrame.f78) needed */
#define g_drop_count          GUEST_VAR(int32_t, 0x8C08308Cu) /* 16 */
#define g_drop_object_entry   GUEST_VAR(int32_t, 0x8C083090u) /* 3 */

#define DROP_GRAVITY      F32_BITS(0xBD85CD79u) /* -0.0653: added to the force's y every update */
#define DROP_GROW_STEPS   F32(30.0f)
#define DROP_START_DROP   F32_BITS(0xBFFFFFFFu) /* -2 (just under): the start below the frame's point */
#define DROP_SPREAD       F32_BITS(0x3F199999u) /* 0.6: of the frame's radius (RibbonFrame.low) */
#define DROP_PUSH         F32_BITS(0x3E4CCCCCu) /* 0.2: of the racer's speed, sideways and up */
#define DROP_LIFT         F32_BITS(0x3E999999u) /* 0.3: the upward push's base */
#define ANGLE_FULL        F32(65536.0f)         /* a turn in Ninja angle units */
#define TWO_PI            F32_BITS(0x40C90FD7u) /* 6.2831836 */

static EffectDrops *drops_at(gaddr a) { return (EffectDrops *)hw_ptr(a); }
static int32_t *int_at(gaddr array, int32_t i) { return &((int32_t *)hw_ptr(array))[i]; }
static f32 *float_at(gaddr array, int32_t i) { return &((f32 *)hw_ptr(array))[i]; }
static gaddr body_of(const EffectDrops *d, int32_t i) { return d->bodies + (gaddr)i * sizeof(Body); }
static gaddr quat_of(gaddr array, int32_t i) { return array + (gaddr)i * QUAT_SIZE; }

/* func_8C062790 behind operator new[]: an array built in a fresh block, 0 when the block is not there */
static gaddr array_new(int32_t n, uint32_t size, gaddr ctor) {
  gaddr block = syMalloc((uint32_t)n * size);
  return block ? array_construct_new(n, size, block, ctor) : 0;
}
#define BODY_CONSTRUCT 0x8C0391F2u /* body_reset_set (game/model.c) */
#define QUAT_CONSTRUCT 0x8C03A4C2u /* quat_identity's thunk (core/ninja_thunks_b.c) */

/* rt_atan2f: func_8C0628E8 (C runtime, float): atan2f(y, x) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}
/* radians to Ninja angle units, truncated to a whole unit */
static f32 angle_units(f32 radians) { return f32_from_int(f32_to_int(fdiv(fmul(radians, ANGLE_FULL), TWO_PI))); }

/* was func_8C03A4C8: build the set: its model, the arrays (16 slots each; the bodies and the two quaternions
 * constructed), and every drop off at size 0; returns the set */
gaddr effect_drops_construct(gaddr set) {
  EffectDrops *d = drops_at(set);
  d->object = ((const gaddr *)hw_ptr(g_shared_entry0))[g_drop_object_entry];
  d->counters = syMalloc(DROP_SLOTS * 4);
  d->states = syMalloc(DROP_SLOTS * 4);
  d->sizes = syMalloc(DROP_SLOTS * 4);
  d->array10 = syMalloc(DROP_SLOTS * 4);
  d->floors = syMalloc(DROP_SLOTS * 4);
  d->bodies = array_new(DROP_SLOTS, sizeof(Body), BODY_CONSTRUCT);
  d->pitches = array_new(DROP_SLOTS, QUAT_SIZE, QUAT_CONSTRUCT);
  d->headings = array_new(DROP_SLOTS, QUAT_SIZE, QUAT_CONSTRUCT);
  for (int32_t i = 0; i < g_drop_count; i++) {
    *int_at(d->counters, i) = g_drop_counter_start;
    *int_at(d->states, i) = DROP_OFF;
    *float_at(d->sizes, i) = F32(0.0f);
    *float_at(d->array10, i) = F32(0.0f);
    *float_at(d->floors, i) = F32(0.0f);
    body_reset(body_of(d, i));
    quat_identity(quat_of(d->pitches, i));
    quat_identity(quat_of(d->headings, i));
  }
  return set;
}
HW_EXPORT(8c03a4c8, RET(effect_drops_construct(ARG0)))

/* was func_8C03A634: free the set's eight arrays */
void effect_drops_free(gaddr set) {
  EffectDrops *d = drops_at(set);
  gaddr p;
  if ((p = d->counters) != 0) syFree(p);
  if ((p = d->states) != 0) syFree(p);
  if ((p = d->sizes) != 0) syFree(p);
  if ((p = d->array10) != 0) syFree(p);
  if ((p = d->floors) != 0) syFree(p);
  if ((p = d->bodies) != 0) syFree(p);
  if ((p = d->pitches) != 0) syFree(p);
  if ((p = d->headings) != 0) syFree(p);
}
HW_EXPORT(8c03a634, effect_drops_free(ARG0))

/* was func_8C03A6B4: one step of each drop: a flying one grows toward g_drop_size_limit, falls (gravity on its
 * force, damped) and lands below its start height; a landed one goes off */
void effect_drops_update(gaddr set) {
  EffectDrops *d = drops_at(set);
  for (int32_t i = 0; i < g_drop_count; i++) {
    int32_t *state = int_at(d->states, i);
    switch (*state) {
    case DROP_FLYING: {
      f32 *size = float_at(d->sizes, i);
      gaddr body = body_of(d, i);
      if (fgt(g_drop_size_limit, *size))
        *size = fadd(*size, fdiv(fsub(g_drop_size_limit, g_drop_size_start), DROP_GROW_STEPS));
      f32_add_to(DROP_GRAVITY, member_addr(body, Body, force.y));
      vec3_fill(member_addr(body, Body, damping), g_drop_damping);
      body_step(body);
      if (fgt(*float_at(d->floors, i), body_at(body)->position.y))
        *state = DROP_LANDED;
      break;
    }
    case DROP_LANDED:
      *state = DROP_OFF;
      break;
    }
  }
}
HW_EXPORT(8c03a6b4, effect_drops_update(ARG0))

/* was func_8C03A75C: draw each drop that is on: its model at the body's position, turned by its heading then its
 * pitch, at its size */
void effect_drops_draw(gaddr set) {
  EffectDrops *d = drops_at(set);
  for (int32_t i = 0; i < g_drop_count; i++) {
    if (*int_at(d->states, i) == DROP_OFF)
      continue;
    matrix_push();
    matrix_translate_v(body_of(d, i)); /* the body's position (through a getter of itself, func_8C038856) */
    matrix_rotate_y(F32_AT(quat_of(d->headings, i)));
    matrix_rotate_x(F32_AT(quat_of(d->pitches, i)));
    matrix_scale_all(*float_at(d->sizes, i));
    nj_ModelL1(d->object);
    matrix_pop();
  }
}
HW_EXPORT(8c03a75c, effect_drops_draw(ARG0))

/* was func_8C03A7E4: when the racer moves fast enough, start the first drop that is off: from the frame's point
 * (RibbonFrame.vec58) a little lower and pushed out to a random side by 0.6 of the frame's radius, thrown with the
 * frame's velocity plus 0.2 of the speed toward that side and up (0.3 + 0.2 of the speed), turned to face its
 * velocity; its floor is the height it starts from */
void effect_drops_emit(gaddr set, gaddr frame) {
  EffectDrops *d = drops_at(set);
  const RibbonFrame *f = (const RibbonFrame *)hw_ptr(frame);
  if (fgt(g_drop_min_speed, f->f78))
    return;
  for (int32_t i = 0; i < g_drop_count; i++) {
    if (*int_at(d->states, i) != DROP_OFF)
      continue;
    gaddr body = body_of(d, i);
    gaddr pitch = quat_of(d->pitches, i), heading = quat_of(d->headings, i);
    /* the start */
    vec3_copy_bytes(body, member_addr(frame, RibbonFrame, vec58));
    f32_add_to(DROP_START_DROP, member_addr(body, Body, position.y));
    int32_t angle = f32_to_int(random_timer_scaled(ANGLE_FULL));
    f32 spread = fmul(f->low, DROP_SPREAD);
    f32_add_to(fmul(njCos(angle), spread), member_addr(body, Body, position.z));
    f32_add_to(fmul(njSin(angle), spread), member_addr(body, Body, position.x));
    /* the throw */
    f32 vz = fmac(F32(1.0f), f->velocity.z, fmul(fmul(f->f78, njCos(angle)), DROP_PUSH));
    f32 vx = fmac(F32(1.0f), f->velocity.x, fmul(fmul(f->f78, njSin(angle)), DROP_PUSH));
    f32 vy = fmac(DROP_PUSH, f->f78, DROP_LIFT);
    *float_at(d->floors, i) = body_at(body)->position.y;
    gaddr v = hw_stack_alloc(sizeof(Vec3)); /* a local Vec3 (its constructor, func_8C03879E, does nothing) */
    vec3_set(vx, vy, vz, v);
    vec3_copy_bytes(member_addr(body, Body, velocity), v);
    vec3_fill(member_addr(body, Body, damping), F32(1.0f));
    /* facing the throw: pitch from (z over y), heading from (x over z) */
    const Vec3 *w = vec3_at(v);
    f32 pitch_units = angle_units(rt_atan2f(w->z, w->y));
    f32 heading_units = angle_units(rt_atan2f(w->x, w->z));
    hw_stack_free(sizeof(Vec3));
    quat_identity(pitch);
    quat_identity(heading);
    F32_AT(pitch) = pitch_units;
    F32_AT(heading) = heading_units;
    *float_at(d->sizes, i) = g_drop_size_start;
    *int_at(d->counters, i) = g_drop_counter_start;
    *int_at(d->states, i) = DROP_FLYING;
    return;
  }
}
HW_EXPORT(8c03a7e4, effect_drops_emit(ARG0, ARG1))
