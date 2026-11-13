/* Pen Pen port, S16 (b04): the racer's running: action 1 of state 3 (func_8C033104, racer_run) and actions 1 and
 * 2 of state 4 (func_8C033F0C, racer_state4_run); game/racer_actions.c calls both through wrappers of these names.
 *
 * In state 3 the racer walks: the stick's length pushes it forward and its y turns it, both by how far the stick
 * points from straight ahead (CharacterParams.walk_push / walk_turn, three ranges); on special ground the push is
 * stronger (1.2 times), scaled by the ground and dropped when fast, and button 0 jumps; elsewhere only a slow racer
 * is pushed (0.75 times) and button 0 lifts. Button 1 starts the dash (action 2). The motion sets follow the
 * ground and the speed (13 on ground 2, 6 / 7 for the words at 0x174 / 0x17C, 2 / 3 walking (backwards beyond 150
 * degrees), 10 when nearly still), and off special ground a step timer alternates the walking set.
 *
 * In state 4 the racer strokes with button 0: the longer it is held, the higher the stroke level (CharacterParams.hold_levels
 * give the thresholds, levels 1-5), and level 6 is the short while after a release; 0 otherwise. The level and the
 * ground pick the forward push and the turn rate (CharacterParams.run_push / run_turn); on special ground the
 * moving body is damped by the level's pair (run_damping) and the push is dropped when the racer is fast (f434 above
 * 9.26), elsewhere it is damped by f200 and the push always dropped. Slow, it turns more and the stick's x pushes it
 * a little. The racer then leans by how the velocity crosses its forward axis, and the motion sets follow the
 * button (17 while released long enough, 19 while held, 20 at a release). */
#include "hw.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "racer.h"
#include "motion.h"

#define FAST_RUN      F32_BITS(0x411425EBu) /* 9.259259: above it the push is dropped on special ground */
#define SLOW_RUN      F32_BITS(0x3EED097Au) /* 0.46296296: below it the racer turns more */
#define STICK_X_PUSH  F32_BITS(0x3C638E38u) /* 0.013888889: the slow racer's push per stick x */
#define LEAN_SPEED    F32_BITS(0x406D097Au) /* 3.7037036: f434 is divided by it to scale the velocity */
#define LEAN_RATE     F32_BITS(0x43E38E37u) /* 455.11108 (0x10000 / 144): Ninja angle units per unit of lean */
#define LEAN_PULL     F32_BITS(0x3CCCCCCCu) /* 0.025: value38c pulled back to 0 */
#define RELEASED_LONG 60                    /* updates after a release before the released motion (17) plays */
#define MOTION_RELEASED 17
#define MOTION_HELD     19
#define MOTION_RELEASE  20
#define WALK_SPECIAL    F32_BITS(0x3F999999u) /* 1.1999999: the push and turn on special ground */
#define WALK_SLOW       F32(0.75f)            /* the push of a slow racer elsewhere */
#define WALK_FAST       F32_BITS(0x41855555u) /* 16.666666: no push above it on special ground */
#define WALK_STILL      F32_BITS(0x3E2AAAAAu) /* 0.16666666: below it the standing motion (10) plays */
#define STICK_AHEAD     F32_BITS(0x468E38E3u) /* 18204.443: 100 degrees (Ninja angle units) */
#define STICK_ACROSS    F32_BITS(0x46D55555u) /* 27306.666: 150 degrees */
#define STEP_TIMER_HELD 3

static CharacterParams *params_of(const Racer *r) { return character_params_at(r->params); }
/* the racer's character's motion player (the MotionPlayer at racer + 0x24) */
static gaddr racer_motion(gaddr racer) { return member_addr(racer, Racer, second_base); }

/* the original's frame of racer_run (0x28 bytes under PR): the Vec3s the helpers are handed */
typedef struct WalkFrame {
  Vec3 velocity;         /* 0x00: half the moving body's velocity */
  int32_t way;           /* 0x0C: 0, 1, 2: how far the stick points from ahead */
  uint8_t pad_10[0x08];  /* 0x10: the stick's angle, the turn */
  Vec3 push;             /* 0x18: the push of this update */
  uint8_t rest[0x2C - 0x24]; /* 0x24: the racer, PR */
} WalkFrame;
HW_OFFSET(WalkFrame, push, 0x18);
HW_SIZE(WalkFrame, 0x2C);

/* was func_8C033104: state 3's walking (see the top of the file) */
void racer_run(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr frame = hw_stack_alloc(sizeof(WalkFrame));
  WalkFrame *f = (WalkFrame *)hw_ptr(frame);
  gaddr push = member_addr(frame, WalkFrame, push);
  gaddr body = member_addr(racer, Racer, body);

  if (r->action_step == 0) {
    racer_contacts_reset(racer);
    r->action_step = 1;
  }
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), push);
  f32 turn = F32(0.0f);
  f32 off_ahead = f32_abs(r->stick_angle);
  if (fgt(STICK_AHEAD, off_ahead))
    f->way = 0;
  else if (fgt(STICK_ACROSS, off_ahead))
    f->way = 1;
  else
    f->way = 2;

  if (r->ground_touch != 0) {
    f32 d = params_of(r)->ground_damping[r->ground];
    vec3_set(d, params_of(r)->damping_y, d, member_addr(body, Body, damping));
    f->push.y = F32(0.0f);
    f->push.x = F32(0.0f);
    f->push.z = fmul(fmul(params_of(r)->walk_push[f->way], r->stick_length), WALK_SPECIAL);
    turn = fmul(fmul(params_of(r)->walk_turn[f->way], r->stick_y), WALK_SPECIAL);
    if (r->edges[0].pressed != 0)
      f->push.y = fadd(f->push.y, params_of(r)->jump_push);
    if (fgt(r->f434, WALK_FAST))
      f->push.z = F32(0.0f);
    f->push.x = fmul(f->push.x, params_of(r)->walk_scale[r->ground]); /* x is 0 here: kept as the original */
    f->push.z = fmul(f->push.z, params_of(r)->walk_scale[r->ground]);
    if (r->mark_a_on != 0 && fgt(r->mark_a_value, SLOW_RUN)) {
      gaddr velocity = member_addr(frame, WalkFrame, velocity);
      vec3_copy(velocity, member_addr(body, Body, velocity));
      vec3_scale(F32(0.5f), velocity);
      vec3_copy_bytes(member_addr(body, Body, velocity), velocity);
    }
  } else {
    vec3_fill(member_addr(body, Body, damping), params_of(r)->damping_y);
    f->push.z = F32(0.0f);
    f->push.y = F32(0.0f);
    f->push.x = F32(0.0f);
    if (r->button[0] != 0)
      f->push.y = fadd(f->push.y, params_of(r)->lift_push);
    if (fgt(SLOW_RUN, r->f434))
      f->push.z = fmul(fmul(params_of(r)->walk_push[f->way], r->stick_length), WALK_SLOW);
    turn = fmul(params_of(r)->walk_turn[f->way], r->stick_y);
  }
  vec3_add(member_addr(body, FrameBody, push), push);
  f32_add_to(turn, member_addr(racer, Racer, heading.force));
  body1_pull_angle(F32(0.0f), LEAN_PULL, member_addr(racer, Racer, value38c));
  if (r->word1a8 != 1 && r->edges[1].pressed != 0)
    racer_set_action(racer, 2);
  racer_dash_check_hits(racer);

  if (r->ground_touch == 0) {
    if (--r->step_timer == 0) {
      motion_player_play_set(racer_motion(racer), 0, F32(1.0f));
      r->word144 = r->word144 == 0;
    } else if (r->step_timer < 0) {
      r->step_timer = -1;
    }
  }
  MotionPlayer *m = motion_player_at(racer_motion(racer));
  if (r->word254 != 0) {
    r->step_timer = STEP_TIMER_HELD;
  } else if (r->ground_touch != 0) {
    if (fgt(r->f434, WALK_STILL)) {
      if (r->ground == 2) {
        m->speed = F32(1.0f);
        if (m->motion_count != 13)
          motion_player_play_set(racer_motion(racer), 13, F32(1.0f));
      } else if (r->word17c != 0 || r->word174 != 0) {
        if (r->word174 != 0)
          m->speed = fmul(r->stick_length, F32(1.5f));
        else
          m->speed = r->stick_length;
        if (m->motion_count != 6 && m->motion_count != 7)
          motion_player_play_set(racer_motion(racer), r->word144 != 0 ? 6 : 7, F32(1.0f));
      } else {
        m->speed = fmul(r->f434, F32_BITS(0x3FE66667u) /* 1.8 */);
        if (fgt(f32_abs(r->stick_angle), STICK_ACROSS))
          m->speed = fneg(m->speed);
        if (m->motion_count != 2 && m->motion_count != 3)
          motion_player_play_set(racer_motion(racer), r->word144 != 0 ? 2 : 3, F32(1.0f));
      }
    } else if (m->motion_count != 10) {
      motion_player_play_set(racer_motion(racer), 10, F32(1.0f));
    }
  }
  hw_stack_free(sizeof(WalkFrame));
}
HW_EXPORT(8c033104, racer_run(ARG0))

/* the original's frame of racer_state4_run (0x30 bytes under PR): the Vec3s the helpers are handed */
typedef struct RunFrame {
  uint8_t pad_00[0x04];  /* 0x00: the lean */
  Vec3 forward;          /* 0x04: the racer's forward axis (matrix3a8's third row) */
  Vec3 velocity;         /* 0x10: vec428 scaled by f434 / 3.7 */
  uint8_t pad_1c[0x04];  /* 0x1C: the turn */
  Vec3 push;             /* 0x20: the push of this update */
  uint8_t rest[0x34 - 0x2C]; /* 0x2C: the racer, PR */
} RunFrame;
HW_OFFSET(RunFrame, forward, 0x04);
HW_OFFSET(RunFrame, velocity, 0x10);
HW_OFFSET(RunFrame, push, 0x20);
HW_SIZE(RunFrame, 0x34);

/* was func_8C033F0C: state 4's running (see the top of the file) */
void racer_state4_run(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr frame = hw_stack_alloc(sizeof(RunFrame));
  RunFrame *f = (RunFrame *)hw_ptr(frame);
  gaddr push = member_addr(frame, RunFrame, push);
  gaddr body = member_addr(racer, Racer, body);

  if (r->action_step == 0)
    r->action_step = 1;
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), push);
  f32 turn = F32(0.0f);
  racer_handle_contacts(racer);

  if (r->button[0] != 0) {
    r->release_frames = 0;
    r->hold_frames++;
  } else {
    r->release_frames++;
    if (r->hold_frames != 0) {
      r->last_hold = r->hold_frames;
      r->hold_frames = 0;
    }
  }
  const CharacterParams *p = params_of(r);
  if (r->hold_frames > p->hold_levels[4])
    r->stroke_level = 5;
  else if (r->hold_frames > p->hold_levels[3])
    r->stroke_level = 4;
  else if (r->hold_frames > p->hold_levels[2])
    r->stroke_level = 3;
  else if (r->hold_frames > p->hold_levels[1])
    r->stroke_level = 2;
  else if (r->hold_frames > p->hold_levels[0])
    r->stroke_level = 1;
  else if (r->release_frames > 0 && !(r->release_frames > p->release_frames_max))
    r->stroke_level = 6;
  else
    r->stroke_level = 0;

  f->push.z = fmac(F32(1.0f), params_of(r)->run_push[r->ground][r->stroke_level], f->push.z);
  turn = fmac(F32(1.0f), fmul(params_of(r)->run_turn[r->ground][r->stroke_level], r->stick_y), turn);
  if (r->ground_touch != 0) {
    const RunDamping *d = &params_of(r)->run_damping[r->ground][r->stroke_level];
    vec3_set(d->x, params_of(r)->f200, d->z, member_addr(body, Body, damping));
    if (fgt(r->f434, FAST_RUN))
      vec3_zero(push);
  } else {
    vec3_fill(member_addr(body, Body, damping), params_of(r)->f200);
    vec3_zero(push);
  }
  if (fgt(SLOW_RUN, r->f434)) {
    f32 slowness = fdiv(fsub(SLOW_RUN, r->f434), SLOW_RUN);
    turn = fmul(turn, fmac(params_of(r)->slow_turn, slowness, F32(1.0f)));
    f->push.z = fmac(STICK_X_PUSH, r->stick_x, f->push.z);
  }
  vec3_add(member_addr(body, FrameBody, push), push);
  f32_add_to(turn, member_addr(racer, Racer, heading.force));

  /* lean: the scaled velocity across the forward axis */
  gaddr velocity = member_addr(frame, RunFrame, velocity);
  gaddr forward = member_addr(frame, RunFrame, forward);
  vec3_copy(velocity, member_addr(racer, Racer, vec428));
  vec3_scale(fdiv(r->f434, LEAN_SPEED), velocity);
  vec3_copy(forward, member_addr(racer, Racer, matrix3a8.m[8]));
  f32 lean = vec3_cross_y(velocity, forward);
  f32_add_to(fmul(lean, LEAN_RATE), member_addr(racer, Racer, value34c.force));
  body1_pull_angle(F32(0.0f), LEAN_PULL, member_addr(racer, Racer, value38c));

  MotionPlayer *m = motion_player_at(racer_motion(racer));
  if (r->edges[0].released != 0)
    motion_player_play_set(racer_motion(racer), MOTION_RELEASE, F32(1.0f));
  if (r->button[0] != 0) {
    if (m->motion_count == MOTION_RELEASED)
      motion_player_play_set(racer_motion(racer), MOTION_HELD, F32(1.0f));
    else if (m->motion_count != MOTION_HELD && m->ended != 0)
      motion_player_play_set(racer_motion(racer), MOTION_HELD, F32(1.0f));
  } else if (r->release_frames > RELEASED_LONG) {
    if (m->motion_count != MOTION_RELEASED)
      motion_player_play_set(racer_motion(racer), MOTION_RELEASED, F32(1.0f));
    else if (m->ended != 0)
      motion_player_play_set(racer_motion(racer), MOTION_RELEASED, F32(1.0f));
  }
  hw_stack_free(sizeof(RunFrame));
}
HW_EXPORT(8c033f0c, racer_state4_run(ARG0))
