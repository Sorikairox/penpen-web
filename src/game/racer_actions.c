/* Pen Pen port, S15 b05: the racer's actions (0x8C031D8C-0x8C0351C8): what a racer does inside its states 3 and 4
 * (the update of state n runs the action in Racer.action, then the common pulls of its moving body).
 *
 * An action runs in steps (Racer.action_step, 0 = start it) and times each step with Racer.action_timer: the step
 * ends when the timer goes below 0. racer_set_action (core/inline.c) starts an action with both cleared. The racer's
 * Contacts carry a kind word (unknown1c: 1 while dashing, 0 otherwise) and a mass that the actions change; the
 * character's parameters (CharacterParams) give the lengths, masses, pushes and dampings. The motion player at
 * racer + 0x24 plays a motion set per action (11 / 12 the dash, 14 / 15 / 16 the hits, 19, 20 in state 4). */
#include "hw.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "racer.h"
#include "race_scene.h"
#include "motion.h"
#include "effects.h"

/* the item scales of action 4 in state 4: an f32 per item */
#define g_item_time_scale ((const f32 *)hw_ptr(0x8C085608u))

#define GRAVITY F32_BITS(0xBDA740D9u)   /* -0.0816667 (-9.8 / 120): added to the moving body's force.y per update */
#define HALF_TURN F32(32768.0f)         /* Ninja angle units */
#define TEN_DEGREES F32_BITS(0x44E38E37u) /* 1820.444 (0x10000 / 36) */

static CharacterParams *params_of(const Racer *r) { return character_params_at(r->params); }
/* the racer's character's motion player (the MotionPlayer at racer + 0x24) */
static gaddr racer_motion(gaddr racer) { return member_addr(racer, Racer, second_base); }
static gaddr racer_damping(gaddr racer) { return member_addr(racer, Racer, body.body.damping); }

/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* still-translated parts of the actions */
/* func_8C033104: action 1 of state 3 (the racer's normal running) */
static void racer_run(gaddr racer) { CALL1(0x8C033104u, racer); }
/* func_8C033810: action 4 of state 3 */
static void racer_state3_action4(gaddr racer) { CALL1(0x8C033810u, racer); }
/* func_8C03390C: the weak hit's start (its frames and motion set 14) */
static void racer_hit_start_weak(gaddr racer) { CALL1(0x8C03390Cu, racer); }
/* func_8C033F0C: actions 1 and 2 of state 4 */
static void racer_state4_run(gaddr racer) { CALL1(0x8C033F0Cu, racer); }
/* func_8C0345A8: action 3 of state 4 */
static void racer_state4_action3(gaddr racer) { CALL1(0x8C0345A8u, racer); }

/* ---- action 2 of state 3: the dash ---- */

/* was func_8C03360A: start the dash: the racer's contacts become kind 1 with the character's dash mass, its moving
 * body gets the dash dampings and a push forward, and the dash's motion set (11) plays for dash_frames */
void racer_dash_start(gaddr racer) {
  Racer *r = racer_at(racer);
  r->contacts.unknown1c = 1;
  r->contacts.mass = params_of(r)->dash_mass;
  vec3_set(params_of(r)->dash_damping, params_of(r)->damping_y, params_of(r)->dash_damping, racer_damping(racer));
  f32_add_to(params_of(r)->dash_push, member_addr(racer, Racer, body.push.z));
  r->action_timer = params_of(r)->dash_frames;
  motion_player_play_set(racer_motion(racer), 11, F32(1.0f));
}
HW_EXPORT(8c03360a, racer_dash_start(ARG0))

/* the attribute (Contacts.unknown48) of contact k of the racer */
static int32_t contact_attr(const Racer *r, int32_t k) { return ((const int32_t *)hw_ptr(r->contacts.unknown48))[k]; }

/* was func_8C033A88: during the dash, what the racer ran into. Into another racer (racer_contact) whose contact has
 * attribute 1: it is hit itself (action 3), harder the heavier the other is than twice its own mass; else, while
 * its contacts are of kind 1 (dashing), it bounces: back to running, and a snapshot of the racer is handed to its
 * effects' set C. Into something else (object_contact) of attribute 3 / 2 / 1: hit, weak / medium / strong. */
void racer_dash_check_hits(gaddr racer) {
  Racer *r = racer_at(racer);
  r->had_contacts = 0;
  r->hit_strength = -1;
  if (r->racer_contact != -1) {
    if (contact_attr(r, r->racer_contact) == 1) {
      f32 other = ((const f32 *)hw_ptr(r->contacts.other_mass))[r->racer_contact];
      f32 twice = fmul(r->contacts.mass, F32(2.0f));
      if (fgt(other, twice))
        r->hit_strength = 2;
      else if (feq(other, twice))
        r->hit_strength = 1;
      else
        r->hit_strength = 0;
      racer_set_action(racer, 3);
    } else if (r->contacts.unknown1c == 1) {
      r->had_contacts = 1;
      if (r->contacts.unknown1c == 1) {
        racer_set_action(racer, 1);
        gaddr frame = hw_stack_alloc(sizeof(RibbonFrame));
        RibbonFrame *f = (RibbonFrame *)hw_ptr(frame);
        matrix_identity(frame);
        memmove(&f->m, &r->matrix3a8, sizeof(Matrix));
        memmove(&f->position, hw_ptr(method_self(member_addr(racer, Racer, body))), sizeof(Vec3));
        memmove(&f->velocity, hw_ptr(member_addr(racer, Racer, body.body.velocity)), sizeof(Vec3));
        memmove(&f->vec58, &r->vec534, sizeof(Vec3));
        memmove(&f->vec64, &r->vec540, sizeof(Vec3));
        f->low = r->contacts.radius;
        f->high = params_of(r)->f04;
        f->f78 = r->f434;
        f->heading = racer_heading(racer);
        f->word80 = r->word144;
        f->word84 = r->word174;
        f->word88 = r->word17c;
        f->word8c = r->word180;
        f->index = r->index;
        /* (the original also passes the contact's normal in r6; set C's push takes two arguments) */
        racer_effects_push_set_c(r->child, frame);
        hw_stack_free(sizeof(RibbonFrame));
      } else {
        r->had_contacts = 1;
      }
    }
  }
  if (r->object_contact != -1) {
    if (contact_attr(r, r->object_contact) == 3) {
      r->hit_strength = 0;
      racer_set_action(racer, 3);
    } else if (contact_attr(r, r->object_contact) == 2) {
      r->hit_strength = 1;
      racer_set_action(racer, 3);
    } else if (contact_attr(r, r->object_contact) == 1) {
      r->hit_strength = 2;
      racer_set_action(racer, 3);
    }
  }
}
HW_EXPORT(8c033a88, racer_dash_check_hits(ARG0))

/* was func_8C0336BC: the dash: started, then for dash_frames it checks what it hits; then the contacts come back to
 * normal and motion set 12 plays for dash_end_frames, after which the racer runs again (action 1). The value at
 * 0x38C is pulled back to 0 all along. */
void racer_dash(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action_step) {
  case 0:
    racer_dash_start(racer);
    r->action_step = 1;
    break;
  case 1:
    if (--r->action_timer < 0) {
      r->contacts.unknown1c = 0;
      r->contacts.mass = params_of(r)->mass;
      r->action_timer = params_of(r)->dash_end_frames;
      motion_player_play_set(racer_motion(racer), 12, F32(1.0f));
      r->action_step = 2;
    }
    racer_dash_check_hits(racer);
    break;
  case 2:
    if (--r->action_timer < 0) racer_set_action(racer, 1);
    break;
  }
  body1_pull_angle(F32(0.0f), F32_BITS(0x3CCCCCCCu) /* 0.025 */, member_addr(racer, Racer, value38c));
}
HW_EXPORT(8c0336bc, racer_dash(ARG0))

/* ---- action 3 of state 3: hit ---- */

/* was func_8C03392A: the medium hit's start: its frames and motion set 15 */
void racer_hit_start_medium(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action_timer = params_of(r)->hit_frames[1];
  motion_player_play_set(racer_motion(racer), 15, F32(1.0f));
}
HW_EXPORT(8c03392a, racer_hit_start_medium(ARG0))

/* was func_8C033948: the strong hit's start: its frames and motion set 16 */
void racer_hit_start_strong(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action_timer = params_of(r)->hit_frames[0];
  motion_player_play_set(racer_motion(racer), 16, F32(1.0f));
}
HW_EXPORT(8c033948, racer_hit_start_strong(ARG0))

/* was func_8C033966: the contacts while hit: kind 0, the character's hit mass */
void racer_hit_contacts(gaddr racer) {
  Racer *r = racer_at(racer);
  r->contacts.unknown1c = 0;
  r->contacts.mass = params_of(r)->hit_mass;
}
HW_EXPORT(8c033966, racer_hit_contacts(ARG0))

/* was func_8C033992: hit (hit_strength 0, 1, 2 picks how hard; anything else goes back to running), with vibration
 * pattern 7; then the moving body is damped by the ground it is on until the hit's frames run out. The value at
 * 0x38C is pulled to half a turn all along. */
void racer_hit(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action_step) {
  case 0:
    r->action_step = 1;
    racer_hit_contacts(racer);
    switch (r->hit_strength) {
    case 0: racer_hit_start_weak(racer); break;
    case 1: racer_hit_start_medium(racer); break;
    case 2: racer_hit_start_strong(racer); break;
    default: racer_set_action(racer, 1); break;
    }
    racer_set_variant(racer, 7);
    break;
  case 1: {
    f32 d = params_of(r)->ground_damping[r->ground];
    vec3_set(d, params_of(r)->damping_y, d, racer_damping(racer));
    if (--r->action_timer < 0) racer_set_action(racer, 1);
    break;
  }
  }
  body1_pull_angle(HALF_TURN, F32_BITS(0x3CCCCCCCu) /* 0.025 */, member_addr(racer, Racer, value38c));
}
HW_EXPORT(8c033992, racer_hit(ARG0))

/* ---- the states' updates ---- */

/* the pulls that end both updates: gravity on the moving body, the value at 0x33C pulled to 0 at `rate33c`, the one
 * at 0x34C at 0.1, and, while it touches special ground, the second body pulled to where it touched */
static void racer_state_pulls(gaddr racer, f32 rate33c) {
  f32_add_to(GRAVITY, member_addr(racer, Racer, body.body.force.y));
  body1_pull_angle(F32(0.0f), rate33c, member_addr(racer, Racer, value33c));
  body1_pull_angle(F32(0.0f), F32_BITS(0x3DCCCCCCu) /* 0.1 */, member_addr(racer, Racer, value34c));
  if (racer_at(racer)->ground_touch != 0)
    body_pull(F32_BITS(0x3D4CCCCCu) /* 0.05 */, member_addr(racer, Racer, body2fc), member_addr(racer, Racer, vec240));
}

/* was func_8C032DBC: state 2's update (before the racer may move): on its first update (action 0) the camera's intro
 * step and the word at 0x528 are reset; every update the controls are cleared, the moving body is sent toward the
 * point of the course cell it is on (its velocity's x and z, 0.926 of the point) and the heading and stick angle
 * follow that direction, the body gets the dampings of ground kind 3, the motion runs at 1.8 times the value at
 * 0x434, and motion set 2 or 3 starts unless one of them plays; then the pulls (gravity, the values at 0x33C and
 * 0x34C toward 0 at 0.1, the one at 0x38C at 0.025, the second body toward vec240), and step 0 counts its timer
 * down to step 1. */
void racer_update_state2(gaddr racer) {
  Racer *r = racer_at(racer);
  if (r->action == 0) {
    method_empty(); /* func_8C032D6C: an empty method */
    r->action = 1;
    r->camera_step = 0;
    r->word528 = 0;
  }
  r->button_prev[0] = 0;
  r->button_prev[1] = 0;
  r->button[0] = 0;
  r->edges[0].pressed = 0;
  r->edges[0].released = 0;
  r->button[1] = 0;
  r->edges[1].pressed = 0;
  r->edges[1].released = 0;
  r->stick_x = F32(0.0f);
  r->stick_y = F32(0.0f);
  r->stick_length = F32(0.0f);

  gaddr toward = hw_stack_alloc(sizeof(Vec3));
  gaddr point = race_scene_cell_point(r->scene, (int32_t)racer_cell(racer));
  vec3_scaled(F32_BITS(0x3F6D097Au) /* 0.9259259 */, toward, point);
  Vec3 *t = vec3_at(toward);
  r->body.body.velocity.x = t->x;
  r->body.body.velocity.z = t->z;
  f32 radians = rt_atan2f(t->x, t->z);
  hw_stack_free(sizeof(Vec3));
  /* radians to Ninja angle units, truncated to a whole unit */
  f32 angle = f32_from_int(f32_to_int(fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 2 pi */)));
  r->heading.value = angle;
  r->stick_angle = angle;

  vec3_set(params_of(r)->ground_damping[3], params_of(r)->damping_y, params_of(r)->ground_damping[3],
           racer_damping(racer));
  MotionPlayer *m = motion_player_at(racer_motion(racer));
  m->speed = fmul(r->f434, F32_BITS(0x3FE66667u) /* 1.8 */);
  if (m->motion_count != 2 && m->motion_count != 3)
    motion_player_play_set(racer_motion(racer), r->word144 != 0 ? 2 : 3, F32(1.0f));

  f32_add_to(GRAVITY, member_addr(racer, Racer, body.body.force.y));
  body1_pull_angle(F32(0.0f), F32_BITS(0x3DCCCCCCu) /* 0.1 */, member_addr(racer, Racer, value33c));
  body1_pull_angle(F32(0.0f), F32_BITS(0x3DCCCCCCu) /* 0.1 */, member_addr(racer, Racer, value34c));
  body1_pull_angle(F32(0.0f), F32_BITS(0x3CCCCCCCu) /* 0.025 */, member_addr(racer, Racer, value38c));
  body_pull(F32_BITS(0x3D4CCCCCu) /* 0.05 */, member_addr(racer, Racer, body2fc), member_addr(racer, Racer, vec240));

  if (r->action_step == 0) {
    if (--r->action_timer < 0) r->action_step = 1;
  }
}
HW_EXPORT(8c032dbc, racer_update_state2(ARG0))

/* was func_8C033004: state 3's update: its action (0 starts running), then the pulls */
void racer_update_state3(gaddr racer) {
  switch (racer_at(racer)->action) {
  case 0: racer_set_action(racer, 1); break;
  case 1: racer_run(racer); break;
  case 2: racer_dash(racer); break;
  case 3: racer_hit(racer); break;
  case 4: racer_state3_action4(racer); break;
  }
  racer_state_pulls(racer, F32_BITS(0x3DCCCCCCu) /* 0.1 */);
}
HW_EXPORT(8c033004, racer_update_state3(ARG0))

/* was func_8C033DC4: state 4's start: action 1, the contacts back to normal, the words at 0x288-0x294 cleared,
 * motion set 20, and the ribbon reset from the contact radius along the matrix at 0x3A8 */
void racer_state4_start(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action = 1;
  r->contacts.unknown1c = 0;
  r->contacts.mass = params_of(r)->mass;
  r->word28c = 0;
  r->word290 = 0;
  r->word294 = 0;
  r->word288 = 0;
  motion_player_play_set(racer_motion(racer), 20, F32(1.0f));
  racer_effects_reset_ribbon(r->child, r->contacts.radius, member_addr(racer, Racer, matrix3a8));
}
HW_EXPORT(8c033dc4, racer_state4_start(ARG0))

/* was func_8C03438C: start action 4 of state 4: motion set 19 for frames378 frames, scaled by the racer's item */
void racer_state4_action4_start(gaddr racer) {
  Racer *r = racer_at(racer);
  motion_player_play_set(racer_motion(racer), 19, F32(1.0f));
  r->action_timer = params_of(r)->frames378;
  uint32_t item = race_scene_at(r->scene)->player_item[r->index];
  r->action_timer = f32_to_int(fmul(f32_from_int(r->action_timer), g_item_time_scale[item]));
}
HW_EXPORT(8c03438c, racer_state4_action4_start(ARG0))

/* was func_8C0343D6: action 4 of state 4: started, then until its frames run out the moving body is damped (and
 * pushed on special ground) and the stick's y turns the heading and the value at 0x34C; the value at 0x38C is
 * pulled back to 0 all along */
void racer_state4_action4(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action_step) {
  case 0:
    racer_state4_action4_start(racer);
    r->action_step++;
    break;
  case 1:
    if (r->ground_touch != 0) {
      vec3_set(params_of(r)->f354, params_of(r)->f200, params_of(r)->f358, racer_damping(racer));
      f32_add_to(params_of(r)->f1f8, member_addr(racer, Racer, body.push.z));
    } else {
      vec3_fill(racer_damping(racer), params_of(r)->f200);
    }
    f32_add_to(fmul(params_of(r)->f1fc, r->stick_y), member_addr(racer, Racer, heading.force));
    f32_add_to(fmul(r->stick_y, TEN_DEGREES), member_addr(racer, Racer, value34c.force));
    if (--r->action_timer < 0) racer_set_action(racer, 1);
    break;
  }
  body1_pull_angle(F32(0.0f), F32_BITS(0x3CCCCCCCu) /* 0.025 */, member_addr(racer, Racer, value38c));
}
HW_EXPORT(8c0343d6, racer_state4_action4(ARG0))

/* was func_8C033E6C: state 4's update: its action (0 starts the state), then the pulls (the value at 0x33C is
 * left alone: rate 0) */
void racer_update_state4(gaddr racer) {
  switch (racer_at(racer)->action) {
  case 0: racer_state4_start(racer); break;
  case 1:
  case 2: racer_state4_run(racer); break;
  case 3: racer_state4_action3(racer); break;
  case 4: racer_state4_action4(racer); break;
  }
  racer_state_pulls(racer, F32(0.0f));
}
HW_EXPORT(8c033e6c, racer_update_state4(ARG0))
