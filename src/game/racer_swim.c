/* Pen Pen port, S18 b06: the racer's swimming strokes, actions 1 and 2 of state 5 (func_8C034A44; racer_actions_b04.c's
 * state 5 update calls it as racer_swim_strokes). The same shape as state 4's running (game/racer_run.c,
 * racer_state4_run): the button's hold and release give the stroke level, which picks the push, the turn and the
 * rate the body tilts at from the character's parameters. */
#include "hw.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "racer.h"
#include "motion.h"
#include "racer_swim.h"

#define SWIM_FAST       F32_BITS(0x42055555u) /* 33.333332: above it the stroke's push is dropped */
#define LEAN_PULL       F32_BITS(0x3CCCCCCCu) /* 0.025: value38c pulled back to 0 */
#define BODY2_PULL      F32_BITS(0x3D4CCCCCu) /* 0.05: body2fc pulled to (0, 1, 0) */
#define RELEASED_LONG   60                    /* updates after a release before the released motion (23) plays */
#define MOTION_RELEASED 23
#define MOTION_HELD     24
#define MOTION_RELEASE  25

static CharacterParams *params_of(const Racer *r) { return character_params_at(r->params); }
/* the racer's character's motion player (the MotionPlayer at racer + 0x24) */
static gaddr racer_motion(gaddr racer) { return member_addr(racer, Racer, second_base); }

/* func_8C0349E8 (game/racer_actions_b04.c, no header): the strokes start over, motion set 0x19 */
static void racer_swim_strokes_start(gaddr racer) { CALL1(0x8C0349E8u, racer); }
/* func_8C035014 (game/racer_actions_b04.c, no header): the contacts with racers and objects in state 5 */
static void racer_bump_contacts(gaddr racer) { CALL1(0x8C035014u, racer); }

/* the original's frame (0x24 bytes under PR) */
typedef struct SwimFrame {
  Vec3 up;            /* 0x00: (0, 1, 0): where body2fc is pulled */
  f32 tilt_rate;      /* 0x0C */
  f32 turn;           /* 0x10 */
  Vec3 push;          /* 0x14: the push of this update */
  gaddr racer;        /* 0x20 */
  uint32_t saved_pr;  /* 0x24 */
} SwimFrame;
HW_OFFSET(SwimFrame, push, 0x14);
HW_SIZE(SwimFrame, 0x28);

/* was func_8C034A44: state 5's strokes (see the top of the file) */
void racer_swim_strokes(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr frame = hw_stack_alloc(sizeof(SwimFrame));
  SwimFrame *f = (SwimFrame *)hw_ptr(frame);
  gaddr push = member_addr(frame, SwimFrame, push);
  gaddr up = member_addr(frame, SwimFrame, up);

  if (r->action_step == 0) {
    racer_swim_strokes_start(racer);
    r->action_step++;
  }
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), push);
  f32 turn = F32(0.0f);
  racer_bump_contacts(racer);

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

  f->push.z = fmac(F32(1.0f), params_of(r)->swim_push[r->stroke_level], f->push.z);
  turn = fmac(F32(1.0f), fmul(params_of(r)->swim_turn[r->stroke_level], r->stick_y), turn);
  f32 tilt_rate = params_of(r)->swim_tilt_rate[r->stroke_level];
  if (fgt(r->f434, SWIM_FAST)) vec3_zero(push);
  vec3_add(member_addr(racer, Racer, body.push), push);
  f32_add_to(turn, member_addr(racer, Racer, heading.force));
  body1_pull_angle(fmul(r->stick_x, params_of(r)->swim4_tilt), tilt_rate, member_addr(racer, Racer, value33c));
  body1_pull_angle(fmul(r->stick_y, params_of(r)->swim4_tilt), tilt_rate, member_addr(racer, Racer, value34c));
  body1_pull_angle(F32(0.0f), LEAN_PULL, member_addr(racer, Racer, value38c));
  vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), up);
  body_pull(BODY2_PULL, member_addr(racer, Racer, body2fc), up);

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
  }
  hw_stack_free(sizeof(SwimFrame));
}
HW_EXPORT(8c034a44, racer_swim_strokes(ARG0))
