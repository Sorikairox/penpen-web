/* Pen Pen port, S17 b03: the scenes of game modes 11 and 12 (game/game.h): the picture gallery and the language
 * set-up (game/gallery.c, game/config_menu.c).
 *
 * Both are Task subclasses hung under the Game's root task; their owner (Task.owner) is the Game, whose next_mode
 * they set to MODE_TITLE when they leave. Each reads player one's pad with a five-update repeat lock on Start, A
 * and B (the edges are cleared while the lock runs) and no lock on the directions. */
#ifndef PENPEN_MODE_SCENES_H
#define PENPEN_MODE_SCENES_H

#include "../core/task.h"
#include "../core/pad.h"
#include "../core/ninja.h"

#define MODE_SCENE_REPEAT_FRAMES 5

/* the buttons a mode scene reads each update (pad_pressed of each) */
typedef struct ModeScenePresses {
  int32_t start;  /* +0x00: PAD_START, repeat-locked */
  int32_t a;      /* +0x04: PAD_A, repeat-locked */
  int32_t b;      /* +0x08: PAD_B, repeat-locked */
  int32_t right;  /* +0x0C: PAD_RIGHT */
  int32_t left;   /* +0x10: PAD_LEFT */
  int32_t up;     /* +0x14: PAD_UP */
  int32_t down;   /* +0x18: PAD_DOWN */
} ModeScenePresses;
HW_SIZE(ModeScenePresses, 0x1C);

/* GalleryScene: mode 11, 0x60 bytes, constructor func_8C021A10, vtable 0x8C23DA2C (destroy func_8C021A42) */
typedef struct GalleryScene {
  Task task;                /* 0x00 */
  Pad pad;                  /* 0x24: port 0 */
  int32_t step;             /* 0x28: 0 load the picture, 1 browse, 2 leave */
  int32_t word2c;           /* 0x2C: 0 at construction */
  gaddr picture_texlist;    /* 0x30: the picture's NJS_TEXLIST (0x8C092E40) */
  gaddr frame_texlist;      /* 0x34: "gallery.pvm"'s (0x8C092E54) */
  ModeScenePresses press;   /* 0x38 */
  int32_t repeat_lock;      /* 0x54 */
  int32_t picture;          /* 0x58: 0 to GALLERY_PICTURES - 1 */
  int32_t number_hidden;    /* 0x5C: A toggles it; 0 = the picture's number is drawn */
} GalleryScene;
HW_OFFSET(GalleryScene, step, 0x28);
HW_OFFSET(GalleryScene, picture_texlist, 0x30);
HW_OFFSET(GalleryScene, press, 0x38);
HW_OFFSET(GalleryScene, repeat_lock, 0x54);
HW_OFFSET(GalleryScene, picture, 0x58);
HW_OFFSET(GalleryScene, number_hidden, 0x5C);
HW_SIZE(GalleryScene, 0x60);
#define GALLERY_PICTURES 42
#define GALLERY_VTABLE   0x8C23DA2Cu

/* ConfigScene: mode 12, 0x8C bytes, constructor func_8C021E0A, vtable 0x8C23DA44 (destroy func_8C021E68): the
 * language choice (seven languages, then two more entries: a cursor `shown` of 1 to 8 moved through a table) */
typedef struct ConfigScene {
  Task task;                /* 0x00 */
  Pad pad;                  /* 0x24: port 0 */
  int32_t step;             /* 0x28: 0, 1 choose, 2 save, 3 leave */
  int32_t word2c;           /* 0x2C: 1 when the language at construction is below 4 */
  uint8_t pad_030[0x34 - 0x30];
  gaddr texlist;            /* 0x34: "config.pvm"'s (0x8C092FA0) */
  NjSprite sprite;          /* 0x38: scale 1, the texlist and the texture animation at 0x8C092FBC */
  ModeScenePresses press;   /* 0x58 */
  int32_t repeat_lock;      /* 0x74 */
  int32_t word78;           /* 0x78: 0 at construction */
  int32_t shown;            /* 0x7C: the cursor (the entry drawn highlighted); first the language */
  int32_t language;         /* 0x80: the game's language at construction (it picks the strings) */
} ConfigScene;
HW_OFFSET(ConfigScene, texlist, 0x34);
HW_OFFSET(ConfigScene, sprite, 0x38);
HW_OFFSET(ConfigScene, press, 0x58);
HW_OFFSET(ConfigScene, repeat_lock, 0x74);
HW_OFFSET(ConfigScene, shown, 0x7C);
HW_OFFSET(ConfigScene, language, 0x80);
#define CONFIG_VTABLE 0x8C23DA44u

/* the cursor's moves: per entry (shown - 1) the entry Right, Left, Up and Down go to */
typedef struct ConfigMoves {
  int32_t right, left, up, down;
} ConfigMoves;
HW_SIZE(ConfigMoves, 0x10);
#define CONFIG_MOVES ((const ConfigMoves *)hw_ptr(0x8C072DC4u))

static inline GalleryScene *gallery_at(gaddr a) { return (GalleryScene *)hw_ptr(a); }
static inline ConfigScene *config_scene_at(gaddr a) { return (ConfigScene *)hw_ptr(a); }

/* the pad step both scenes share (each has its own copy in the original) */
void mode_scene_read_pad(gaddr pad, ModeScenePresses *press, int32_t *repeat_lock);

gaddr gallery_init(gaddr scene);
gaddr gallery_construct(gaddr scene);
void gallery_destroy(gaddr scene);
gaddr gallery_delete(gaddr scene, int32_t free_memory);
gaddr gallery_destroy_self(gaddr scene);
void gallery_update(gaddr scene);
void gallery_draw(gaddr scene);

gaddr config_scene_init(gaddr scene);
gaddr config_scene_construct(gaddr scene);
void config_scene_destroy(gaddr scene);
gaddr config_scene_delete(gaddr scene, int32_t free_memory);
gaddr config_scene_destroy_self(gaddr scene);
void config_scene_update(gaddr scene);
void config_scene_draw(gaddr scene);
/* S18 b02: game/config_entries.c */
void config_scene_draw_entry(gaddr scene, int32_t selected, int32_t n);

#endif
