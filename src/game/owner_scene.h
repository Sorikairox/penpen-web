/* Pen Pen port, S17 b04: the owner scene (game/owner_scene.c): the RaceSetup's third scene (RaceSetup.requested 2,
 * constructor func_8C02FC12 -> func_8C02F850, a Task; deleting destructor func_8C02FCD0). It loads 5110Owner.bin (a
 * container: the texture list, then per course an object and its motion pairs, 0x20 bytes apart) and 5111Owner.pvm,
 * shows the course's owner (a shape motion cycling through its motion pairs, scaled and placed by a Body) with two
 * or three penguins (motion players) and the course's music cue, until a button or the timeout, then asks the
 * set-up for its next scene. */
#ifndef PENPEN_OWNER_SCENE_H
#define PENPEN_OWNER_SCENE_H

#include "../core/task.h"
#include "../core/pad.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "../core/ninja.h"

#define OWNER_PENGUINS 3 /* motion players: 3, or 2 for variant 4 (penguin_count) */

typedef struct OwnerScene {
  Task task;                 /* 0x000 */
  Pad pad;                   /* 0x024 */
  gaddr setup;               /* 0x028: the RaceSetup (its `requested` is set at the end) */
  int32_t state;             /* 0x02C: 0 start, 1 shown (until a button or the timer), 2 leaving */
  int32_t timer;             /* 0x030: frames left in the state */
  int32_t substate;          /* 0x034: cleared with each new state (field_set_and_clear) */
  uint8_t pad_038[0x40 - 0x38];
  gaddr penguins[OWNER_PENGUINS]; /* 0x040: MotionPlayer (0 = none), penguin_count of them */
  int32_t penguin_character[OWNER_PENGUINS]; /* 0x04C: the RaceSetup's words68[slot]: the penguin's character */
  int32_t penguin_variant[OWNER_PENGUINS];   /* 0x058: the RaceSetup's words88[slot]: its model file */
  gaddr texlist64;           /* 0x064: the result screen's NJS_TEXLIST (6010result.pvm), released at the end */
  uint8_t pad_068[0x74 - 0x68];
  NjSprite sprite;           /* 0x074: a full-screen sprite of that texture list (scale 1, its own texture anim) */
  gaddr file;                /* 0x094: 5110Owner.bin */
  gaddr texlist;             /* 0x098: its texture list (the file's second word) */
  gaddr object;              /* 0x09C: the course's owner object */
  gaddr pairs;               /* 0x0A0: its shape motion pairs (OwnerPair) */
  int32_t pair;              /* 0x0A4: the pair playing */
  int32_t pair_prev;         /* 0x0A8: (= pair at the reset) */
  f32 frame;                 /* 0x0AC: its frame */
  uint8_t pad_0b0[0xB4 - 0xB0];
  int32_t played;            /* 0x0B4: 0 at the reset, 1 once a frame has run (the draw's loop flag) */
  f32 fb8;                   /* 0x0B8: 0 at the reset */
  Body body;                 /* 0x0BC: the owner's place (its position) */
  f32 scale;                 /* 0x0EC: 1 at the reset */
  int32_t course;            /* 0x0F0: the set-up's course (picks the music, and course 2's motion cycle) */
  int32_t variant;           /* 0x0F4: the set-up's variant */
  int32_t penguin_count;     /* 0x0F8: 3 (2 for variant 4) */
  int32_t penguin_slot[OWNER_PENGUINS]; /* 0x0FC: the race set-up's words44[i] (indexes its words68 / words88) */
  int32_t word108;           /* 0x108: the race set-up's word38 */
} OwnerScene;
HW_OFFSET(OwnerScene, setup, 0x028);
HW_OFFSET(OwnerScene, state, 0x02C);
HW_OFFSET(OwnerScene, timer, 0x030);
HW_OFFSET(OwnerScene, penguins, 0x040);
HW_OFFSET(OwnerScene, texlist64, 0x064);
HW_OFFSET(OwnerScene, file, 0x094);
HW_OFFSET(OwnerScene, pairs, 0x0A0);
HW_OFFSET(OwnerScene, frame, 0x0AC);
HW_OFFSET(OwnerScene, played, 0x0B4);
HW_OFFSET(OwnerScene, body, 0x0BC);
HW_OFFSET(OwnerScene, scale, 0x0EC);
HW_OFFSET(OwnerScene, penguin_count, 0x0F8);
HW_OFFSET(OwnerScene, pad, 0x024);
HW_OFFSET(OwnerScene, penguin_character, 0x04C);
HW_OFFSET(OwnerScene, penguin_variant, 0x058);
HW_OFFSET(OwnerScene, sprite, 0x074);
HW_OFFSET(OwnerScene, texlist, 0x098);
HW_OFFSET(OwnerScene, object, 0x09C);
HW_OFFSET(OwnerScene, course, 0x0F0);
HW_OFFSET(OwnerScene, variant, 0x0F4);
HW_OFFSET(OwnerScene, penguin_slot, 0x0FC);
HW_OFFSET(OwnerScene, word108, 0x108);
HW_SIZE(OwnerScene, 0x10C);

static inline OwnerScene *owner_scene_at(gaddr a) { return (OwnerScene *)hw_ptr(a); }

/* a shape motion pair of the owner file (shape_motion_pair_draw, game/house.c) */
typedef struct OwnerPair {
  gaddr motion;     /* 0x00: NjMotion (core/file.h) */
  gaddr shape;      /* 0x04 */
} OwnerPair;
HW_SIZE(OwnerPair, 8);

gaddr owner_scene_construct(gaddr scene, gaddr setup); /* game/owner_scene_construct.c (func_8C02FC12, resumable) */
gaddr owner_scene_init(gaddr scene, gaddr setup);      /* game/owner_scene_init.c (func_8C02F850, resumable) */
gaddr owner_penguin_construct(gaddr penguin, int32_t character, int32_t variant, gaddr container); /* owner_scene.c */
void owner_scene_reset_owner(gaddr scene);             /* game/owner_scene.c (func_8C030138) */
#endif
