/* Pen Pen port, S18 (b03): the ribbon set's emitter at a given point (game/effects_ribbon_at.c), a variant of
 * ribbon_set_push (game/effects_push.c) for a caller that hands the point, the side and the heading itself (its only
 * caller is func_8C01F752). See game/effects.h for the set. */
#include "hw.h"
#include "sdk.h"
#include "effects.h"
#include "../core/vec.h"

#define g_ribbon_counter_start GUEST_VAR(int32_t, 0x8C083034u) /* 20: frames a new particle waits in state 1 */
#define g_ribbon_scale         GUEST_VAR(f32, 0x8C082F98u)     /* 0.318: a new particle's scale */
#define g_ribbon_push          GUEST_VAR(f32, 0x8C083050u)     /* 0.7: the push opposite the heading */
#define g_ribbon_drop          GUEST_VAR(f32, 0x8C082FA8u)     /* 3.5: the offset's height, before the given one */

#define RIBBON_SIDE    F32(2.5f)
#define RIBBON_DAMPING F32_BITS(0x3F733333u) /* 0.95 */

/* the C runtime's float functions of fr4 (func_8C062938 / func_8C062ADC, as guest code): by their use, the cosine
 * and the sine of the heading */
static f32 rt_float_8c062938(f32 x) { FARGS1(x); CALL0(0x8C062938u); return hw_fret(); }
static f32 rt_float_8c062adc(f32 x) { FARGS1(x); CALL0(0x8C062ADCu); return hw_fret(); }

/* the original's frame (0x54 bytes): two local Vec3s (their constructor, func_8C03879E, does nothing; the first is
 * the offset), then its copies of the arguments and of the set's array pointers */
typedef struct RibbonAtFrame {
  Vec3 unused;     /* 0x00 */
  Vec3 offset;     /* 0x0C */
  uint8_t rest[0x54 - 0x18];
} RibbonAtFrame;
HW_OFFSET(RibbonAtFrame, offset, 0x0C);
HW_SIZE(RibbonAtFrame, 0x54);

static int32_t *int_at(gaddr array, int32_t i) { return &((int32_t *)hw_ptr(array))[i]; }
static f32 *float_at(gaddr array, int32_t i) { return &((f32 *)hw_ptr(array))[i]; }

/* was func_8C039B14: start the first free particle of the ribbon set at `position`: a sprite (kind 0) beside it,
 * 2.5 to the side of `heading` (the other side when `other_side`), g_ribbon_drop - `height` higher, pushed opposite
 * the heading */
void ribbon_set_push_at(gaddr set, gaddr position, int32_t other_side, f32 height, f32 heading) {
  RibbonSet *s = (RibbonSet *)hw_ptr(set);
  for (int32_t i = 0; i < s->count; i++) {
    if (*int_at(s->states, i) != 0)
      continue;
    gaddr body = s->bodies + (gaddr)i * sizeof(Body);
    *int_at(s->counters, i) = g_ribbon_counter_start;
    *float_at(s->sizes, i) = F32(0.0f);
    *float_at(s->alphas, i) = F32(1.0f);
    *int_at(s->states, i) = 1;
    *int_at(s->kinds, i) = 0;
    *float_at(s->scales, i) = g_ribbon_scale;
    body_reset(body);
    vec3_fill(member_addr(body, Body, damping), RIBBON_DAMPING);
    vec3_copy_bytes(body, position);
    gaddr frame = hw_stack_alloc(sizeof(RibbonAtFrame));
    Vec3 *offset = &((RibbonAtFrame *)hw_ptr(frame))->offset;
    if (other_side != 0) {
      offset->z = fneg(fmul(rt_float_8c062938(heading), RIBBON_SIDE));
      offset->x = fmul(rt_float_8c062adc(heading), RIBBON_SIDE);
    } else {
      offset->z = fmul(rt_float_8c062938(heading), RIBBON_SIDE);
      offset->x = fneg(fmul(rt_float_8c062adc(heading), RIBBON_SIDE));
    }
    offset->y = fsub(g_ribbon_drop, height);
    f32_add_to(fneg(fmul(njSin(f32_to_int(heading)), g_ribbon_push)), member_addr(body, Body, force.x));
    f32_add_to(fneg(fmul(njCos(f32_to_int(heading)), g_ribbon_push)), member_addr(body, Body, force.z));
    vec3_add(body, member_addr(frame, RibbonAtFrame, offset));
    hw_stack_free(sizeof(RibbonAtFrame));
    return;
  }
}
HW_EXPORT(8c039b14, ribbon_set_push_at(ARG0, ARG1, (int32_t)ARG2, FARG0, FARG1))
