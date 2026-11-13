/* Pen Pen port, S17 b04: more of the racer's actions (game/racer_actions.c has the first ones): state 3's action 4,
 * state 4's action 3 (hit), state 5 (swimming: under the surface of the course's water, func_8C02C9C0) and its
 * actions 0, 3 and 4, the bumps of its contacts, and a lighter reset that puts a racer at a position with its
 * idle motion.
 *
 * As in racer_actions.c an action runs in steps (Racer.action_step) timed by Racer.action_timer, the character's
 * parameters (CharacterParams) give its lengths, pushes, turn rates and dampings, and the motion player at
 * racer + 0x24 plays a motion set per action. Some lengths are scaled by the racer's item (RaceScene.player_item,
 * a float per item in a table of the action). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/contact.h"
#include "racer.h"
#include "race_scene.h"
#include "motion.h"

void frame_body_set_matrices(gaddr obj, gaddr world, gaddr local); /* game/racer.c (not in a header) */

/* the item scales of state 3's action 4 and state 5's action 4 (an f32 per item) */
#define g_action4_item_scale ((const f32 *)hw_ptr(0x8C0855E8u))
#define g_swim4_item_scale ((const f32 *)hw_ptr(0x8C085628u))

#define HALF_TURN F32(32768.0f) /* Ninja angle units */
#define PULL_38C F32_BITS(0x3CCCCCCCu) /* 0.025: the rate value38c is pulled at in every action */

static CharacterParams *params_of(const Racer *r) { return character_params_at(r->params); }
/* the racer's character's motion player (the MotionPlayer at racer + 0x24) */
static gaddr racer_motion(gaddr racer) { return member_addr(racer, Racer, second_base); }
static gaddr racer_damping(gaddr racer) { return member_addr(racer, Racer, body.body.damping); }
/* `frames` scaled by the racer's item in `scales` (truncated) */
static int32_t item_scaled(const Racer *r, int32_t frames, const f32 *scales) {
  uint32_t item = race_scene_at(r->scene)->player_item[r->index];
  return f32_to_int(fmul(f32_from_int(frames), scales[item]));
}
/* the per-contact kind word (Contacts.unknown48) */
static int32_t contact_kind(const Contacts *c, int32_t k) { return ((const int32_t *)hw_ptr(c->unknown48))[k]; }
static int32_t contact_other(const Contacts *c, int32_t k) { return ((const int32_t *)hw_ptr(c->other))[k]; }

/* ---- motion sets ---- */

/* was func_8C0329AC: play motion set 10 (the idle set of a racer holding an item) */
void racer_play_motion10(gaddr racer) { motion_player_play_set(racer_motion(racer), 10, F32(1.0f)); }
HW_EXPORT(8c0329ac, racer_play_motion10(ARG0))

/* was func_8C033EFE: play motion set 20 (state 4) */
void racer_play_motion20(gaddr racer) { motion_player_play_set(racer_motion(racer), 20, F32(1.0f)); }
HW_EXPORT(8c033efe, racer_play_motion20(ARG0))

/* ---- state 3: action 3's weak hit, action 4 ---- */

/* was func_8C03390C: the weak hit's start: its frames and motion set 14 */
void racer_hit_begin_weak(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action_timer = params_of(r)->hit_frames[2];
  motion_player_play_set(racer_motion(racer), 14, F32(1.0f));
}
HW_EXPORT(8c03390c, racer_hit_begin_weak(ARG0))

/* was func_8C03379C: state 3's action 4 starts: the contacts become kind 0 with the character's mass, motion set 13
 * plays for the action's frames scaled by the item */
void racer_action4_start(gaddr racer) {
  Racer *r = racer_at(racer);
  r->contacts.unknown1c = 0;
  r->contacts.mass = params_of(r)->mass;
  motion_player_play_set(racer_motion(racer), 13, F32(1.0f));
  r->action_timer = params_of(r)->action4_frames;
  r->action_timer = item_scaled(r, r->action_timer, g_action4_item_scale);
}
HW_EXPORT(8c03379c, racer_action4_start(ARG0))

/* was func_8C033810: state 3's action 4: started, then the moving body is damped (x and z alike), pushed forward
 * (1.2 times the character's push) and turned by the stick until the frames run out (back to action 1); value38c is
 * pulled to 0 all along */
void racer_action4_run(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action_step) {
  case 0:
    racer_action4_start(racer);
    r->action_step = 1;
    break;
  case 1:
    /* (ground_damping[6]: the last of the seven, 0x80) */
    vec3_set(params_of(r)->ground_damping[6], params_of(r)->damping_y, params_of(r)->ground_damping[6],
             racer_damping(racer));
    f32_add_to(fmul(params_of(r)->action4_push, F32_BITS(0x3F999999u) /* 1.2 */),
               member_addr(racer, Racer, body.push.z));
    f32_add_to(fmul(params_of(r)->action4_turn, r->stick_y), member_addr(racer, Racer, heading.force));
    if (--r->action_timer < 0) racer_set_action(racer, 1);
    break;
  }
  body1_pull_angle(F32(0.0f), PULL_38C, member_addr(racer, Racer, value38c));
}
HW_EXPORT(8c033810, racer_action4_run(ARG0))

/* ---- state 4: action 3 (hit) ---- */

/* was func_8C0344D6 (and its copy func_8C0344F4): state 4's weak / medium hit: its frames and motion set 0x15 */
void racer_state4_hit_start_light(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action_timer = params_of(r)->state4_hit_frames[1];
  motion_player_play_set(racer_motion(racer), 0x15, F32(1.0f));
}
HW_EXPORT(8c0344d6, racer_state4_hit_start_light(ARG0))
HW_EXPORT(8c0344f4, racer_state4_hit_start_light(ARG0))

/* was func_8C034512: state 4's strong hit: its frames and motion set 0x16 */
void racer_state4_hit_start_strong(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action_timer = params_of(r)->state4_hit_frames[0];
  motion_player_play_set(racer_motion(racer), 0x16, F32(1.0f));
}
HW_EXPORT(8c034512, racer_state4_hit_start_strong(ARG0))

/* was func_8C0345A8: state 4's action 3 (hit, hit_strength 0 and 1 light, anything else strong), with vibration
 * pattern 7; then the moving body is damped by the ground it is on until the frames run out. value38c is pulled to
 * half a turn all along. */
void racer_state4_hit(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action_step) {
  case 0:
    r->action_step = 1;
    racer_hit_contacts(racer);
    switch (r->hit_strength) {
    case 0: racer_state4_hit_start_light(racer); break;
    case 1: racer_state4_hit_start_light(racer); break; /* (the copy, func_8C0344F4) */
    default: racer_state4_hit_start_strong(racer); break;
    }
    racer_set_variant(racer, 7);
    break;
  case 1:
    vec3_set(params_of(r)->ground_damping[r->ground], params_of(r)->damping_y, params_of(r)->ground_damping[r->ground],
             racer_damping(racer));
    if (--r->action_timer < 0) racer_set_action(racer, 1);
    break;
  }
  body1_pull_angle(HALF_TURN, PULL_38C, member_addr(racer, Racer, value38c));
}
HW_EXPORT(8c0345a8, racer_state4_hit(ARG0))

/* ---- state 5: swimming ---- */

/* was func_8C03481A: state 5's action 0: on to action 1 (set directly), the heading's and the values at 0x33C and
 * 0x34C damped by half, motion set 0x19 */
void racer_swim_start(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action = 1;
  r->heading.damping = F32(0.5f);
  r->value33c.damping = F32(0.5f);
  r->value34c.damping = F32(0.5f);
  motion_player_play_set(racer_motion(racer), 0x19, F32(1.0f));
}
HW_EXPORT(8c03481a, racer_swim_start(ARG0))

/* was func_8C0349E8: the strokes start over (no hold, no release counted), motion set 0x19 */
void racer_swim_strokes_start(gaddr racer) {
  Racer *r = racer_at(racer);
  r->hold_frames = 0;
  r->last_hold = 0;
  r->release_frames = 0;
  motion_player_play_set(racer_motion(racer), 0x19, F32(1.0f));
}
HW_EXPORT(8c0349e8, racer_swim_strokes_start(ARG0))

/* was func_8C034D96: state 5's action 4 starts: motion set 0x18 for its frames scaled by the item */
void racer_swim4_start(gaddr racer) {
  Racer *r = racer_at(racer);
  r->action_timer = params_of(r)->swim_frames[0];
  motion_player_play_set(racer_motion(racer), 0x18, F32(1.0f));
  r->action_timer = item_scaled(r, r->action_timer, g_swim4_item_scale);
}
HW_EXPORT(8c034d96, racer_swim4_start(ARG0))

/* was func_8C034DE2: state 5's action 4: started, then pushed forward, turned by stick_y, value33c pulled toward
 * stick_x (tilted), and value34c turned by -20 degrees every update until the frames run out (back to action 1);
 * value38c is pulled to 0 all along */
void racer_swim_action4(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action_step) {
  case 0:
    racer_swim4_start(racer);
    r->action_step++;
    break;
  case 1:
    f32_add_to(params_of(r)->swim4_push, member_addr(racer, Racer, body.push.z));
    f32_add_to(fmul(params_of(r)->swim4_turn, r->stick_y), member_addr(racer, Racer, heading.force));
    body1_pull_angle(fmul(r->stick_x, params_of(r)->swim4_tilt), params_of(r)->swim4_tilt_rate,
                     member_addr(racer, Racer, value33c));
    f32_add_to(F32_BITS(0xC5638E38u) /* -3640.889: -20 degrees */, member_addr(racer, Racer, value34c.force));
    if (--r->action_timer < 0) racer_set_action(racer, 1);
    break;
  }
  body1_pull_angle(F32(0.0f), PULL_38C, member_addr(racer, Racer, value38c));
}
HW_EXPORT(8c034de2, racer_swim_action4(ARG0))

/* was func_8C034EB4 / func_8C034ED6 / func_8C034EF8: state 5's hit: motion set 0x1A / 0x1C / 0x1B for its frames */
void racer_swim_hit_start_weak(gaddr racer) {
  motion_player_play_set(racer_motion(racer), 0x1A, F32(1.0f));
  racer_at(racer)->action_timer = params_of(racer_at(racer))->swim_frames[3];
}
void racer_swim_hit_start_medium(gaddr racer) {
  motion_player_play_set(racer_motion(racer), 0x1C, F32(1.0f));
  racer_at(racer)->action_timer = params_of(racer_at(racer))->swim_frames[2];
}
void racer_swim_hit_start_strong(gaddr racer) {
  motion_player_play_set(racer_motion(racer), 0x1B, F32(1.0f));
  racer_at(racer)->action_timer = params_of(racer_at(racer))->swim_frames[1];
}
HW_EXPORT(8c034eb4, racer_swim_hit_start_weak(ARG0))
HW_EXPORT(8c034ed6, racer_swim_hit_start_medium(ARG0))
HW_EXPORT(8c034ef8, racer_swim_hit_start_strong(ARG0))

/* was func_8C034F1E: state 5's action 3 (hit by hit_strength 0, 1, 2; anything else goes back to swimming), with
 * vibration pattern 7; when its frames run out, the strong hit's set 0x1B goes on with set 0x1C (the medium hit's
 * frames), any other back to action 1. value38c is pulled to half a turn all along. */
void racer_swim_hit(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action_step) {
  case 0:
    method_empty(); /* (func_8C034F1A on the racer) */
    switch (r->hit_strength) {
    case 0: racer_swim_hit_start_weak(racer); break;
    case 1: racer_swim_hit_start_medium(racer); break;
    case 2: racer_swim_hit_start_strong(racer); break;
    default: racer_set_action(racer, 1); break;
    }
    racer_set_variant(racer, 7);
    r->action_step++;
    break;
  case 1:
    if (--r->action_timer < 0) {
      if (motion_player_at(racer_motion(racer))->motion_count == 0x1B) {
        motion_player_play_set(racer_motion(racer), 0x1C, F32(1.0f));
        r->action_timer = params_of(r)->swim_frames[2];
      } else {
        racer_set_action(racer, 1);
      }
    }
    break;
  }
  body1_pull_angle(HALF_TURN, PULL_38C, member_addr(racer, Racer, value38c));
}
HW_EXPORT(8c034f1e, racer_swim_hit(ARG0))

/* ---- contacts ---- */

/* was func_8C035014: go through the racer's contacts (had_contacts set when there is one): another racer (other <= 7)
 * that moves toward it and that it moves into turns it away (its heading's force +-30 degrees by the side of the
 * contact's normal); an object of kind 1 to 3 hits it when it is fast (speed > 1.85: hit_strength 2, > 0.926: 0) */
void racer_bump_contacts(gaddr racer) {
  Racer *r = racer_at(racer);
  Contacts *c = &r->contacts;
  gaddr velocity = member_addr(racer, Racer, body.body.velocity);
  r->had_contacts = 0;
  for (int32_t k = 0; k < c->count; k++) {
    r->had_contacts = 1;
    if (contact_other(c, k) <= 7) {
      if (!fgt(njInnerProduct(c->velocity + (uint32_t)k * 12, velocity), F32(0.0f))) continue;
      if (!fgt(njInnerProduct(c->normal + (uint32_t)k * 12, velocity), F32(0.0f))) continue;
      gaddr normal = c->normal + (uint32_t)k * 12;
      if (fgt(vec3_cross_y(velocity, normal), F32(0.0f)))
        f32_add_to(F32_BITS(0x45AAAAAAu) /* 5461.333: 30 degrees */, member_addr(racer, Racer, heading.force));
      else
        f32_add_to(F32_BITS(0xC5AAAAAAu) /* -5461.333 */, member_addr(racer, Racer, heading.force));
    } else if (contact_kind(c, k) >= 1 && contact_kind(c, k) <= 3) {
      if (fgt(r->speed, F32_BITS(0x3FED097Au) /* 1.8518518 */)) {
        r->hit_strength = 2;
        racer_set_action(racer, 3);
      } else if (fgt(r->speed, F32_BITS(0x3F6D097Au) /* 0.9259259 */)) {
        r->hit_strength = 0;
        racer_set_action(racer, 3);
      }
    }
  }
}
HW_EXPORT(8c035014, racer_bump_contacts(ARG0))

/* ---- set-up ---- */

/* the racer's local Vec3 frame (vec3_scaled writes a guest Vec3; the original's slot under its saved PR) */
typedef struct CellSpeedFrame {
  Vec3 v;              /* 0x00 */
  uint8_t rest[0x14 - 0x0C]; /* 0x0C: the racer (kept), PR */
} CellSpeedFrame;
HW_SIZE(CellSpeedFrame, 0x14);

/* was func_8C032A3A: the moving body's horizontal velocity from its cell's point, scaled by 0.926 (x and z only) */
void racer_velocity_from_cell(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr frame = hw_stack_alloc(sizeof(CellSpeedFrame));
  CellSpeedFrame *f = (CellSpeedFrame *)hw_ptr(frame);
  gaddr point = race_scene_cell_point(r->scene, (int32_t)racer_cell(racer));
  vec3_scaled(F32_BITS(0x3F6D097Au) /* 0.9259259 */, frame, point);
  r->body.body.velocity.x = f->v.x;
  r->body.body.velocity.z = f->v.z;
  hw_stack_free(sizeof(CellSpeedFrame));
}
HW_EXPORT(8c032a3a, racer_velocity_from_cell(ARG0))

/* was func_8C03138E: put the racer at `position` with `heading` and its idle motion (set 10 with an item, else by
 * the character: 0x1D for 0, 0x1E for 2, 0x20 for 5, 0x21 for 7, 0x1F for the others); the vector at 0x428 points
 * down, the collider takes the moving body's velocity and the character's radius and mass, and the frame's matrices
 * are set */
void racer_place(gaddr racer, gaddr position, f32 heading) {
  Racer *r = racer_at(racer);
  racer_reset_at(racer, position, F32(0.0f), heading, F32(0.0f));
  if (race_scene_at(r->scene)->player_item[r->index] != 0) {
    motion_player_play_set(racer_motion(racer), 10, F32(1.0f));
  } else if (r->character == 0) {
    motion_player_play_set(racer_motion(racer), 0x1D, F32(1.0f));
  } else if (r->character == 2) {
    motion_player_play_set(racer_motion(racer), 0x1E, F32(1.0f));
  } else if (r->character == 5) {
    motion_player_play_set(racer_motion(racer), 0x20, F32(1.0f));
  } else if (r->character == 7) {
    motion_player_play_set(racer_motion(racer), 0x21, F32(1.0f));
  } else {
    motion_player_play_set(racer_motion(racer), 0x1F, F32(1.0f));
  }
  r->f434 = F32(0.0f);
  r->vec428.z = F32(0.0f);
  r->vec428.y = F32(-1.0f);
  r->vec428.x = F32(0.0f);
  const CharacterParams *params = (const CharacterParams *)hw_ptr(CHARACTER_PARAMS);
  r->contacts.owner_velocity = member_addr(racer, Racer, body.body.velocity);
  r->contacts.radius = params[r->character].radius;
  r->contacts.mass = params[r->character].mass;
  frame_body_set_matrices(member_addr(racer, Racer, body), member_addr(racer, Racer, matrix3a8),
                          member_addr(racer, Racer, matrix3e8));
}
HW_EXPORT(8c03138e, racer_place(ARG0, ARG1, FARG0))

/* ---- state 5's update ---- */

/* func_8C034A44: state 5's actions 1 and 2, the strokes (still translated) */
static void racer_swim_strokes(gaddr racer) { CALL1(0x8C034A44u, racer); }
/* game/race_scene_players.c (S17 b04): the height of the water's surface under a course cell (`unknown` for a
 * course other than 0-3: the original's FR0 at the call) */
f32 race_scene_floor_height(gaddr scene, int32_t cell, f32 unknown);

/* was func_8C034866: state 5's update: its action (0 start, 1 and 2 the strokes, 3 hit, 4 the action 4); then, off
 * special ground: above the water's surface the moving body sinks (a downward force growing with the height, up to
 * one radius) and is damped by damping_y; under it, it is damped by the stroke level's swimming dampings (level 0's
 * in action 4) */
void racer_update_state5(gaddr racer) {
  Racer *r = racer_at(racer);
  switch (r->action) {
  case 0: racer_swim_start(racer); break;
  case 1:
  case 2: racer_swim_strokes(racer); break;
  case 3: racer_swim_hit(racer); break;
  case 4: racer_swim_action4(racer); break;
  }
  if (r->ground_touch != 0) return;
  f32 y = r->body.body.position.y;
  /* (for an unknown course the original's FR0 still holds y: the height is 0) */
  f32 height = fsub(y, race_scene_floor_height(r->scene, r->contacts.cell, y));
  if (fgt(height, F32(0.0f))) {
    f32 depth = fdiv(height, fmul(params_of(r)->radius, F32(1.0f)));
    if (fgt(depth, F32(1.0f))) depth = F32(1.0f);
    depth = fmul(depth, F32_BITS(0xBDFAE145u) /* -0.1225 */);
    r->body.body.force.x = F32(0.0f);
    f32_add_to(depth, member_addr(racer, Racer, body.body.force.y));
    r->body.body.force.z = F32(0.0f);
    vec3_fill(racer_damping(racer), params_of(r)->damping_y);
  } else if (r->action == 4) {
    vec3_set(params_of(r)->swim_damping_xy[0], params_of(r)->swim_damping_xy[0], params_of(r)->swim_damping_z[0],
             racer_damping(racer));
  } else {
    int32_t level = r->stroke_level;
    vec3_set(params_of(r)->swim_damping_xy[level], params_of(r)->swim_damping_xy[level],
             params_of(r)->swim_damping_z[level], racer_damping(racer));
  }
}
HW_EXPORT(8c034866, racer_update_state5(ARG0))
