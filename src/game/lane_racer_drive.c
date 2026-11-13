/* Pen Pen port, S17 (r3): the computer player's controls in racer state 3 (func_8C03E150, the LaneRacer's step that
 * lane_racer_update runs in state 3).
 *
 * It runs one of three manoeuvres (LaneRacer.manoeuvre):
 *  - racing (0): it aims at its lane's racing line one row ahead (pulled a tenth of the way toward the next row's),
 *    or at a marker the look ahead finds, or at a pending kind 3 marker while that one is still ahead; the stick turns
 *    it toward the aim (0.3, 0.6 or full by how far off its heading the aim is) and pushes forward. Button 0 makes
 *    strokes for 50 updates when the look ahead or the ground asks (held 60 updates of every 61); button 1 when the
 *    look ahead asks or when its paired racer is close, level with it and straight ahead. Every 130 updates (while
 *    mark_b_on) it checks how far it went: under 12 units, it is stuck and works loose. Character 7 also paces
 *    player 0: well ahead it holds back, behind it turns round or dashes.
 *  - working loose (1): stick back 60 updates, forward 40, forward with strokes 100, then racing again.
 *  - holding back (2, character 7): stopped, backing off to one side, or gliding with strokes (by the step its
 *    random-looking pick gave), until player 0 catches up or 600 updates pass; then a dash, half of the time.
 * At the end the two buttons are held or released as decided (a held button's lock is cleared). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h" /* method_self, field_get_f32, racer_cell, racer_set_action, racer_matrix_axis_z, ... */
#include "race_scene.h"
#include "scenery.h"        /* race_player_body, race_player_cell */
#include "lane_markers.h"
#include "racer.h"

/* the manoeuvres (LaneRacer.manoeuvre) */
enum { MANOEUVRE_RACING = 0, MANOEUVRE_WORK_LOOSE = 1, MANOEUVRE_HOLD_BACK = 2 };
/* the racing manoeuvre's steps for character 7 (LaneRacer.manoeuvre_step) */
enum { PACE_WATCH = 0, PACE_TURNED = 1, PACE_DASH_SOON = 2, PACE_DASH_STRAIGHT = 3 };

#define ACTION_DASH        4
#define CHARACTER_PACER    7                     /* the character that paces player 0 */
#define PENDING_NONE       (-1)
#define PENDING_AIM        3                     /* a pending marker to aim at while it is ahead */
#define PENDING_SIDE       4                     /* a pending marker giving the side of the lane (target_x) */

#define LINE_SIDE          F32(0.5f)             /* the middle of the row (from its first vertex to its second) */
#define NEXT_ROW_PULL      F32_BITS(0x3DCCCCCCu) /* 0.1: how far the aim is pulled toward the next row's point */
#define AHEAD_ENOUGH       F32(0.25f)            /* dot(direction to the pending marker, course direction) */
#define TURN_UNITS         F32(65536.0f)         /* Ninja angle units per turn */
#define TWO_PI             F32_BITS(0x40C90FD7u) /* 6.2831855 */
#define HALF_TURN          F32(32768.0f)
#define FAR_OFF            F32_BITS(0x46471C71u) /* 12743.111: 70 degrees in Ninja units */
#define OFF                F32_BITS(0x45AAAAAAu) /* 5461.333: 30 degrees */
#define STICK_FULL         F32(1.0f)
#define STICK_HALF         F32_BITS(0x3F199999u) /* 0.6 */
#define STICK_LIGHT        F32_BITS(0x3E999999u) /* 0.3 */
#define STROKE_UPDATES     50                    /* stroke_frames a marker or the ground asks for */
#define STROKE_CLOCK_MAX   F32(60.0f)            /* the button is let go (one update) once stroke_clock passes it */
#define RIVAL_NEAR         F32(50.0f)            /* the paired racer's distance (RaceScene.values) */
#define RIVAL_LEVEL        F32(5.0f)             /* |height difference| */
#define RIVAL_AHEAD        F32_BITS(0x3F7851EBu) /* 0.97: dot(forward axis, direction to it) */
#define STUCK_CHECK        130                   /* updates between two checks */
#define STUCK_DISTANCE     F32_BITS(0x413FFFFFu) /* 11.999999: |dx| + |dz| below it is stuck */
#define PACE_WAIT          240                   /* updates character 7 races before it judges its lead */
#define PACE_AHEAD         2                     /* cells ahead of player 0: hold back */
#define PACE_FAR_BEHIND    (-4)                  /* cells behind (or more): dash once straight */
#define PACE_BEHIND        (-2)                  /* cells behind (or more): turn round or dash soon */
#define PACE_HOLD_KINDS    F32_BITS(0x403F5C28u) /* 2.99: the x fraction times it picks the hold-back step 0-2 */
#define PACE_TURN_CHANCE   F32_BITS(0x3F333333u) /* 0.7: x fraction below it turns round, else dashes soon */
#define PACE_TURN_LEAD     2                     /* turning round ends past this lead ... */
#define PACE_TURN_MAX      600                   /* ... or after this many updates (or on a dash) */
#define PACE_STROKE_AT     5                     /* dash soon: strokes asked at this update ... */
#define PACE_STROKES       60
#define PACE_DASH_AT       30                    /* ... and the dash at this one */
#define PACE_STRAIGHT      F32_BITS(0x3E999999u) /* 0.3: |stick_y| at most this: straight enough to dash */
#define LOOSE_BACK_FRAMES  60
#define LOOSE_GO_FRAMES    40
#define LOOSE_STROKE_FRAMES 100
#define LOOSE_BACK         F32(-0.5f)
#define HOLD_MAX           600
#define HOLD_BACK_OFF      F32_BITS(0xBF333333u) /* -0.7 */
#define HOLD_GLIDE         F32_BITS(0xBE4CCCCCu) /* -0.2 */
#define HOLD_DASH_CHANCE   F32(0.5f)             /* x fraction below it: a dash when holding back ends */

/* a row of a lane: its two vertices (LaneRacer.lane_points, 0x18 bytes) */
typedef struct LaneRow {
  Vec3 a; /* 0x00 */
  Vec3 b; /* 0x0C */
} LaneRow;
HW_SIZE(LaneRow, 0x18);

/* the original's frame (0xAC bytes of locals, then the saved r8 and PR): the vectors the vector helpers and the
 * Ninja calls work on, and the look ahead's result, at the original's places */
typedef struct DriveFrame {
  union {
    struct {               /* the pending marker's test */
      uint8_t pad_00[0x08];
      Vec3 position;       /* 0x08: the racer's */
      Vec3 to_marker;      /* 0x14: the marker's position, then the unit direction to it */
      uint8_t pad_20[0x04];
      Vec3 course_dir;     /* 0x24: the course's direction at the racer's cell */
    } pending;
    struct {               /* the paired racer's test */
      uint8_t pad_00[0x04];
      Vec3 forward;        /* 0x04: the racer's z axis */
      Vec3 direction;      /* 0x10: the scene's point for the pair (RaceScene.points) */
    } rival;
  };
  uint8_t pad_30[0x3C - 0x30];
  LaneLook look;           /* 0x3C */
  Vec3 aim;                /* 0x4C */
  Vec3 line;               /* 0x58: the racing line's point on the row ahead */
  Vec3 row_b;              /* 0x64 */
  Vec3 row_a;              /* 0x70 */
  uint8_t pad_7c[0x84 - 0x7C];
  Vec3 position;           /* 0x84: the racer's */
  uint8_t pad_90[0xB4 - 0x90]; /* the flags, `this`, then r8 and PR */
} DriveFrame;
HW_OFFSET(DriveFrame, pending.course_dir, 0x24);
HW_OFFSET(DriveFrame, rival.direction, 0x10);
HW_OFFSET(DriveFrame, look, 0x3C);
HW_OFFSET(DriveFrame, aim, 0x4C);
HW_OFFSET(DriveFrame, position, 0x84);
HW_SIZE(DriveFrame, 0xB4);

/* the buttons decided this update */
typedef struct DriveButtons {
  int hold0, hold1;
} DriveButtons;

/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* the point across a row at `side` (0: its first vertex, 1: its second) into `out`, through the frame's row_a /
 * row_b */
static void row_point(gaddr frame, const LaneRow *row, f32 side, gaddr out) {
  DriveFrame *f = (DriveFrame *)hw_ptr(frame);
  f->row_a = row->a;
  f->row_b = row->b;
  *vec3_at(out) = f->row_b;
  vec3_sub(out, member_addr(frame, DriveFrame, row_a));
  vec3_scale(side, out);
  vec3_add(out, member_addr(frame, DriveFrame, row_a));
}

/* the aim: the racing line's point on the row ahead, pulled toward the next row's (none after the lane's last but
 * one quad); a marker the look ahead found (by its kind: its position, or (a, y, b)); while a pending kind 3 marker
 * is out of its range and ahead, that marker (once in range or behind, it is dropped) */
static void drive_aim(gaddr self, gaddr frame) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  DriveFrame *f = (DriveFrame *)hw_ptr(frame);
  gaddr aim = member_addr(frame, DriveFrame, aim);
  gaddr line = member_addr(frame, DriveFrame, line);

  f->position = r->body.body.position;
  f32 side = LINE_SIDE;
  if (l->pending_kind == PENDING_SIDE)
    side = l->target_x;
  const LaneRow *rows = (const LaneRow *)hw_ptr(l->lane_points);
  row_point(frame, &rows[l->quad + 1], side, line);
  method_self(aim); /* Vec3's empty constructor */
  if (l->quad < l->lane_last[l->lane] - 1) {
    row_point(frame, &rows[l->quad + 2], side, aim);
    vec3_sub(aim, line);
    vec3_scale(NEXT_ROW_PULL, aim);
    vec3_add(aim, line);
  } else {
    f->aim = f->line;
  }

  if (lane_racer_look_ahead(self, member_addr(frame, DriveFrame, look))) {
    const LaneMarker *m = (const LaneMarker *)hw_ptr(f->look.marker);
    switch (m->kind) {
    case 0: case 1: case 2: case 9: case 14:
      f->aim = m->position;
      break;
    case 6:
      f->aim.x = m->a;
      f->aim.y = m->position.y;
      f->aim.z = m->b;
      break;
    case 13:
      if (f->look.closer) {
        f->aim.x = m->a;
        f->aim.y = m->position.y;
        f->aim.z = m->b;
      } else {
        f->aim = m->position;
      }
      break;
    default:
      break;
    }
  }

  /* (the original also tests pending_kind against -1 first) */
  if (l->pending_kind != PENDING_NONE && l->pending_kind == PENDING_AIM) {
    gaddr course_dir = member_addr(frame, DriveFrame, pending.course_dir);
    gaddr position = member_addr(frame, DriveFrame, pending.position);
    gaddr to_marker = member_addr(frame, DriveFrame, pending.to_marker);
    vec3_copy(course_dir, race_scene_cell_point(r->scene, (int32_t)racer_cell(self)));
    const LaneMarker *m = (const LaneMarker *)hw_ptr(l->pending_marker);
    method_self(to_marker); /* Vec3's empty constructor */
    vec3_set(m->position.x, m->position.y, m->position.z, to_marker);
    vec3_copy(position, method_self(member_addr(self, Racer, body)));
    f32 distance = vec3_distance(position, to_marker);
    if (fgt(distance, m->range)) {
      vec3_sub(to_marker, position);
      njUnitVector(to_marker);
      if (fgt(njInnerProduct(to_marker, course_dir), AHEAD_ENOUGH)) {
        f->aim = m->position;
        return;
      }
    }
    l->pending_kind = PENDING_NONE;
    l->pending_level = -1;
  }
}

/* the stick: sideways toward the aim (by how far off the heading it is), forward (backward when turned round) */
static void drive_stick(gaddr self, gaddr frame) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  const DriveFrame *f = (const DriveFrame *)hw_ptr(frame);
  /* the original's flags: steering and throttle are always on, the full reverse always off (dead tests kept) */
  const int steer = 1, throttle = 1, full_reverse = 0;

  r->stick_y = F32(0.0f);
  if (steer) {
    f32 dx = fsub(f->aim.x, f->position.x);
    f32 dz = fsub(f->aim.z, f->position.z);
    f32 angle = f32_from_int(f32_to_int(fdiv(fmul(rt_atan2f(dx, dz), TURN_UNITS), TWO_PI)));
    if (l->manoeuvre_step == PACE_TURNED)
      angle = fadd(angle, HALF_TURN);
    f32 heading = field_get_f32(member_addr(self, Racer, heading), 0); /* Body1's value */
    f32 off = f32_from_int((int16_t)f32_to_int(fsub(heading, angle)));
    f32 amount = F32(0.0f);
    f32 size = f32_abs(off);
    if (fgt(size, FAR_OFF))
      amount = STICK_FULL;
    else if (fgt(size, OFF))
      amount = STICK_HALF;
    else
      amount = STICK_LIGHT;
    if (fgt(F32(0.0f), off))
      amount = fneg(amount);
    r->stick_y = amount;
  }
  if (throttle)
    r->stick_x = l->manoeuvre_step == PACE_TURNED ? fneg(F32(1.0f)) : F32(1.0f);
  else
    r->stick_x = F32(0.0f);
  if (full_reverse)
    r->stick_x = fneg(F32(1.0f));
}

/* button 0 while stroke_frames last: held, let go one update when stroke_clock passes 60 */
static void drive_strokes(LaneRacer *l, DriveButtons *buttons) {
  l->stroke_frames--;
  buttons->hold0 = 1;
  l->stroke_clock = fadd(l->stroke_clock, F32(1.0f));
  if (fgt(l->stroke_clock, STROKE_CLOCK_MAX)) {
    buttons->hold0 = 0;
    l->stroke_clock = F32(0.0f);
  }
}

/* what asks for strokes or button 1 (when no strokes are running): the look ahead's flags, mark_b_on, and the paired
 * racer close, level and straight ahead */
static void drive_buttons(gaddr self, gaddr frame, DriveButtons *buttons) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  DriveFrame *f = (DriveFrame *)hw_ptr(frame);
  if (f->look.flag1 && r->ground != 0)
    l->stroke_frames = STROKE_UPDATES;
  if (r->mark_b_on)
    l->stroke_frames = STROKE_UPDATES;
  if (f->look.flag2)
    buttons->hold1 = 1;

  const RaceScene *rs = race_scene_at(r->scene);
  int32_t rival = rs->player_pairs[r->index][0];
  if (!fgt(RIVAL_NEAR, rs->values[r->index][rival]))
    return;
  f32 rival_y = vec3_at(race_player_body(r->scene, rival))->y;
  if (!fgt(RIVAL_LEVEL, f32_abs(fsub(f->position.y, rival_y))))
    return;
  gaddr direction = member_addr(frame, DriveFrame, rival.direction);
  gaddr forward = member_addr(frame, DriveFrame, rival.forward);
  *vec3_at(direction) = rs->points[r->index][rival];
  vec3_copy(forward, racer_matrix_axis_z(self));
  if (fgt(njInnerProduct(forward, direction), RIVAL_AHEAD))
    buttons->hold1 = 1;
}

/* every 130 updates while mark_b_on: under 12 units (|dx| + |dz|) since the last check, work loose */
static void drive_stuck_check(gaddr self, gaddr frame) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  const DriveFrame *f = (const DriveFrame *)hw_ptr(frame);
  l->stuck_frames++;
  if (r->mark_b_on == 0 || l->stuck_frames <= STUCK_CHECK)
    return;
  l->stuck_frames = 0;
  f32 moved = f32_abs(fsub(f->position.x, l->reset_position.x));
  moved = fadd(moved, f32_abs(fsub(f->position.z, l->reset_position.z)));
  l->reset_position = f->position;
  if (fgt(STUCK_DISTANCE, moved)) {
    l->manoeuvre = MANOEUVRE_WORK_LOOSE;
    l->manoeuvre_step = 0;
    l->manoeuvre_frames = 0;
  }
}

/* character 7 against player 0 (lead: its cell minus player 0's) */
static void drive_pace(gaddr self, int32_t lead) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  switch (l->manoeuvre_step) {
  case PACE_WATCH:
    if (++l->manoeuvre_frames <= PACE_WAIT)
      break;
    if (lead >= PACE_AHEAD) {
      l->manoeuvre = MANOEUVRE_HOLD_BACK;
      l->manoeuvre_step = f32_to_int(fmul(racer_x_fraction(self), PACE_HOLD_KINDS));
      l->manoeuvre_frames = 0;
    } else if (lead <= PACE_FAR_BEHIND) {
      l->manoeuvre_step = PACE_DASH_STRAIGHT;
    } else if (lead <= PACE_BEHIND) {
      if (fgt(PACE_TURN_CHANCE, racer_x_fraction(self)))
        l->manoeuvre_step = PACE_TURNED;
      else
        l->manoeuvre_step = PACE_DASH_SOON;
      l->manoeuvre_frames = 0;
    }
    break;
  case PACE_TURNED:
    l->manoeuvre_frames++;
    if (r->action == ACTION_DASH || lead > PACE_TURN_LEAD || l->manoeuvre_frames > PACE_TURN_MAX) {
      l->manoeuvre_frames = 0;
      l->manoeuvre_step = PACE_WATCH;
    }
    break;
  case PACE_DASH_SOON:
    l->manoeuvre_frames++;
    if (l->manoeuvre_frames == PACE_STROKE_AT)
      l->stroke_frames = PACE_STROKES;
    if (l->manoeuvre_frames == PACE_DASH_AT) {
      racer_set_action(self, ACTION_DASH);
      l->manoeuvre_frames = 0;
      l->manoeuvre_step = PACE_WATCH;
    }
    break;
  case PACE_DASH_STRAIGHT:
    if (fgt(f32_abs(r->stick_y), PACE_STRAIGHT))
      break;
    racer_set_action(self, ACTION_DASH);
    l->manoeuvre_frames = 0;
    l->manoeuvre_step = PACE_WATCH;
    break;
  default:
    break;
  }
}

/* manoeuvre 0: racing */
static void drive_race(gaddr self, DriveButtons *buttons) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  gaddr frame = hw_stack_alloc(sizeof(DriveFrame)); /* the original's frame, at the same place */
  drive_aim(self, frame);
  drive_stick(self, frame);
  if (l->stroke_frames != 0)
    drive_strokes(l, buttons);
  else
    drive_buttons(self, frame, buttons);
  drive_stuck_check(self, frame);
  hw_stack_free(sizeof(DriveFrame));

  uint32_t player0_cell = race_player_cell(r->scene, 0);
  int32_t lead = (int32_t)(racer_cell(self) - player0_cell);
  if (r->character == CHARACTER_PACER)
    drive_pace(self, lead);
}

/* manoeuvre 1: working loose: back 60 updates, forward 40, forward with button 0 for 100, then racing again */
static void drive_work_loose(gaddr self, DriveButtons *buttons) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  l->manoeuvre_frames++;
  r->stick_y = F32(0.0f);
  switch (l->manoeuvre_step) {
  case 0:
    r->stick_x = LOOSE_BACK;
    if (l->manoeuvre_frames > LOOSE_BACK_FRAMES) {
      l->manoeuvre_frames = 0;
      l->manoeuvre_step++;
    }
    break;
  case 1:
    r->stick_x = F32(1.0f);
    if (l->manoeuvre_frames > LOOSE_GO_FRAMES) {
      l->manoeuvre_frames = 0;
      l->manoeuvre_step++;
    }
    break;
  case 2:
    r->stick_x = F32(1.0f);
    buttons->hold0 = 1;
    if (l->manoeuvre_frames > LOOSE_STROKE_FRAMES) {
      l->manoeuvre_frames = 0;
      l->manoeuvre_step = 0;
      l->manoeuvre = MANOEUVRE_RACING;
    }
    break;
  default:
    break;
  }
}

/* manoeuvre 2 (character 7): holding back by its step (stopped, backing off, gliding with strokes) until player 0
 * is level or ahead, or 600 updates pass; then racing again, with a dash half of the time */
static void drive_hold_back(gaddr self, DriveButtons *buttons) {
  Racer *r = racer_at(self);
  LaneRacer *l = lane_racer_at(self);
  switch (l->manoeuvre_step) {
  case 0:
    r->stick_y = F32(0.0f);
    r->stick_x = F32(0.0f);
    break;
  case 1:
    r->stick_y = HOLD_BACK_OFF;
    r->stick_x = HOLD_BACK_OFF;
    break;
  case 2:
    r->stick_y = HOLD_GLIDE;
    r->stick_x = F32(0.0f);
    buttons->hold0 = 1;
    l->stroke_clock = fadd(l->stroke_clock, F32(1.0f));
    if (fgt(l->stroke_clock, STROKE_CLOCK_MAX)) {
      buttons->hold0 = 0;
      l->stroke_clock = F32(0.0f);
    }
    break;
  default:
    break;
  }
  l->manoeuvre_frames++;
  uint32_t cell = racer_cell(self);
  int32_t lead = (int32_t)(cell - race_player_cell(r->scene, 0));
  if (lead > 0 && l->manoeuvre_frames <= HOLD_MAX)
    return;
  l->manoeuvre = MANOEUVRE_RACING;
  l->manoeuvre_step = 0;
  if (fgt(HOLD_DASH_CHANCE, racer_x_fraction(self)))
    racer_set_action(self, ACTION_DASH);
}

/* was func_8C03E150: the computer player's stick and buttons in racer state 3 (see the top of the file) */
void lane_racer_drive(gaddr self) {
  Racer *r = racer_at(self);
  DriveButtons buttons = {0, 0};
  switch (lane_racer_at(self)->manoeuvre) {
  case MANOEUVRE_RACING:
    drive_race(self, &buttons);
    break;
  case MANOEUVRE_WORK_LOOSE:
    drive_work_loose(self, &buttons);
    break;
  case MANOEUVRE_HOLD_BACK:
    drive_hold_back(self, &buttons);
    break;
  default:
    break;
  }
  if (buttons.hold0) {
    r->button[0] = 1;
    r->button_lock[0] = 0;
  } else {
    r->button[0] = 0;
  }
  if (buttons.hold1) {
    r->button[1] = 1;
    r->button_lock[1] = 0;
  } else {
    r->button[1] = 0;
  }
}
HW_EXPORT(8c03e150, lane_racer_drive(ARG0))
