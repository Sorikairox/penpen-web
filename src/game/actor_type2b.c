/* Pen Pen port, S16 (b06): the update of the actor of placement type 0x2B (ActorType2B, game/actor.h;
 * func_8C04A1B2).
 *
 * A creature that reaches out and back: idle (state 0) it waits a random 300 to 600 updates, then reaches out
 * (state 1: `reach` counts up to reach_max), holds 30 updates (state 2) and goes back, turning to the other side
 * (side flips; values[4] turns to angle_a or angle_b). Its body is pulled toward the side's points: points_a +
 * reach times points_b. A racer that hits it faster than 0.463 (while idle) jolts its three scale values and plays
 * the hit (states 10-15): motion 1 forward and held, back, a blend into motion 2, motion 2, a blend back into
 * motion 0. Its contacts go into the race scene's slot list 8 every update. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/file.h"     /* NjMotion */
#include "game.h"             /* panel_get */
#include "actor.h"
#include "race_scene.h"       /* race_scene_slot_insert */
#include "scenery.h"          /* random_scaled, random_timer_scaled */

#define g_reacher_jolt      GUEST_VAR(f32, 0x8C088190u) /* 0.14999999: the scale values' jolt and pull rate */
#define REACHER_HIT_SPEED   F32_BITS(0x3EED097Au)       /* 0.46296296 */
#define REACHER_TURN_JOLT   F32(32768.0f)               /* a hit turns it by -16384 .. 16384 (Ninja units) */
#define REACHER_TURN_RATE   F32_BITS(0x3CA3D70Au)       /* 0.02 */
#define REACHER_IDLE_RANDOM F32(300.0f)
#define REACHER_IDLE_MIN    300
#define REACHER_HOLD        30
#define REACHER_HIT_HOLD    60
#define REACHER_PULL        F32_BITS(0x3DCCCCCCu)       /* 0.1: the body toward its place */
#define REACHER_DAMPING     F32_BITS(0x3F666666u)       /* 0.9 */
#define RACER_ID_LIMIT      8                           /* contacts with an id below this are racers */
#define REACHER_SLOT        8

enum {
  REACHER_IDLE = 0, REACHER_OUT = 1, REACHER_HOLDING = 2,
  REACHER_HIT = 10, REACHER_HIT_HELD = 11, REACHER_HIT_BACK = 12, REACHER_BLEND_IN = 13, REACHER_REACH = 14,
  REACHER_BLEND_OUT = 15
};

/* the last frame of motion `index` of the table */
static f32 reacher_last_frame(const ActorType2B *a, int32_t index) {
  const Type2BMotion *m = &((const Type2BMotion *)hw_ptr(a->motions))[index];
  return f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(m->motion))->nb_frames - 1));
}

/* the original's frame (0x1C bytes under r8 and PR): the place the body is pulled to */
typedef struct ReacherFrame {
  Vec3 base;             /* 0x00: the side's points_a */
  Vec3 place;            /* 0x0C: its points_b times reach, plus base */
  uint8_t rest[0x24 - 0x18]; /* 0x18: the actor, r8, PR */
} ReacherFrame;
HW_OFFSET(ReacherFrame, place, 0x0C);
HW_SIZE(ReacherFrame, 0x24);

/* was func_8C04A1B2: the update (see the top of the file) */
void type2b_update(gaddr obj) {
  ActorType2B *a = (ActorType2B *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType2B, body);
  if (a->state == REACHER_IDLE) {
    for (int32_t i = 0; i < a->contacts.count; i++) {
      if (((const int32_t *)hw_ptr(a->contacts.other))[i] > RACER_ID_LIMIT - 1)
        continue;
      if (!fgt(njScalor(a->contacts.velocity + (uint32_t)i * sizeof(Vec3)), REACHER_HIT_SPEED))
        continue;
      f32_add_to(g_reacher_jolt, member_addr(obj, ActorType2B, values[0].force));
      f32_add_to(g_reacher_jolt, member_addr(obj, ActorType2B, values[2].force));
      f32_add_to(g_reacher_jolt, member_addr(obj, ActorType2B, values[1].force));
      a->state = REACHER_HIT;
      a->motion_index = 1;
      a->frame = F32(0.0f);
      a->frame_end = reacher_last_frame(a, a->motion_index);
      a->angle = fadd(a->angle, fadd(random_timer_scaled(REACHER_TURN_JOLT), F32(-16384.0f)));
    }
  }
  body1_pull(F32(1.0f), g_reacher_jolt, member_addr(obj, ActorType2B, values[0]));
  body1_pull(F32(1.0f), g_reacher_jolt, member_addr(obj, ActorType2B, values[2]));
  body1_pull(F32(1.0f), g_reacher_jolt, member_addr(obj, ActorType2B, values[1]));
  body1_step(member_addr(obj, ActorType2B, values[0]));
  body1_step(member_addr(obj, ActorType2B, values[1]));
  body1_step(member_addr(obj, ActorType2B, values[2]));
  body1_step(member_addr(obj, ActorType2B, values[3]));
  body1_pull_angle(a->angle, REACHER_TURN_RATE, member_addr(obj, ActorType2B, values[4]));
  body1_step(member_addr(obj, ActorType2B, values[4]));
  body1_step(member_addr(obj, ActorType2B, values[5]));

  switch (a->state) {
  case REACHER_IDLE:
    a->frame = fadd(a->frame, F32(0.5f));
    if (a->active == 0) break;
    a->timer--;
    if (a->timer >= 0) break;
    a->reach = 0;
    a->state++;
    break;
  case REACHER_OUT:
    a->frame = fadd(a->frame, fadd(F32(1.0f), F32(1.0f)));
    a->reach++;
    if (a->reach < a->reach_max) break;
    a->state++;
    a->reach = a->reach_max;
    a->timer = REACHER_HOLD;
    break;
  case REACHER_HOLDING:
    a->frame = fadd(a->frame, F32(0.5f));
    a->timer--;
    if (a->timer >= 0) break;
    a->state = REACHER_IDLE;
    a->timer = f32_to_int(random_scaled(REACHER_IDLE_RANDOM)) + REACHER_IDLE_MIN;
    a->side = a->side == 0;
    a->reach = 0;
    if (a->side != 0)
      a->angle = a->angle_b;
    else
      a->angle = a->angle_a;
    break;
  case REACHER_HIT:
    a->frame = fadd(a->frame, F32(3.0f));
    if (!fgt(a->frame, a->frame_end)) break;
    a->frame = a->frame_end;
    a->timer = REACHER_HIT_HOLD;
    a->state++;
    break;
  case REACHER_HIT_HELD:
    /* the last two frames in turn */
    if (feq(a->frame, a->frame_end))
      a->frame = fadd(a->frame_end, fneg(F32(1.0f)));
    else
      a->frame = a->frame_end;
    a->timer--;
    if (a->timer >= 0) break;
    a->frame = a->frame_end;
    a->timer = 0;
    a->state++;
    break;
  case REACHER_HIT_BACK: {
    a->frame = fsub(a->frame, F32(1.0f));
    if (!fgt(F32(0.0f), a->frame)) break;
    a->frame = F32(0.0f);
    a->state++;
    if (a->side != 0)
      a->angle = a->angle_b;
    else
      a->angle = a->angle_a;
    a->blend_time = F32(0.0f);
    a->blend_length = F32(20.0f);
    const Type2BMotion *m = (const Type2BMotion *)hw_ptr(a->motions);
    a->blend_motion[0] = m[1].motion;
    a->blend_motion[1] = m[2].motion;
    a->blend_word[0] = m[1].word4;
    a->blend_word[1] = m[2].word4;
    a->blend_frame_b[0] = F32(0.0f);
    a->blend_frame[0] = F32(0.0f);
    a->blend_frame_b[1] = F32(0.0f);
    a->blend_frame[1] = F32(0.0f);
    a->blending = 1;
    break;
  }
  case REACHER_BLEND_IN:
    a->blend_time = fadd(a->blend_time, F32(1.0f));
    if (fgt(a->blend_length, a->blend_time)) break;
    a->blend_time = a->blend_length;
    a->motion_index = 2;
    a->frame_end = reacher_last_frame(a, a->motion_index);
    a->blending = 0;
    a->timer = 0;
    a->state++;
    break;
  case REACHER_REACH: {
    a->frame = fadd(a->frame, fadd(F32(1.0f), F32(1.0f)));
    if (!fgt(a->frame, a->frame_end)) break;
    a->frame = a->frame_end;
    a->state++;
    a->blend_time = F32(0.0f);
    a->blend_length = F32(10.0f);
    const Type2BMotion *m = (const Type2BMotion *)hw_ptr(a->motions);
    a->blend_motion[0] = m[2].motion;
    a->blend_motion[1] = m[0].motion;
    a->blend_word[0] = m[2].word4;
    a->blend_word[1] = m[0].word4;
    a->blend_frame_b[0] = a->frame_end;
    a->blend_frame[0] = a->frame_end;
    a->blend_frame_b[1] = F32(0.0f);
    a->blend_frame[1] = F32(0.0f);
    a->blending = 1;
    break;
  }
  case REACHER_BLEND_OUT:
    a->blend_time = fadd(a->blend_time, F32(1.0f));
    if (fgt(a->blend_length, a->blend_time)) break;
    a->blend_time = a->blend_length;
    a->motion_index = 0;
    a->frame_end = reacher_last_frame(a, a->motion_index);
    a->blending = 0;
    a->state = REACHER_IDLE;
    a->timer = f32_to_int(random_scaled(REACHER_IDLE_RANDOM)) + REACHER_IDLE_MIN;
    break;
  default:
    break;
  }

  if (a->active != 0) {
    gaddr frame = hw_stack_alloc(sizeof(ReacherFrame));
    ReacherFrame *f = (ReacherFrame *)hw_ptr(frame);
    gaddr place = member_addr(frame, ReacherFrame, place);
    if (a->side != 0) {
      memmove(&f->base, &a->points_a[1], sizeof(Vec3));
      memmove(&f->place, &a->points_b[1], sizeof(Vec3));
    } else {
      memmove(&f->base, &a->points_a[0], sizeof(Vec3));
      memmove(&f->place, &a->points_b[0], sizeof(Vec3));
    }
    vec3_scale(f32_from_int(a->reach), place);
    vec3_add(place, member_addr(frame, ReacherFrame, base));
    body_pull(REACHER_PULL, body, place);
    a->body.damping.z = REACHER_DAMPING; /* func_8C049A1A */
    a->body.damping.y = REACHER_DAMPING;
    a->body.damping.x = REACHER_DAMPING;
    body_step(body);
    hw_stack_free(sizeof(ReacherFrame));
  }
  race_scene_slot_insert(panel_get(), member_addr(obj, ActorType2B, contacts), REACHER_SLOT);
}
HW_EXPORT(8c04a1b2, type2b_update(ARG0))
