/* Pen Pen port, S14 (batch b05): the racer's body pieces (0x8C0304F2-0x8C0329F4): its collider, its reset to a
 * position, its state changes, the destructor body, and the thunks that hand the racer's frame to each of its
 * effects (RacerEffects, game/effects.h). The racer is game/racer.h's Racer. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/transform.h"
#include "racer.h"
#include "motion.h"
#include "effects.h"
#include "race_scene.h"
#include "sound.h"

/* ---- calls ---- */

/* func_8C0385AC: a motion player starts motion n at a speed (still translated) */
static void motion_player_play(gaddr player, int32_t n, f32 speed) { FARGS1(speed); CALL2(0x8C0385ACu, player, n); }
/* the effects' methods that take the racer's frame (still translated) */
static void ribbon_set_push(gaddr set, gaddr frame) { CALL2(0x8C0397A8u, set, frame); }    /* func_8C0397A8 */
static void effect_set_c_push(gaddr set, gaddr frame) { CALL2(0x8C03C198u, set, frame); }  /* func_8C03C198 */

/* ---- the current matrix ---- */

/* the frame of matrix_scale_current_along: the matrix at 0x10 (its address goes to Ninja) */
typedef struct ScaleCurrentFrame {
  uint32_t spill[4];   /* 0x00: the arguments' slots */
  Matrix m;            /* 0x10 */
  uint32_t saved_pr;   /* 0x50 */
} ScaleCurrentFrame;
HW_OFFSET(ScaleCurrentFrame, m, 0x10);
HW_SIZE(ScaleCurrentFrame, 0x54);

/* was func_8C0304F2: scale Ninja's current matrix by (sx, sy, sz) in the frame whose up axis is dir */
void matrix_scale_current_along(gaddr dir, f32 sx, f32 sy, f32 sz) {
  gaddr frame = hw_stack_alloc(sizeof(ScaleCurrentFrame));
  gaddr m = member_addr(frame, ScaleCurrentFrame, m);
  matrix_identity(m);
  njGetMatrix(m);
  matrix_scale_along(m, dir, sx, sy, sz);
  njSetMatrix(0, m);
  hw_stack_free(sizeof(ScaleCurrentFrame));
}
HW_EXPORT(8c0304f2, matrix_scale_current_along(ARG0, FARG0, FARG1, FARG2))

/* ---- the collider ---- */

/* was func_8C0308BC: a collider at a position: its Contacts (room for 8) owned by the position, of the given
 * radius; the anchor kept beside it (the third argument is not used) */
gaddr collider_construct(gaddr self, gaddr position, f32 radius, gaddr unused, gaddr anchor) {
  Collider *c = (Collider *)hw_ptr(self);
  c->data = member_addr(self, Collider, store);
  c->word4 = -1;
  contacts_construct(member_addr(self, Collider, contacts), position, radius, 8);
  c->position = position;
  c->anchor = anchor;
  return self;
}
HW_EXPORT(8c0308bc, RET(collider_construct(ARG0, ARG1, FARG0, ARG2, ARG3)))

/* was func_8C0309A4: the racer's destructor body: its second Contacts' arrays, its collider, its character's
 * model file */
void racer_body_destroy(gaddr racer) {
  contacts_free(member_addr(racer, Racer, contacts2));
  contacts_holder_destroy(member_addr(racer, Racer, collider_data));
  motion_player_free(member_addr(racer, Racer, second_base));
}
HW_EXPORT(8c0309a4, racer_body_destroy(ARG0))

/* ---- the effects (the racer's child task) ---- */

/* was func_8C030AF8 / func_8C030B0C / func_8C030B20 / func_8C030B2A: hand the racer's frame to the ribbon, the
 * ribbon set, set B and set C */
void racer_effects_push_ribbon(gaddr fx, gaddr frame) { ribbon_push(racer_effects_at(fx)->ribbon, frame); }
void racer_effects_push_ribbons(gaddr fx, gaddr frame) { ribbon_set_push(racer_effects_at(fx)->ribbons, frame); }
void racer_effects_push_set_b(gaddr fx, gaddr frame) { effect_set_b_push(racer_effects_at(fx)->set_b, frame); }
void racer_effects_push_set_c(gaddr fx, gaddr frame) { effect_set_c_push(racer_effects_at(fx)->set_c, frame); }
HW_EXPORT(8c030af8, racer_effects_push_ribbon(ARG0, ARG1))
HW_EXPORT(8c030b0c, racer_effects_push_ribbons(ARG0, ARG1))
HW_EXPORT(8c030b20, racer_effects_push_set_b(ARG0, ARG1))
HW_EXPORT(8c030b2a, racer_effects_push_set_c(ARG0, ARG1))

/* ---- states ---- */

/* was func_8C030B80: a new state, with no action; the racer's port-1 one-shot stopped and both of its ports reset
 * to full volume, no pan, no pitch */
void racer_set_state(gaddr racer, int32_t state) {
  Racer *r = racer_at(racer);
  r->state = state;
  r->action = 0;
  r->camera_vec51c.y = F32(0.0f);
  r->camera_vec51c.z = F32(0.0f);
  race_sound_stop(r->scene, r->index, 1);
  race_sound_adjust_by_mode(r->scene, r->index, 0, 0xF, SOUND_VOLUME_MAX, 0, 0);
  race_sound_adjust_by_mode(r->scene, r->index, 1, 0xF, SOUND_VOLUME_MAX, 0, 0);
}
HW_EXPORT(8c030b80, racer_set_state(ARG0, (int32_t)ARG1))

/* was func_8C0329F4: when the race scene's word at 0x6C is set: state 3, the ribbon dropped from the racer's
 * contact radius along its matrix at 0x3A8, and the word at 0x528 cleared */
void racer_check_scene_stop(gaddr racer) {
  Racer *r = racer_at(racer);
  if (race_scene_word6c(r->scene) == 0) return;
  racer_set_state(racer, 3);
  racer_effects_reset_ribbon(r->child, r->contacts.radius, member_addr(racer, Racer, matrix3a8));
  r->word528 = 0;
}
HW_EXPORT(8c0329f4, racer_check_scene_stop(ARG0))

/* ---- reset ---- */

static void body1_set(Body1 *b, f32 value, f32 damping) {
  b->value = value;
  b->velocity = F32(0.0f);
  b->damping = damping;
}

/* was func_8C0314B4: put the racer at a position at rest: its anchor and moving body there (no velocity, full
 * damping), the heading and the two values beside it (damping 1/2), the second body at (0, 1, 0), the three scale
 * values at 1, the up vectors (0, 0, 1), and motion 10 at normal speed. The floats come as (b, heading, c). */
void racer_reset_at(gaddr racer, gaddr position, f32 b, f32 heading, f32 c) {
  Racer *r = racer_at(racer);
  memmove(&r->vec2ac, hw_ptr(position), sizeof(Vec3));
  gaddr body = member_addr(racer, Racer, body);
  vec3_copy_bytes(member_addr(racer, Racer, body.body.position), position);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_fill(member_addr(body, Body, damping), F32(0.0f));
  body1_set(&r->heading, heading, F32(0.5f));
  body1_set(&r->value33c, b, F32(0.5f));
  body1_set(&r->value34c, c, F32(0.5f));
  gaddr body2 = member_addr(racer, Racer, body2fc);
  vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), body2);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body2, Body, velocity));
  vec3_fill(member_addr(body2, Body, damping), F32(0.75f));
  for (int k = 0; k < 3; k++) {
    r->value35c[k].value = F32(1.0f);
    r->value35c[k].velocity = F32(0.0f);
    r->value35c[k].damping = F32(1.0f);
  }
  vec3_set(F32(0.0f), F32(0.0f), F32(1.0f), member_addr(racer, Racer, vec39c));
  r->value38c.value = F32(0.0f);
  r->value38c.velocity = F32(0.0f);
  r->value38c.force = F32(0.0f);
  r->value38c.damping = F32(0.75f);
  vec3_set(F32(0.0f), F32(0.0f), F32(1.0f), member_addr(racer, Racer, vec428));
  r->f434 = F32(0.0f);
  motion_player_play(member_addr(racer, Racer, second_base), 10, F32(1.0f));
}
HW_EXPORT(8c0314b4, racer_reset_at(ARG0, ARG1, FARG0, FARG1, FARG2))
