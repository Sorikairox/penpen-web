/* Pen Pen port, S16 (b06): the update of the actor of placement type 0x24 (ActorType24, game/actor.h;
 * func_8C05235A).
 *
 * A floating thing that drifts along a curve: its motion frame counts up; a racer that hits it faster than 0.463
 * jolts its three scale values (which spring back to 1) and on its first frame the fourth value jolts too (back to
 * 0.9). The curve parameter f54 runs from 0 to 1 (faster for size 0); at 1 the next four control points are picked
 * from points_a by the table at 0x8C088438. Its second body is pulled toward the curve's point, and its body is
 * placed at home plus the second body's position. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_scene_slot_insert */
#include "scenery.h"      /* contacts_call_handlers */

#define g_float_jolt       GUEST_VAR(f32, 0x8C088430u) /* 0.099999994: the scale values' jolt and pull rate */
#define g_float_speed_big  GUEST_VAR(f32, 0x8C08841Cu) /* 0.0055: f54 per update, size 0 */
#define g_float_speed      GUEST_VAR(f32, 0x8C088420u) /* 0.0041: f54 per update, other sizes */
#define g_float_point_order ((const int32_t *)hw_ptr(0x8C088438u)) /* which points_a each control point takes */
#define FLOAT_HIT_SPEED    F32_BITS(0x3EED097Au) /* 0.46296296 */
#define FLOAT_LAST_REST    F32_BITS(0x3F666666u) /* 0.9: the fourth value's rest */
#define FLOAT_LAST_JOLT    F32_BITS(0x3DCCCCCCu) /* 0.099999994 */
#define FLOAT_DAMPING      F32_BITS(0x3F599999u) /* 0.85: the second body's */
#define FLOAT_PULL         F32_BITS(0x3D4CCCCCu) /* 0.05: toward the curve's point */
#define RACER_ID_LIMIT     8                     /* contacts with an id below this are racers */
#define FLOAT_SLOT         8

/* njCalcScreen (Ninja, by sdk_calls.tsv; by its use here: the point at t of the curve through four points) */
static inline void njCalcScreen(gaddr points, gaddr out, int32_t flags, f32 t) {
  FARGS1(t);
  CALL3(0x8C0C4B28u, points, out, flags);
}

/* the original's frame (0x20 bytes under fr12, r8 and PR): the curve's point that njCalcScreen writes */
typedef struct FloatFrame {
  uint8_t pad_00[0x08];  /* 0x00: the control point counter, a pointer */
  Vec3 point;            /* 0x08 */
  uint8_t rest[0x2C - 0x14]; /* 0x14: the contact counter, the racer contact, the actor, fr12, r8, PR */
} FloatFrame;
HW_OFFSET(FloatFrame, point, 0x08);
HW_SIZE(FloatFrame, 0x2C);

/* was func_8C05235A: the update (see the top of the file) */
void type24_update(gaddr obj) {
  ActorType24 *a = (ActorType24 *)hw_ptr(obj);
  gaddr body2 = member_addr(obj, ActorType24, body2);
  a->frame = fadd(a->frame, F32(1.0f));
  for (int32_t i = 0; i < a->contacts.count; i++) {
    if (((const int32_t *)hw_ptr(a->contacts.other))[i] >= RACER_ID_LIMIT)
      continue;
    if (fgt(njScalor(a->contacts.velocity + (uint32_t)i * sizeof(Vec3)), FLOAT_HIT_SPEED)) {
      f32_add_to(g_float_jolt, member_addr(obj, ActorType24, values[0].force));
      f32_add_to(g_float_jolt, member_addr(obj, ActorType24, values[2].force));
      f32_add_to(g_float_jolt, member_addr(obj, ActorType24, values[1].force));
    }
  }
  body1_pull(F32(1.0f), g_float_jolt, member_addr(obj, ActorType24, values[0]));
  body1_pull(F32(1.0f), g_float_jolt, member_addr(obj, ActorType24, values[1]));
  body1_pull(F32(1.0f), g_float_jolt, member_addr(obj, ActorType24, values[2]));
  body1_step(member_addr(obj, ActorType24, values[0]));
  body1_step(member_addr(obj, ActorType24, values[1]));
  body1_step(member_addr(obj, ActorType24, values[2]));
  if (feq(a->frame, F32(1.0f)))
    f32_add_to(FLOAT_LAST_JOLT, member_addr(obj, ActorType24, values[3].force));
  body1_pull(FLOAT_LAST_REST, FLOAT_LAST_JOLT, member_addr(obj, ActorType24, values[3]));
  body1_step(member_addr(obj, ActorType24, values[3]));

  if (a->size == 0)
    a->f54 = fadd(a->f54, g_float_speed_big);
  else
    a->f54 = fadd(a->f54, g_float_speed);
  if (fgt(a->f54, F32(1.0f))) {
    a->word44++;
    if (a->word44 > 3)
      a->word44 = 0;
    a->f54 = F32(0.0f);
    for (int32_t j = 0; j < 4; j++)
      memmove(&a->points_b[j], &a->points_a[g_float_point_order[a->word44 + j]], sizeof(Vec3));
  }
  gaddr frame = hw_stack_alloc(sizeof(FloatFrame));
  const Vec3 *point = &((const FloatFrame *)hw_ptr(frame))->point;
  njCalcScreen(member_addr(obj, ActorType24, points_b), member_addr(frame, FloatFrame, point), 0, a->f54);
  a->curve_point.x = point->x;
  a->curve_point.y = point->y;
  a->curve_point.z = point->z;
  hw_stack_free(sizeof(FloatFrame));
  vec3_fill(member_addr(body2, Body, damping), FLOAT_DAMPING);
  body_pull(FLOAT_PULL, body2, member_addr(obj, ActorType24, curve_point));
  body_step(body2);
  contacts_call_handlers(member_addr(obj, ActorType24, contacts), body2, 0, 0);
  a->body.position.x = fadd(a->home.x, a->body2.position.x);
  a->body.position.y = fadd(a->home.y, a->body2.position.y);
  a->body.position.z = fadd(a->home.z, a->body2.position.z);
  race_scene_slot_insert(panel_get(), member_addr(obj, ActorType24, contacts), FLOAT_SLOT);
}
HW_EXPORT(8c05235a, type24_update(ARG0))
