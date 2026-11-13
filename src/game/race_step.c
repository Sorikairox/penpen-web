/* Pen Pen port, S17 (r2): the race scene's step while the race runs (game/race_step.c): state 3 of
 * race_scene_update (race_scene.h calls it RACE_STATE_RESULTS; it is the race itself: the countdown's end enters it
 * and a finished player moves the scene on to state 4).
 *
 * One step: the pause screen while the tree is paused; else the race's recording (record or play back the stick
 * words and, every 8 frames, each player's position, velocity and heading), the scene's clock, the two start sprites
 * pulled to their places while they fade, the players' ranking along the course, the offsets, distances and
 * directions between every two players and each one's nearest rival, the Start button / a pulled controller that
 * pauses the race, and the end of the race. */
#include "hw.h"
#include "sdk.h"
#include "race_scene.h"
#include "racer.h"
#include "scenery.h" /* race_player_body */
#include "../core/task.h"
#include "../core/pad.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "../core/inline.h"

/* RaceScene.word_ca8: the race's recording */
enum { RACE_RECORD = 1, RACE_PLAYBACK = 2 };
#define RECORD_SAMPLE_FRAMES 8 /* a sample of the players every 8 frames */
#define RACE_STICK_PORTS 4     /* RaceScene.scores: a stick word per port */
#define RACE_STEP_SPRITES 2    /* the start sprites (bodies[0..1], values1[0..1]) */
#define RACE_FINISHED 1        /* racer_state: the player has finished */
#define PAUSE_BUTTON 8         /* port_pressed's mask: Start */
#define PAUSE_DELAY 30
#define NO_PLAYER (-1)

#define g_record_frames GUEST_VAR(int32_t, 0x8C07FBF8u) /* the recording's length in frames (0x8000) */
/* the start sprites' places, set the first time a race runs (Vec3[2]) */
#define START_SPRITE_TARGETS 0x8C096234u
#define g_start_target0_ready GUEST_VAR(int32_t, 0x8C1BAAF8u)
#define g_start_target1_ready GUEST_VAR(int32_t, 0x8C1BAAFCu)

#define CLOCK_STEP F32_BITS(0x3C888888u)  /* 1/60 */
#define SPRITE_FADE_STEP F32_BITS(0x3BB60B60u) /* 1/180 */
#define SPRITE_PULL F32(0.25f)
#define FAR_AWAY F32(10000000.0f)         /* a player's distance to itself */
#define ANGLE_UNITS F32(65536.0f)         /* a turn in Ninja angle units */
#define TWO_PI F32_BITS(0x40C90FD7u)      /* 6.2831855 */

/* hand-written but not declared in a header */
/* func_8C02CE48 (game/race_hud.c): the pause screen's step */
static void race_pause_update(gaddr scene) { CALL1(0x8C02CE48u, scene); }
/* func_8C02C7C8 (game/race_scene.c): player i's heading */
static f32 race_player_heading(gaddr scene, int32_t i) {
  CALL2(0x8C02C7C8u, scene, i);
  return hw_fret();
}
/* still translated: the Racer's setters (its moving body's position and velocity copied in, its heading) */
static void racer_set_position(gaddr racer, gaddr v) { CALL2(0x8C0291ACu, racer, v); } /* func_8C0291AC */
static void racer_set_velocity(gaddr racer, gaddr v) { CALL2(0x8C0291C2u, racer, v); } /* func_8C0291C2 */
static void racer_set_heading(gaddr racer, f32 heading) {                             /* func_8C0291DA */
  FARGS1(heading);
  CALL1(0x8C0291DAu, racer);
}
/* func_8C0628E8 (C runtime, float): atan2f(y, x) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* a played-back sample of one player, on the guest stack (the setters read it there) */
typedef struct ReplaySample {
  f32 heading;  /* 0x00 */
  Vec3 velocity; /* 0x04 */
  Vec3 position; /* 0x10 */
} ReplaySample;
HW_OFFSET(ReplaySample, position, 0x10);
HW_SIZE(ReplaySample, 0x1C);

static int32_t player_count(const RaceScene *s) { return race_setup_at(s->setup)->player_count; }
static int16_t *stick_marks(gaddr marks) { return (int16_t *)hw_ptr(marks); }
static f32 *heading_list(gaddr list) { return (f32 *)hw_ptr(list); }
/* an angle in radians as a whole number of Ninja angle units, kept as a float */
static f32 angle_units(f32 radians) { return f32_from_int(f32_to_int(fdiv(fmul(radians, ANGLE_UNITS), TWO_PI))); }

/* the stick words of ports 3 down to `last` into scores[] */
static void read_sticks(gaddr scene, int32_t last) {
  RaceScene *s = race_scene_at(scene);
  for (int32_t port = RACE_STICK_PORTS - 1; port >= last; port--)
    s->scores[port] = (int16_t)pad_stick_word(member_addr(scene, RaceScene, pad), port);
}

/* recording: port 0's stick word for this frame (when there is a buffer), the sticks, and every 8 frames each
 * player's position, velocity and heading */
static void race_record(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (s->marks_a != 0) {
    int16_t *mark = &stick_marks(s->marks_a)[s->word_cac];
    *mark = (int16_t)pad_stick_word(member_addr(scene, RaceScene, pad), 0);
  }
  read_sticks(scene, 0);
  if (++s->word_cb8 < RECORD_SAMPLE_FRAMES)
    return;
  s->word_cb8 = 0;
  int32_t n = player_count(s);
  for (int32_t i = 0; i < n; i++) {
    memmove(vec3_at(s->vec_lists_a[i]) + s->word_cbc, hw_ptr(race_player_body(scene, i)), sizeof(Vec3));
    memmove(vec3_at(s->vec_lists_b[i]) + s->word_cbc, hw_ptr(race_player_velocity(scene, i)), sizeof(Vec3));
    f32 *heading = &heading_list(s->word_lists[i])[s->word_cbc];
    *heading = race_player_heading(scene, i);
  }
  s->word_cbc++;
}

/* playback: port 0 from the recording, the other sticks read; every 8 frames each player is checked against its
 * sample, and from the first one that differs on, the players are put back on their samples */
static void race_play_back(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  s->scores[0] = stick_marks(s->marks_a)[s->word_cac];
  read_sticks(scene, 1);
  int32_t same = 1; /* not reset per player: once one differs, every later player is set too */
  if (++s->word_cb8 < RECORD_SAMPLE_FRAMES)
    return;
  s->word_cb8 = 0;
  int32_t n = player_count(s);
  gaddr sample = hw_stack_alloc(sizeof(ReplaySample));
  ReplaySample *smp = (ReplaySample *)hw_ptr(sample);
  for (int32_t i = 0; i < n; i++) {
    vec3_copy(member_addr(sample, ReplaySample, position),
              s->vec_lists_a[i] + (gaddr)s->word_cbc * (gaddr)sizeof(Vec3));
    vec3_copy(member_addr(sample, ReplaySample, velocity),
              s->vec_lists_b[i] + (gaddr)s->word_cbc * (gaddr)sizeof(Vec3));
    smp->heading = heading_list(s->word_lists[i])[s->word_cbc];
    const Vec3 *position = vec3_at(race_player_body(scene, i));
    if (!feq(position->x, smp->position.x)) same = 0;
    if (!feq(position->y, smp->position.y)) same = 0;
    if (!feq(position->z, smp->position.z)) same = 0;
    const Vec3 *velocity = vec3_at(race_player_velocity(scene, i));
    if (!feq(velocity->x, smp->velocity.x)) same = 0;
    if (!feq(velocity->y, smp->velocity.y)) same = 0;
    if (!feq(velocity->z, smp->velocity.z)) same = 0;
    if (!feq(smp->heading, race_player_heading(scene, i))) same = 0;
    if (!same) {
      racer_set_position(s->players[i], member_addr(sample, ReplaySample, position));
      racer_set_velocity(s->players[i], member_addr(sample, ReplaySample, velocity));
      racer_set_heading(s->players[i], smp->heading);
    }
  }
  hw_stack_free(sizeof(ReplaySample));
  s->word_cbc++;
}

/* a start sprite's place, the first time: (x, 192, 0) */
static void start_target_init(int32_t k, f32 x) {
  gaddr v = hw_stack_alloc(sizeof(Vec3));
  vec3_set(x, F32(192.0f), F32(0.0f), v);
  vec3_copy(START_SPRITE_TARGETS + (gaddr)k * (gaddr)sizeof(Vec3), v);
  hw_stack_free(sizeof(Vec3));
}

/* focus[at..] moves one place down (the last one drops out) and player i takes place `at` */
static void rank_insert(RaceScene *s, int32_t at, int32_t i) {
  for (int32_t k = player_count(s) - 1; k > at; k--)
    s->focus[k] = s->focus[k - 1];
  s->focus[at] = i;
}

/* player i ahead of the player ranked at `at`: a later course cell, or the same cell and ahead along the cell's
 * direction */
static int32_t rank_ahead(gaddr scene, int32_t i, int32_t at) {
  RaceScene *s = race_scene_at(scene);
  if ((int32_t)racer_cell(s->players[i]) > (int32_t)racer_cell(s->players[s->focus[at]]))
    return 1;
  if ((int32_t)racer_cell(s->players[i]) != (int32_t)racer_cell(s->players[s->focus[at]]))
    return 0;
  gaddr mine = racer_body(s->players[i]);
  gaddr theirs = racer_body(s->players[s->focus[at]]);
  gaddr offset = hw_stack_alloc(sizeof(Vec3));
  vec3_diff(offset, theirs, mine);
  f32 along = njInnerProduct(offset, race_scene_cell_point(scene, (int32_t)racer_cell(s->players[i])));
  hw_stack_free(sizeof(Vec3));
  return fgt(along, F32(0.0f));
}

/* the ranking: focus[rank] = the player (an insertion sort, first place first), player_item[player] = its rank */
static void race_rank(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  for (int32_t i = 0; i < player_count(s); i++)
    s->focus[i] = NO_PLAYER;
  for (int32_t i = 0; i < player_count(s); i++) {
    for (int32_t at = 0; at < player_count(s); at++) {
      if (s->focus[at] == NO_PLAYER) {
        s->focus[at] = i;
        break;
      }
      if (rank_ahead(scene, i, at)) {
        rank_insert(s, at, i);
        break;
      }
    }
  }
  for (int32_t i = 0; i < player_count(s); i++)
    s->player_item[s->focus[i]] = (uint32_t)i;
}

/* between every two players: the offset, its length and direction, the direction's heading and pitch */
static void race_measure(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  for (int32_t i = 0; i < player_count(s); i++) {
    for (int32_t j = 0; j < player_count(s); j++) {
      gaddr offset = member_addr(scene, RaceScene, vec15c[i * 8 + j]);
      if (i == j) {
        vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), offset);
        s->values[i][j] = FAR_AWAY;
        s->headings[i][j] = F32(0.0f);
        s->pitches[i][j] = F32(0.0f);
        continue;
      }
      gaddr from = racer_body(s->players[i]);
      vec3_diff(offset, from, racer_body(s->players[j]));
      s->values[i][j] = vec3_normalized(offset, member_addr(scene, RaceScene, points[i][j]));
      const Vec3 *dir = &s->points[i][j];
      s->headings[i][j] = angle_units(rt_atan2f(dir->x, dir->z));
      f32 flat = fadd(f32_square(dir->x), f32_square(dir->z));
      s->pitches[i][j] = angle_units(rt_atan2f(fneg(dir->y), njSqrt(flat)));
    }
  }
}

/* each player's nearest rival: player_pairs[i][0] (the previous one kept in [1]) */
static void race_nearest(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  for (int32_t i = 0; i < player_count(s); i++) {
    s->player_pairs[i][1] = s->player_pairs[i][0];
    s->player_pairs[i][0] = i;
  }
  for (int32_t i = 0; i < player_count(s); i++)
    for (int32_t j = 0; j < player_count(s); j++)
      if (i != j && fgt(s->values[i][s->player_pairs[i][0]], s->values[i][j]))
        s->player_pairs[i][0] = j;
}

/* Start pressed or a controller pulled out, on the ports the game's mode uses (from the highest down): the port
 * and whether it was pulled go to pause_port / pad_removed; returns the last test's result (non-zero: pause) */
static int32_t race_pause_asked(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  int32_t mode = (int32_t)field_get(s->setup, (uint32_t)offsetof(RaceSetup, mode)); /* func_8C029032 */
  int32_t first;
  if (mode == 3)
    first = 3;
  else if (mode == 2)
    first = 2;
  else if (mode == 1)
    first = 1;
  else if (mode == 0 || mode == 4)
    first = 0;
  else
    return 0;
  int32_t asked = 0;
  for (int32_t port = first; port >= 0; port--) {
    asked = port_pressed(port, PAUSE_BUTTON);
    if (asked) {
      s->pause_port = port;
      s->pad_removed = 0;
      break;
    }
    asked = port_has_controller(port) == 0; /* func_8C028F44 (it is passed the scene's pad, unread) */
    if (asked) {
      s->pause_port = port;
      s->pad_removed = 1;
      break;
    }
  }
  return asked;
}

/* the race is over: state 4 (the ending), substate 2; the final ranking (words7c) copied to player_item and its
 * inverse to focus; the other slots cleared */
static void race_finish(gaddr scene) {
  field_set_and_clear(scene, RACE_STATE_ENDING); /* func_8C0291E2 */
  RaceScene *s = race_scene_at(scene);
  s->substate = 2;
  int32_t i;
  for (i = 0; i < player_count(s); i++) {
    s->player_item[i] = (uint32_t)s->words7c[i];
    s->focus[s->words7c[i]] = i;
  }
  for (; i < RACE_RACERS; i++) {
    s->focus[i] = 0;
    s->player_item[i] = 0;
  }
}

/* was func_8C029EFA: state 3's step (see the top of the file) */
void race_scene_race_step(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  s->pause_delay--;
  if (task_is_paused(&s->task)) { /* func_8C028E64 */
    race_pause_update(scene);
    return;
  }

  /* the recording */
  if (++s->word_cac < g_record_frames) {
    if (s->word_ca8 == RACE_RECORD)
      race_record(scene);
    else if (s->word_ca8 == RACE_PLAYBACK)
      race_play_back(scene);
  } else {
    /* the recording is full: its last frame takes port 0's stick word, whatever the mode (no buffer check) */
    s->word_cac = g_record_frames - 1;
    int16_t *mark = &stick_marks(s->marks_a)[s->word_cac];
    *mark = (int16_t)pad_stick_word(member_addr(scene, RaceScene, pad), 0);
    read_sticks(scene, 0);
  }

  s->f74 = fadd(s->f74, CLOCK_STEP);

  if (g_start_target0_ready == 0) {
    g_start_target0_ready++;
    start_target_init(0, F32(116.0f));
  }
  if (g_start_target1_ready == 0) {
    g_start_target1_ready++;
    start_target_init(1, F32(308.0f));
  }

  /* the start sprites, while they fade: pulled to their places, their scale to 1 */
  s->sprite_fade = fsub(s->sprite_fade, SPRITE_FADE_STEP);
  if (fgt(s->sprite_fade, F32(0.0f))) {
    for (int32_t i = 0; i < RACE_STEP_SPRITES; i++) {
      body_pull(SPRITE_PULL, member_addr(scene, RaceScene, bodies[i]),
                START_SPRITE_TARGETS + (gaddr)i * (gaddr)sizeof(Vec3));
      body1_pull(F32(1.0f), SPRITE_PULL, member_addr(scene, RaceScene, values1[i]));
      body_step(member_addr(scene, RaceScene, bodies[i]));
      body1_step(member_addr(scene, RaceScene, values1[i]));
    }
  }

  race_rank(scene);
  race_measure(scene);
  race_nearest(scene);

  int32_t asked = race_pause_asked(scene);
  if (s->pause_delay < 0 && asked) {
    s->pause_delay = PAUSE_DELAY;
    if (s->word_ca8 == RACE_RECORD)
      tasktree_set_pause(tasktree_at(s->task.owner), 1); /* func_8C010508 */
    else
      race_finish(scene);
  }
  /* the playback has reached the recording's end */
  if (s->word_ca8 == RACE_PLAYBACK && s->word_cac == g_record_frames - 1)
    race_finish(scene);

  /* a finished player ends the race: player 0 against the computer (modes 0 and 4), any player in modes 1-3 */
  int32_t mode = (int32_t)field_get(s->setup, (uint32_t)offsetof(RaceSetup, mode)); /* func_8C029032 */
  if (mode == 0 || mode == 4) {
    if (racer_state(s->players[0]) == RACE_FINISHED)
      field_set_and_clear(scene, RACE_STATE_ENDING);
  } else if (mode == 1 || mode == 2 || mode == 3) {
    for (int32_t i = 0; i < player_count(s); i++)
      if (racer_state(s->players[i]) == RACE_FINISHED)
        field_set_and_clear(scene, RACE_STATE_ENDING);
  }
}
HW_EXPORT(8c029efa, race_scene_race_step(ARG0))
