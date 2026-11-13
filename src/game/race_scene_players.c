/* Pen Pen port, S17 b04: more of the race scene's per-player accessors (0x8C02C7B0-0x8C02C90A: the scene forwards
 * a question about player i to that player's Racer, game/racer_fields.c), the nearest player to a point, and three
 * per-course answers (the floor's height under a course cell, the course's reference point and direction, the
 * results' score). The course is the RaceSetup's course (0 to 3); an unknown course answers nothing. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "race_scene.h"
#include "racer.h"

/* game/race_scene.c (declared in game/scenery.h) */
gaddr race_player_body(gaddr scene, int32_t i);

/* player i of the scene (no bound check: the original reads any slot) */
static gaddr player(gaddr scene, int32_t i) { return race_scene_at(scene)->players[i]; }

/* ---- player i's fields ---- */

/* was func_8C02C7B0: the address of player i's second Body */
gaddr race_player_body2(gaddr scene, int32_t i) { return racer_body2fc(player(scene, i)); }
HW_EXPORT(8c02c7b0, RET(race_player_body2(ARG0, (int32_t)ARG1)))

/* was func_8C02C7E0: the value of player i's spring at 0x33C */
f32 race_player_value33c(gaddr scene, int32_t i) { return racer_value33c(player(scene, i)); }
HW_EXPORT(8c02c7e0, RETF(race_player_value33c(ARG0, (int32_t)ARG1)))

/* was func_8C02C7F8: player i's float at 0x434 */
f32 race_player_f434(gaddr scene, int32_t i) { return racer_f434(player(scene, i)); }
HW_EXPORT(8c02c7f8, RETF(race_player_f434(ARG0, (int32_t)ARG1)))

/* was func_8C02C828: player i's progress word (Racer.word1a8) */
int32_t race_player_progress(gaddr scene, int32_t i) { return racer_word1a8(player(scene, i)); }
HW_EXPORT(8c02c828, RET(race_player_progress(ARG0, (int32_t)ARG1)))

/* was func_8C02C840: the kind of special ground player i touches */
int32_t race_player_ground_kind(gaddr scene, int32_t i) { return racer_ground_kind(player(scene, i)); }
HW_EXPORT(8c02c840, RET(race_player_ground_kind(ARG0, (int32_t)ARG1)))

/* was func_8C02C870: player i's action */
int32_t race_player_action(gaddr scene, int32_t i) { return racer_action(player(scene, i)); }
HW_EXPORT(8c02c870, RET(race_player_action(ARG0, (int32_t)ARG1)))

/* was func_8C02C888: whether player i's stick is damped */
int32_t race_player_stick_damped(gaddr scene, int32_t i) { return racer_stick_damped(player(scene, i)); }
HW_EXPORT(8c02c888, RET(race_player_stick_damped(ARG0, (int32_t)ARG1)))

/* was func_8C02C8A0: player i's float at 0x188 */
f32 race_player_f188(gaddr scene, int32_t i) { return racer_f188(player(scene, i)); }
HW_EXPORT(8c02c8a0, RETF(race_player_f188(ARG0, (int32_t)ARG1)))

/* was func_8C02C8B8: set player i's stick_damped */
void race_player_set_stick_damped(gaddr scene, int32_t i, int32_t value) {
  racer_set_stick_damped(player(scene, i), value);
}
HW_EXPORT(8c02c8b8, race_player_set_stick_damped(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C02C8D4: damp player i's stick (stick_damped = 4) and set its float at 0x188 */
void race_player_damp_stick(gaddr scene, int32_t i, f32 value) {
  racer_set_stick_damped(player(scene, i), 4);
  racer_set_f188(player(scene, i), value);
}
HW_EXPORT(8c02c8d4, race_player_damp_stick(ARG0, (int32_t)ARG1, FARG0))

/* was func_8C02C90A: whether player i had contacts this frame */
int32_t race_player_had_contacts(gaddr scene, int32_t i) { return racer_had_contacts(player(scene, i)); }
HW_EXPORT(8c02c90a, RET(race_player_had_contacts(ARG0, (int32_t)ARG1)))

/* ---- the nearest player ---- */

/* the original's frame below its saved registers: the out-parameter slots, the direction to the player being
 * looked at, and the best one so far (njUnitVector works on guest memory; the best direction is copied out
 * uninitialised when no player is nearer than 10000, so it keeps the original's place on the stack) */
typedef struct NearestFrame {
  uint8_t args[0x0C];  /* 0x00: the point, the outputs (kept by the original) */
  Vec3 dir;            /* 0x0C */
  Vec3 best;           /* 0x18 */
  uint8_t saved[0x14]; /* 0x24: r14, r13, r12, fr15, PR */
} NearestFrame;
HW_OFFSET(NearestFrame, dir, 0x0C);
HW_OFFSET(NearestFrame, best, 0x18);
HW_SIZE(NearestFrame, 0x38);

/* was func_8C02C922: the player nearest to `point` closer than 10000 (from the last player down, the first of
 * equal distances kept), or -1; its unit direction from the point goes to out_dir and its distance to out_distance
 * when they are given (out_distance gets 10000 when none is nearer) */
int32_t race_scene_nearest_player(gaddr scene, gaddr point, gaddr out_dir, gaddr out_distance) {
  gaddr frame = hw_stack_alloc(sizeof(NearestFrame));
  gaddr dir = member_addr(frame, NearestFrame, dir);
  NearestFrame *f = (NearestFrame *)hw_ptr(frame);
  f32 nearest = F32(10000.0f);
  int32_t found = -1;
  for (int32_t i = race_setup_at(race_scene_at(scene)->setup)->player_count - 1; i >= 0; i--) {
    vec3_diff(dir, point, race_player_body(scene, i));
    f32 distance = njUnitVector(dir);
    if (fgt(nearest, distance)) {
      nearest = distance;
      memmove(&f->best, &f->dir, sizeof(Vec3));
      found = i;
    }
  }
  if (out_dir != 0) memmove(hw_ptr(out_dir), &f->best, sizeof(Vec3));
  if (out_distance != 0) *(f32 *)hw_ptr(out_distance) = nearest;
  hw_stack_free(sizeof(NearestFrame));
  return found;
}
HW_EXPORT(8c02c922, RET(race_scene_nearest_player(ARG0, ARG1, ARG2, ARG3)))

/* ---- per-course answers ---- */

/* the course of the race (RaceSetup.course) */
static int32_t scene_course(gaddr scene) { return race_setup_at(race_scene_at(scene)->setup)->course; }

/* was func_8C02C9C0: the height of the floor under course cell `cell` (by the course and the cell's section).
 * For a course other than 0-3 the original returns without setting its result: `unknown` (the caller's FR0 at
 * the call) stands for it. */
f32 race_scene_floor_height(gaddr scene, int32_t cell, f32 unknown) {
  switch (scene_course(scene)) {
  case 0:
    return F32(-405.0f);
  case 1:
    if (cell < 0x4F) return F32_BITS(0xC40CFFFFu); /* -563.99994 */
    if (cell < 0x52) return F32_BITS(0xC3F8FFFFu); /* -497.99997 */
    if (cell < 0x67) return F32_BITS(0xC3FF7FFFu); /* -510.99997 */
    return F32_BITS(0xC3FAFFFFu);                  /* -501.99997 */
  case 2:
    return F32_BITS(0x428D3333u); /* 70.6 */
  case 3:
    if (cell < 0x57) return F32_BITS(0xC46ED998u); /* -955.3999 */
    if (cell < 0x5B) return F32(-1140.0f);
    if (cell < 0x65) return F32(1000.0f);
    if (cell < 0x81) return F32(-1120.0f);
    if (cell < 0x8C) return F32_BITS(0xC4875FFFu); /* -1082.9999 */
    if (cell < 0xA0) return F32_BITS(0xC49ABFFFu); /* -1237.9999 */
    return F32_BITS(0xC39E3FFFu);                  /* -316.49997 */
  default:
    return unknown;
  }
}
HW_EXPORT(8c02c9c0, RETF(race_scene_floor_height(ARG0, (int32_t)ARG1, F32_BITS(hw_e_.c->fr.u[0]))))

/* was func_8C02CB28: the course's reference point (3955, 0, -6935) and direction (0, 0, -1) into out[0] / out[1]
 * (the same for the four courses; nothing for another course or a null out) */
void race_scene_course_origin(gaddr scene, gaddr out) {
  int32_t course = scene_course(scene);
  if (course != 0 && course != 1 && course != 2 && course != 3) return;
  if (out == 0) return;
  vec3_set(F32(3955.0f), F32(0.0f), F32(-6935.0f), out);
  vec3_set(F32(0.0f), F32(0.0f), F32(-1.0f), out + sizeof(Vec3));
}
HW_EXPORT(8c02cb28, race_scene_course_origin(ARG0, ARG1))

/* was func_8C02E7C6: the results' score (only in RACE_STATE_RESULTS; 0 otherwise) */
int32_t race_scene_result_score(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (s->state == RACE_STATE_RESULTS) return s->result_score;
  return 0;
}
HW_EXPORT(8c02e7c6, RET(race_scene_result_score(ARG0)))
