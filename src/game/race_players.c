/* Pen Pen port, S15 (b04): the race scene's players at the start of a race (0x8C02BDAE-0x8C02C770), one function per
 * kind of game.
 *
 * Each puts the players at the course's start positions facing the course's start heading. A slot that is still
 * empty (the first race of the set-up) gets a new player: a Racer for a human (syMalloc(0x5A8), racer_construct, with
 * its two words of the set-up) or a LaneRacer for the computer (syMalloc(0x6C0), lane_racer_construct, with the
 * set-up's range for that player) added to the scene's tree. Every player is then reset at its start position
 * through its virtual table. Last, the split mode and the views: which player each view shows (-1: none) and on
 * which view each player is shown. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/view.h"
#include "../core/vec.h"
#include "race_scene.h"
#include "racer.h"
#include "course.h"

/* ---- calls ---- */

/* func_8C029BD0 (still translated): the scene's set-up for a race: the course and the actors (unless kept), the
 * players by the game's mode (the functions below) */
static void race_scene_setup(gaddr scene) { CALL1(0x8C029BD0u, scene); }

/* func_8C041F70 (game/course.c, still translated): start position i of the course */
/* func_8C01AD42 (still translated): the set-up's range row for computer player i */
static int32_t race_setup_range(gaddr setup, int32_t i) { return (int32_t)CALL2(0x8C01AD42u, setup, i); }

/* the racer's virtual reset (RacerVtbl.reset): at `position`, word 0, facing `heading` */
static void racer_vreset(gaddr racer, gaddr position, f32 heading) {
  const RacerVtbl *vt = (const RacerVtbl *)hw_ptr(racer_at(racer)->task.vtable);
  FARGS1(heading);
  hw_call(vt->reset.fn, 3, racer + (uint32_t)vt->reset.this_adjust, position, 0, 0);
}

/* the original's frame (0x14 bytes: the player and its virtual table are kept at 0 and 4, the start position at 8;
 * func_8C02C5C2's has no vtable slot: 0x10 bytes, the position at 4) */
typedef struct StartFrame {
  gaddr racer;    /* 0x00 */
  gaddr vtable;   /* 0x04 */
  Vec3 position;  /* 0x08 */
} StartFrame;
HW_OFFSET(StartFrame, position, 0x08);
HW_SIZE(StartFrame, 0x14);
typedef struct DemoStartFrame {
  gaddr racer;    /* 0x00 */
  Vec3 position;  /* 0x04 */
} DemoStartFrame;
HW_OFFSET(DemoStartFrame, position, 0x04);
HW_SIZE(DemoStartFrame, 0x10);

/* a new human player in slot i (0 when out of memory; i is also its fifth argument), added to the scene's tree */
static void player_new_racer(gaddr scene, int32_t i, gaddr position, f32 heading) {
  RaceScene *s = race_scene_at(scene);
  gaddr mem = syMalloc(sizeof(Racer));
  gaddr racer;
  if (mem == 0) {
    racer = 0;
  } else {
    RaceSetup *setup = race_setup_at(s->setup);
    uint32_t b = setup->words88[i];
    uint32_t a = setup->words68[i];
    racer = racer_construct(mem, scene, (uint32_t)i, position, a, b, (uint32_t)i, heading);
  }
  s->players[i] = racer;
  task_add_children(&s->task, task_at(racer));
}

/* a new computer player in slot i with range row `range` (0 when out of memory), added to the scene's tree */
static void player_new_lane_racer(gaddr scene, int32_t i, gaddr position, f32 heading, int has_range) {
  RaceScene *s = race_scene_at(scene);
  gaddr mem = syMalloc(sizeof(LaneRacer));
  gaddr racer;
  if (mem == 0) {
    racer = 0;
  } else {
    int32_t range = has_range ? race_setup_range(s->setup, i) : 0;
    RaceSetup *setup = race_setup_at(s->setup);
    uint32_t b = setup->words88[i];
    uint32_t a = setup->words68[i];
    racer = lane_racer_construct(mem, scene, (uint32_t)i, position, a, b, range, heading);
  }
  s->players[i] = racer;
  task_add_children(&s->task, task_at(racer));
}

/* slot i's start: the course's start position i, a new player when the slot is empty (human or computer), the
 * reset */
enum { PLAYER_HUMAN, PLAYER_COMPUTER, PLAYER_COMPUTER_FIRST };
static void player_start(gaddr scene, int32_t i, gaddr position, f32 heading, int kind) {
  RaceScene *s = race_scene_at(scene);
  course_start_position(s->course, i, position);
  if (s->players[i] == 0) {
    if (kind == PLAYER_HUMAN)
      player_new_racer(scene, i, position, heading);
    else
      player_new_lane_racer(scene, i, position, heading, kind == PLAYER_COMPUTER);
  }
  racer_vreset(s->players[i], position, heading);
}

/* the split mode, then the views' players (view_player[0..3]) and the players' views (player_view[0..7]) */
static void views_assign(RaceScene *s, int32_t split, const int32_t view_player[4], const int32_t player_view[8]) {
  view_set_split(split);
  for (int k = 0; k < 4; k++)
    s->view_player[k] = view_player[k];
  for (int k = 0; k < RACE_RACERS; k++)
    s->player_view[k] = player_view[k];
}

static const int32_t ONE_VIEW[4] = {0, -1, -1, -1};
static const int32_t PLAYER0_SHOWN[8] = {0, -1, -1, -1, -1, -1, -1, -1};

/* was func_8C02C4D8: mode 4 (the time trial), full screen: player 0 alone (a Racer) at start position 0 */
void race_start_trial(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  f32 heading = course_value(s->course, -1);
  gaddr frame = hw_stack_alloc(sizeof(StartFrame));
  gaddr position = member_addr(frame, StartFrame, position);
  player_start(scene, 0, position, heading, PLAYER_HUMAN);
  hw_stack_free(sizeof(StartFrame));
  views_assign(s, SPLIT_FULL, ONE_VIEW, PLAYER0_SHOWN);
}
HW_EXPORT(8c02c4d8, race_start_trial(ARG0))

/* was func_8C02BDAE: mode 0, one player against the computer, full screen (word_ca8 = 1): player 0 a Racer, the others up
 * to the set-up's player count LaneRacers */
void race_start_versus_computer(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  s->word_ca8 = 1;
  f32 heading = course_value(s->course, -1);
  gaddr frame = hw_stack_alloc(sizeof(StartFrame));
  gaddr position = member_addr(frame, StartFrame, position);
  player_start(scene, 0, position, heading, PLAYER_HUMAN);
  for (int32_t i = 1; i < race_setup_at(s->setup)->player_count; i++)
    player_start(scene, i, position, heading, PLAYER_COMPUTER);
  hw_stack_free(sizeof(StartFrame));
  views_assign(s, SPLIT_FULL, ONE_VIEW, PLAYER0_SHOWN);
}
HW_EXPORT(8c02bdae, race_start_versus_computer(ARG0))

/* the first `humans` players are Racers (always), the rest up to the player count LaneRacers */
static void players_start_split(gaddr scene, int32_t humans) {
  RaceScene *s = race_scene_at(scene);
  gaddr frame = hw_stack_alloc(sizeof(StartFrame));
  gaddr position = member_addr(frame, StartFrame, position);
  f32 heading = course_value(s->course, -1);
  int32_t i = 0;
  do {
    player_start(scene, i, position, heading, PLAYER_HUMAN);
    i++;
  } while (i < humans);
  for (; i < race_setup_at(s->setup)->player_count; i++)
    player_start(scene, i, position, heading, PLAYER_COMPUTER);
  hw_stack_free(sizeof(StartFrame));
}

/* was func_8C02BFCC: mode 1, two players on a two-view split (with the panel): players 0 and 1 Racers on views 0 and 1,
 * the computer's after them */
void race_start_two(gaddr scene) {
  static const int32_t view_player[4] = {0, 1, -1, -1};
  static const int32_t player_view[8] = {0, 1, -1, -1, -1, -1, -1, -1};
  players_start_split(scene, 2);
  views_assign(race_scene_at(scene), SPLIT_TWO_WITH_PANEL, view_player, player_view);
}
HW_EXPORT(8c02bfcc, race_start_two(ARG0))

/* was func_8C02C2D8: mode 3, four players on four views: players 0 to 3 Racers, the computer's after them */
void race_start_four(gaddr scene) {
  static const int32_t view_player[4] = {0, 1, 2, 3};
  static const int32_t player_view[8] = {0, 1, 2, 3, -1, -1, -1, -1};
  players_start_split(scene, 4);
  views_assign(race_scene_at(scene), SPLIT_FOUR, view_player, player_view);
}
HW_EXPORT(8c02c2d8, race_start_four(ARG0))

/* was func_8C02C5C2: mode 5, the demo: every player a LaneRacer (player 0 with range row 0), full screen with no player
 * on the view (the course's camera) */
void race_start_demo(gaddr scene) {
  static const int32_t view_player[4] = {-1, -1, -1, -1};
  RaceScene *s = race_scene_at(scene);
  f32 heading = course_value(s->course, -1);
  gaddr frame = hw_stack_alloc(sizeof(DemoStartFrame));
  gaddr position = member_addr(frame, DemoStartFrame, position);
  for (int32_t i = 0; i < race_setup_at(s->setup)->player_count; i++)
    player_start(scene, i, position, heading, i == 0 ? PLAYER_COMPUTER_FIRST : PLAYER_COMPUTER);
  hw_stack_free(sizeof(DemoStartFrame));
  views_assign(s, SPLIT_FULL, view_player, PLAYER0_SHOWN);
}
HW_EXPORT(8c02c5c2, race_start_demo(ARG0))

/* ---- a race's start ---- */

/* was func_8C029E92: a race starts: word_ca8 = 1, the words at 0xCAC, 0xCB8, 0xCBC cleared, the set-up, then the
 * running time f78 and the eight words from 0x7C cleared */
void race_scene_start(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  s->word_ca8 = 1;
  s->word_cac = 0;
  s->word_cb8 = 0;
  s->word_cbc = 0;
  race_scene_setup(scene);
  s->f78 = F32(0.0f);
  for (int k = 0; k < 8; k++)
    s->words7c[k] = 0;
}
HW_EXPORT(8c029e92, race_scene_start(ARG0))
