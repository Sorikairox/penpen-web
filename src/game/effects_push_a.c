/* Pen Pen port, S18 b06: effect set A's emitter (func_8C03A252), handed the racer's matrix, cell, radius, speed and
 * heading through racer_effects_set_a_push (game/racer_effects_b04.c).
 *
 * It notes the heading (the puffs' drift) and the cell (whose floor hides them), then starts the first free puff
 * (state 0) of the set: at a random point near the matrix's origin (njCalcPoint of a local offset), waiting a
 * random count around a base, at the set's scale, with no offset, size 0 and alpha 1; its type is 1 (rising faster)
 * one time in eight. Slow (speed under 1) it only emits every fourth call, the point a little ahead (z = radius +
 * up to radius / 2) in a box of a quarter of the radius; fast, every call, in a box of the radius across and ahead
 * and of half of it in height. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "effects.h"
#include "scenery.h" /* random_timer_scaled */

#define g_set_a_wait_slow  GUEST_VAR(int32_t, 0x8C083064u) /* a slow puff's base wait (state 1) */
#define g_set_a_wait_fast  GUEST_VAR(int32_t, 0x8C083068u) /* a fast puff's */
#define g_set_a_scale      GUEST_VAR(f32, 0x8C083070u)     /* 0.05 (game/effects.c) */
#define SET_A_SLOW_SPEED   F32(1.0f)  /* under it the set emits every SET_A_SLOW_EVERY calls */
#define SET_A_SLOW_EVERY   4
#define SET_A_WAIT_RANGE   F32(8.0f)  /* the wait is base + (random(8) - 4) */
#define SET_A_WAIT_SHIFT   F32(-4.0f)
#define SET_A_TYPE_RANGE   F32(8.0f)  /* type 1 when (int)random(8) is 0 */

enum { SET_A_STATE_OFF = 0, SET_A_STATE_WAITING = 1 };

/* the original's frame (0x38 bytes): the point's local offset is a Vec3 at 0x2C (its constructor, func_8C03879E,
 * does nothing) */
#define SET_A_FRAME        0x38
#define SET_A_FRAME_POINT  0x2C

static inline int32_t *i32_at(gaddr a) { return (int32_t *)hw_ptr(a); }
static inline f32 *f32_at(gaddr a) { return (f32 *)hw_ptr(a); }

/* the puff's own fields once its point is placed: the wait, the scale, the offset (cleared by `clear_offset`), the
 * size, the state, the alpha and the type */
static void set_a_start(gaddr counter, gaddr scale, gaddr offset, gaddr size, gaddr state, gaddr alpha, gaddr type,
                        int32_t wait_base, int fast) {
  int32_t wait = f32_to_int(fadd(random_timer_scaled(SET_A_WAIT_RANGE), SET_A_WAIT_SHIFT));
  *i32_at(counter) = wait + wait_base;
  *f32_at(scale) = g_set_a_scale;
  if (fast) {
    vec3_zero(offset);
  } else {
    Vec3 *o = vec3_at(offset);
    o->z = F32(0.0f);
    o->y = F32(0.0f);
    o->x = F32(0.0f);
  }
  *f32_at(size) = F32(0.0f);
  *i32_at(state) = SET_A_STATE_WAITING;
  *f32_at(alpha) = F32(1.0f);
  if (f32_to_int(random_timer_scaled(SET_A_TYPE_RANGE)) == 0)
    *i32_at(type) = 1;
  else
    *i32_at(type) = 0;
}

/* was func_8C03A252: start a puff of set A (see the top of the file) */
void effect_set_a_push(gaddr set, gaddr m, int32_t cell, f32 radius, f32 speed, f32 heading) {
  EffectSetA *a = (EffectSetA *)hw_ptr(set);
  gaddr counter = a->counters, origin = a->origins, offset = a->offsets, scale = a->scales;
  gaddr size = a->sizes, alpha = a->alphas, type = a->types, state = a->states;
  a->heading = heading;
  a->cell = cell;
  f32 half = fdiv(radius, F32(2.0f));
  f32 quarter = fdiv(radius, F32(4.0f));
  gaddr frame = hw_stack_alloc(SET_A_FRAME);
  gaddr point = frame + SET_A_FRAME_POINT;
  Vec3 *p = vec3_at(point);

  if (fgt(SET_A_SLOW_SPEED, speed)) {
    /* slow: every fourth call */
    a->word24 = a->word24 - 1;
    if (!((int32_t)a->word24 > 0)) {
      f32 eighth = fdiv(radius, F32(8.0f));
      a->word24 = SET_A_SLOW_EVERY;
      for (int32_t n = a->count; n > 0; n--) {
        if (*i32_at(state) == SET_A_STATE_OFF) {
          p->x = fsub(random_timer_scaled(quarter), eighth);
          p->y = fsub(random_timer_scaled(quarter), eighth);
          p->z = fadd(radius, random_timer_scaled(half));
          njCalcPoint(m, point, origin);
          set_a_start(counter, scale, offset, size, state, alpha, type, g_set_a_wait_slow, 0);
          break;
        }
        counter += 4;
        state += 4;
        origin += sizeof(Vec3);
        size += 4;
        alpha += 4;
        scale += 4;
        offset += sizeof(Vec3);
        type += 4;
      }
    }
  } else {
    /* fast: every call */
    for (int32_t n = a->count; n > 0; n--) {
      if (*i32_at(state) == SET_A_STATE_OFF) {
        p->x = fsub(random_timer_scaled(radius), half);
        p->y = fsub(random_timer_scaled(half), quarter);
        p->z = random_timer_scaled(radius);
        njCalcPoint(m, point, origin);
        set_a_start(counter, scale, offset, size, state, alpha, type, g_set_a_wait_fast, 1);
        break;
      }
      counter += 4;
      state += 4;
      origin += sizeof(Vec3);
      size += 4;
      alpha += 4;
      scale += 4;
      offset += sizeof(Vec3);
      type += 4;
    }
  }
  hw_stack_free(SET_A_FRAME);
}
HW_EXPORT(8c03a252, effect_set_a_push(ARG0, ARG1, (int32_t)ARG2, FARG0, FARG1, FARG2))
