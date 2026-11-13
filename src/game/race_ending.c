/* Pen Pen port, S18 (b03): the race scene's ending (RACE_STATE_ENDING, game/race_ending.c).
 *
 * When the race is over the scene runs four substeps (RaceScene.substate):
 *   0  the race's outcome: the final time, the places handed to the set-up, the place's music and the progress
 *      record (unlocks, counts, record times); then the two result sprites are put back at the screen's centre;
 *   1  the sprites slide toward their corner of the screen while they fade, for 720 frames;
 *   2  the ending's music, and every player put at its place on the finish;
 *   3  the after-race menu (race_results_menu_update). */
#include "hw.h"
#include "sdk.h"
#include "race_scene.h"
#include "racer.h"
#include "sound.h"
#include "save.h"
#include "scenery.h"
#include "actor_b05.h"
#include "../core/task.h"
#include "../core/vec.h"

/* hand-written but not declared in a header */
void race_results_menu_update(gaddr scene); /* game/race_results_menu.c (func_8C02D150) */
/* sd_CmdReset (Manatee): drop the sound driver's queued commands */
static inline void sd_CmdReset(void) { CALL0(0x8C09A524u); }

enum { ENDING_OUTCOME = 0, ENDING_SLIDE = 1, ENDING_FINISH = 2, ENDING_MENU = 3 };

#define ENDING_SPRITE_COUNT 5          /* the outcome resets five bodies (bodies[] / values1[]) */
#define ENDING_SLIDING      2          /* two of them slide in substep 1 */
#define ENDING_FRAMES       720        /* RaceScene.word30 counts substep 1's frames */
#define ENDING_FADE_STEP    F32_BITS(0x3ADA740Du) /* 1/600 a frame */
#define ENDING_PULL         F32_BITS(0x3E800000u) /* 0.25 */
#define SPRITE_DAMPING      F32_BITS(0x3F666666u) /* 0.9 */
#define TIME_JITTER         F32_BITS(0x3C888888u) /* 1/60: the final time gets a random part of a frame */
#define RECORD_REPLAY       2          /* RaceScene.word_ca8: the race was a recording played back */

/* the place music (by the first player's place, 0-2; 41 for the others) and the ending's */
enum { MUSIC_PLACE_FIRST = 38, MUSIC_PLACE_OTHER = 41, MUSIC_ENDING = 42 };
/* progress flags the unlocks hand out */
enum { FLAG_UNLOCK_21 = 21, FLAG_UNLOCK_22 = 22, UNLOCK_AT_20 = 20, UNLOCK_AT_40 = 40, BYTE44_UNLOCK = 7 };

/* The sprites' corners: six function-static Vec3s (C++ statics built on first use, each with its guard word),
 * two per layout: the full screen (modes 0 and 4), the two-player and the four-player one. */
#define ENDING_CORNERS 6
#define g_ending_corner_built(i) GUEST_VAR(int32_t, 0x8C1BAB00u + 4u * (uint32_t)(i)) /* the guards */
#define ENDING_CORNER_ADDR(i) (0x8C09624Cu + 12u * (uint32_t)(i))                    /* the Vec3s */
/* the size the second body (values1[]) is pulled toward, per layout */
#define g_ending_scale(i) GUEST_VAR(f32, 0x8C07FBFCu + 4u * (uint32_t)(i))

static const uint32_t corner_x[ENDING_CORNERS] = {
  0x42E80000u, 0x439A0000u, 0x42E80000u, 0x439A0000u, 0x00000000u, 0x431A0000u, /* 116 308 116 308 0 154 */
};
static const uint32_t corner_y[ENDING_CORNERS] = {
  0x43400000u, 0x43400000u, 0x42A00000u, 0x42A00000u, 0x42A80000u, 0x42A80000u, /* 192 192 80 80 84 84 */
};

static RaceSetup *scene_setup(RaceScene *s) { return race_setup_at(s->setup); }

/* the bodies back at the screen's centre, still, at size 0 */
static void ending_sprites_reset(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  for (int32_t i = 0; i < ENDING_SPRITE_COUNT; i++) {
    vec3_set(F32(320.0f), F32(240.0f), F32(0.0f), member_addr(scene, RaceScene, bodies[i]));
    vec3_fill(member_addr(scene, RaceScene, bodies[i].damping), SPRITE_DAMPING);
    s->values1[i].value = F32(0.0f);
    s->values1[i].damping = SPRITE_DAMPING;
  }
}

/* the first player won (place 0) its course in the single-player game: the course's item, a random unlock, the
 * unlocks for 20 and 40 items, then the win counted */
static void ending_record_win(RaceScene *s) {
  RaceSetup *setup = scene_setup(s);
  int32_t cup = save_cup0_complete() ? 1 : 0;
  if (save_course_flag(cup, setup->course, setup->variant) == 0) {
    save_set_course_flag(cup, setup->course, setup->variant);
    int32_t flag;
    do
      flag = save_pick_unset_flag();
    while (flag == FLAG_UNLOCK_21 || flag == FLAG_UNLOCK_22);
    save_set_flag(flag);
    save_grant_rewards();
    setup->word38 = flag; /* func_8C02905C */
    if (save_course_flags_count() == UNLOCK_AT_20) {
      save_set_flag(FLAG_UNLOCK_22);
      save_set_byte44(BYTE44_UNLOCK, FLAG_UNLOCK_22);
      race_setup_set_word3c(s->setup, 1);
    }
    if (save_course_flags_count() == UNLOCK_AT_40) {
      save_set_flag(FLAG_UNLOCK_21);
      race_setup_set_word40(s->setup, 1);
    }
  }
  save_count_b((int32_t)setup->words68[0]);
}

/* mode 0 (against the computer): the place's music, the race counted, and by the first player's place */
static void ending_outcome_versus(RaceScene *s) {
  RaceSetup *setup = scene_setup(s);
  int32_t track;
  switch (s->player_item[0]) {
  case 0: track = MUSIC_PLACE_FIRST; break;
  case 1: track = MUSIC_PLACE_FIRST + 1; break;
  case 2: track = MUSIC_PLACE_FIRST + 2; break;
  default: track = MUSIC_PLACE_OTHER; break;
  }
  music_play(track, 0, 1);
  save_count_a((int32_t)setup->words68[0]);
  if (s->player_item[0] == 0)
    ending_record_win(s);
  else if ((int32_t)s->player_item[0] == setup->player_count - 1 && setup->variant != 4)
    save_count_c((int32_t)setup->words68[0]); /* last (not on variant 4) */
}

/* the other modes: the first music, and a new record time keeps the time, the player's word and, on variant 0,
 * the first player's four lap times */
static void ending_outcome_record(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  RaceSetup *setup = scene_setup(s);
  music_play(MUSIC_PLACE_FIRST, 0, 1);
  f32 time = s->f78;
  if (!fgt(save_time_a(setup->course, setup->variant), time))
    return;
  save_set_time_a(setup->course, setup->variant, s->f78);
  save_set_rank(setup->course, setup->variant, (int32_t)setup->words68[0]);
  if (setup->variant != 0)
    return;
  for (int32_t i = 0; i < 4; i++) {
    f32 lap = racer_time(s->players[0], i);
    save_set_time_b(setup->course, i, lap);
  }
}

/* substep 0: the race's outcome */
static void ending_outcome(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  sound_reset_requests();
  sd_CmdReset();
  if (s->word_ca8 != RECORD_REPLAY) {
    s->f74 = fadd(s->f74, random_scaled(TIME_JITTER));
    s->f78 = s->f74;
    for (int32_t i = 0; i < RACE_RACERS; i++)
      s->words7c[i] = (int32_t)s->player_item[i]; /* func_8C0291EA */
    /* the set-up gets the player at each place: words44[place] = player (S18: the writer had it the other way
     * round, words44[player] = place; found by a synthetic sample: the four words held the inverse permutation) */
    for (int32_t i = 0; i < RACE_RACERS; i++)
      race_setup_set_word44(s->setup, s->words7c[i], i);
    if (scene_setup(s)->mode == 0)
      ending_outcome_versus(s);
    else
      ending_outcome_record(scene);
  }
  ending_sprites_reset(scene);
  s->sprite_fade = F32(1.0f);
  s->word30 = ENDING_FRAMES;
  s->result = 0;
  s->word70 = 1;
  s->substate++;
  tasktree_set_pause(tasktree_at(s->task.owner), 0);
}

/* substep 1: the corners built on first use; the two sprites pulled toward theirs (by the layout of the game's
 * mode) as the sprites fade; after 720 frames, the next substep */
static void ending_slide(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  gaddr tmp = hw_stack_alloc(12); /* the original's temporary Vec3 */
  for (int32_t i = 0; i < ENDING_CORNERS; i++) {
    if (g_ending_corner_built(i) == 0) {
      g_ending_corner_built(i)++;
      vec3_set(F32_BITS(corner_x[i]), F32_BITS(corner_y[i]), F32(0.0f), tmp);
      vec3_copy(ENDING_CORNER_ADDR(i), tmp);
    }
  }
  hw_stack_free(12);
  gaddr corners;
  f32 scale;
  switch (scene_setup(s)->mode) {
  case 0:
  case 4: corners = ENDING_CORNER_ADDR(0); scale = g_ending_scale(0); break;
  case 1: corners = ENDING_CORNER_ADDR(2); scale = g_ending_scale(1); break;
  case 2: corners = ENDING_CORNER_ADDR(4); scale = g_ending_scale(2); break;
  case 3: corners = ENDING_CORNER_ADDR(4); scale = g_ending_scale(2); break;
  default: corners = ENDING_CORNER_ADDR(0); scale = g_ending_scale(0); break;
  }
  s->sprite_fade = fsub(s->sprite_fade, ENDING_FADE_STEP);
  if (fgt(s->sprite_fade, F32(0.0f))) {
    for (int32_t i = 0; i < ENDING_SLIDING; i++) {
      body_pull(ENDING_PULL, member_addr(scene, RaceScene, bodies[i]), corners + 12u * (uint32_t)i);
      body1_pull(scale, ENDING_PULL, member_addr(scene, RaceScene, values1[i]));
      body_step(member_addr(scene, RaceScene, bodies[i]));
      body1_step(member_addr(scene, RaceScene, values1[i]));
    }
  }
  int32_t left = (int32_t)s->word30 - 1;
  s->word30 = (uint32_t)left;
  if (left < 0)
    s->substate++;
}

/* the player's virtual finish (RacerVtbl.finish) */
static void racer_vfinish(gaddr racer, gaddr position, f32 value) {
  const RacerVtbl *vt = (const RacerVtbl *)hw_ptr(racer_at(racer)->task.vtable);
  FARGS1(value);
  hw_call(vt->finish.fn, 2, racer + (uint32_t)vt->finish.this_adjust, position, 0, 0);
}

/* substep 2: the ending's music, each player at its place's position on the finish */
static void ending_finish(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  sound_reset_requests();
  sd_CmdReset();
  music_play(MUSIC_ENDING, 15, 1);
  gaddr position = hw_stack_alloc(12); /* a Vec3 (its constructor, func_8C028D5C, does nothing) */
  for (int32_t i = 0; i < scene_setup(s)->player_count; i++) {
    race_scene_finish_position(scene, s->words7c[i], position);
    f32 value = race_scene_finish_value(scene);
    racer_vfinish(s->players[i], position, value);
  }
  hw_stack_free(12);
  s->substate++;
}

/* was func_8C02AF9C: the race scene's ending (RACE_STATE_ENDING), one substep a frame */
void race_scene_ending(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  switch (s->substate) {
  case ENDING_OUTCOME:
    ending_outcome(scene);
    break;
  case ENDING_SLIDE:
    ending_slide(scene);
    break;
  case ENDING_FINISH:
    ending_finish(scene);
    race_results_menu_update(scene); /* the menu's first frame follows at once */
    break;
  case ENDING_MENU:
    race_results_menu_update(scene);
    break;
  }
}
HW_EXPORT(8c02af9c, race_scene_ending(ARG0))
