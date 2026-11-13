/* Pen Pen port, S18 b06: the racer's update in state 1, past the finish line (func_8C032ABC, racer_update.c's
 * racer_update_state1; game/racer_actions.c has state 2's, which it resembles). */
#include "hw.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "racer.h"
#include "race_scene.h"
#include "motion.h"
#include "racer_state1.h"

#define GRAVITY        F32_BITS(0xBDA740D9u) /* -0.0816667 (-9.8 / 120): added to the moving body's force.y per update */
#define STILL_SPEED    F32_BITS(0x3E6D097Au) /* 0.23148148 (25 / 108): above it the racer still swims on */
#define SWIM_RATE      F32_BITS(0x3FE66667u) /* 1.8: the motion's speed per unit of f434 */
#define FINISH_PULL    F32_BITS(0x3C23D70Au) /* 0.01: the body's pull to its finish position */
#define FINISH_DAMPING F32(0.5f)
#define MOTION_SET_STEP_A  2                 /* the swim sets (word144 picks the foot) */
#define MOTION_SET_STEP_B  3
#define MOTION_SET_PLACED  10                /* standing at the finish with an item */
/* the characters' cheers at the finish without an item */
#define MOTION_SET_CHEER_0     29
#define MOTION_SET_CHEER_2     30
#define MOTION_SET_CHEER       31
#define MOTION_SET_CHEER_5     32
#define MOTION_SET_CHEER_7     33

/* func_8C032A3A (game/racer_actions_b04.c, no header): the moving body's horizontal velocity from its cell's point */
static void racer_velocity_from_cell(gaddr racer) { CALL1(0x8C032A3Au, racer); }

/* game/actor_b05.h's (exported by func_8C030AB2's copy; that header's actor.h clashes with motion.h): the scene's
 * finish position i */
void race_scene_finish_position(gaddr scene, int32_t i, gaddr out);

static gaddr racer_motion(gaddr racer) { return member_addr(racer, Racer, second_base); }

/* the motion set `set` from its start at speed 1, unless it is the one playing */
static void racer_play_once(gaddr racer, int32_t set) {
  if (motion_player_at(racer_motion(racer))->motion_count == set) return;
  motion_player_set_speed(racer_motion(racer), F32(1.0f));
  motion_player_play_set(racer_motion(racer), set, F32(1.0f));
}

/* was func_8C032ABC: state 1's update: action 0 starts it (the velocity from the cell's point, action 1); the
 * controls are cleared; while still faster than 0.23 it swims on (the swim motion at 1.8 times f434), else it
 * stands: motion 10 when it holds an item (the scene's player_item), else its character's cheer. The body is pulled
 * to the scene's finish position of its item number (damping 0.5), falls, and the angle springs settle */
void racer_update_state1(gaddr racer) {
  Racer *r = racer_at(racer);
  if (r->action == 0) {
    racer_velocity_from_cell(racer);
    r->action = 1;
  }
  r->button_prev[0] = 0;
  r->button_prev[1] = 0;
  r->button[0] = 0;
  r->edges[0].pressed = 0;
  r->edges[0].released = 0;
  r->button[1] = 0;
  r->edges[1].pressed = 0;
  r->edges[1].released = 0;
  r->edges[1].released = 0; /* the original clears it twice */
  r->stick_y = F32(0.0f);
  r->stick_x = F32(0.0f);

  if (fgt(r->f434, STILL_SPEED)) {
    MotionPlayer *m = motion_player_at(racer_motion(racer));
    m->speed = fmul(r->f434, SWIM_RATE);
    if (m->motion_count != MOTION_SET_STEP_A && m->motion_count != MOTION_SET_STEP_B)
      motion_player_play_set(racer_motion(racer), r->word144 != 0 ? MOTION_SET_STEP_A : MOTION_SET_STEP_B,
                             F32(1.0f));
  } else if (race_scene_at(r->scene)->player_item[r->index] != 0) {
    racer_play_once(racer, MOTION_SET_PLACED);
  } else {
    switch (r->character) {
    case 0: racer_play_once(racer, MOTION_SET_CHEER_0); break;
    case 2: racer_play_once(racer, MOTION_SET_CHEER_2); break;
    case 5: racer_play_once(racer, MOTION_SET_CHEER_5); break;
    case 7: racer_play_once(racer, MOTION_SET_CHEER_7); break;
    default: racer_play_once(racer, MOTION_SET_CHEER); break;
    }
  }

  /* the original's frame (0x10 bytes under PR): the finish position is a Vec3 at its bottom */
  gaddr frame = hw_stack_alloc(0x14);
  gaddr target = frame;
  RaceScene *s = race_scene_at(r->scene);
  race_scene_finish_position(r->scene, (int32_t)s->player_item[r->index], target);
  vec3_fill(member_addr(racer, Racer, body.body.damping), FINISH_DAMPING);
  body_pull(FINISH_PULL, member_addr(racer, Racer, body), target);
  f32_add_to(GRAVITY, member_addr(racer, Racer, body.body.force.y));
  body1_pull_angle(F32(0.0f), F32_BITS(0x3DCCCCCCu) /* 0.1 */, member_addr(racer, Racer, value33c));
  body1_pull_angle(F32(0.0f), F32_BITS(0x3DCCCCCCu) /* 0.1 */, member_addr(racer, Racer, value34c));
  body1_pull_angle(F32(0.0f), F32_BITS(0x3CCCCCCCu) /* 0.025 */, member_addr(racer, Racer, value38c));
  body_pull(F32_BITS(0x3D4CCCCCu) /* 0.05 */, member_addr(racer, Racer, body2fc), member_addr(racer, Racer, vec240));
  hw_stack_free(0x14);
}
HW_EXPORT(8c032abc, racer_update_state1(ARG0))
