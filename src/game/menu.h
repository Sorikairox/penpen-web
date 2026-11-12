/* Pen Pen port, S14: the menu that sets a game up, and a menu's model drawer (core/accessors.c, game/menu_sprites.c).
 *
 * SetupMenu: a scene Task whose update is func_8C0233EA (states 0 to 8, each its own update). b02: PenPen House and
 * the selections that follow it (game/house.c); 0x424 bytes, constructor func_8C022D80 (with the RaceSetup it sets
 * up), vtable 0x8C23DA5C {func_8C0233B4, func_8C0233EA, func_8C0235D8}. It keeps player one's pad, the buttons pressed
 * this frame (with a 30-frame repeat lock on two of them), words per player, the penguins it shows, and the sprites it
 * draws: an NJS_SPRITE (core/ninja.h) moved by Bodies (position and scale; two more Bodies placed per entry of a
 * table) and a backdrop from its texture list. */
#ifndef PENPEN_MENU_H
#define PENPEN_MENU_H

#include "../core/task.h"
#include "../core/vec.h"
#include "../core/pad.h"
#include "../core/ninja.h"

typedef struct SetupMenu {
  Task task;                /* 0x000 */
  Pad pad;                  /* 0x024 */
  union {
    uint32_t word28;        /* 0x028 (S13's name) */
    gaddr setup;            /* 0x028: b02: the RaceSetup it sets up (the constructor's argument) */
  };
  int32_t state;            /* 0x02C: b02: 0 the house, 1 and 2 two selections, 3, 4; 5-8 leave (setup_menu_update) */
  uint32_t word30;          /* 0x030 */
  union {
    uint32_t word34;        /* 0x034 (S13's name) */
    int32_t phase;          /* 0x034: b02: 0 = the state was just entered (cleared with each new state) */
  };
  uint32_t pressed[6];      /* 0x038: buttons 4, 2 (repeat-locked), 0x40, 0x80, 0x10, 0x20 (MENU_PRESS_*) */
  int32_t repeat_lock;      /* 0x050 */
  int32_t cursor;           /* 0x054: b02: the house's choice (0 to cursor_count - 1) */
  int32_t cursor_count;     /* 0x058 */
  int32_t course;           /* 0x05C: b02: handed to the set-up's course on leaving */
  uint32_t word60;          /* 0x060 */
  int32_t variant;          /* 0x064: b02: handed to the set-up's variant */
  union {
    struct {
      uint32_t words68[8];  /* 0x068 (S13's names: core/accessors.c's setters of these, func_8C0229EE / func_8C0229FA,
                             * and of word30 / word34, are called with menu->setup: they are RaceSetup's fields) */
      uint32_t words88[8];  /* 0x088 */
    };
    struct {
      uint32_t word68;      /* 0x068 */
      int32_t order[8];     /* 0x06C: b02: i at the selection's start */
      int32_t picked[8];    /* 0x08C: b02: 0 at the selection's start */
      uint32_t player_a[8]; /* 0x0AC: b02: the set-up's words68, copied back on leaving; each passed to the VMU picture */
      uint32_t player_b[8]; /* 0x0CC: b02: the set-up's words88 */
    };
  };
  gaddr penguin_file;       /* 0x0EC: b02: the penguins' model file ("house_pen.bin") */
  gaddr penguins[8][2];     /* 0x0F0: b02: MotionPlayer (game/motion.h) per character and variant, or 0 */
  uint8_t pad_130[0x158 - 0x130];
  gaddr select_file;        /* 0x158: b02: "select.dat" */
  gaddr map_texlist;        /* 0x15C: b02: the course map's textures (the file's first texture list) */
  gaddr texlist;            /* 0x160: the backdrop's texture list (b02: the house's, "penhouse.pvm") */
  uint8_t pad_164[0x17C - 0x164];
  NjSprite sprite;          /* 0x17C */
  uint8_t pad_19c[0x1B0 - 0x19C];
  gaddr owner_file;         /* 0x1B0: b02: "Owner.bin" */
  gaddr owner_texlist;      /* 0x1B4: b02: its texture list */
  uint8_t pad_1b8[0x1E0 - 0x1B8];
  f32 f1e0;                 /* 0x1E0: b02: 0 when state 2 starts */
  uint8_t pad_1e4[0x220 - 0x1E4];
  Body place_a;             /* 0x220: placed from the table (icon_place) */
  Body place_b;             /* 0x250 */
  Body icon_position;       /* 0x280: the sprite's position (x, y) and priority (z) */
  Body icon_scale;          /* 0x2B0: the sprite's scale (x, y) */
  Body slots[4];            /* 0x2E0: b02: where the first selection draws each player's slot */
  Body1 alphas[4];          /* 0x3A0: b02: each slot's alpha (its value) */
  int32_t player_count;     /* 0x3E0: b02: entries of player_a / player_b copied back on leaving */
  Body1 fades[4];           /* 0x3E4: b02: value 0 when state 2 starts */
} SetupMenu;
HW_OFFSET(SetupMenu, pad, 0x024);
HW_OFFSET(SetupMenu, setup, 0x028);
HW_OFFSET(SetupMenu, state, 0x02C);
HW_OFFSET(SetupMenu, phase, 0x034);
HW_OFFSET(SetupMenu, repeat_lock, 0x050);
HW_OFFSET(SetupMenu, cursor, 0x054);
HW_OFFSET(SetupMenu, course, 0x05C);
HW_OFFSET(SetupMenu, variant, 0x064);
HW_OFFSET(SetupMenu, words68, 0x068);
HW_OFFSET(SetupMenu, words88, 0x088);
HW_OFFSET(SetupMenu, order, 0x06C);
HW_OFFSET(SetupMenu, picked, 0x08C);
HW_OFFSET(SetupMenu, player_a, 0x0AC);
HW_OFFSET(SetupMenu, player_b, 0x0CC);
HW_OFFSET(SetupMenu, penguin_file, 0x0EC);
HW_OFFSET(SetupMenu, penguins, 0x0F0);
HW_OFFSET(SetupMenu, select_file, 0x158);
HW_OFFSET(SetupMenu, texlist, 0x160);
HW_OFFSET(SetupMenu, sprite, 0x17C);
HW_OFFSET(SetupMenu, owner_file, 0x1B0);
HW_OFFSET(SetupMenu, owner_texlist, 0x1B4);
HW_OFFSET(SetupMenu, f1e0, 0x1E0);
HW_OFFSET(SetupMenu, place_a, 0x220);
HW_OFFSET(SetupMenu, icon_position, 0x280);
HW_OFFSET(SetupMenu, icon_scale, 0x2B0);
HW_OFFSET(SetupMenu, slots, 0x2E0);
HW_OFFSET(SetupMenu, alphas, 0x3A0);
HW_OFFSET(SetupMenu, player_count, 0x3E0);
HW_OFFSET(SetupMenu, fades, 0x3E4);
HW_SIZE(SetupMenu, 0x424);
/* b02: the entries of `pressed` */
enum { MENU_PRESS_A, MENU_PRESS_B, MENU_PRESS_LEFT, MENU_PRESS_RIGHT, MENU_PRESS_UP, MENU_PRESS_DOWN };
#define MENU_REPEAT_FRAMES 30

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

/* game/menu_sprites.c (b02: declared for game/house.c) */
void icon_bodies_init(gaddr self);
void icon_draw(gaddr self, int32_t n);
void icon_place(gaddr self, int32_t n);
void backdrop_draw(gaddr self);

/* b02, game/house.c */
void setup_menu_destroy(gaddr menu);
void setup_menu_update(gaddr menu);
void setup_menu_draw(gaddr menu);
void house_enter(gaddr menu);
void house_update(gaddr menu);
void house_draw(gaddr menu);
void house_view_camera(int32_t index, gaddr eye, gaddr target, f32 roll, f32 fov);
void selection_enter(gaddr menu);
void selection_draw(gaddr menu);
void selection2_enter(gaddr menu);

/* game/house.c: the C runtime's floorf (func_8C062988), through the game's thunk */
f32 rt_floor(f32 x);

int32_t shape_motion_pair_draw(gaddr object, gaddr pair, gaddr frame, int32_t loop);

#endif
