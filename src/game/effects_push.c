/* Pen Pen port, S15 (b06): the ribbon set's emitter (func_8C0397A8), called with the racer's frame (RibbonFrame)
 * through racer_effects_push_ribbons (game/racer_body.c). See game/effects.h and game/effects.c for the set.
 *
 * It starts the first free particle (state 0) of the set: what kind it is comes from the frame's words (a motion
 * model, kinds 8 and 10, for word84 by word94; a random sprite for word8c; kind 2 for word88, left off), then its
 * Body is put beside the racer: 2.5 to its side (the other side with word80) through the frame's matrix; the
 * sprites start at the frame's height range and get a push opposite the frame's heading. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "effects.h"
#include "scenery.h" /* random_timer_scaled */

#define g_ribbon_counter_start GUEST_VAR(int32_t, 0x8C083034u) /* 20: frames a new particle waits in state 1 */
#define g_ribbon_scale         GUEST_VAR(f32, 0x8C082F98u)     /* 0.318: a new particle's scale */
#define g_ribbon_scale_random  GUEST_VAR(f32, 0x8C082F9Cu)     /* 0.046875: the random sprites' */
#define g_ribbon_scale_kind2   GUEST_VAR(f32, 0x8C082FA4u)     /* 0.0893: kind 2's */
#define g_ribbon_push          GUEST_VAR(f32, 0x8C083050u)     /* 0.7: the sprites' push */
#define g_shared_entry0        GUEST_VAR(gaddr, 0x8C096D14u)   /* model.c's: the shared container's first entry (a
                                                                * table of model trees) */
#define g_ribbon_object_a      GUEST_VAR(int32_t, 0x8C083054u) /* its entry for kind 8 */
#define g_ribbon_object_b      GUEST_VAR(int32_t, 0x8C083058u) /* its entry for kind 10 */

enum {
  RIBBON_KIND_SPRITE_0 = 0,
  RIBBON_KIND_2 = 2,
  RIBBON_KIND_SPRITE_3 = 3, /* the random sprites are 3, 4, 5 */
  RIBBON_KIND_SPRITE_4 = 4,
  RIBBON_KIND_SPRITE_5 = 5,
  RIBBON_KIND_MODEL_A = 8,
  RIBBON_KIND_MODEL_B = 10,
};
enum { FRAME_MODEL_A = 2, FRAME_MODEL_B = 12 }; /* RibbonFrame.word94 */
#define RIBBON_SIDE      F32(2.5f)
#define RIBBON_MODEL_SCALE F32(1.5f)
#define RIBBON_DAMPING   F32_BITS(0x3F733333u) /* 0.95 */

/* the original's frame (0x4C bytes under r8 and PR): the side point and where the matrix puts it (Ninja writes it) */
typedef struct RibbonPushFrame {
  uint32_t word0;      /* 0x00 */
  Vec3 placed;         /* 0x04 */
  Vec3 side;           /* 0x10 */
  uint8_t rest[0x54 - 0x1C]; /* 0x1C: the array pointers it keeps, r8, PR */
} RibbonPushFrame;
HW_OFFSET(RibbonPushFrame, placed, 0x04);
HW_OFFSET(RibbonPushFrame, side, 0x10);
HW_SIZE(RibbonPushFrame, 0x54);

static int32_t *int_at(gaddr array, int32_t i) { return &((int32_t *)hw_ptr(array))[i]; }
static f32 *float_at(gaddr array, int32_t i) { return &((f32 *)hw_ptr(array))[i]; }
static gaddr shared_object(int32_t entry) { return ((const gaddr *)hw_ptr(g_shared_entry0))[entry]; }

/* was func_8C0397A8: start a particle of the ribbon set from the racer's frame (see above) */
void ribbon_set_push(gaddr set, gaddr frame) {
  RibbonSet *s = (RibbonSet *)hw_ptr(set);
  const RibbonFrame *f = (const RibbonFrame *)hw_ptr(frame);
  gaddr locals = hw_stack_alloc(sizeof(RibbonPushFrame));
  gaddr side = member_addr(locals, RibbonPushFrame, side);
  gaddr placed = member_addr(locals, RibbonPushFrame, placed);
  int32_t i = 0;
  while (i < s->count && *int_at(s->states, i) != 0)
    i++;
  if (i < s->count) {
    int32_t *kind = int_at(s->kinds, i);
    f32 *scale = float_at(s->scales, i);
    gaddr body = s->bodies + (gaddr)i * sizeof(Body);
    *int_at(s->counters, i) = g_ribbon_counter_start;
    *float_at(s->sizes, i) = F32(0.0f);
    *float_at(s->alphas, i) = F32(1.0f);
    *int_at(s->states, i) = 1;
    *kind = RIBBON_KIND_SPRITE_0;
    *scale = g_ribbon_scale;
    if (f->word84 != 0) {
      if (f->word94 == FRAME_MODEL_A) {
        *kind = RIBBON_KIND_MODEL_A;
        s->object = shared_object(g_ribbon_object_a);
      } else if (f->word94 == FRAME_MODEL_B) {
        *kind = RIBBON_KIND_MODEL_B;
        s->object = shared_object(g_ribbon_object_b);
      }
      *float_at(s->angles, i) = f->heading;
      *float_at(s->frames, i) = F32(0.0f);
      *scale = RIBBON_MODEL_SCALE;
    }
    if (f->word8c != 0) {
      *kind = f32_to_int(random_timer_scaled(F32(3.0f))) + RIBBON_KIND_SPRITE_3;
      *scale = g_ribbon_scale_random;
    }
    if (f->word88 != 0) {
      *kind = RIBBON_KIND_2;
      *scale = g_ribbon_scale_kind2;
      *int_at(s->states, i) = 0;
    }
    body_reset(body);
    vec3_fill(member_addr(body, Body, damping), RIBBON_DAMPING);
    switch (*kind) {
    case RIBBON_KIND_MODEL_A:
    case RIBBON_KIND_MODEL_B:
      vec3_set(f->word80 != 0 ? fneg(RIBBON_SIDE) : RIBBON_SIDE, F32(0.0f), F32(0.0f), side);
      njCalcPoint(frame, side, placed);
      vec3_copy_bytes(body, placed);
      ((Body *)hw_ptr(body))->position.y = f->vec58.y;
      break;
    case RIBBON_KIND_SPRITE_3:
    case RIBBON_KIND_SPRITE_4:
    case RIBBON_KIND_SPRITE_5:
    case RIBBON_KIND_SPRITE_0: {
      f32 x = f->word80 != 0 ? fneg(RIBBON_SIDE) : RIBBON_SIDE;
      vec3_set(x, fsub(f->high, f->low), F32(0.0f), side);
      f32 sin_h = njSin(f32_to_int(f->heading));
      f32_add_to(fneg(fmul(sin_h, g_ribbon_push)), member_addr(body, Body, force.x));
      f32 cos_h = njCos(f32_to_int(f->heading));
      f32_add_to(fneg(fmul(cos_h, g_ribbon_push)), member_addr(body, Body, force.z));
      njCalcPoint(frame, side, placed);
      vec3_copy_bytes(body, placed);
      break;
    }
    default:
      break;
    }
  }
  hw_stack_free(sizeof(RibbonPushFrame));
}
HW_EXPORT(8c0397a8, ribbon_set_push(ARG0, ARG1))
