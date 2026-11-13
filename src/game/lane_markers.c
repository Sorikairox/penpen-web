/* Pen Pen port, S16 (b05): the computer player looks ahead along its lane for markers (func_8C03FC68).
 *
 * Each quad of a lane may hold a list of LaneMarkers (Course data: a kind, two values, a position and a range). The
 * computer player (LaneRacer, game/racer.h) looks at the quads from a few ahead of its own (more the faster it
 * goes: 2 + f434 * 0.3) back to its own, and picks the marker that matters most: each kind has a priority and a
 * test (how far the marker is against its range, whether it is in front of the racer along the course's direction
 * at its cell, which side of the lane the racer is on, ...). The marker found goes to the caller's LaneLook; some
 * kinds set one of its flags, some (3, 4, 11, 12, 18: actions on the racer's own quad) become the racer's pending
 * marker instead of a plain find. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"   /* racer_cell, race_scene_cell_point */
#include "race_scene.h"       /* race_setup_get, RaceSetup */
#include "racer.h"
#include "lane_markers.h"   /* LaneMarker, LaneLook (S17: shared with lane_racer_steer.c) */


#define LOOK_AHEAD_RATE F32_BITS(0x3E999999u) /* 0.29999998: quads looked ahead per unit of f434 */
#define AHEAD_ENOUGH    F32(0.25f)            /* the marker is in front: dot(direction to it, course direction) */
#define FAST_ENOUGH     F32_BITS(0x3FED097Au) /* 1.8518518: f434 above it for kinds 18 and 19 */
#define NO_MARKER       (-1)

/* the original's frame (0x68 bytes under PR): the Vec3s handed to njUnitVector / njInnerProduct and the helpers */
typedef struct LookFrame {
  uint8_t pad_00[0x04]; /* 0x00: kind 13's distance */
  Vec3 point;           /* 0x04: kind 13's point */
  uint8_t pad_10[0x04]; /* 0x10: the dot product */
  Vec3 direction;       /* 0x14: from the racer to the marker, made unit */
  uint8_t pad_20[0x04]; /* 0x20: the distance */
  Vec3 position;        /* 0x24: the marker's position */
  uint8_t pad_30[0x10]; /* 0x30: the marker, the counter */
  Vec3 course_dir;      /* 0x40: the course's direction at the racer's cell */
  uint8_t rest[0x6C - 0x4C]; /* 0x4C: best, the row, the quads, the result, the arguments, PR */
} LookFrame;
HW_OFFSET(LookFrame, point, 0x04);
HW_OFFSET(LookFrame, direction, 0x14);
HW_OFFSET(LookFrame, position, 0x24);
HW_OFFSET(LookFrame, course_dir, 0x40);
HW_SIZE(LookFrame, 0x6C);

/* the racer becomes busy with marker m (kinds 3, 4, 11, 12, 18): it is the result too, at that priority */
static void lane_racer_take_marker(LaneRacer *lr, LaneLook *look, gaddr m, int32_t kind, int32_t level) {
  lr->pending_kind = kind;
  lr->pending_marker = m;
  look->marker = m;
  lr->pending_level = level;
}

/* was func_8C03FC68: look ahead along the lane for the marker that matters most; 1 when one was found (some kinds
 * only make it pending: see the top of the file) */
int32_t lane_racer_look_ahead(gaddr racer, gaddr look_at) {
  Racer *r = racer_at(racer);
  LaneRacer *lr = lane_racer_at(racer);
  LaneLook *look = (LaneLook *)hw_ptr(look_at);
  gaddr frame = hw_stack_alloc(sizeof(LookFrame));
  gaddr position = member_addr(frame, LookFrame, position);
  gaddr direction = member_addr(frame, LookFrame, direction);
  gaddr course_dir = member_addr(frame, LookFrame, course_dir);
  gaddr body = member_addr(racer, Racer, body);

  int32_t found = 0;
  look->flag1 = 0;
  look->flag2 = 0;
  int32_t ahead = f32_to_int(fmul(r->f434, LOOK_AHEAD_RATE)) + 2;
  int32_t quad = lr->quad + ahead;
  if (quad > lr->lane_last[lr->lane] - 1)
    quad = lr->lane_last[lr->lane];
  gaddr row = lr->lane_markers[lr->lane][lr->branch] + (uint32_t)quad * sizeof(gaddr);
  int32_t best = NO_MARKER;
  vec3_copy(course_dir, race_scene_cell_point(r->scene, (int32_t)racer_cell(racer)));

  for (int32_t j = 0; j < ahead + 1; j++, quad--) {
    gaddr m = *(const gaddr *)hw_ptr(row);
    row -= sizeof(gaddr);
    if (m == 0)
      continue;
    for (; ((const LaneMarker *)hw_ptr(m))->kind != -1; m += sizeof(LaneMarker)) {
      const LaneMarker *mk = (const LaneMarker *)hw_ptr(m);
      vec3_set(mk->position.x, mk->position.y, mk->position.z, position);
      f32 distance = vec3_distance(body, position);
      vec3_copy(direction, position);
      vec3_sub(direction, body);
      njUnitVector(direction);
      f32 dot = njInnerProduct(direction, course_dir);
      switch ((uint32_t)mk->kind) {
      case 0: { /* priority a: beyond its range and in front */
        int32_t level = f32_to_int(mk->a);
        if (best > level) break;
        if (!fgt(distance, mk->range)) break;
        if (!fgt(dot, AHEAD_ENOUGH)) break;
        look->marker = m;
        found = 1;
        best = level;
        break;
      }
      case 1: /* in front: found at 1; flag1 when within range */
        if (best > 1) break;
        if (!fgt(dot, AHEAD_ENOUGH)) break;
        look->marker = m;
        found = 1;
        best = 1;
        if (fgt(mk->range, distance))
          look->flag1 = 1;
        break;
      case 2: /* on ground 3 or 4, beyond its range and in front */
        if (best > 0) break;
        if (r->ground != 3 && r->ground != 4) break;
        if (!fgt(distance, mk->range)) break;
        if (!fgt(dot, AHEAD_ENOUGH)) break;
        look->marker = m;
        found = 1;
        best = 0;
        break;
      case 3: /* on another quad, beyond range, in front, the racer at most at side a: pending */
        if (best > 1) break;
        if (lr->pending_kind != -1) break;
        if (quad == lr->quad) break;
        if (!fgt(distance, mk->range)) break;
        if (!fgt(dot, AHEAD_ENOUGH)) break;
        if (fgt(racer_x_fraction(racer), mk->a)) break;
        lane_racer_take_marker(lr, look, m, 3, 1);
        best = 1;
        break;
      case 4: /* on the racer's quad: pending, steering to side b (racer past the middle) or a */
        if (best > 2) break;
        if (lr->pending_kind != -1) break;
        if (quad != lr->quad) break;
        if (fgt(racer_x_fraction(racer), F32(0.5f)))
          lr->target_x = mk->b;
        else
          lr->target_x = mk->a;
        lane_racer_take_marker(lr, look, m, 4, 2);
        best = 2;
        break;
      case 5: /* on the racer's quad: the pending marker is done */
        if (quad != lr->quad) break;
        lr->pending_kind = -1;
        lr->pending_level = -1;
        break;
      case 6: /* within range */
        if (best > 2) break;
        if (!fgt(mk->range, distance)) break;
        look->marker = m;
        best = 2;
        found = 1;
        break;
      case 7: /* within range: flag1 */
        if (best > 1) break;
        if (!fgt(mk->range, distance)) break;
        look->marker = m;
        look->flag1 = 1;
        best = 1;
        found = 1;
        break;
      case 8: /* within range, the racer at most at side a: flag2 */
        if (best > 1) break;
        if (!fgt(mk->range, distance)) break;
        if (fgt(racer_x_fraction(racer), mk->a)) break;
        look->marker = m;
        look->flag2 = 1;
        best = 1;
        found = 1;
        break;
      case 9: /* beyond range and in front */
        if (best > 2) break;
        if (!fgt(distance, mk->range)) break;
        if (!fgt(dot, AHEAD_ENOUGH)) break;
        look->marker = m;
        found = 1;
        best = 2;
        break;
      case 10: { /* priority a, for the range_index b allows (b > 0: above b; b < 0: below -b; 0: all), ahead */
        int32_t level = f32_to_int(mk->a);
        int32_t allowed = 1;
        int32_t limit = f32_to_int(mk->b);
        if (limit != 0) {
          if (limit > 0) {
            if (!(limit > lr->range_index))
              allowed = 0;
          } else {
            if (-limit >= lr->range_index)
              allowed = 0;
          }
        }
        if (!allowed) break;
        if (best > level) break;
        if (!fgt(dot, F32(0.0f))) break;
        look->marker = m;
        found = 1;
        best = level;
        break;
      }
      case 11: /* on the racer's quad: pending */
      case 12:
        if (best > 2) break;
        if (lr->pending_kind != -1) break;
        if (quad != lr->quad) break;
        lane_racer_take_marker(lr, look, m, mk->kind, 2);
        best = 2;
        break;
      case 13: { /* ahead: the nearer of the position and the point (a, position.y, b) against the range */
        if (best > 2) break;
        if (!fgt(dot, F32(0.0f))) break;
        gaddr point = member_addr(frame, LookFrame, point);
        vec3_set(mk->a, mk->position.y, mk->b, point);
        f32 to_point = vec3_distance(point, body);
        if (fgt(to_point, distance)) {
          look->closer = 0;
        } else {
          distance = to_point;
          look->closer = 1;
        }
        if (!fgt(distance, mk->range)) break;
        look->marker = m;
        found = 1;
        best = 2;
        break;
      }
      case 14: /* ahead and the racer above the marker */
        if (best > 4) break;
        if (!fgt(dot, F32(0.0f))) break;
        if (!fgt(frame_body_at(body)->body.position.y, mk->position.y)) break;
        look->marker = m;
        found = 1;
        best = 4;
        break;
      case 15: { /* beyond range, ahead, for the racers of one parity: a == 0 the even ones, else the odd ones */
        if (best > 2) break;
        if (!fgt(distance, mk->range)) break;
        if (!fgt(dot, F32(0.0f))) break;
        int32_t mine = 0;
        if (feq(mk->a, F32(0.0f))) {
          if (!(r->index & 1))
            mine = 1;
        } else {
          if (r->index & 1)
            mine = 1;
        }
        if (!mine) break;
        look->marker = m;
        found = 1;
        best = 2;
        break;
      }
      case 16: /* within range, at any priority */
        if (!fgt(mk->range, distance)) break;
        look->marker = m;
        found = 1;
        best = 2;
        break;
      case 17: /* with the set-up's variant above 0: within range and ahead: flag2 */
        if (!(race_setup_at(race_setup_get())->variant > 0)) break;
        if (best > 2) break;
        if (!fgt(mk->range, distance)) break;
        if (!fgt(dot, F32(0.0f))) break;
        look->marker = m;
        found = 1;
        look->flag2 = 1;
        best = 2;
        break;
      case 18: /* fast, on the racer's quad: pending */
        if (best > 2) break;
        if (!fgt(r->f434, FAST_ENOUGH)) break;
        if (lr->pending_kind != -1) break;
        if (quad != lr->quad) break;
        lane_racer_take_marker(lr, look, m, 18, 2);
        best = 2;
        break;
      case 19: /* on the racer's quad and fast: flag2 (the priority stays) */
        if (quad != lr->quad) break;
        if (!fgt(r->f434, FAST_ENOUGH)) break;
        look->marker = m;
        found = 1;
        look->flag2 = 1;
        break;
      case 20: /* on the racer's quad, the racer below the marker: flag2 */
        if (best > 3) break;
        if (quad != lr->quad) break;
        if (!fgt(mk->position.y, frame_body_at(body)->body.position.y)) break;
        look->marker = m;
        look->flag2 = 1;
        best = 3;
        found = 1;
        break;
      default:
        break;
      }
    }
  }
  hw_stack_free(sizeof(LookFrame));
  return found;
}
HW_EXPORT(8c03fc68, RET(lane_racer_look_ahead(ARG0, ARG1)))
