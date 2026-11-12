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
#define RACE_RACERS  8 /* S15: the scene's racer slots (its construction clears 8: players[] and player_view[]) */

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
  gaddr players[RACE_RACERS];   /* 0x04C: Racer (0 = an empty slot) */
  uint32_t word6c;              /* 0x06C */
  uint8_t pad_070[0x074 - 0x070];
  f32 f74;                      /* 0x074 */
  f32 f78;                      /* 0x078: (func_8C02DF96 tests it against 0) */
  union {
    int32_t ending_kind;        /* 0x07C: which result drawer the ending's third step uses (0 / else) */
    int32_t words7c[8];         /* 0x07C: S15 b04: cleared together when a race starts (race_scene_start) */
  };
  int32_t view_player[4];       /* 0x09C: the player a view shows (-1: none) */
  int32_t view_cell[4];         /* 0x0AC: the course cell under the view's camera */
  int32_t player_view[RACE_RACERS]; /* 0x0BC: the view a player is shown on (-1: none) */
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
  int32_t word_cac;             /* 0xCAC: S15 b04: 0 when a race starts */
  gaddr marks_a;                /* 0xCB0: S15: a halfword per entry of g_race_mark_count, cleared at construction */
  gaddr marks_b;                /* 0xCB4: the same */
  int32_t word_cb8;             /* 0xCB8: S15 b04: 0 when a race starts */
  int32_t word_cbc;             /* 0xCBC: the same */
  int16_t scores[RACE_PLAYERS]; /* 0xCC0: shown with the results */
  uint8_t pad_cc8[0xCCC - 0xCC8];
  gaddr vec_lists_a[RACE_RACERS]; /* 0xCCC: S15: per racer slot, g_race_mark_count / 8 + 1 Vec3s */
  gaddr vec_lists_b[RACE_RACERS]; /* 0xCEC: the same */
  gaddr word_lists[RACE_RACERS];  /* 0xD0C: the same number of words */
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
HW_OFFSET(RaceScene, words7c, 0x07C);
HW_OFFSET(RaceScene, word_cac, 0xCAC);
HW_OFFSET(RaceScene, word_cb8, 0xCB8);
HW_OFFSET(RaceScene, word_cbc, 0xCBC);
HW_OFFSET(RaceScene, scores, 0xCC0);
HW_OFFSET(RaceScene, marks_a, 0xCB0);
HW_OFFSET(RaceScene, vec_lists_a, 0xCCC);
HW_OFFSET(RaceScene, word_lists, 0xD0C);
HW_SIZE(RaceScene, 0xD2C);

enum { RACE_STATE_RESULTS = 3, RACE_STATE_ENDING = 4 };
/* the substate of RACE_STATE_ENDING whose sprites / result are drawn */
enum { ENDING_SPRITES = 1, ENDING_RESULT = 3 };

static inline RaceScene *race_scene_at(gaddr a) { return (RaceScene *)hw_ptr(a); }
static inline RaceSetup *race_setup_at(gaddr a) { return (RaceSetup *)hw_ptr(a); }

#define g_race_setup GUEST_VAR(gaddr, 0x8C08C794u) /* the RaceSetup of the race being played (0 = none) */

/* game/camera.c */
int32_t race_scene_view_cell(gaddr scene, int32_t view);

/* core/accessors.c (S15) */
void race_setup_set_variant(gaddr setup, int32_t value);
void race_setup_set_course(gaddr setup, int32_t value);
void race_setup_set_word68(gaddr setup, int32_t i, uint32_t value);
void race_setup_set_word88(gaddr setup, int32_t i, uint32_t value);

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

/* S15 b04: the rest of game/race_hud.c's drawers (exported there; declared for game/race_draw.c) */
void race_pause_draw_overlay(gaddr scene);
void race_results_draw_full(gaddr scene);
void race_results_draw_two(gaddr scene);
void race_results_draw_four(gaddr scene);
void race_ending_draw_full(gaddr scene);
void race_ending_draw_other(gaddr scene);
void race_item_draw_full(gaddr scene, int32_t i);
void race_item_draw_two(gaddr scene, int32_t i);
void race_item_draw_four(gaddr scene, int32_t i);
void race_results_draw_banner(gaddr scene, int32_t i);
/* game/race_scene.c */
void race_ending_draw_a(gaddr scene);
void race_ending_draw_b(gaddr scene);
/* core/accessors.c */
int32_t race_scene_view_player(gaddr scene, int32_t view);
/* game/save.c: the record time of a course (by course and variant) */
f32 save_time_a(int32_t cup, int32_t course);
/* game/race_scene_methods.c */
void race_scene_course_camera(gaddr scene, uint32_t a, uint32_t b, uint32_t c, gaddr out);

/* S15 b04: game/race_draw.c (the scene's drawing over each view) */
void race_draw_time(gaddr scene);
void race_draw_record(gaddr scene);
void race_draw_view_full(gaddr scene);
void race_draw_view_two(gaddr scene);
void race_draw_view_four(gaddr scene);
void race_draw_view_trial(gaddr scene);
void race_draw_view_demo(gaddr scene);
void race_draw_views(gaddr scene);
/* S15 b04: game/race_players.c (the players at the start of a race, by the game's mode) */
void race_start_versus_computer(gaddr scene);
void race_start_two(gaddr scene);
void race_start_four(gaddr scene);
void race_start_trial(gaddr scene);
void race_start_demo(gaddr scene);
void race_scene_start(gaddr scene);

#endif
