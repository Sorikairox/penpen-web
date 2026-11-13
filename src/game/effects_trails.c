/* Pen Pen port, S18 (b03): a pair of trails (game/effects_trails.c), beside the racer's effects (game/effects.h);
 * the method has no direct caller in the game code (only data references).
 *
 * Each of the g_trail_count (2) trails is a Ninja model whose points come in pairs (g_trail_points of them: the
 * edges of a strip) and a chain of 2 * g_trail_links Bodies. Every push moves the strip one pair along (the oldest
 * pair is dropped at the end) while the bodies above the ground fall, and puts a new pair at the front: two points
 * across the trail (x = -2.5 + 5 * trail, from y = -height to 5 - height, z = -5) turned by the matrix. */
#include "hw.h"
#include "sdk.h"
#include "scenery.h"   /* array_construct */
#include "actor_b05.h" /* body_set_damping_y */
#include "../core/vec.h"
#include "../core/file.h" /* NjModel */

void body_set_damping_z(gaddr body, f32 d); /* game/model.c (func_8C038918) */

#define TRAIL_SLOTS 2 /* the words the object has for each array (g_trail_count may not exceed it) */

typedef struct EffectTrails {
  gaddr models[TRAIL_SLOTS]; /* 0x00: a Ninja model (NjModel) per trail: its points are the strip */
  gaddr bodies[TRAIL_SLOTS]; /* 0x08: a chain of Bodies per trail */
} EffectTrails;
HW_OFFSET(EffectTrails, bodies, 0x08);

#define g_trail_count    GUEST_VAR(int32_t, 0x8C083094u) /* 2 */
#define g_trail_links    GUEST_VAR(int32_t, 0x8C083098u) /* 8: half the chain's bodies */
#define g_trail_points   GUEST_VAR(int32_t, 0x8C0830A0u) /* 32: the model's points */
#define g_trail_damping_y GUEST_VAR(f32, 0x8C0830A4u)    /* 0.9 */
#define g_trail_damping_z GUEST_VAR(f32, 0x8C0830A8u)    /* 0.9 */
#define TRAIL_FRONT_VELOCITY 0x8C1BAC44u                 /* a Vec3: the front body's velocity is set to it */

#define TRAIL_GRAVITY  F32_BITS(0xBDA740D9u) /* -0.0817: added to a falling body's force y */
#define TRAIL_WIDTH    F32(5.0f)
#define TRAIL_LEFT     F32(-2.5f)
#define TRAIL_DEPTH    F32(-5.0f)
#define VEC3_CONSTRUCT 0x8C03898Cu           /* Vec3's empty constructor (for the array helper) */

/* the original's frame (0x50 bytes; the rest holds its pointers and copies): the turned pair, then the pair */
typedef struct TrailFrame {
  uint8_t words[0x20];  /* 0x00 */
  Vec3 turned[2];       /* 0x20 */
  Vec3 pair[2];         /* 0x38 */
} TrailFrame;
HW_OFFSET(TrailFrame, turned, 0x20);
HW_OFFSET(TrailFrame, pair, 0x38);
HW_SIZE(TrailFrame, 0x50);

static void vec3_copy_fields(Vec3 *dst, const Vec3 *src) {
  dst->x = src->x;
  dst->y = src->y;
  dst->z = src->z;
}

/* was func_8C03ADCE: one push of each trail with the matrix `m` and the height `height` (see the top) */
void effect_trails_push(gaddr set, gaddr m, f32 height) {
  EffectTrails *t = (EffectTrails *)hw_ptr(set);
  f32 low = fsub(F32(0.0f), height);
  f32 high = fsub(TRAIL_WIDTH, height);
  gaddr frame = hw_stack_alloc(sizeof(TrailFrame));
  gaddr pair = member_addr(frame, TrailFrame, pair);
  gaddr turned = member_addr(frame, TrailFrame, turned);
  for (int32_t i = 0; i < g_trail_count; i++) {
    int32_t links = g_trail_links * 2 - 1;
    gaddr points = ((const NjModel *)hw_ptr(t->models[i]))->points;
    gaddr body = t->bodies[i] + (gaddr)links * sizeof(Body);
    Vec3 *point = (Vec3 *)hw_ptr(points) + (g_trail_points - 1);
    /* from the back: each body above the ground falls (at rest on it), and the strip moves one pair along */
    for (int32_t k = links; k > 0; k--) {
      if (fgt(body_at(body)->position.y, F32(0.0f))) {
        f32_add_to(TRAIL_GRAVITY, member_addr(body, Body, force.y));
        body_set_damping_y(body, g_trail_damping_y);
        body_set_damping_z(body, g_trail_damping_z);
        body_step(body);
      } else {
        vec3_zero(member_addr(body, Body, velocity));
      }
      vec3_copy_fields(point, point - 2);
      point--;
      vec3_copy_fields(point, point - 2);
      point--;
      body -= sizeof(Body);
    }
    /* the new front pair */
    array_construct(2, sizeof(Vec3), pair, VEC3_CONSTRUCT);
    array_construct(2, sizeof(Vec3), turned, VEC3_CONSTRUCT);
    f32 x = fmac(TRAIL_WIDTH, f32_from_int(i), TRAIL_LEFT);
    vec3_set(x, low, TRAIL_DEPTH, pair);
    vec3_set(x, high, TRAIL_DEPTH, pair + sizeof(Vec3));
    njCalcPoint(m, pair, turned);
    njCalcPoint(m, pair + sizeof(Vec3), turned + sizeof(Vec3));
    vec3_copy_bytes(member_addr(body, Body, velocity), TRAIL_FRONT_VELOCITY);
    const Vec3 *tv = vec3_at(turned);
    for (int32_t k = 1; k >= 0; k--) {
      vec3_copy_fields(point, &tv[k]);
      point--;
    }
  }
  hw_stack_free(sizeof(TrailFrame));
}
HW_EXPORT(8c03adce, effect_trails_push(ARG0, ARG1, FARG0))
