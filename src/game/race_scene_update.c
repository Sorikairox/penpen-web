/* Pen Pen port, S16 (b03): the race scene's update (game/race_scene_update.c): one step of the scene's state
 * machine, then (unless the tree is paused) the slot collisions and the music, and the pause screen's fade. */
#include "hw.h"
#include "sdk.h"
#include "race_scene.h"
#include "sound.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "../core/inline.h"

/* the scene's states (RaceScene.state) */
enum {
  RACE_STATE_STARTING = 0,  /* the race starts (race_scene_start) */
  RACE_STATE_1 = 1,
  RACE_STATE_COUNTDOWN = 2, /* word30 counts down, then the results' set-up */
  RACE_STATE_RESULTS_ = 3,  /* RACE_STATE_RESULTS (race_scene.h) */
  RACE_STATE_ENDING_ = 4,   /* RACE_STATE_ENDING */
  /* 5-9 leave the race: the set-up is asked for another scene */
};

/* a music cue: the track, its fade, whether it repeats (the first word is not read here) */
typedef struct RaceMusicCue {
  int32_t unknown00;
  int32_t track;   /* 0x04 */
  int32_t fade;    /* 0x08 */
  int32_t repeat;  /* 0x0C: != 0 */
} RaceMusicCue;
HW_SIZE(RaceMusicCue, 16);
#define RACE_MUSIC_KINDS    3 /* 0: against the computer or the demo, 1: the time trial, 2: the split screens */
#define RACE_MUSIC_VARIANTS 5
#define RACE_MUSIC_CUES     8
/* by course, kind, variant and the scene's music cue */
#define g_race_music \
  ((const RaceMusicCue (*)[RACE_MUSIC_KINDS][RACE_MUSIC_VARIANTS][RACE_MUSIC_CUES])hw_ptr(0x8C067314u))

#define RESULT_SPRITES 2
#define RESULT_PAUSE_DELAY 30
#define PAUSE_FADE_STEP F32_BITS(0x3C888888u) /* 1/60 */

/* hand-written but not declared in a header */
/* func_8C02F2C4 (game/race_scene.c): the race's music */
static void race_music_update(gaddr scene) { CALL1(0x8C02F2C4u, scene); }
/* func_8C02903A / func_8C029036 (core/inline.c): the set-up's course / variant, through their getters */
static int32_t setup_course(gaddr setup) { return (int32_t)field_get(setup, (uint32_t)offsetof(RaceSetup, course)); }
static int32_t setup_variant(gaddr setup) { return (int32_t)field_get(setup, (uint32_t)offsetof(RaceSetup, variant)); }
/* func_8C029032 (copies/core/inline.c): the set-up's mode */
static int32_t setup_mode(gaddr setup) { return (int32_t)field_get(setup, (uint32_t)offsetof(RaceSetup, mode)); }
/* still translated */
static void race_scene_state1(gaddr scene) { CALL1(0x8C029ED2u, scene); } /* func_8C029ED2 */
static void race_scene_ending(gaddr scene) { CALL1(0x8C02AF9Cu, scene); } /* func_8C02AF9C */

/* the end of the countdown: the scene's next music cue, the result sprites reset, then the results */
static void race_scene_countdown_over(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  s->word6c = 1;
  sound_play(7, 0, 15);
  int32_t kind = 0;
  int32_t mode = setup_mode(s->setup);
  if (mode == 0 || mode == 5)
    kind = 0;
  else if (mode == 4)
    kind = 1;
  else if (mode == 1 || mode == 2 || mode == 3)
    kind = 2;
  /* the original reads the course and variant again for each word of the cue */
  int32_t repeat = g_race_music[setup_course(s->setup)][kind][setup_variant(s->setup)][s->music_cue].repeat != 0;
  int32_t fade = g_race_music[setup_course(s->setup)][kind][setup_variant(s->setup)][s->music_cue].fade;
  int32_t track = g_race_music[setup_course(s->setup)][kind][setup_variant(s->setup)][s->music_cue].track;
  music_play(track, fade, repeat);
  s->music_cue++;
  for (int32_t i = 0; i < RESULT_SPRITES; i++) {
    vec3_set(F32(320.0f), F32(240.0f), F32(0.0f), member_addr(scene, RaceScene, bodies[i]));
    vec3_fill(member_addr(scene, RaceScene, bodies[i].damping), F32_BITS(0x3F666666u)); /* 0.9 */
    s->values1[i].value = F32(0.0f);
    s->values1[i].damping = F32_BITS(0x3F666666u); /* 0.9 */
  }
  s->sprite_fade = F32(1.0f);
  s->pause_delay = RESULT_PAUSE_DELAY;
  field_set_and_clear(scene, RACE_STATE_RESULTS); /* func_8C0291E2: state 3, substate 0 */
}

/* was func_8C0298E8: the scene's update */
void race_scene_update(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  switch (s->state) {
  case RACE_STATE_STARTING:
    race_scene_start(scene);
    break;
  case RACE_STATE_1:
    race_scene_state1(scene);
    break;
  case RACE_STATE_COUNTDOWN: {
    int32_t left = (int32_t)s->word30 - 1;
    s->word30 = (uint32_t)left;
    if (left < 0)
      race_scene_countdown_over(scene);
    break;
  }
  case RACE_STATE_RESULTS_:
    race_scene_race_step(scene);
    break;
  case RACE_STATE_ENDING_:
    race_scene_ending(scene);
    break;
  /* leaving: the set-up is asked for its next scene (func_8C02902E) */
  case 5: field_set(s->setup, (uint32_t)offsetof(RaceSetup, requested), 2); break;
  case 6: field_set(s->setup, (uint32_t)offsetof(RaceSetup, requested), 0); break;
  case 7: field_set(s->setup, (uint32_t)offsetof(RaceSetup, requested), 6); break;
  case 8: field_set(s->setup, (uint32_t)offsetof(RaceSetup, requested), 3); break;
  case 9: field_set(s->setup, (uint32_t)offsetof(RaceSetup, requested), 4); break;
  }
  if (!task_is_paused(&s->task)) {
    slot_list_collide(member_addr(scene, RaceScene, slots));
    race_music_update(scene);
  }
  /* the pause screen's fade runs between 0 and 1 and turns at either end */
  s->pause_fade = fadd(s->pause_fade, s->pause_fade_step);
  if (fgt(s->pause_fade, F32(1.0f))) {
    s->pause_fade = F32(1.0f);
    s->pause_fade_step = fneg(PAUSE_FADE_STEP);
  }
  if (fgt(F32(0.0f), s->pause_fade)) {
    s->pause_fade = F32(0.0f);
    s->pause_fade_step = PAUSE_FADE_STEP;
  }
}
HW_EXPORT(8c0298e8, race_scene_update(ARG0))
