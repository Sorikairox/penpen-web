/* Pen Pen port, S14: the menu that sets a game up, and a menu's model drawer (core/accessors.c, game/menu_sprites.c).
 *
 * SetupMenu: a scene Task whose update is func_8C0233EA (states 0 to 5, each its own update; the class's
 * constructor and size are not identified): player one's pad, the buttons pressed this frame (with a 30-frame repeat
 * lock on two of them), words per player, and the sprites it draws: an NJS_SPRITE (core/ninja.h) moved by Bodies
 * (position and scale; two more Bodies placed per entry of a table) and a backdrop from its texture list. */
#ifndef PENPEN_MENU_H
#define PENPEN_MENU_H

#include "../core/task.h"
#include "../core/vec.h"
#include "../core/pad.h"
#include "../core/ninja.h"

typedef struct SetupMenu {
  Task task;                /* 0x000 */
  Pad pad;                  /* 0x024 */
  uint32_t word28;          /* 0x028 */
  int32_t state;            /* 0x02C */
  uint32_t word30;          /* 0x030 */
  uint32_t word34;          /* 0x034 */
  uint32_t pressed[6];      /* 0x038: buttons 4, 2 (repeat-locked), 0x40, 0x80, 0x10, 0x20 */
  int32_t repeat_lock;      /* 0x050 */
  uint8_t pad_054[0x068 - 0x054];
  uint32_t words68[8];      /* 0x068: a word per player (core/inline.c's copies read them) */
  uint32_t words88[8];      /* 0x088 */
  uint8_t pad_0a8[0x160 - 0x0A8];
  gaddr texlist;            /* 0x160: the backdrop's texture list */
  uint8_t pad_164[0x17C - 0x164];
  NjSprite sprite;          /* 0x17C */
  uint8_t pad_19c[0x220 - 0x19C];
  Body place_a;             /* 0x220: placed from the table (icon_place) */
  Body place_b;             /* 0x250 */
  Body icon_position;       /* 0x280: the sprite's position (x, y) and priority (z) */
  Body icon_scale;          /* 0x2B0: the sprite's scale (x, y) */
} SetupMenu;
HW_OFFSET(SetupMenu, pad, 0x024);
HW_OFFSET(SetupMenu, state, 0x02C);
HW_OFFSET(SetupMenu, repeat_lock, 0x050);
HW_OFFSET(SetupMenu, words68, 0x068);
HW_OFFSET(SetupMenu, words88, 0x088);
HW_OFFSET(SetupMenu, texlist, 0x160);
HW_OFFSET(SetupMenu, sprite, 0x17C);
HW_OFFSET(SetupMenu, place_a, 0x220);
HW_OFFSET(SetupMenu, icon_position, 0x280);
HW_OFFSET(SetupMenu, icon_scale, 0x2B0);

static inline SetupMenu *setup_menu_at(gaddr a) { return (SetupMenu *)hw_ptr(a); }

/* A menu object that draws a model in one of its motions (func_8C028802; the class is not identified): model and
 * motion tables by its `current` number, a motion picked in the motion table's 8-byte entries, a frame, an angle and
 * a position. */
typedef struct MenuModel {
  uint8_t pad_000[0x05C];
  uint32_t current;         /* 0x05C */
  uint8_t pad_060[0x1B4 - 0x060];
  gaddr texlist;            /* 0x1B4 */
  gaddr models[4];          /* 0x1B8 */
  gaddr motion_tables[4];   /* 0x1C8: 8-byte entries (MotionEntry) */
  uint32_t motion;          /* 0x1D8: the entry of the table */
  uint8_t pad_1dc[0x1E0 - 0x1DC];
  f32 frame;                /* 0x1E0 */
  uint8_t pad_1e4[0x1E8 - 0x1E4];
  uint32_t loop;            /* 0x1E8 */
  f32 angle;                /* 0x1EC: about y */
  Vec3 position;            /* 0x1F0 */
} MenuModel;
HW_OFFSET(MenuModel, current, 0x05C);
HW_OFFSET(MenuModel, texlist, 0x1B4);
HW_OFFSET(MenuModel, motion_tables, 0x1C8);
HW_OFFSET(MenuModel, frame, 0x1E0);
HW_OFFSET(MenuModel, loop, 0x1E8);
HW_OFFSET(MenuModel, position, 0x1F0);

#endif
