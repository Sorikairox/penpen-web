/* Pen Pen port, S14: the race scene and the race's set-up (game/race_scene.c, game/actor.c, game/stage.c,
 * game/camera.c).
 *
 * The race scene (constructor func_8C0293F0, a Task) is the scene of a race: it keeps itself in g_race_scene (the
 * object game.h calls the panel: the scene draws over the views), holds the race's set-up, builds the course
 * (game/course.h) at 0x38 and the scenery actors (game/actor.h) at 0x48, then up to four players (game/racer.h) at
 * 0x4C. Each player is shown on a view (0 or 1 on a split screen); the camera follows one player and the other view
 * may watch another. */
#ifndef PENPEN_RACE_SCENE_H
#define PENPEN_RACE_SCENE_H

#include "../core/task.h"
#include "../core/vec.h"
#include "../core/pad.h"
#include "../core/ninja.h"

/* The race's set-up: mode 7's scene (0xA8 bytes, constructor func_8C01A8DE -> func_8C01A6B6, vtable 0x8C23D954, a
 * Task) keeps itself in g_race_setup and is the race scene's 0x28: the game's mode (0 to 5: how many players and
 * which game; the mode argument of Game mode 7), the course and its variant. */
typedef struct RaceSetup {
  Task task;          /* 0x00 */
  int32_t state;      /* 0x24: what it shows now (7 at construction: nothing) */
  int32_t requested;  /* 0x28: what it is asked to show (race_setup_update): 0 the SetupMenu, 1 the RaceScene (from
                       * the start in mode 5, the demo), 2 a third scene, 3-6 leave the mode */
  int32_t mode;       /* 0x2C */
  int32_t course;     /* 0x30 */
  int32_t variant;    /* 0x34 */
  int32_t word38;     /* 0x38: -1 */
  uint32_t word3c;    /* 0x3C */
  uint32_t word40;    /* 0x40 */
  int32_t words44[8]; /* 0x44: -1 */
  int32_t player_count; /* 0x64: how many players race (the scenery loops over their bodies; the full-screen
                        * HUD shows it beside the item, func_8C02DCDC) */
  uint32_t words68[8]; /* 0x68: a word per player (the SetupMenu copies them to and from its player_a) */
  uint32_t words88[8]; /* 0x88: another (the SetupMenu's player_b) */
} RaceSetup;
HW_OFFSET(RaceSetup, mode, 0x2C);
HW_OFFSET(RaceSetup, variant, 0x34);
HW_OFFSET(RaceSetup, words44, 0x44);
HW_OFFSET(RaceSetup, player_count, 0x64);
HW_OFFSET(RaceSetup, words68, 0x68);
HW_OFFSET(RaceSetup, words88, 0x88);

/* The scene's slot list (stage.c: {size, used, slots}) */
typedef struct SlotList {
  int32_t size;   /* 0x00 */
  int32_t used;   /* 0x04: slots handed out by slot_list_insert(-1) */
  gaddr slots;    /* 0x08: `size` words, each the first item of a chain */
} SlotList;
HW_SIZE(SlotList, 12);

#define RACE_PLAYERS 4

typedef struct RaceScene {
  Task task;                    /* 0x000 */
  Pad pad;                      /* 0x024: the pad the pause screen is read from (X + Y held hides it) */
  gaddr setup;                  /* 0x028: RaceSetup */
  int32_t state;                /* 0x02C: RACE_STATE_* */
  uint32_t word30;              /* 0x030 */
  int32_t substate;             /* 0x034 */
  gaddr course;                 /* 0x038: Course */
  SlotList slots;               /* 0x03C: 0x40 slots */
  gaddr actors;                 /* 0x048: ActorGroup */
  gaddr players[RACE_PLAYERS];  /* 0x04C: Racer */
  uint8_t pad_05c[0x06C - 0x05C];
  uint32_t word6c;              /* 0x06C */
  uint8_t pad_070[0x074 - 0x070];
  f32 f74;                      /* 0x074 */
  f32 f78;                      /* 0x078: (func_8C02DF96 tests it against 0) */
  int32_t ending_kind;          /* 0x07C: which result drawer the ending's third step uses (0 / else) */
  uint8_t pad_080[0x09C - 0x080];
  int32_t view_player[4];       /* 0x09C: the player a view shows (-1: none) */
  int32_t view_cell[4];         /* 0x0AC: the course cell under the view's camera */
  int32_t player_view[RACE_PLAYERS]; /* 0x0BC: the view a player is shown on */
  uint8_t pad_0cc[0x0DC - 0x0CC];
  int32_t focus[4];             /* 0x0DC: per view, the player its camera follows (focus[0]: the one the sounds
                                 * and the ending go by) */
  uint8_t pad_0ec[0x0FC - 0x0EC];
  uint32_t player_item[8];      /* 0x0FC */
  int32_t watched;              /* 0x11C: the player watched from the other view */
  uint8_t pad_120[0x15C - 0x120];
  Vec3 vec15c[64];              /* 0x15C */
  f32 values[8][8];             /* 0x45C: row 0 = the players' distances (the watched player's sound fades by it) */
  Vec3 points[8][8];            /* 0x55C */
  uint8_t pad_85c[0xA5C - 0x85C];
  NjSprite sprite;              /* 0xA5C: the scene's own sprite (results, ending, pause cells) */
  int32_t music_cue;            /* 0xA7C: the next music cue */
  Vec3 vecA80[16];              /* 0xA80 */
  int32_t pause_port;           /* 0xB40: the port (0-3) of the player who paused */
  int32_t pause_cursor;         /* 0xB44: the pause menu's line (0 to g_pause_last_line) */
  int32_t pad_removed;          /* 0xB48: the pausing player's controller was pulled out */
  gaddr result;                 /* 0xB4C */
  int32_t keep_course;          /* 0xB50: 0 = the set-up builds the course and actors */
  f32 pause_fade;               /* 0xB54: the pause screen's fade (> 0: drawn) */
  f32 pause_fade_step;          /* 0xB58: +-1/30 per frame, by whether the scene is paused */
  uint8_t pad_b5c[0xB60 - 0xB5C];
  Body bodies[5];               /* 0xB60 */
  Body1 values1[5];             /* 0xC50 */
  f32 sprite_fade;              /* 0xCA0: the results' / ending's sprites' alpha (> 0: drawn) */
  int32_t pause_delay;          /* 0xCA4: frames before the pause menu reads the pad again (30 after a choice) */
  int32_t word_ca8;             /* 0xCA8: S14 b11: the type-0x22 actor's mode 6 stops at 2 */
  uint8_t pad_cac[0xCC0 - 0xCAC];
  int16_t scores[RACE_PLAYERS]; /* 0xCC0: shown with the results */
} RaceScene;
HW_OFFSET(RaceScene, setup, 0x028);
HW_OFFSET(RaceScene, course, 0x038);
HW_OFFSET(RaceScene, slots, 0x03C);
HW_OFFSET(RaceScene, actors, 0x048);
HW_OFFSET(RaceScene, players, 0x04C);
HW_OFFSET(RaceScene, word6c, 0x06C);
HW_OFFSET(RaceScene, pad, 0x024);
HW_OFFSET(RaceScene, f74, 0x074);
HW_OFFSET(RaceScene, f78, 0x078);
HW_OFFSET(RaceScene, ending_kind, 0x07C);
HW_OFFSET(RaceScene, view_player, 0x09C);
HW_OFFSET(RaceScene, view_cell, 0x0AC);
HW_OFFSET(RaceScene, player_view, 0x0BC);
HW_OFFSET(RaceScene, focus, 0x0DC);
HW_OFFSET(RaceScene, player_item, 0x0FC);
HW_OFFSET(RaceScene, watched, 0x11C);
HW_OFFSET(RaceScene, vec15c, 0x15C);
HW_OFFSET(RaceScene, values, 0x45C);
HW_OFFSET(RaceScene, points, 0x55C);
HW_OFFSET(RaceScene, sprite, 0xA5C);
HW_OFFSET(RaceScene, music_cue, 0xA7C);
HW_OFFSET(RaceScene, pause_port, 0xB40);
HW_OFFSET(RaceScene, pause_cursor, 0xB44);
HW_OFFSET(RaceScene, pad_removed, 0xB48);
HW_OFFSET(RaceScene, pause_fade, 0xB54);
HW_OFFSET(RaceScene, pause_fade_step, 0xB58);
HW_OFFSET(RaceScene, vecA80, 0xA80);
HW_OFFSET(RaceScene, result, 0xB4C);
HW_OFFSET(RaceScene, keep_course, 0xB50);
HW_OFFSET(RaceScene, bodies, 0xB60);
HW_OFFSET(RaceScene, values1, 0xC50);
HW_OFFSET(RaceScene, sprite_fade, 0xCA0);
HW_OFFSET(RaceScene, pause_delay, 0xCA4);
HW_OFFSET(RaceScene, word_ca8, 0xCA8);
HW_OFFSET(RaceScene, scores, 0xCC0);

enum { RACE_STATE_RESULTS = 3, RACE_STATE_ENDING = 4 };
/* the substate of RACE_STATE_ENDING whose sprites / result are drawn */
enum { ENDING_SPRITES = 1, ENDING_RESULT = 3 };

static inline RaceScene *race_scene_at(gaddr a) { return (RaceScene *)hw_ptr(a); }
static inline RaceSetup *race_setup_at(gaddr a) { return (RaceSetup *)hw_ptr(a); }

#define g_race_setup GUEST_VAR(gaddr, 0x8C08C794u) /* the RaceSetup of the race being played (0 = none) */

/* game/camera.c */
int32_t race_scene_view_cell(gaddr scene, int32_t view);

/* game/race_scene.c, game/stage.c */
gaddr race_setup_get(void);
int32_t race_scene_slot_insert(gaddr scene, gaddr contacts, int32_t slot);

/* S14 b05: game/race_scene.c's per-player sound helpers and getters (exported there) */
uint32_t race_scene_word6c(gaddr scene);
void race_sound_stop(gaddr scene, int32_t i, int32_t port);
void race_sound_play(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sound, int32_t pan,
                     int32_t pitch);
void race_sound_adjust(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t volume, int32_t pan,
                       int32_t pitch);
void race_sound_play_split(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sound, int32_t unused,
                           int32_t pitch);
void race_sound_adjust_split(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t volume, int32_t unused,
                             int32_t pitch);
void race_sound_play_panned(gaddr scene, int32_t i, int32_t offset, int32_t priority, int32_t sound, int32_t unused,
                            int32_t pitch);
void race_sound_adjust_panned(gaddr scene, int32_t i, int32_t offset, int32_t priority, int32_t volume,
                              int32_t unused, int32_t pitch);
void race_midi_play(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence);

/* S14 b05: game/race_hud.c */
gaddr race_player_velocity(gaddr scene, int32_t i);
int32_t race_player_state(gaddr scene, int32_t i);
void race_sound_play_by_mode(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sound, int32_t pan,
                             int32_t pitch);
void race_sound_adjust_by_mode(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t volume, int32_t pan,
                               int32_t pitch);
void race_midi_play_by_mode(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence);
void race_pause_draw_a(gaddr scene);
void race_pause_draw_b(gaddr scene);

/* game/slot_collide.c */
void slot_list_collide(gaddr scene);

#endif
