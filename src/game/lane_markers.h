/* Pen Pen port, S17 r1: the lane markers' types (moved out of game/lane_markers.c, S16 b05, which declares the same
 * two structs: the lead merges them so that lane_markers.c includes this header). */
#ifndef PENPEN_LANE_MARKERS_H
#define PENPEN_LANE_MARKERS_H

#include "../core/vec.h"

/* a marker of a lane quad (0x1C bytes; a list ends with kind -1) */
typedef struct LaneMarker {
  int32_t kind;   /* 0x00: 0..20; -1 ends the list */
  f32 a;          /* 0x04: a value (a priority, a side of the lane, an x, ... by kind) */
  f32 b;          /* 0x08: a second one */
  Vec3 position;  /* 0x0C */
  f32 range;      /* 0x18: a distance the racer's is compared with */
} LaneMarker;
HW_OFFSET(LaneMarker, position, 0x0C);
HW_SIZE(LaneMarker, 0x1C);

/* what the look ahead found (its caller's local) */
typedef struct LaneLook {
  gaddr marker;   /* 0x00: the LaneMarker */
  int32_t flag1;  /* 0x04: cleared first; kinds 1 and 7 set it */
  int32_t flag2;  /* 0x08: cleared first; kinds 8, 17, 19, 20 set it */
  int32_t closer; /* 0x0C: kind 13: 1 when the point (a, position.y, b) is nearer than the position */
} LaneLook;
HW_OFFSET(LaneLook, closer, 0x0C);
HW_SIZE(LaneLook, 0x10);

/* marker kinds the steering reads (lane_racer_steer.c) */
#define LANE_MARKER_AIM_0    0   /* kinds 0, 9, 10, 15: aim at the marker's position */
#define LANE_MARKER_AIM_9    9
#define LANE_MARKER_AIM_10   10
#define LANE_MARKER_AIM_15   15
#define LANE_MARKER_REJOIN   16  /* leave the side way for the main way */
#define LANE_PENDING_SIDE    4   /* pending kinds (LaneRacer.pending_kind) */
#define LANE_PENDING_STROKE2 11
#define LANE_PENDING_GLIDE   12
#define LANE_PENDING_COAST   18

/* game/lane_markers.c (S16 b05) */
int32_t lane_racer_look_ahead(gaddr racer, gaddr look);

#endif
