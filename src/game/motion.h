/* Pen Pen port, S13/S14: the motion player of the animated models (game/model.c, game/racer.c).
 *
 * A penguin's model and its motion player (constructor func_8C037D24, at least 0xD8 bytes: the racer has one at its
 * 0x24, the menus their own) plays motion `index` of its motion table from a request {next request, start frame,
 * ...}; when it switches motion it keeps the previous one and its frame to blend from. Its model, motions and
 * texture list come from a model file it loads (or from a container it is given). */
#ifndef PENPEN_MOTION_H
#define PENPEN_MOTION_H
#include "../core/vec.h"

typedef struct MotionRequest {
  gaddr next;          /* 0x00: the request that follows (0 = none: no blend) */
  f32 blend_speed;     /* 0x04 */
  f32 start;           /* 0x08: the first frame; below 0, counted from the motion's end */
  uint32_t unknown0c;  /* 0x0C */
  int32_t index;       /* 0x10: the motion */
  uint32_t flags;      /* 0x14 */
} MotionRequest;
HW_OFFSET(MotionRequest, index, 0x10);
HW_OFFSET(MotionRequest, flags, 0x14);

/* an entry of a motion table: the NJS_MOTION and a word */
typedef struct MotionEntry {
  gaddr motion;        /* 0x00: NJS_MOTION (core/file.h: NjMotion) */
  uint32_t word4;      /* 0x04 */
} MotionEntry;
HW_SIZE(MotionEntry, 8);

typedef struct MotionPlayer {
  uint32_t unknown00;  /* 0x00 */
  gaddr next;          /* 0x04: the request's follower */
  f32 blend;           /* 0x08: 0 = the previous motion, 1 = this one */
  f32 blend_speed;     /* 0x0C */
  uint8_t unknown10[0x18 - 0x10];
  gaddr motion_table;  /* 0x18: the motion descriptions (set at construction) */
  int32_t motion_count; /* 0x1C */
  gaddr request;       /* 0x20: MotionRequest */
  uint32_t flags;      /* 0x24 */
  uint32_t unknown28;  /* 0x28 */
  int32_t index;       /* 0x2C: the motion playing */
  f32 frame;           /* 0x30 */
  int32_t prev_index;  /* 0x34: the motion blended from */
  f32 prev_frame;      /* 0x38 */
  f32 speed;           /* 0x3C: 1 at construction */
  gaddr file;          /* 0x40: the model file (container) it loaded, or 0; freed by func_8C037F44 */
  gaddr model;         /* 0x44: the file's first entry */
  gaddr motions;       /* 0x48: MotionEntry per motion (the file's second entry) */
  gaddr texlist;       /* 0x4C: the file's third entry */
} MotionPlayer;
HW_OFFSET(MotionPlayer, next, 0x04);
HW_OFFSET(MotionPlayer, motion_table, 0x18);
HW_OFFSET(MotionPlayer, request, 0x20);
HW_OFFSET(MotionPlayer, index, 0x2C);
HW_OFFSET(MotionPlayer, prev_frame, 0x38);
HW_OFFSET(MotionPlayer, file, 0x40);
HW_OFFSET(MotionPlayer, motions, 0x48);
HW_OFFSET(MotionPlayer, texlist, 0x4C);

static inline MotionPlayer *motion_player_at(gaddr a) { return (MotionPlayer *)hw_ptr(a); }

#endif
