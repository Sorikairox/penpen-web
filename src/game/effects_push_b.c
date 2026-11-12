/* Pen Pen port, S15 (c05): effect set B's emitter (func_8C03B6F2), called with the racer's frame (RibbonFrame)
 * through racer_effects_push_set_b (game/racer_body.c). The same shape as the ribbon set's (game/effects_push.c).
 *
 * It starts the first free particle (state 0) of the set: it is given a Body at rest pushed back against the
 * frame's heading, a strength from the frame's f78, and then what kind it is comes from the frame's words:
 *   - word84: a motion model (kinds 12-15, its model tree and motion from the shared data) placed at the frame's
 *     vec58, or a flat sprite (kinds 8-11) that the set's own sprite turns towards the frame's velocity;
 *   - word8c: a random sprite (kinds 4-6) sized by the strength, every other time (EffectSetB.word30);
 *   - word88: kind 2, at vec58, pushed in a random direction around the heading;
 *   - none: kind 0, sized by the strength, every other time.
 * The sprites (kinds 0 and 4-6) start just behind the frame's matrix, between its two heights. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h" /* method_self */
#include "effects.h"
#include "scenery.h" /* random_timer_scaled */

#define g_shared_entry0     GUEST_VAR(gaddr, 0x8C096D14u)   /* model.c's: the shared container's first entry (a
                                                             * table of model trees) */
#define g_shared_entry1     GUEST_VAR(gaddr, 0x8C096D18u)   /* model.c's: its second entry (RibbonKind table) */
#define g_set_b_counter_start GUEST_VAR(int32_t, 0x8C0830ACu) /* 1: frames a new particle waits in state 1 */
#define g_set_b_damping     GUEST_VAR(f32, 0x8C0830D4u)     /* 0.95 */
#define g_set_b_min_f78     GUEST_VAR(f32, 0x8C0830D8u)     /* 0.1: below it, no sprite and no kind 2 */
#define g_set_b_flat_scale  GUEST_VAR(f32, 0x8C0830E0u)     /* 0.15625: the flat sprites' */
#define g_set_b_scale_random GUEST_VAR(f32, 0x8C082F9Cu)    /* 0.046875: the random sprites' (the ribbon set's) */
#define g_set_b_scale_0     GUEST_VAR(f32, 0x8C082F98u)     /* 0.318: kind 0's (the ribbon set's) */
#define g_set_b_push_2      GUEST_VAR(f32, 0x8C0830BCu)     /* 0.3: kind 2's push */
#define g_set_b_scale_2     GUEST_VAR(f32, 0x8C0830B4u)     /* 0.15625: kind 2's scale */
#define g_set_b_counter_2   GUEST_VAR(int32_t, 0x8C0830B0u) /* 10: kind 2's counter */
/* the shared data's entries of the motion models (frame word94 3, 4, 13, 14 -> kinds 12, 14, 13, 15) */
#define g_set_b_object_12   GUEST_VAR(int32_t, 0x8C0830E4u) /* 2: the model tree of kind 12 */
#define g_set_b_object_13   GUEST_VAR(int32_t, 0x8C0830E8u) /* 6: kind 13 */
#define g_set_b_object_14   GUEST_VAR(int32_t, 0x8C0830F0u) /* 1: kind 14 */
#define g_set_b_object_15   GUEST_VAR(int32_t, 0x8C0830F4u) /* 5: kind 15 */
#define g_set_b_motion_a    GUEST_VAR(int32_t, 0x8C0830ECu) /* 2: the motion of kinds 12 and 13 */
#define g_set_b_motion_b    GUEST_VAR(int32_t, 0x8C0830F8u) /* 1: the motion of kinds 14 and 15 */

#define SET_B_BACK_PUSH     F32_BITS(0x3E2AAAAAu) /* 1/6: the push against the heading */
#define SET_B_F78_SHIFT     F32_BITS(0xBEED097Au) /* -0.46296: the strength is f78 + this (at least 0) */
#define SET_B_F78_RANGE     F32_BITS(0x409425EBu) /* 4.62963: the random sprites' strength is divided by it */
#define SET_B_FLAT_SIZE     F32_BITS(0x3F4CCCCCu) /* 0.8 */
#define SET_B_RANDOM_RANGE  F32_BITS(0x3E4CCCCCu) /* 0.2 */
#define SET_B_RANDOM_SHIFT  F32_BITS(0x3F666666u) /* 0.9 */
#define SET_B_KIND0_SHIFT   F32_BITS(0x3E999999u) /* 0.3 */
#define SET_B_KIND0_FACTOR  F32_BITS(0x3F4CCCCCu) /* 0.8 */
#define SET_B_BEHIND        F32_BITS(0xBF7FFFFFu) /* -0.99999994: the sprites' start, behind the matrix */
#define SET_B_TURN_RANGE    F32_BITS(0x46AAAAAAu) /* 21845.332 (a third of a turn) */
#define SET_B_TURN_SHIFT    F32_BITS(0x46AAAAABu) /* 21845.334 */
#define SET_B_TWO_PI        F32_BITS(0x40C90FD7u) /* 6.2831836 (the binary's 2 pi) */

enum {
  SET_B_KIND_SPRITE_0 = 0,
  SET_B_KIND_2 = 2,
  SET_B_KIND_RANDOM_FIRST = 4, /* the random sprites are 4, 5, 6 */
  SET_B_KIND_FLAT_8 = 8,       /* 8-11: flat sprites */
  SET_B_KIND_FLAT_9 = 9,
  SET_B_KIND_FLAT_10 = 10,
  SET_B_KIND_FLAT_11 = 11,
  SET_B_KIND_MODEL_12 = 12,    /* 12-15: motion models */
  SET_B_KIND_MODEL_13 = 13,
  SET_B_KIND_MODEL_14 = 14,
  SET_B_KIND_MODEL_15 = 15,
};

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* the original's frame (0x50 bytes under r8, fr12 and PR): the sprites' side point and where the matrix puts it
 * (Ninja writes it) */
typedef struct SetBPushFrame {
  uint32_t words[3];   /* 0x00: scratch the C keeps in variables */
  Vec3 placed;         /* 0x0C */
  Vec3 side;           /* 0x18 */
  uint8_t rest[0x5C - 0x24]; /* 0x24: the array pointers it keeps, r8, fr12, PR */
} SetBPushFrame;
HW_OFFSET(SetBPushFrame, placed, 0x0C);
HW_OFFSET(SetBPushFrame, side, 0x18);
HW_SIZE(SetBPushFrame, 0x5C);

static int32_t *int_at(gaddr array, int32_t i) { return &((int32_t *)hw_ptr(array))[i]; }
static f32 *float_at(gaddr array, int32_t i) { return &((f32 *)hw_ptr(array))[i]; }

/* a motion model's kind: its model tree, motion and texture from the shared data */
static void set_b_model(EffectSetB *s, int32_t *kind, int32_t k, int32_t object, int32_t motion) {
  *kind = k;
  s->object = ((const gaddr *)hw_ptr(g_shared_entry0))[object];
  s->motion = ((const RibbonKind *)hw_ptr(g_shared_entry1))[motion].model;
  s->texture = ((const RibbonKind *)hw_ptr(g_shared_entry1))[motion].texture;
}

/* a sprite starts behind the frame's matrix, at the height between the frame's two */
static void set_b_place_sprite(const RibbonFrame *f, gaddr frame, gaddr side, gaddr placed, gaddr body) {
  vec3_set(F32(0.0f), fsub(f->high, f->low), SET_B_BEHIND, side);
  njCalcPoint(frame, side, placed);
  vec3_copy_bytes(body, placed);
}

/* was func_8C03B6F2: start a particle of set B from the racer's frame (see above) */
void effect_set_b_push(gaddr set, gaddr frame) {
  EffectSetB *s = (EffectSetB *)hw_ptr(set);
  const RibbonFrame *f = (const RibbonFrame *)hw_ptr(frame);
  gaddr locals = hw_stack_alloc(sizeof(SetBPushFrame));
  gaddr side = member_addr(locals, SetBPushFrame, side);
  gaddr placed = member_addr(locals, SetBPushFrame, placed);
  int32_t count = s->count;
  int32_t i = 0;
  while (i < count && *int_at(s->states, i) != 0)
    i++;
  if (i < count) {
    int32_t *kind = int_at(s->kinds, i);
    int32_t *state = int_at(s->states, i);
    gaddr body = s->bodies + (gaddr)i * sizeof(Body);
    *state = 1;
    *float_at(s->alphas, i) = F32(1.0f);
    *float_at(s->sizes, i) = F32(0.0f);
    method_self(side);   /* the locals' (empty) constructors */
    method_self(placed);
    body_reset(body);
    f32 c = njCos(f32_to_int(f->heading));
    f32_add_to(fneg(fmul(c, SET_B_BACK_PUSH)), member_addr(body, Body, velocity.z));
    f32 sn = njSin(f32_to_int(f->heading));
    f32_add_to(fneg(fmul(sn, SET_B_BACK_PUSH)), member_addr(body, Body, velocity.x));
    body_at(body)->damping.z = g_set_b_damping;
    body_at(body)->damping.x = g_set_b_damping;
    f32 strength = fadd(f->f78, SET_B_F78_SHIFT);
    if (fgt(F32(0.0f), strength))
      strength = F32(0.0f);
    *int_at(s->counters, i) = g_set_b_counter_start;

    if (f->word84 != 0) {
      int model = 0;
      switch (f->word94) {
      case 0: *kind = SET_B_KIND_FLAT_9; break;
      case 1: *kind = SET_B_KIND_FLAT_8; break;
      case 3: set_b_model(s, kind, SET_B_KIND_MODEL_12, g_set_b_object_12, g_set_b_motion_a); model = 1; break;
      case 4: set_b_model(s, kind, SET_B_KIND_MODEL_14, g_set_b_object_14, g_set_b_motion_b); model = 1; break;
      case 10: *kind = SET_B_KIND_FLAT_11; break;
      case 11: *kind = SET_B_KIND_FLAT_10; break;
      case 13: set_b_model(s, kind, SET_B_KIND_MODEL_13, g_set_b_object_13, g_set_b_motion_a); model = 1; break;
      case 14: set_b_model(s, kind, SET_B_KIND_MODEL_15, g_set_b_object_15, g_set_b_motion_b); model = 1; break;
      default: break;
      }
      if (model) {
        vec3_copy_bytes(body, member_addr(frame, RibbonFrame, vec58));
        *float_at(s->frames, i) = F32(0.0f);
        *float_at(s->scales, i) = F32(1.0f);
      } else {
        /* a flat sprite: the set's sprite, sized at random, at the origin, turned towards the frame's motion */
        *float_at(s->scales, i) = SET_B_FLAT_SIZE;
        f32 size = fmul(g_set_b_flat_scale, fadd(random_timer_scaled(SET_B_RANDOM_RANGE), SET_B_RANDOM_SHIFT));
        s->sprite.sx = size;
        s->sprite.sy = size;
        s->sprite.p.x = F32(0.0f);
        s->sprite.p.y = F32(0.0f);
        s->sprite.p.z = F32(0.0f);
        f32 turn = rt_atan2f(fneg(f->velocity.x), fneg(f->velocity.z));
        int32_t ang = f32_to_int(fdiv(fmul(turn, F32(65536.0f)), SET_B_TWO_PI));
        s->sprite.ang = f32_to_int(f32_from_int(ang));
        vec3_copy_bytes(body, member_addr(frame, RibbonFrame, vec58));
        *float_at(s->frames, i) = F32(-1.0f);
      }
    } else if (f->word8c != 0) {
      if (fgt(g_set_b_min_f78, f->f78)) {
        *state = 0;
      } else if (s->word30 > 0) {
        s->word30--;
        *kind = f32_to_int(random_timer_scaled(F32(3.0f))) + SET_B_KIND_RANDOM_FIRST;
        strength = fmul(strength, f32_reciprocal(SET_B_F78_RANGE));
        if (fgt(strength, F32(1.0f)))
          strength = F32(1.0f);
        f32 scale = fmul(g_set_b_scale_random, strength);
        *float_at(s->scales, i) =
            fmul(scale, fadd(random_timer_scaled(SET_B_RANDOM_RANGE), SET_B_RANDOM_SHIFT));
        set_b_place_sprite(f, frame, side, placed, body);
      } else {
        *state = 0;
        s->word30 = 1;
      }
    } else if (f->word88 != 0) {
      if (fgt(g_set_b_min_f78, f->f78)) {
        *state = 0;
      } else {
        *kind = SET_B_KIND_2;
        vec3_copy_bytes(body, member_addr(frame, RibbonFrame, vec58));
        /* a random direction within a third of a turn, a third of a turn from the heading (the original also
         * leaves low * 0.3 in an unread stack slot) */
        f32 heading = f->heading;
        f32 turn = fadd(fadd(heading, random_timer_scaled(SET_B_TURN_RANGE)), SET_B_TURN_SHIFT);
        f32 sx = njSin(f32_to_int(turn));
        f32_add_to(fmul(sx, g_set_b_push_2), member_addr(body, Body, force.x));
        f32 cz = njCos(f32_to_int(turn));
        f32_add_to(fmul(cz, g_set_b_push_2), member_addr(body, Body, force.z));
        *float_at(s->scales, i) = g_set_b_scale_2;
        *int_at(s->counters, i) = g_set_b_counter_2;
      }
    } else {
      if (fgt(g_set_b_min_f78, f->f78)) {
        *state = 0;
      } else if (s->word30 > 0) {
        s->word30--;
        *kind = SET_B_KIND_SPRITE_0;
        f32 scale = fmul(g_set_b_scale_0, strength);
        scale = fmul(scale, fadd(random_timer_scaled(SET_B_RANDOM_RANGE), SET_B_KIND0_SHIFT));
        *float_at(s->scales, i) = fmul(scale, SET_B_KIND0_FACTOR);
        set_b_place_sprite(f, frame, side, placed, body);
      } else {
        *state = 0;
        s->word30 = 1;
      }
    }
  }
  hw_stack_free(sizeof(SetBPushFrame));
}
HW_EXPORT(8c03b6f2, effect_set_b_push(ARG0, ARG1))
