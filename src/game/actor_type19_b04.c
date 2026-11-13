/* Pen Pen port, S18 b04: steps 0, 1 and 3 of the actor of placement type 0x19 (ActorType19, game/actor.h; the
 * update type19_update in game/actor_bodies.c runs the step of its state; step 2 is game/actor_type19.c).
 *
 * Its body is pulled toward points[1] every update and its yaw (turns[1]) follows the way it moves; its roll
 * (turns[2]) follows roll_target. The steps run phases (ActorType19.phase) and move points[1] along the class's two
 * half-cosine paths (actor_type19_path_start / _step: the first path a point, the second's x an angle that bobs it).
 *   step 0: waits for the focused player to reach its trigger cell, waits a little, hops to one of four spots, then
 *           wanders about there in hops;
 *   step 1: circles its home (points[0]) at `radius`, now and then bobbing along a path;
 *   step 3: shuttles between the two points 20 ahead and 20 behind it, rolling over at each end. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_scene_slot_insert, race_scene_view_cell */
#include "scenery.h"      /* race_player_cell, race_scene_view_focus, random_scaled */
#include "actor_b06.h"    /* actor_type19_path_start / _step */

#define g_type19_spots       0x8C0883A4u                   /* Vec3[4]: step 0's spots (relative to nothing: placed) */
#define g_type19_turn_rate   GUEST_VAR(f32, 0x8C088324u)   /* 0.05: the yaw and roll pulls */
#define g_type19_pull        GUEST_VAR(f32, 0x8C08832Cu)   /* 0.4: the body's pull to points[1] */
#define g_type19_damping     GUEST_VAR(f32, 0x8C088330u)   /* 0.6: the body's damping */
#define g_type19_pull_hop    GUEST_VAR(f32, 0x8C088334u)   /* 0.1: the pull while it hops about (step 0, phase 3) */
#define g_type19_damping_hop GUEST_VAR(f32, 0x8C088338u)   /* 0.5 */
#define g_type19_circle_speed GUEST_VAR(f32, 0x8C088368u)  /* 82.3: step 1's speed along its circle */
#define TYPE19_MOVING        F32_BITS(0x3A83126Eu)         /* 0.001: |vx| + |vz| above it turns it */
#define TYPE19_DEGREE        F32_BITS(0x43360B60u)         /* 182.04: one degree (Ninja units) */

/* game/race_sound_ports.c (exported there, no header yet) */
void race_channel_sound_play(gaddr scene, int32_t i, int32_t channel, int32_t priority, int32_t sound);

static inline ActorType19 *type19_at(gaddr a) { return (ActorType19 *)hw_ptr(a); }

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* was func_8C0502EA (this unit's copy, still translated: written here): the body's damping, z, y, x */
static void type19_body_damping(gaddr body, f32 d) {
  Body *b = body_at(body);
  b->damping.z = d;
  b->damping.y = d;
  b->damping.x = d;
}

/* the views whose player stands within one cell of it hear sound 0x28 */
static void type19_sound(gaddr obj) {
  for (int32_t i = 0; i < (int32_t)type19_at(obj)->base.word0c; i++) {
    int32_t cell = type19_at(obj)->base.cell;
    if (!fgt(f32_abs(f32_from_int(cell - race_scene_view_cell(panel_get(), i))), F32(1.0f)))
      race_channel_sound_play(panel_get(), i, 0, 8, 0x28);
  }
}

/* while it moves (|vx| + |vz| above 0.001), its yaw is pulled toward the way it moves */
static void type19_face_motion(gaddr obj) {
  ActorType19 *a = type19_at(obj);
  f32 moving = fadd(f32_abs(a->body.velocity.x), f32_abs(a->body.velocity.z));
  if (fgt(moving, TYPE19_MOVING)) {
    f32 vz = a->body.velocity.z;
    f32 radians = rt_atan2f(a->body.velocity.x, vz);
    f32 heading = f32_from_int(f32_to_int(fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 2 pi */)));
    body1_pull_angle(heading, g_type19_turn_rate, member_addr(obj, ActorType19, turns[1]));
  }
}

/* points[1] = the first path's point, bobbed down by `height` sin(the second path's x) */
static void type19_follow_path(gaddr obj, f32 height) {
  ActorType19 *a = type19_at(obj);
  vec3_copy_bytes(member_addr(obj, ActorType19, points[1]), member_addr(obj, ActorType19, point14c));
  f32 s = njSin(f32_to_int(a->point17c.x));
  a->points[1].y = fadd(a->points[1].y, fneg(fmul(s, height)));
}

/* a hop from where it is to a random point about home (points[0]): 20 x 10 x 20 about it, lower; 60 to 70 updates */
static void type19_hop_about_home(gaddr obj, gaddr spot) {
  ActorType19 *a = type19_at(obj);
  Vec3 *v = vec3_at(spot);
  vec3_copy(spot, member_addr(obj, ActorType19, points[0]));
  v->x = fadd(v->x, fadd(random_scaled(F32(20.0f)), F32(-10.0f)));
  v->y = fadd(v->y, fadd(random_scaled(F32(10.0f)), F32(-10.0f)));
  v->z = fadd(v->z, fadd(random_scaled(F32(20.0f)), F32(-10.0f)));
  a->delay = f32_to_int(random_scaled(F32(10.0f))) + 60;
  vec3_copy_bytes(member_addr(obj, ActorType19, path_from), member_addr(obj, ActorType19, body));
  vec3_copy_bytes(member_addr(obj, ActorType19, path_to), spot);
  actor_type19_path_start(obj);
}

/* was func_8C050BAE (with its tail func_8C050F48 / func_8C050F4C): step 0 (see the top of the file). Phases: 0
 * waits for the focused player to reach the trigger cell, then 5..65 updates; 1 counts them down, then hops (a
 * path of 60..70 updates) to one of the four spots and sounds; 2 follows the hop, at its end makes the spot its home
 * and starts a hop about it; 3 hops about home again and again, its body held loosely */
void type19_step0(gaddr obj) {
  ActorType19 *a = type19_at(obj);
  gaddr frame = hw_stack_alloc(0x18); /* the original's frame: a Vec3 at 0 */
  int32_t hopping = 0;
  switch (a->phase) {
  case 0: {
    int32_t focus = race_scene_view_focus(panel_get(), 0);
    if ((int32_t)race_player_cell(panel_get(), focus) >= a->trigger_cell) {
      a->delay = f32_to_int(random_scaled(F32(60.0f))) + 5;
      a->phase++;
    }
    break;
  }
  case 1:
    a->delay--;
    if (a->delay >= 0) break;
    a->delay = f32_to_int(random_scaled(F32(10.0f))) + 60;
    a->path_length = f32_from_int(a->delay);
    a->path_b_from.x = F32(0.0f);
    a->path_b_to.x = F32(32768.0f);
    vec3_copy_bytes(member_addr(obj, ActorType19, path_from), member_addr(obj, ActorType19, body));
    {
      int32_t k = f32_to_int(random_scaled(F32(4.0f)));
      const Vec3 *spot = &((const Vec3 *)hw_ptr(g_type19_spots))[k];
      vec3_set(spot->x, spot->y, spot->z, member_addr(obj, ActorType19, points[1]));
    }
    vec3_copy_bytes(member_addr(obj, ActorType19, path_to), member_addr(obj, ActorType19, points[1]));
    actor_type19_path_start(obj);
    a->roll_target = F32(0.0f);
    a->phase++;
    a->slow = 0;
    type19_sound(obj);
    break;
  case 2:
    if (actor_type19_path_step(obj) != 0) {
      vec3_copy_bytes(member_addr(obj, ActorType19, points[0]), member_addr(obj, ActorType19, body));
      type19_hop_about_home(obj, frame);
      a->phase++;
    } else {
      type19_follow_path(obj, F32(30.0f));
    }
    break;
  case 3:
    hopping = 1;
    if (actor_type19_path_step(obj) != 0)
      type19_hop_about_home(obj, frame);
    else
      type19_follow_path(obj, F32(25.0f));
    break;
  default: /* 4, 5 */
    break;
  }

  gaddr body = member_addr(obj, ActorType19, body);
  f32 pull;
  if (hopping != 0) {
    type19_body_damping(body, g_type19_damping_hop);
    pull = g_type19_pull_hop;
  } else {
    type19_face_motion(obj);
    type19_body_damping(body, g_type19_damping);
    pull = g_type19_pull;
  }
  body_pull(pull, body, member_addr(obj, ActorType19, points[1]));
  body1_pull_angle(a->roll_target, g_type19_turn_rate, member_addr(obj, ActorType19, turns[2]));
  body_step(body);
  hw_stack_free(0x18);
}
HW_EXPORT(8c050bae, type19_step0(ARG0))

/* was func_8C05106C (with its tails func_8C0510EE and func_8C0511CC): step 1 (see the top of the file): the clock
 * counts, points[1] goes round home (its angle grows by 82.3 / radius degrees); phase 0 counts its delay down, then
 * starts a bob of 120..180 updates (phase 1), which ends with a delay of 0..180 back in phase 0 */
void type19_step1(gaddr obj) {
  ActorType19 *a = type19_at(obj);
  a->clock = fadd(a->clock, F32(1.0f));
  gaddr angle = member_addr(obj, ActorType19, circle_angle);
  f32 turn = fmul(g_type19_circle_speed, f32_reciprocal(a->radius));
  F32_AT(angle) = fmac(TYPE19_DEGREE, turn, F32_AT(angle));
  vec3_copy_bytes(member_addr(obj, ActorType19, points[1]), member_addr(obj, ActorType19, points[0]));
  f32 c = njCos(f32_to_int(a->circle_angle));
  a->points[1].x = fmac(a->radius, c, a->points[1].x);
  f32 s = njSin(f32_to_int(a->circle_angle));
  a->points[1].z = fmac(a->radius, s, a->points[1].z);
  switch (a->phase) {
  case 0:
    a->delay--;
    if (a->delay >= 0) break;
    a->delay = f32_to_int(random_scaled(F32(60.0f))) + 120;
    a->path_length = f32_from_int(a->delay);
    a->path_b_from.x = F32(0.0f);
    a->path_b_to.x = F32(32768.0f);
    actor_type19_path_start(obj);
    a->phase++;
    break;
  case 1:
    if (actor_type19_path_step(obj) != 0) {
      a->delay = f32_to_int(random_scaled(F32(180.0f)));
      a->phase--;
    }
    a->points[1].y = fadd(a->points[1].y, fneg(fmul(njSin(f32_to_int(a->point17c.x)), F32(25.0f))));
    break;
  default:
    break;
  }
  gaddr body = member_addr(obj, ActorType19, body);
  type19_face_motion(obj);
  type19_body_damping(body, g_type19_damping);
  body_pull(g_type19_pull, body, member_addr(obj, ActorType19, points[1]));
  body_step(body);
}
HW_EXPORT(8c05106c, type19_step1(ARG0))

/* the original's frame of step 3 (0x6C bytes below its saved registers) */
enum {
  STEP3_SWAP = 0x04,    /* Vec3: the path's ends swapped through it */
  STEP3_MATRIX = 0x10,  /* Matrix */
  STEP3_BEHIND = 0x50,  /* Vec3: (0, 0, -20) */
  STEP3_AHEAD = 0x5C,   /* Vec3: (0, 0, 20) */
  STEP3_FRAME = 0x6C
};

/* was func_8C051548: step 3 (see the top of the file). Phases: 0 sets the path from the point 20 ahead of it to
 * the point 20 behind (as it is turned: z, y, x), waits 180..240 updates and rolls over; 1 counts down, then swaps
 * the path's ends, starts it (60..90 updates), rolls back and sounds; 2 follows it, at its end waits 180..240 again
 * and rolls over (back to phase 1) */
void type19_step3(gaddr obj) {
  ActorType19 *a = type19_at(obj);
  gaddr frame = hw_stack_alloc(STEP3_FRAME);
  method_self(frame + STEP3_AHEAD);   /* the locals' (empty) constructors */
  method_self(frame + STEP3_BEHIND);
  matrix_identity(frame + STEP3_MATRIX);
  switch (a->phase) {
  case 0: {
    gaddr m = frame + STEP3_MATRIX;
    vec3_set(F32(0.0f), F32(0.0f), F32(20.0f), frame + STEP3_AHEAD);
    vec3_set(F32(0.0f), F32(0.0f), F32(-20.0f), frame + STEP3_BEHIND);
    njUnitMatrix(m);
    njTranslateV(m, member_addr(obj, ActorType19, body));
    njRotateZ(m, f32_to_int(a->turns[2].x));
    njRotateY(m, f32_to_int(a->turns[1].x));
    njRotateX(m, f32_to_int(a->turns[0].x));
    njCalcPoint(m, frame + STEP3_AHEAD, member_addr(obj, ActorType19, path_from));
    njCalcPoint(m, frame + STEP3_BEHIND, member_addr(obj, ActorType19, path_to));
    a->path_b_from.x = F32(0.0f);
    a->path_b_to.x = F32(32768.0f);
    a->delay = f32_to_int(random_scaled(F32(60.0f))) + 180;
    a->slow = 1;
    a->phase++;
    break;
  }
  case 1:
    a->delay--;
    if (a->delay >= 0) break;
    a->delay = f32_to_int(random_scaled(F32(30.0f))) + 60;
    a->path_length = f32_from_int(a->delay);
    vec3_copy(frame + STEP3_SWAP, member_addr(obj, ActorType19, path_to));
    vec3_copy_bytes(member_addr(obj, ActorType19, path_to), member_addr(obj, ActorType19, path_from));
    vec3_copy_bytes(member_addr(obj, ActorType19, path_from), frame + STEP3_SWAP);
    actor_type19_path_start(obj);
    a->roll_target = F32(0.0f);
    a->slow = 0;
    a->phase++;
    type19_sound(obj);
    break;
  case 2:
    if (actor_type19_path_step(obj) != 0) {
      a->delay = f32_to_int(random_scaled(F32(60.0f))) + 180;
      a->roll_target = F32(32768.0f);
      a->slow = 1;
      a->phase--;
    }
    type19_follow_path(obj, F32(25.0f));
    break;
  default:
    break;
  }
  gaddr body = member_addr(obj, ActorType19, body);
  type19_face_motion(obj);
  type19_body_damping(body, g_type19_damping);
  body_pull(g_type19_pull, body, member_addr(obj, ActorType19, points[1]));
  body1_pull_angle(a->roll_target, g_type19_turn_rate, member_addr(obj, ActorType19, turns[2]));
  body_step(body);
  hw_stack_free(STEP3_FRAME);
}
HW_EXPORT(8c051548, type19_step3(ARG0))
