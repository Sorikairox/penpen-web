/* Pen Pen port, S17 (r1): the computer player's controls in racer state 4 (func_8C03ED24, the LaneRacer's step that
 * lane_racer_update runs in state 4).
 *
 * It aims at a point of its lane a few rows ahead (more the faster it goes, two more on the special ground of kind
 * 2): on that row's racing line (lane_line), or where a pending side marker says, or at the position of a marker
 * the look ahead finds. The stick turns it fully left or right while the point is more than 2 degrees off its
 * heading. The button makes its strokes: stroke_clock counts up every update, the button is held from range1_min
 * on and the stroke ends (both clocks cleared) once the clock passes range1_max plus a margin; the clock is set back
 * (so the next stroke comes sooner, or it holds on) by the ground, the pending marker and how sharp the line turns.
 * Character 7 also holds back while it is ahead of player 0 and dashes when it falls behind. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"   /* racer_cell, racer_set_action */
#include "course.h"           /* course_words */
#include "race_scene.h"
#include "scenery.h"          /* race_player_cell */
#include "lane_markers.h"
#include "racer.h"

#define AHEAD_RATE        F32(0.5f)              /* rows looked ahead per unit of f434, past the first two */
#define AHEAD_MIN         2
#define AHEAD_MAX         5
#define AHEAD_GROUND2     2                      /* more rows on special ground of kind 2 */
#define GROUND_KIND2      2
#define LINE_LIMIT        F32_BITS(0x3EE66666u)  /* 0.45: the racing line is kept this far inside the row */
#define LINE_CENTRE       F32(0.5f)              /* the line's offset -> a fraction from the row's first vertex */
#define TURN_UNITS        F32(65536.0f)          /* Ninja angle units per turn */
#define TWO_PI            F32_BITS(0x40C90FD7u)  /* 6.2831855 */
#define AIM_DEAD_ZONE     F32_BITS(0x43B60B60u)  /* 364.0889: 2 degrees in Ninja units */
#define CLOCK_BACK        F32_BITS(0xBF8CCCCCu)  /* -1.0999999: just under the threshold it is added to */
#define OFF_GROUND_FRAMES 20                     /* updates off special ground before the clock is set back */
#define GROUND2_FAST      F32_BITS(0x409425EBu)  /* 4.6296296 (125 / 27): f434 above it on ground kind 2 */
#define GROUND2_MARGIN    F32(10.0f)             /* the longer stroke on ground kind 2 when fast */
#define GROUND2_BASE      F32_BITS(0xBF6D097Au)  /* -0.9259259 (-25 / 27) */
#define COAST_FRAMES      F32(240.0f)            /* pending kind 18 ends after this long released */
#define FLAG2_FRAMES      F32(120.0f)
#define LINE_STRAIGHT     F32_BITS(0x3DCCCCCCu)  /* 0.1: |line| above it is a turn */
#define LINE_BENT         F32_BITS(0xBDCCCCCCu)  /* -0.1 */
#define TURN_SLOPE        F32_BITS(0x415E38E2u)  /* 13.888888 (125 / 9) */
#define TURN_SPEED        F32_BITS(0x41392F67u)  /* 11.574074 */
#define TURN_SPEED_MIN    F32_BITS(0x404F684Au)  /* 3.2407408 */
#define AIM_FAR_OFF       F32_BITS(0x45E38E38u)  /* 7281.778: 40 degrees in Ninja units */
#define CHARACTER_PACER   7                      /* the character that waits for player 0 */
#define PACER_FRAMES      300
#define PACER_LEAD        3                      /* cells ahead of player 0 to hold back */
#define PACER_BEHIND      (-2)                   /* cells behind (or more) to dash */
#define PACER_CATCH_UP    1                      /* holding back ends at this lead or less */
#define ACTION_DASH       4

/* a row of a lane: its two vertices (LaneRacer.lane_points, 0x18 bytes) */
typedef struct LaneRow {
  Vec3 a; /* 0x00 */
  Vec3 b; /* 0x0C */
} LaneRow;
HW_SIZE(LaneRow, 0x18);

/* the original's frame (0x98 bytes under PR): the look ahead's result sits at 0x44 */
typedef struct SteerFrame {
  uint8_t locals[0x44]; /* 0x00: the clocks' margins, the cells, the angles, the vectors */
  LaneLook look;        /* 0x44 */
  uint8_t rest[0x9C - 0x54]; /* 0x54: the target, the row's vertices, the row, the flags, `this`, PR */
} SteerFrame;
HW_OFFSET(SteerFrame, look, 0x44);
HW_SIZE(SteerFrame, 0x9C);

/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* was func_8C03ED24: the computer player's stick and button in racer state 4 (see the top of the file) */
void lane_racer_steer(gaddr self) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);

  int on_ground2 = 0;
  if (r->ground == GROUND_KIND2) on_ground2 = 1;
  int32_t ahead = f32_to_int(fmul(r->f434, AHEAD_RATE)) + AHEAD_MIN;
  if (ahead > AHEAD_MAX) ahead = AHEAD_MAX;
  int32_t row = l->quad + ahead;
  if (on_ground2) row += AHEAD_GROUND2;
  if (row > l->lane_last[l->lane]) row = l->lane_last[l->lane];

  /* the point across the row: on the racing line kept inside the row, or the pending side marker's */
  f32 line = ((const f32 *)hw_ptr(l->lane_line[l->lane][l->branch]))[row];
  f32 side = line;
  if (fgt(side, LINE_LIMIT)) side = LINE_LIMIT;
  if (fgt(fneg(LINE_LIMIT), side)) side = fneg(LINE_LIMIT);
  f32 fraction = fadd(side, LINE_CENTRE);
  const LaneRow *rw = (const LaneRow *)hw_ptr(l->lane_points) + row;
  if (l->pending_kind == LANE_PENDING_SIDE) fraction = l->target_x;
  Vec3 target;
  target.x = fsub(rw->b.x, rw->a.x);
  target.y = fsub(rw->b.y, rw->a.y);
  target.z = fsub(rw->b.z, rw->a.z);
  target.x = fmul(target.x, fraction);
  target.y = fmul(target.y, fraction);
  target.z = fmul(target.z, fraction);
  target.x = fadd(rw->a.x, target.x);
  target.y = fadd(rw->a.y, target.y);
  target.z = fadd(rw->a.z, target.z);

  /* a marker ahead: aim at it (kinds 0, 9, 10, 15), or leave the side way (16) */
  gaddr stack = hw_stack_alloc(sizeof(SteerFrame));
  int32_t found = lane_racer_look_ahead(self, member_addr(stack, SteerFrame, look));
  LaneLook look = ((const SteerFrame *)hw_ptr(stack))->look;
  if (found != 0) {
    const LaneMarker *m = (const LaneMarker *)hw_ptr(look.marker);
    switch (m->kind) {
    case LANE_MARKER_AIM_0:
    case LANE_MARKER_AIM_9:
    case LANE_MARKER_AIM_10:
    case LANE_MARKER_AIM_15:
      target = m->position;
      break;
    case LANE_MARKER_REJOIN:
      if (l->branch == 1) {
        l->branch = 0;
        const int32_t *ways = (const int32_t *)hw_ptr(course_words(race_scene_at(r->scene)->course)) + l->lane * 2;
        l->quad = ways[1];
        l->prev_quad = l->quad;
        l->branch_checked = l->quad;
        l->lane_points = l->lanes[l->lane][l->branch];
      }
      break;
    default:
      break;
    }
  }

  /* the direction to the point against the heading, in Ninja units (16 bits) */
  Vec3 position = r->body.body.position;
  f32 dx = fsub(target.x, position.x);
  f32 dz = fsub(target.z, position.z);
  f32 aim = f32_from_int(f32_to_int(fdiv(fmul(rt_atan2f(dx, dz), TURN_UNITS), TWO_PI)));
  f32 off = f32_from_int((int16_t)f32_to_int(fsub(r->heading.value, aim)));
  r->stick_y = F32(0.0f);
  if (fgt(off, AIM_DEAD_ZONE)) {
    r->stick_y = F32(1.0f);
  } else if (fgt(fneg(AIM_DEAD_ZONE), off)) {
    r->stick_y = fneg(F32(1.0f));
  }
  r->stick_x = F32(0.0f);

  /* the stroke clock */
  f32 margin = F32(0.0f);
  f32 turn_base = F32(0.0f);
  if (r->ground == 0) {
    l->off_ground_frames++;
    if (l->off_ground_frames > OFF_GROUND_FRAMES) l->stroke_clock = fadd(l->range1_min, CLOCK_BACK);
  } else {
    l->off_ground_frames = 0;
  }
  if (l->pending_kind == LANE_PENDING_STROKE2) l->stroke_clock = fadd(l->range1_max, CLOCK_BACK);
  if (on_ground2) {
    if (fgt(r->f434, GROUND2_FAST)) margin = fadd(margin, GROUND2_MARGIN);
    turn_base = GROUND2_BASE;
  }
  if (l->pending_kind == LANE_PENDING_COAST) {
    l->stroke_clock = F32(0.0f);
    if (fgt(l->glide_clock, COAST_FRAMES)) {
      l->pending_kind = -1;
      l->pending_level = -1;
    }
  }
  if (look.flag2 != 0) {
    if (fgt(l->glide_clock, FLAG2_FRAMES)) {
      l->stroke_clock = fadd(l->range1_min, F32(1.0f));
    } else {
      l->stroke_clock = fadd(l->range1_min, CLOCK_BACK);
    }
  }
  if (l->pending_kind != LANE_PENDING_GLIDE) {
    /* on a bent line, no stroke above a speed that falls as the line bends more */
    f32 bend = f32_abs(line);
    if (fgt(bend, LINE_STRAIGHT)) {
      f32 limit = fadd(fsub(turn_base, fmul(fadd(bend, LINE_BENT), TURN_SLOPE)), TURN_SPEED);
      if (fgt(TURN_SPEED_MIN, limit)) limit = TURN_SPEED_MIN;
      if (fgt(r->f434, limit)) l->stroke_clock = F32(0.0f);
    }
    /* nor while the point is far off the heading */
    if (fgt(f32_abs(off), AIM_FAR_OFF)) l->stroke_clock = F32(0.0f);
  }

  /* character 7 holds back while it is ahead of player 0 */
  uint32_t leader_cell = race_player_cell(r->scene, 0);
  uint32_t cell = racer_cell(self);
  int32_t lead = (int32_t)(cell - leader_cell);
  if (r->character == CHARACTER_PACER) {
    switch (l->holding_back) {
    case 0:
      l->hold_timer++;
      if (l->hold_timer > PACER_FRAMES) {
        if (lead >= PACER_LEAD) {
          l->holding_back = 1;
          l->hold_timer = 0;
        } else if (!(lead > PACER_BEHIND)) {
          racer_set_action(self, ACTION_DASH);
          l->hold_timer = 0;
        }
      }
      break;
    case 1:
      l->stroke_clock = F32(0.0f);
      if (!(lead > PACER_CATCH_UP)) {
        racer_set_action(self, ACTION_DASH);
        l->holding_back = 0;
      }
      break;
    default:
      break;
    }
  }

  /* the button: released until the clock reaches range1_min; the stroke ends past range1_max + margin */
  l->stroke_clock = fadd(l->stroke_clock, F32(1.0f));
  int press;
  if (fgt(l->range1_min, l->stroke_clock)) {
    press = 0;
    l->glide_clock = fadd(l->glide_clock, F32(1.0f));
  } else {
    press = 1;
    if (fgt(l->stroke_clock, fadd(l->range1_max, margin))) {
      l->stroke_clock = F32(0.0f);
      l->glide_clock = F32(0.0f);
    }
  }
  if (press) {
    r->button[0] = 1;
    r->button_lock[0] = 0;
  } else {
    r->button[0] = 0;
  }
  hw_stack_free(sizeof(SteerFrame));
}
HW_EXPORT(8c03ed24, lane_racer_steer(ARG0))
