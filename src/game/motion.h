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
  f32 speed;           /* 0x0C: b07: frames per update (times the player's speed) */
  int32_t index;       /* 0x10: the motion */
  uint32_t flags;      /* 0x14 */
} MotionRequest;
HW_OFFSET(MotionRequest, index, 0x10);
HW_OFFSET(MotionRequest, flags, 0x14);
HW_SIZE(MotionRequest, 0x18); /* b07: a motion set is an array of them (flags 0 / 1 go on to the next) */

/* an entry of a motion table: the NJS_MOTION and a word */
typedef struct MotionEntry {
  gaddr motion;        /* 0x00: NJS_MOTION (core/file.h: NjMotion) */
  uint32_t word4;      /* 0x04: S15 b06: the shape motion drawn with it (motion_player_draw) */
} MotionEntry;
HW_SIZE(MotionEntry, 8);

typedef struct MotionPlayer {
  uint32_t unknown00;  /* 0x00 */
  gaddr next;          /* 0x04: the request's follower */
  f32 blend;           /* 0x08: 0 = the previous motion, 1 = this one */
  f32 blend_speed;     /* 0x0C */
  int32_t loop;        /* 0x10: S15 b06: the loop flag the draw passes to the motion drawers (core/motion.c) */
  int32_t word14;      /* 0x14: b07: 1 at construction */
  gaddr motion_table;  /* 0x18: the motion descriptions (set at construction); b07: a MotionRequest set per entry */
  int32_t motion_count; /* 0x1C: b07: the set being played (motion_player_play) */
  gaddr request;       /* 0x20: MotionRequest */
  int32_t flags;       /* 0x24: b07: the request's: -1 loop, -2 hold the end, -3 hold then replay the set, 0 the next
                        * request at once, 1 the next one at the end, n > 1 play n more times */
  int32_t ended;       /* 0x28: b07: the motion reached its end this update */
  int32_t index;       /* 0x2C: the motion playing */
  f32 frame;           /* 0x30 */
  int32_t prev_index;  /* 0x34: the motion blended from */
  f32 prev_frame;      /* 0x38 */
  f32 speed;           /* 0x3C: 1 at construction */
  gaddr file;          /* 0x40: the model file (container) it loaded, or 0; freed by func_8C037F44 */
  gaddr model;         /* 0x44: the file's first entry */
  gaddr motions;       /* 0x48: MotionEntry per motion (the file's second entry) */
  gaddr texlist;       /* 0x4C: the file's third entry (S15 b06: the draw hands it to its node callback as four
                        * node pointers, the nodes the parts are attached to) */
  int32_t attached[4]; /* 0x50: S15 b06: the draw clears them, the draw's node callback sets one to 1 when it
                        * passes the matching attach node (the parts are drawn at the matrices it saved) */
  int32_t looks[4];    /* 0x60: b02: four style numbers by the character (menu_penguin_set_looks: from the table at
                        * 0x8C085514 by the save's byte for it; 0, 0, 1, 1 for the other variant) */
  f32 timers[4];       /* 0x70: b07: count up by 1 every update (the parts' frames: motion_player_frame) */
  int32_t character;   /* 0x80: b02: which penguin (the constructor's second argument) */
  int32_t variant;     /* 0x84: b02: which of its two model files (the third) */
  uint8_t unknown88[0xD4 - 0x88];
  int32_t draw_l2;     /* 0xD4: b07: 0 at construction (the draw picks its Ninja calls by it) */
} MotionPlayer;
HW_OFFSET(MotionPlayer, next, 0x04);
HW_OFFSET(MotionPlayer, motion_table, 0x18);
HW_OFFSET(MotionPlayer, request, 0x20);
HW_OFFSET(MotionPlayer, index, 0x2C);
HW_OFFSET(MotionPlayer, prev_frame, 0x38);
HW_OFFSET(MotionPlayer, file, 0x40);
HW_OFFSET(MotionPlayer, motions, 0x48);
HW_OFFSET(MotionPlayer, texlist, 0x4C);
HW_OFFSET(MotionPlayer, looks, 0x60);
HW_OFFSET(MotionPlayer, character, 0x80);
HW_OFFSET(MotionPlayer, variant, 0x84);
HW_OFFSET(MotionPlayer, word14, 0x14);
HW_OFFSET(MotionPlayer, ended, 0x28);
HW_OFFSET(MotionPlayer, timers, 0x70);
HW_OFFSET(MotionPlayer, draw_l2, 0xD4);
HW_OFFSET(MotionPlayer, loop, 0x10);
HW_OFFSET(MotionPlayer, attached, 0x50);

static inline MotionPlayer *motion_player_at(gaddr a) { return (MotionPlayer *)hw_ptr(a); }

/* game/racer.c */
void motion_player_free(gaddr player);

/* game/model.c */
gaddr motion_player_delete(gaddr player, int32_t free_memory);

/* b02, game/menu_penguin.c: the menus' penguins (a MotionPlayer of 0xF8 bytes whose first word points at its 0xDC) */
gaddr menu_penguin_construct(gaddr penguin, int32_t character, int32_t variant, gaddr container, uint32_t arg5);
void menu_penguin_set_looks(gaddr penguin);
void menu_penguin_reset(gaddr penguin, int32_t waving);

/* game/menu_penguin.c: calls of the still-translated frame */
void motion_player_update(gaddr player);
/* S15 b06, game/motion_draw.c (motion_player_draw was menu_penguin.c's call of the translated draw) */
void motion_player_draw(gaddr player);
void motion_player_draw_extra(gaddr player);

/* b07, game/motion_player.c */
gaddr motion_player_build(gaddr player, int32_t character, int32_t variant, gaddr container, int32_t table);
void motion_player_play_set(gaddr player, int32_t set, f32 speed);
void motion_player_advance(gaddr player);

#endif
