/* Pen Pen port, S16 (b03): the race scene's build (game/race_scene_build.c): the course and the scenery actors
 * (made the first time, reset when the scene runs again), the players of the set-up's mode, each placed at its
 * start position, then the scene's starting state. */
#include "hw.h"
#include "sdk.h"
#include "race_scene.h"
#include "actor.h"
#include "course.h"
#include "../core/task.h"
#include "../core/inline.h"

#define COURSE_SIZE      0x5790 /* sizeof Course (game/course.h) */
#define ACTOR_GROUP_SIZE 0x554  /* sizeof ActorGroup (game/actor.h) */
#define RACE_SEED        0x19670422u /* the C runtime's random seed at every build */
#define RACE_WORD30      0x1E0
enum { RACE_STATE_START = 2 };

/* a player's vtable (Racer, LaneRacer): the Task's three methods, then the start placement */
typedef struct PlayerVtbl {
  TaskVtbl task;
  VMethod place; /* 0x18: (this, const StartPose *, 0) with a float in FR4 (the course's start value) */
} PlayerVtbl;
HW_OFFSET(PlayerVtbl, place, 0x18);

/* the start position the course hands a player (a local of 12 bytes in the original's frame, at r15 + 4) */
#define START_POSE_SIZE  12
#define START_FRAME_SIZE 0x10 /* the original's frame: a spill word at r15, the pose after it */
#define START_POSE_AT    4

/* hand-written but not declared in a header */
/* func_8C041114 (game/course_build.c): the Course's constructor (the original passes 0 in r5 too) */
static gaddr course_construct(gaddr course) { return CALL1(0x8C041114u, course); }
/* func_8C029108 (game/race_scene_methods.c): the course's start value (a float) */
static f32 course_start_value(gaddr course) {
  CALL1(0x8C029108u, course);
  return hw_fret();
}
/* still translated */
/* func_8C062B2C: the C runtime's srand */
static void rt_srand(uint32_t seed) { CALL1(0x8C062B2Cu, seed); }
void race_start_three(gaddr scene); /* game/race_start_three.c (func_8C02C1AA): mode 2's players */
/* func_8C041350: the course reset for another run */
static void course_reset(gaddr course) { CALL1(0x8C041350u, course); }
/* func_8C044766: the scenery actors reset for another run */
static void actor_group_reset(gaddr group) { CALL1(0x8C044766u, group); }
/* func_8C041F70: start position i of the course into *pose */

/* the players of the set-up's mode (game/race_players.c; the original passes a second register the callees do
 * not read) */
static void race_start_players(gaddr scene, int32_t mode) {
  if (mode == 0)
    race_start_versus_computer(scene);
  else if (mode == 1)
    race_start_two(scene);
  else if (mode == 2)
    race_start_three(scene);
  else if (mode == 3)
    race_start_four(scene);
  else if (mode == 4)
    race_start_trial(scene);
  else if (mode == 5)
    race_start_demo(scene);
}

/* the player's start placement (its vtable's fourth method) */
static void player_place(gaddr player, gaddr pose, f32 value) {
  const PlayerVtbl *vt = (const PlayerVtbl *)hw_ptr(task_at(player)->vtable);
  FARGS1(value);
  CALL3(vt->place.fn, player + (gaddr)vt->place.this_adjust, pose, 0);
}

/* was func_8C029BD0: builds the race. The first time (keep_course 0) it makes the Course and the ActorGroup as
 * children of the scene and populates the actors, else it resets both; then the mode's players. The players are
 * placed from the last start position down: the slot k (from 0) that holds a player gets position count - 1 - k, at
 * the course's start value (the first build) or the scene's (later ones). Then the scene's starting state; keep_course
 * counts the builds. */
void race_scene_build(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  rt_srand(RACE_SEED);
  s->word6c = 0;
  s->word70 = 0;
  s->f74 = F32(0.0f);
  gaddr frame = hw_stack_alloc(START_FRAME_SIZE);
  gaddr pose = frame + START_POSE_AT;
  if (s->keep_course == 0) {
    gaddr memory = syMalloc(COURSE_SIZE);
    gaddr course = memory != 0 ? course_construct(memory) : 0;
    s->course = course;
    task_add_children(task_at(scene), task_at(course));
    memory = syMalloc(ACTOR_GROUP_SIZE);
    gaddr actors = memory != 0 ? actor_group_construct(memory) : 0;
    s->actors = actors;
    task_add_children(task_at(scene), task_at(actors));
    actor_group_populate(s->actors);
    race_start_players(scene, race_setup_at(s->setup)->mode);
    gaddr *slot = s->players;
    for (int32_t i = race_setup_at(s->setup)->player_count - 1; i >= 0; i--, slot++) {
      if (*slot == 0)
        continue;
      f32 value = course_start_value(s->course);
      course_start_position(s->course, i, pose);
      player_place(*slot, pose, value);
    }
  } else {
    course_reset(s->course);
    actor_group_reset(s->actors);
    race_start_players(scene, race_setup_at(s->setup)->mode);
    gaddr *slot = s->players;
    /* func_8C02903E: the set-up's player count, read through its getter this time */
    int32_t count = (int32_t)field_get(s->setup, (uint32_t)offsetof(RaceSetup, player_count));
    for (int32_t i = count - 1; i >= 0; i--, slot++) {
      if (*slot == 0)
        continue;
      f32 value = race_scene_start_value(scene);
      method_self(pose); /* the pose's (empty) constructor */
      course_start_position(s->course, i, pose);
      player_place(*slot, pose, value);
    }
  }
  hw_stack_free(START_FRAME_SIZE);
  for (int32_t i = 0; i < RACE_RACERS; i++) {
    s->focus[i] = i;
    s->player_item[i] = (uint32_t)i;
    s->player_pairs[i][1] = i;
    s->player_pairs[i][0] = i;
  }
  s->music_cue = 0;
  s->pause_cursor = 0;
  s->result = 0;
  s->word30 = RACE_WORD30;
  s->state = RACE_STATE_START;
  s->substate = 0;
  s->pause_fade = F32(0.0f);
  s->pause_fade_step = F32_BITS(0x3C888888u); /* 1/60 */
  s->keep_course++;
}
HW_EXPORT(8c029bd0, race_scene_build(ARG0))
