/* Pen Pen port, S18 b06: the computer player's controls in racer state 5, swimming (func_8C03F470, the LaneRacer's
 * step that lane_racer_update runs in state 5; state 4's is game/lane_racer_steer.c).
 *
 * It aims at a point of its lane two rows ahead (one on the lane's last quad): across the row on its racing line
 * (lane_line, kept within half the row), at a height halfway between the row's first vertex and the first vertex of
 * the same row in the lane's second table (lane_extra); or at a marker the look ahead finds (kinds 0, 9, 10, 15; kind
 * 20 lifts the point by 150). The stick turns it fully left or right by the side the point is on (no dead zone), and
 * pitches it by the height to the point (0.015 a unit, within -1..1). The button makes the strokes: stroke_clock
 * counts up every update, the button is held from range2_min on and the clock clears once it passes range2_max; it is
 * set just under range2_min while the point is more than 30 degrees off the heading, and just over it while dashing.
 * Character 7 holds back while it is two cells ahead of player 0 (no strokes, level), and dashes when it falls two
 * behind, or once it is back level and in the inner 0.4 of the way across. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"   /* racer_cell, racer_set_action */
#include "race_scene.h"
#include "scenery.h"          /* race_player_cell */
#include "lane_markers.h"
#include "racer.h"

#define LINE_LIMIT        F32(0.5f)              /* the racing line is kept within the row */
#define LINE_CENTRE       F32(0.5f)              /* the line's offset / 2 -> a fraction from the row's first vertex */
#define TURN_UNITS        F32(65536.0f)          /* Ninja angle units per turn */
#define TWO_PI            F32_BITS(0x40C90FD7u)  /* 6.2831855 */
#define LIFT_MARKER       20                     /* a marker kind that lifts the point */
#define LIFT_HEIGHT       F32(150.0f)
#define PITCH_RATE        F32_BITS(0x3C75C28Fu)  /* 0.015: stick per unit of height to the point */
#define AIM_FAR_OFF       F32_BITS(0x45AAAAAAu)  /* 5461.333: 30 degrees in Ninja units */
#define CLOCK_BACK        F32_BITS(0xBF8CCCCCu)  /* -1.0999999: just under the threshold it is added to */
#define PACE_X_INNER      F32_BITS(0x3ECCCCCCu)  /* 0.4: racer_x_fraction under it for the dash */
#define CHARACTER_PACER   7                      /* the character that waits for player 0 */
#define PACER_FRAMES      300
#define PACER_LEAD        2                      /* cells ahead of player 0 to hold back */
#define PACER_BEHIND      (-2)                   /* cells behind (or more) to dash */
#define PACER_CATCH_UP    (-1)                   /* holding back ends at this lead or less */
#define ACTION_DASH       4

/* a row of a lane: its two vertices (LaneRacer.lane_points, and lane_extra's rows, 0x18 bytes) */
typedef struct LaneRow {
  Vec3 a; /* 0x00 */
  Vec3 b; /* 0x0C */
} LaneRow;
HW_SIZE(LaneRow, 0x18);

/* the original's frame (0x84 bytes under PR): the look ahead's result sits at 0x38 */
typedef struct SwimFrame {
  uint8_t locals[0x38];      /* 0x00: the pitch, the press, the cells, the angles, the position, the direction */
  LaneLook look;             /* 0x38 */
  uint8_t rest[0x88 - 0x48]; /* 0x48: the point, the row's vertices, the rows, `this`, PR */
} SwimFrame;
HW_OFFSET(SwimFrame, look, 0x38);
HW_SIZE(SwimFrame, 0x88);

/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* was func_8C03F470: the computer player's stick and button in racer state 5 (see the top of the file) */
void lane_racer_swim(gaddr self) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);

  /* the row: two ahead, one on the last quad */
  int32_t row;
  if (l->quad >= l->lane_last[l->lane] - 1)
    row = l->quad + 1;
  else
    row = l->quad + 2;
  const LaneRow *rw = (const LaneRow *)hw_ptr(l->lane_points) + row;
  const LaneRow *extra = (const LaneRow *)hw_ptr(l->lane_extra[l->lane][l->branch]) + row;

  /* the point across the row on the racing line */
  Vec3 a = rw->a;
  Vec3 b = rw->b;
  Vec3 target = b;
  target.x = fsub(target.x, a.x);
  target.y = fsub(target.y, a.y);
  target.z = fsub(target.z, a.z);
  f32 side = ((const f32 *)hw_ptr(l->lane_line[l->lane][l->branch]))[row];
  if (fgt(side, LINE_LIMIT)) side = LINE_LIMIT;
  if (fgt(fneg(LINE_LIMIT), side)) side = fneg(LINE_LIMIT);
  f32 fraction = fadd(fdiv(side, F32(2.0f)), LINE_CENTRE);
  target.x = fmul(target.x, fraction);
  target.y = fmul(target.y, fraction);
  target.z = fmul(target.z, fraction);
  target.x = fadd(target.x, a.x);
  target.y = fadd(target.y, a.y);
  target.z = fadd(target.z, a.z);
  /* its height: halfway up to the second table's first vertex */
  target.y = fadd(fmul(fsub(extra->a.y, a.y), F32(0.5f)), a.y);

  /* a marker ahead: aim at it (kinds 0, 9, 10, 15), or lift the point (20) */
  gaddr stack = hw_stack_alloc(sizeof(SwimFrame));
  if (lane_racer_look_ahead(self, member_addr(stack, SwimFrame, look)) != 0) {
    const LaneMarker *m = (const LaneMarker *)hw_ptr(((const SwimFrame *)hw_ptr(stack))->look.marker);
    switch (m->kind) {
    case LANE_MARKER_AIM_0:
    case LANE_MARKER_AIM_9:
    case LANE_MARKER_AIM_10:
    case LANE_MARKER_AIM_15:
      target.x = m->position.x;
      target.y = m->position.y;
      target.z = m->position.z;
      break;
    case LIFT_MARKER:
      target.y = fadd(target.y, LIFT_HEIGHT);
      break;
    default:
      break;
    }
  }

  /* the direction to the point against the heading, in Ninja units (16 bits): full stick to its side */
  Vec3 position = r->body.body.position;
  f32 dx = fsub(target.x, position.x);
  f32 dz = fsub(target.z, position.z);
  f32 aim = f32_from_int(f32_to_int(fdiv(fmul(rt_atan2f(dx, dz), TURN_UNITS), TWO_PI)));
  f32 off = f32_from_int((int16_t)f32_to_int(fsub(r->heading.value, aim)));
  if (fgt(off, F32(0.0f)))
    r->stick_y = F32(1.0f);
  else
    r->stick_y = fneg(F32(1.0f));

  /* the pitch by the height to the point */
  f32 pitch = fmul(fsub(position.y, target.y), PITCH_RATE);
  if (fgt(pitch, F32(1.0f))) pitch = F32(1.0f);
  if (fgt(fneg(F32(1.0f)), pitch)) pitch = fneg(F32(1.0f));
  r->stick_x = pitch;

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
      r->stick_x = F32(0.0f);
      if (!(lead > PACER_CATCH_UP)) {
        if (fgt(PACE_X_INNER, racer_x_fraction(self))) racer_set_action(self, ACTION_DASH);
        l->holding_back = 0;
        l->hold_timer = 0;
      }
      break;
    default:
      break;
    }
  }

  /* the button: released until the clock reaches range2_min; the clock clears past range2_max */
  int press = 0;
  l->stroke_clock = fadd(l->stroke_clock, F32(1.0f));
  if (fgt(l->range2_min, l->stroke_clock)) {
    press = 0;
  } else {
    press = 1;
    if (fgt(l->stroke_clock, l->range2_max)) l->stroke_clock = F32(0.0f);
  }
  if (fgt(f32_abs(off), AIM_FAR_OFF)) l->stroke_clock = fadd(l->range2_min, CLOCK_BACK);
  if (r->action == ACTION_DASH) l->stroke_clock = fadd(l->range2_min, F32(1.0f));
  if (press) {
    r->button[0] = 1;
    r->button_lock[0] = 0;
  } else {
    r->button[0] = 0;
  }
  hw_stack_free(sizeof(SwimFrame));
}
HW_EXPORT(8c03f470, lane_racer_swim(ARG0))
