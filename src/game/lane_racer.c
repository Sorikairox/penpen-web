/* Pen Pen port, S14 (batch b08): the computer player (LaneRacer, game/racer.h): its construction and destruction,
 * its reset on the course's lanes and its update (0x8C03DB60-0x8C03E14A, 0x8C03F9CE).
 *
 * A LaneRacer is a Racer that follows the course's lanes: the ground's cell kinds 1-5 tell which of the four lanes
 * it is on, and it keeps the quad of that lane it stands on (a quad is the strip between two rows of the lane's
 * vertex pairs). Where the course's words say so, a lane has a side way (branch 1) that the odd-numbered players
 * take. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/contact.h"
#include "motion.h"
#include "course.h"
#include "race_scene.h"
#include "racer.h"

/* ---- calls into code that is still translated ---- */

/* func_8C030BF4: the Racer's base construction (this, a, b, c, d; the collider's radius in FR4) */
static void racer_base_construct(gaddr self, uint32_t a, uint32_t b, uint32_t c, uint32_t d, f32 radius) {
  FARGS1(radius);
  CALL5(0x8C030BF4u, self, a, b, c, d);
}

/* func_8C030F00: the Racer's reset (position, a word, a float) */
static void racer_base_reset(gaddr self, gaddr position, uint32_t word, f32 value) {
  FARGS1(value);
  CALL3(0x8C030F00u, self, position, word);
}

/* func_8C03F470: the computer player's step in racer state 5 */
static void lane_racer_state5(gaddr self) { CALL1(0x8C03F470u, self); }

/* func_8C0317D4: the Racer's update (after the controls are made) */
static void racer_base_update(gaddr self) { CALL1(0x8C0317D4u, self); }

/* func_8C03E150 / func_8C03ED24: the computer player's step in racer states 3 and 4 */
static void lane_racer_state3(gaddr self) { CALL1(0x8C03E150u, self); }
static void lane_racer_state4(gaddr self) { CALL1(0x8C03ED24u, self); }

/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* ---- tables ---- */

/* the order the quads near a guess are tried in: 7 offsets from it (0 first) */
#define LANE_QUAD_OFFSETS ((const int32_t *)hw_ptr(0x8C0832C8u))
#define LANE_QUAD_TRIES 7

/* the two range tables, a {min, width} pair per range_index */
typedef struct LaneRange {
  f32 min, width;
} LaneRange;
#define LANE_RANGES1 ((const LaneRange *)hw_ptr(0x8C083258u))
#define LANE_RANGES2 ((const LaneRange *)hw_ptr(0x8C083290u))

/* ---- construction ---- */

/* was func_8C03DB60: the base construction: the Racer's (with a, b, c, d and the radius), then the scene and the
 * range row; word588 cleared */
gaddr lane_racer_construct_base(gaddr self, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                                int32_t range_index, f32 radius) {
  racer_base_construct(self, a, b, c, d, radius);
  racer_at(self)->scene = scene;
  lane_racer_at(self)->range_index = range_index;
  racer_at(self)->word588 = 0;
  return self;
}
HW_EXPORT(8c03db60, RET(lane_racer_construct_base(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5, (int32_t)ARG6, FARG0)))

/* was func_8C03DB9A: the constructor: the vtable, the second base, then the base construction */
gaddr lane_racer_construct(gaddr self, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                           int32_t range_index, f32 radius) {
  racer_at(self)->task.vtable = LANE_RACER_VTABLE;
  racer_at(self)->second_base = member_addr(self, LaneRacer, second_base_at);
  lane_racer_construct_base(self, scene, a, b, c, d, range_index, radius);
  return self;
}
HW_EXPORT(8c03db9a, RET(lane_racer_construct(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5, (int32_t)ARG6, FARG0)))

/* was func_8C03DBC4: the destructor's body: the second Contacts, the collider, the character's model file (the base
 * part's destructor, func_8C03D9EA, is empty) */
void lane_racer_destroy(gaddr self) {
  contacts_free(member_addr(self, Racer, contacts2));
  contacts_holder_destroy_self(member_addr(self, Racer, collider_data));
  motion_player_free(member_addr(self, Racer, second_base));
  method_empty();
}
HW_EXPORT(8c03dbc4, lane_racer_destroy(ARG0))

/* was func_8C03DBEA: the deleting destructor (vtable 0x8C23DB34's first method) */
gaddr lane_racer_delete(gaddr self, int32_t free_memory) {
  lane_racer_destroy(self);
  if (free_memory != 0) syFree(self);
  return self;
}
HW_EXPORT(8c03dbea, RET(lane_racer_delete(ARG0, (int32_t)ARG1)))

/* ---- lanes ---- */

/* was func_8C03F9CE: the quad of the current lane that `point` is in, looked for near `near` (near, then +-1, +-2,
 * ... by the offset table); `near` itself when none of the seven contains it */
int32_t lane_racer_find_quad(gaddr self, gaddr point, int32_t near) {
  const LaneRacer *l = lane_racer_at(self);
  int32_t found = -1;
  for (int32_t i = 0; i < LANE_QUAD_TRIES; i++) {
    int32_t quad = LANE_QUAD_OFFSETS[i] + near;
    if (quad >= 0 && quad < l->lane_last[l->lane] && lane_quad_contains(self, point, quad)) {
      found = quad;
      break;
    }
  }
  return found == -1 ? near : found;
}
HW_EXPORT(8c03f9ce, RET(lane_racer_find_quad(ARG0, ARG1, (int32_t)ARG2)))

/* the end of a range: min + width, 10 more for the characters that get the bonus */
static f32 range_max(f32 min, f32 width, int bonus) {
  f32 max = fadd(width, min);
  return bonus ? fadd(max, F32(10.0f)) : max;
}

/* was func_8C03DC54: the reset: the Racer's (position, word, value), then the lanes from the course (every lane's
 * two vertex-pair tables, motions, texture lists, extra words and quad counts), lane 0 and the main way, the two
 * ranges of its range row, the quad under its position, and the rest of its state */
void lane_racer_reset(gaddr self, gaddr position, uint32_t word, f32 value) {
  racer_base_reset(self, position, word, value);
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  const CourseModelSet *sets = (const CourseModelSet *)hw_ptr(course_models(race_scene_at(r->scene)->course));
  const int32_t *rows = (const int32_t *)hw_ptr(course_env_words(race_scene_at(r->scene)->course));
  for (int32_t lane = 0; lane < 4; lane++) {
    for (int32_t branch = 0; branch < 2; branch++) {
      const CourseModelSet *set = &sets[lane * 2 + branch];
      l->lanes[lane][branch] = set->model;
      l->lane_motion[lane][branch] = set->motion;
      l->lane_texlist[lane][branch] = set->texlist;
      l->lane_extra[lane][branch] = set->extra;
    }
    l->lane_last[lane] = rows[lane] - 1;
  }
  l->prev_lane = 0;
  l->lane = 0;
  l->f678 = F32(0.0f);
  l->f660 = F32(0.0f);
  l->word67c = 0;
  l->lane_points = l->lanes[0][0];
  l->branch = 0;
  l->branch_checked = 0;

  const LaneRange *range1 = &LANE_RANGES1[l->range_index];
  const LaneRange *range2 = &LANE_RANGES2[l->range_index];
  int bonus = r->character == 1 || r->character == 4 || r->character == 6;
  l->range1_min = range1->min;
  l->range1_max = range_max(l->range1_min, range1->width, bonus);
  l->range2_min = range2->min;
  l->range2_max = range_max(l->range2_min, range2->width, bonus);
  l->word664 = 0;

  l->quad = lane_racer_find_quad(self, member_addr(self, Racer, body), 0);
  l->prev_quad = l->quad;
  l->word63c = 0;
  l->word640 = 0;
  l->word668 = 0;
  l->reset_position = r->body.body.position;
  l->word680 = 0;
  l->word684 = 0;
  l->word688 = -1;
  l->word68c = 0;
  l->word690 = -1;
  l->f694 = F32(0.5f);
}
HW_EXPORT(8c03dc54, lane_racer_reset(ARG0, ARG1, ARG2, FARG0))

/* ---- the update ---- */

/* the ground kinds that tell the lane: 1 = lane 0, 2-5 = lanes 0-3; 8 and 9 mark a lane change without one */
static int lane_from_ground(LaneRacer *l, int32_t kind) {
  switch (kind) {
  case 1:
    l->lane = 0;
    return 1;
  case 2: case 3: case 4: case 5:
    l->lane = kind - 2;
    return 1;
  case 8: case 9:
    return 1;
  default:
    return 0;
  }
}

/* was func_8C03DE18: the update (not while paused): the buttons' edges are kept, the lane from the ground under
 * the racer; on a lane change the quad starts at the lane's first quad (to a higher lane) or its last (to a lower
 * one); the side way is entered at the course's quad for it (odd players only) and left at its end, back to the
 * main way's rejoining quad; on ground that marks a lane, the racer's state step runs (states 3, 4, 5) and the quad
 * is found again. Then the buttons' edges, the stick (scaled down when damped), its length and its angle in Ninja
 * units; then the Racer's update. */
void lane_racer_update(gaddr self) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  if (r->task.paused != 0) return;
  r->button_prev[0] = r->button[0];
  r->button_prev[1] = r->button[1];

  uint16_t attr = (uint16_t)course_cell_attr(race_scene_at(r->scene)->course, r->contacts.cell);
  int on_lane_mark = lane_from_ground(l, CELL_KIND(attr));

  if (l->prev_lane < l->lane) {
    l->quad = 0;
    l->prev_quad = 0;
    l->word63c = 0;
  } else if (l->prev_lane > l->lane) {
    l->quad = l->lane_last[l->lane] - 1;
    l->prev_quad = l->quad;
    l->word63c = 0;
  }
  l->lane_points = l->lanes[l->lane][l->branch];

  /* the side way of this lane: {the main way's quad it leaves at, the main way's quad it rejoins at} */
  const int32_t *side = (const int32_t *)hw_ptr(course_words(race_scene_at(r->scene)->course)) + l->lane * 2;
  if (side[0] != 0 && l->quad != l->branch_checked) {
    if (l->branch == 0) {
      if (l->quad == side[0] && (r->index & 1) != 0) {
        l->branch_length = side[1] - l->quad;
        l->branch = 1;
        l->quad = 0;
        l->prev_quad = 0;
      }
    } else if (l->quad == l->branch_length - 1) {
      l->branch = 0;
      l->quad = side[1];
      l->prev_quad = l->quad;
    }
    l->branch_checked = l->quad;
    l->lane_points = l->lanes[l->lane][l->branch];
  }

  int stepped = 0;
  if (on_lane_mark) {
    switch (r->state) {
    case 3: lane_racer_state3(self); stepped = 1; break;
    case 4: lane_racer_state4(self); stepped = 1; break;
    case 5: lane_racer_state5(self); stepped = 1; break;
    default: break;
    }
  }
  if (stepped) {
    l->prev_lane = l->lane;
    l->prev_quad = l->quad;
    l->quad = lane_racer_find_quad(self, member_addr(self, Racer, body), l->prev_quad);
    if (l->quad == -1) l->quad = l->prev_quad;
  }

  for (int b = 0; b < 2; b++) {
    r->edges[b].pressed = r->button[b] != 0 && r->button_prev[b] == 0;
    r->edges[b].released = r->button[b] == 0 && r->button_prev[b] != 0;
  }

  if (r->stick_damped != 0) {
    r->stick_x = fmul(r->stick_x, F32(0.5f));
    r->stick_y = fmul(r->stick_y, F32(0.75f));
  }
  r->stick_length = njSqrt(fadd(fmul(r->stick_x, r->stick_x), fmul(r->stick_y, r->stick_y)));
  if (fgt(r->stick_length, F32_BITS(0x3DCCCCCCu) /* 0.1 */)) {
    f32 radians = rt_atan2f(fneg(r->stick_y), r->stick_x);
    f32 units = fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 6.2831855: 2 pi */);
    r->stick_angle = f32_from_int(f32_to_int(units));
    r->stick_angle = f32_from_int((int16_t)f32_to_int(r->stick_angle));
  }
  racer_base_update(self);
}
HW_EXPORT(8c03de18, lane_racer_update(ARG0))

/* was func_8C03E14A: a jump to func_8C0373B8 with the same `this` (a method the LaneRacer shares with the Racer) */
static uint32_t racer_method_8c0373b8(gaddr self) { return CALL1(0x8C0373B8u, self); }
HW_EXPORT(8c03e14a, RET(racer_method_8c0373b8(ARG0)))

/* was func_8C040682: the fractional part of the racer's x position (x - its truncation toward zero) */
f32 racer_x_fraction(gaddr racer) {
  f32 x = racer_at(racer)->body.body.position.x;
  return fsub(x, f32_from_int(f32_to_int(x)));
}
HW_EXPORT(8c040682, RETF(racer_x_fraction(ARG0)))
