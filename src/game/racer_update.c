/* Pen Pen port, S16 (w4): the racer's update after its controls (func_8C0317D4; the Racer's and the LaneRacer's
 * update both end with it, once the controls are read).
 *
 * In order: the stick length is capped at 1; the follow camera steps; the racer's state may change to the one the
 * contacts asked for (word210); the contacts are sorted; the moving frame, the second body and the four angle
 * springs step; the speed (f434) and the direction of motion (vec428) are noted; the draw matrix is built from the
 * position, the character's height, the ground's up vector and the heading, tilt and roll (and its inverse kept);
 * the state's own update runs; the special ground is checked and the racer is put into its slot of the race
 * scene's cell lists. Then the race progress of the cell it is on is counted (a time is noted when it advances),
 * the marks, the squash, the character's motion, the trail and the effects run, the "going backwards" watch runs
 * and the variant (with the pad's vibration) is picked. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "../core/contact.h"
#include "../core/transform.h"
#include "motion.h"
#include "race_scene.h"
#include "racer.h"

#define STICK_LENGTH_MAX F32(1.0f)
#define BACKWARDS_DOT F32(-0.25f)            /* facing against the cell's direction below this cosine */
#define BACKWARDS_FRAMES_MAX F32(120.0f)     /* updates facing backwards before `backwards` is set */
#define MARK_B_FAST F32_BITS(0x3FB1C71Bu)    /* 1.3888888 (25 / 18) */
#define MARK_A_FAST F32_BITS(0x3F31C71Bu)    /* 0.6944444 (25 / 36) */
#define CELL_KIND_PROGRESS_END 7             /* cell kinds below it count as race progress (word1a8) */
#define CELL_KIND_FINISH 8                   /* sets word1ac */
#define CELL_KIND_RESTART 9                  /* clears progress_prev */
#define VARIANT_FAST 5                       /* racer_set_variant: a mark counted fast enough */
#define VARIANT_DAMPED 2                     /* racer_set_variant: the stick is damped */
#define VIB_PATTERNS 0x8C0806C4u             /* 8-byte vibration parameters per variant (pdVibMxStart) */

/* func_8C032ABC: state 1's update (still translated) */
static void racer_update_state1(gaddr racer) { CALL1(0x8C032ABCu, racer); }
/* func_8C034866: state 5's update (still translated) */
static void racer_update_state5(gaddr racer) { CALL1(0x8C034866u, racer); }
/* SDK pdVibMxStart (0x8C0A115E): start the vibration unit at port with the parameters at param */
static inline void pdVibMxStart(int32_t port, gaddr param) { CALL2(0x8C0A115Eu, port, param); }
/* SDK pdVibMxStop (0x8C0A118C): stop the vibration unit at port */
static inline void pdVibMxStop(int32_t port) { CALL1(0x8C0A118Cu, port); }

/* the racer's draw matrix: at its position, lowered by the character's height (params->f04), turned so that its up
 * is the ground's (body2fc), then by heading + value38c, value33c and value34c; matrix3e8 keeps its inverse */
static void racer_build_matrix(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr m = member_addr(racer, Racer, matrix3a8);
  njUnitMatrix(m);
  njTranslateV(m, member_addr(racer, Racer, body.body.position));
  njTranslate(m, F32(0.0f), fneg(character_params_at(r->params)->f04), F32(0.0f));
  matrix_align_up(m, member_addr(racer, Racer, body2fc.position));
  f32 yaw = fadd(r->heading.value, r->value38c.value);
  njRotateY(m, f32_to_int(yaw));
  njRotateX(m, f32_to_int(r->value33c.value));
  njRotateZ(m, f32_to_int(r->value34c.value));
  r->matrix3e8 = r->matrix3a8;
  njInvertMatrix(member_addr(racer, Racer, matrix3e8));
}

/* the race progress of the cell the racer is on: kinds below 7 raise word1a8, 8 sets word1ac, 9 clears
 * progress_prev; when the progress moved, the scene's time is noted for the step it left */
static void racer_count_progress(gaddr racer) {
  Racer *r = racer_at(racer);
  r->progress_prev = r->word1a8;
  int32_t kind = race_scene_cell_kind(r->scene, r->contacts.cell);
  if (kind < CELL_KIND_PROGRESS_END) {
    if (r->word1a8 < kind) r->word1a8 = kind;
  } else if (kind == CELL_KIND_FINISH) {
    r->word1ac = 1;
  } else if (kind == CELL_KIND_RESTART) {
    r->progress_prev = 0;
  }
  int32_t left = r->progress_prev - 2;
  int32_t now = r->word1a8 - 2;
  if (now < 0) now = 0;
  if (left < 0) left = 0;
  if (left != now) r->times298[left] = race_scene_f74(r->scene); /* left is at most 4 (word1a8 stays below 7) */
}

/* in action 1, count the updates the racer's forward axis points against its cell's direction; past 120 it is
 * `backwards` (newly set: backwards_count counts it) */
static void racer_watch_backwards(gaddr racer) {
  Racer *r = racer_at(racer);
  r->backwards_prev = r->backwards;
  r->backwards = 0;
  if (r->action == 1) {
    gaddr axis = racer_matrix_axis_z(racer);
    gaddr point = race_scene_cell_point(r->scene, (int32_t)racer_cell(racer));
    if (fgt(BACKWARDS_DOT, njInnerProduct(point, axis))) {
      r->backwards_frames++;
      if (fgt(f32_from_int(r->backwards_frames), BACKWARDS_FRAMES_MAX)) r->backwards = 1;
    } else {
      r->backwards_frames = 0;
    }
  } else {
    r->backwards_frames = 0;
  }
  if (r->backwards != 0 && r->backwards_prev == 0) r->backwards_count++;
}

/* the variant shown: 5 when a counted mark is fast enough, 2 while the stick is damped; then the pad vibrates
 * (one pattern per variant) while vib_frames lasts */
static void racer_pick_variant(gaddr racer) {
  Racer *r = racer_at(racer);
  int32_t fast = 0;
  if (r->mark_b_on != 0 && fgt(r->mark_b_value, MARK_B_FAST)) fast = 1;
  if (r->mark_a_on != 0 && fgt(r->mark_a_value, MARK_A_FAST)) fast = 1;
  if (fast != 0) racer_set_variant(racer, VARIANT_FAST);
  if (r->stick_damped != 0) racer_set_variant(racer, VARIANT_DAMPED);
  if (r->vib_ready != 0) {
    if (r->vib_frames != 0) {
      pdVibMxStart(r->vib_port, VIB_PATTERNS + (uint32_t)r->variant * 8);
      r->vib_frames--;
    } else {
      pdVibMxStop(r->vib_port);
    }
  }
}

/* was func_8C0317D4: the racer's update once its controls are read (see the top of the file) */
void racer_base_update(gaddr racer) {
  Racer *r = racer_at(racer);
  if (fgt(r->stick_length, STICK_LENGTH_MAX)) r->stick_length = STICK_LENGTH_MAX;
  racer_camera_update(racer);
  r->state_prev = r->state;
  r->vec2ac = r->body.body.position;
  /* a copy of the second body's position into a local the function never reads (kept: the original's) */
  gaddr unused = hw_stack_alloc(sizeof(Vec3));
  vec3_copy(unused, member_addr(racer, Racer, body2fc.position));
  hw_stack_free(sizeof(Vec3));
  r->on_kind4_prev = r->on_kind4;
  if (r->state != 0 && r->state != 1 && r->state != r->word210) racer_set_state(racer, r->word210);
  racer_sort_contacts(racer);
  frame_body_step(member_addr(racer, Racer, body));
  body_step(member_addr(racer, Racer, body2fc));
  body1_step(member_addr(racer, Racer, heading));
  body1_step(member_addr(racer, Racer, value33c));
  body1_step(member_addr(racer, Racer, value34c));
  body1_step(member_addr(racer, Racer, value38c));

  r->f434 = njScalor(member_addr(racer, Racer, body.body.velocity));
  if (feq(r->f434, F32(0.0f)))
    vec3_set(F32(0.0f), F32(-1.0f), F32(0.0f), member_addr(racer, Racer, vec428));
  else
    vec3_normalized(member_addr(racer, Racer, body.body.velocity), member_addr(racer, Racer, vec428));

  racer_build_matrix(racer);

  switch (r->state) {
  case 0: racer_check_scene_stop(racer); break;
  case 1: racer_update_state1(racer); break;
  case 2: racer_update_state2(racer); break;
  case 3: racer_update_state3(racer); break;
  case 4: racer_update_state4(racer); break;
  case 5: racer_update_state5(racer); break;
  }
  racer_ground(racer);
  race_scene_slot_insert(r->scene, member_addr(racer, Racer, contacts), r->index);

  racer_count_progress(racer);
  racer_check_marks(racer);
  racer_squash_update(racer);
  motion_player_advance(member_addr(racer, Racer, second_base)); /* the character's MotionPlayer at racer + 0x24 */
  vec3_add(member_addr(racer, Racer, vec534), member_addr(racer, Racer, body.body.position));
  racer_emit_effects(racer);
  racer_play_sounds(racer);
  racer_watch_backwards(racer);
  racer_pick_variant(racer);
}
HW_EXPORT(8c0317d4, racer_base_update(ARG0))
