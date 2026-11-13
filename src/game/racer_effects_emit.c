/* Pen Pen port, S17 (r1): what the racer hands its effects in an update (func_8C0351C8, run by racer_base_update).
 *
 * A snapshot of the racer (RibbonFrame: its draw matrix, moving body, vectors, sizes, speed, heading and words) is
 * built on the stack, effect_clock grows by the speed (f434), and by the racer's state:
 *  - state 3 (walking): during the dash (action 2) on special ground, set B gets a burst at a fixed speed; else, on
 *    special ground, each foot step of the walk motions (a window of the motion's frames, by motion and character,
 *    where word144 tells which foot) and mark A push the ribbon set (and flip word144); off the floor with word174,
 *    set B gets a splash by how fast it moves (vertically or on the ground) once the clock is high enough; with
 *    word17c, two of them;
 *  - state 4 (swimming / sliding): the ribbon (trail) and set B by the clock, the ribbon restarted where course 3's
 *    cell 57 (or mark A) says;
 *  - state 5: set A (puffs) every update, set B when word178 says so (a hard landing) or by the clock.
 * The snapshot's word94 tells the effect which variant to emit (+10 inside cells 200-203 of course 3). */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"   /* racer_heading */
#include "race_scene.h"       /* race_setup_get */
#include "scenery.h"          /* race_player_cell */
#include "effects.h"
#include "racer.h"

#define CLOCK_SHORT      F32(5.0f)
#define CLOCK_LONG       F32(10.0f)
#define CLOCK_SLOW_STEP  F32_BITS(0x3E4CCCCCu) /* 0.19999999: the clock's extra step while nearly at rest */
#define NEARLY_AT_REST   F32_BITS(0x3DBDA12Du) /* 0.09259259 (5 / 54): f434 below it */
#define MARK_A_MIN       F32_BITS(0x3EED097Au) /* 0.46296296 (25 / 54) */
#define DASH_SPEED       F32_BITS(0x409425EBu) /* 4.6296296 (125 / 27): the speed set B is told in the dash */
#define FALL_SPEED       F32_BITS(0x3F4CCCCCu) /* 0.79999995: |vertical velocity| below it is no fall */
#define ABOVE_FLOOR      F32(5.0f)             /* the feet this far above vec534.y are off the floor */

#define ZONE_COURSE      3                     /* the course with the zone */
#define ZONE_FIRST_CELL  200                   /* its cells 200-203 */
#define ZONE_LAST_CELL   203
#define ZONE_KIND        10                    /* the variants inside the zone: + 10 */
#define RIBBON_RESTART_CELL 57                 /* course 3: the ribbon restarts here in state 4 */

/* RibbonFrame.word94: the variant the effect emits */
enum { EMIT_REST = 0, EMIT_MOVE = 1, EMIT_STEP = 2, EMIT_FALL = 3 };

/* the original's frame (0xAC bytes under PR): the snapshot sits at 0x10 */
typedef struct EffectsFrame {
  uint8_t locals[0x10]; /* 0x00: a height, the step flag, the motion / cell, the zone flag */
  RibbonFrame frame;    /* 0x10 */
  uint8_t rest[0xB0 - 0xA8]; /* 0xA8: `this`, PR */
} EffectsFrame;
HW_OFFSET(EffectsFrame, frame, 0x10);
HW_SIZE(EffectsFrame, 0xB0);

/* func_8C030B16: hand the racer's matrix, cell, radius, speed and heading to its effects' set A (func_8C03A252) */
static void racer_effects_push_set_a(gaddr fx, gaddr m, uint32_t cell, f32 radius, f32 speed, f32 heading) {
  FARGS3(radius, speed, heading);
  CALL3(0x8C030B16u, fx, m, cell);
}

/* func_8C02C9C0: the floor's height under a course cell */
static f32 floor_height(gaddr scene, uint32_t cell) {
  CALL2(0x8C02C9C0u, scene, cell);
  return hw_fret();
}

static const CharacterParams *params_of(const Racer *r) { return character_params_at(r->params); }

/* the motion's frame is strictly inside (lo, hi) */
static int frame_within(f32 t, f32 lo, f32 hi) { return fgt(t, lo) && fgt(hi, t); }

/* state 3 on special ground: whether a foot touches down in this update. The walk motions put a step in a window
 * of their frames (by motion and character); word144 tells which foot is due, so the step counts either inside the
 * window (one foot) or outside it (the other). */
static int racer_foot_down(const Racer *r) {
  int32_t motion = r->motion;
  f32 t = r->motion_frame;
  if (motion == 0x15 || motion == 0x16) {
    if (r->character == 1 || r->character == 5 || r->character == 3 || r->character == 7 || r->character == 2) {
      if (frame_within(t, F32(21.0f), F32(51.0f))) return r->word144 == 0;
      return r->word144 != 0;
    }
    if (frame_within(t, F32(21.0f), F32(51.0f))) return r->word144 != 0;
    return r->word144 == 0;
  }
  if (motion == 4 || motion == 0x17) {
    if (frame_within(t, F32(2.0f), F32(64.0f))) return r->word144 != 0;
    return r->word144 == 0;
  }
  if (motion != 8) return 0;
  if (r->character == 0) {
    if (fgt(F32(15.0f), t)) return r->word144 != 0;
    return r->word144 == 0;
  }
  if (r->character == 1 || r->character == 2) {
    if (fgt(t, F32(15.0f))) return r->word144 != 0;
    return r->word144 == 0;
  }
  if (r->character == 3) {
    if (fgt(t, F32(20.0f))) return r->word144 != 0;
    return r->word144 == 0;
  }
  if (r->character == 4 || r->character == 6) {
    if (frame_within(t, F32(10.0f), F32(20.0f))) return r->word144 != 0;
    if (frame_within(t, F32(30.0f), F32(40.0f))) return r->word144 != 0;
    if (fgt(t, F32(50.0f))) return r->word144 != 0;
    return r->word144 == 0;
  }
  if (r->character == 5 || r->character == 7) {
    if (frame_within(t, F32(7.0f), F32(15.0f))) return r->word144 != 0;
    if (frame_within(t, F32(22.0f), F32(30.0f))) return r->word144 != 0;
    if (frame_within(t, F32(37.0f), F32(45.0f))) return r->word144 != 0;
    if (fgt(t, F32(52.0f))) return r->word144 != 0;
    return r->word144 == 0;
  }
  return 0;
}

/* set B's variant by the clock: nearly at rest the clock also creeps up (REST at 5), else MOVE at 5 */
static void racer_emit_by_clock(Racer *r, gaddr frame, int32_t zone) {
  RibbonFrame *f = (RibbonFrame *)hw_ptr(frame);
  if (fgt(NEARLY_AT_REST, f->f78)) {
    r->effect_clock = fadd(r->effect_clock, CLOCK_SLOW_STEP);
    if (fgt(r->effect_clock, CLOCK_SHORT)) {
      f->word94 = zone + EMIT_REST;
      racer_effects_push_set_b(r->child, frame);
      r->effect_clock = F32(0.0f);
    }
  } else if (fgt(r->effect_clock, CLOCK_SHORT)) {
    f->word94 = zone + EMIT_MOVE;
    racer_effects_push_set_b(r->child, frame);
    r->effect_clock = F32(0.0f);
  }
}

/* state 3 (see the top of the file) */
static void racer_emit_walking(gaddr self, gaddr frame) {
  Racer *r = racer_at(self);
  RibbonFrame *f = (RibbonFrame *)hw_ptr(frame);
  int in_zone = 0;
  if (race_setup_at(race_setup_get())->course == ZONE_COURSE) {
    int32_t cell = (int32_t)race_player_cell(r->scene, r->index);
    if (cell >= ZONE_FIRST_CELL && !(cell > ZONE_LAST_CELL)) in_zone = 1;
  }

  if (r->action == 2) {
    /* the dash: early in it, on special ground */
    if (r->ground_touch == 0) return;
    if (r->action_step >= 2) return;
    if (r->word174 != 0) {
      if (!fgt(r->effect_clock, CLOCK_SHORT)) return;
      f->word94 = in_zone ? ZONE_KIND + EMIT_MOVE : EMIT_MOVE;
      r->effect_clock = F32(0.0f);
      f->f78 = DASH_SPEED;
      racer_effects_push_set_b(r->child, frame);
    } else {
      f->f78 = DASH_SPEED;
      racer_effects_push_set_b(r->child, frame);
    }
    return;
  }

  if (r->ground_touch != 0) {
    int step = racer_foot_down(r);
    /* (the original also computes the feet's height here, unused) */
    if (r->word174 != 0) f->word94 = in_zone ? ZONE_KIND + EMIT_STEP : EMIT_STEP;
    if (r->mark_a_on != 0 && fgt(r->mark_a_value, MARK_A_MIN)) {
      /* mark A: both feet */
      if (in_zone) f->word94 = ZONE_KIND + EMIT_STEP;
      r->effect_clock = F32(0.0f);
      racer_effects_push_ribbons(r->child, frame);
      r->word144 = r->word144 == 0;
      r->word554 = 1;
      f->word80 = r->word144;
      racer_effects_push_ribbons(r->child, frame);
      r->word144 = r->word144 == 0;
    }
    if (step) {
      racer_effects_push_ribbons(r->child, frame);
      r->word144 = r->word144 == 0;
      r->word554 = 1;
    }
  }

  if (r->word174 != 0) {
    const CharacterParams *p = params_of(r);
    f32 feet = fsub(fsub(r->body.body.position.y, p->f04), p->radius);
    if (!fgt(fadd(feet, ABOVE_FLOOR), f->vec58.y)) {
      f32 vy = r->body.body.velocity.y;
      if (fgt(FALL_SPEED, f32_abs(vy))) {
        racer_emit_by_clock(r, frame, in_zone ? ZONE_KIND : 0);
      } else if (fgt(r->effect_clock, CLOCK_LONG)) {
        f->word94 = in_zone ? ZONE_KIND + EMIT_FALL : EMIT_FALL;
        racer_effects_push_set_b(r->child, frame);
        r->effect_clock = F32(0.0f);
      }
      return;
    }
  }
  if (r->word17c != 0 && fgt(r->effect_clock, CLOCK_SHORT)) {
    r->effect_clock = F32(0.0f);
    racer_effects_push_set_b(r->child, frame);
    racer_effects_push_set_b(r->child, frame);
  }
}

/* state 4 (see the top of the file) */
static void racer_emit_swimming(gaddr self, gaddr frame) {
  Racer *r = racer_at(self);
  if (race_setup_at(race_setup_get())->course == ZONE_COURSE &&
      (int32_t)race_player_cell(r->scene, r->index) == RIBBON_RESTART_CELL) {
    racer_effects_reset_ribbon(r->child, r->contacts.radius, member_addr(self, Racer, matrix3a8));
  }
  if (r->action == 4) {
    if (r->ground_touch == 0) return;
    if (fgt(r->effect_clock, CLOCK_LONG)) {
      racer_effects_push_ribbon(r->child, frame);
      r->effect_clock = F32(0.0f);
      if (r->word17c != 0) racer_effects_push_set_b(r->child, frame);
    }
    if (r->word17c == 0) racer_effects_push_set_b(r->child, frame);
    return;
  }
  if (r->ground_touch == 0) return;
  if (r->word174 != 0) {
    racer_emit_by_clock(r, frame, 0);
    return;
  }
  if (fgt(r->effect_clock, CLOCK_LONG)) {
    if (r->mark_a_on != 0 && fgt(r->mark_a_value, MARK_A_MIN)) {
      racer_effects_reset_ribbon(r->child, r->contacts.radius, member_addr(self, Racer, matrix3a8));
    }
    racer_effects_push_ribbon(r->child, frame);
    r->effect_clock = F32(0.0f);
    if (r->word17c != 0) racer_effects_push_set_b(r->child, frame);
  }
  if (r->word17c == 0) racer_effects_push_set_b(r->child, frame);
}

/* state 5 (see the top of the file) */
static void racer_emit_state5(gaddr self, gaddr frame) {
  Racer *r = racer_at(self);
  RibbonFrame *f = (RibbonFrame *)hw_ptr(frame);
  uint32_t cell = racer_cell(self);
  f32 heading = racer_heading(self);
  racer_effects_push_set_a(r->child, member_addr(self, Racer, matrix3a8), cell, r->contacts.radius, r->f434,
                           heading);
  f32 floor = floor_height(r->scene, racer_cell(self));
  f32 vy = r->body.body.velocity.y;
  f->vec58.y = floor;
  if (r->word178 != 0) {
    if (fgt(FALL_SPEED, f32_abs(vy))) return;
    f->word94 = EMIT_FALL;
    racer_effects_push_set_b(r->child, frame);
    return;
  }
  if (r->word174 == 0) return;
  racer_emit_by_clock(r, frame, 0);
}

/* was func_8C0351C8: the racer's effects of this update (see the top of the file) */
void racer_emit_effects(gaddr self) {
  Racer *r = racer_at(self);
  gaddr stack = hw_stack_alloc(sizeof(EffectsFrame));
  gaddr frame = member_addr(stack, EffectsFrame, frame);
  RibbonFrame *f = (RibbonFrame *)hw_ptr(frame);

  matrix_identity(frame);
  memmove(&f->m, &r->matrix3a8, sizeof(Matrix));
  memmove(&f->position, hw_ptr(method_self(member_addr(self, Racer, body))), sizeof(Vec3));
  memmove(&f->velocity, hw_ptr(member_addr(self, Racer, body.body.velocity)), sizeof(Vec3));
  memmove(&f->vec58, &r->vec534, sizeof(Vec3));
  memmove(&f->vec64, &r->vec540, sizeof(Vec3));
  f->low = r->contacts.radius;
  f->high = params_of(r)->f04;
  f->f78 = r->f434;
  f->heading = racer_heading(self);
  f->word80 = r->word144;
  f->word84 = r->word174;
  f->word88 = r->word17c;
  f->word8c = r->word180;
  f->index = r->index;
  f->word94 = EMIT_MOVE;
  r->effect_clock = fadd(r->effect_clock, r->f434);

  switch (r->state) {
  case 3: racer_emit_walking(self, frame); break;
  case 4: racer_emit_swimming(self, frame); break;
  case 5: racer_emit_state5(self, frame); break;
  default: break;
  }
  hw_stack_free(sizeof(EffectsFrame));
}
HW_EXPORT(8c0351c8, racer_emit_effects(ARG0))
