/* Pen Pen port, S16 (b01): the RaceSetup's (game/race_scene.h, mode 7's scene) construction body (game/
 * race_setup_init.c): func_8C01A8DE sets the vtable and calls it. */
#include "hw.h"
#include "sdk.h"
#include "race_scene.h"
#include "scenery.h" /* random_timer_scaled (core/random.c) */

enum {
  SETUP_STATE_NONE = 7,      /* RaceSetup.state at construction: nothing shown yet */
  SETUP_MODE_DEMO = 5,       /* the attract demo: it starts on the RaceScene (requested = 1), random course */
  SETUP_SHOW_RACE = 1,
  DEMO_COURSES = 4,          /* the demo picks one of the first four courses */
};
#define SETUP_UNSET (-1)

/* the per-course, per-variant tables the computer players' ranges come from (game/race_setup.c reads them as
 * g_range_tables); the demo takes its racer count from word 0 of course c's row (64 bytes) at variant v (32 bytes
 * on) */
#define g_demo_racer_counts ((const int32_t (*)[2][8])hw_ptr(0x8C070758u))

/* was func_8C01A6B6: the set-up for game mode `mode` (0 to 5): an active Task with no links, nothing shown, course
 * and variant 0, it becomes g_race_setup; the players by mode: mode 0 has 8 (the first with words 0 / 0, the others
 * 0 / 1), modes 1-4 have 2, 3, 4, 1 (all 0 / 0); the demo (mode 5) picks a random course among the first four, takes
 * the number of racers from its table, gives racer i word68 = i and a random word88 (0 for the first; else 1 when a
 * random number in [0, 3) is at least 2). Returns the set-up. */
gaddr race_setup_init(gaddr setup, int32_t mode) {
  RaceSetup *s = race_setup_at(setup);
  s->task.linger = 0;
  s->task.active = 1;
  s->task.stops_on_pause = 1;
  s->task.child = 0;
  s->task.next = 0;
  s->task.parent = 0;
  s->task.owner = 0;
  s->state = SETUP_STATE_NONE;
  s->requested = 0;
  s->mode = mode;
  s->course = 0;
  s->word38 = SETUP_UNSET;
  s->word3c = 0;
  s->word40 = 0;
  g_race_setup = setup;
  /* the original stores these three again */
  s->requested = mode == SETUP_MODE_DEMO ? SETUP_SHOW_RACE : 0;
  s->state = SETUP_STATE_NONE;
  s->course = 0;
  for (uint32_t i = 0; i < 8; i++)
    s->words44[i] = SETUP_UNSET;

  switch (s->mode) {
  case 0:
    s->variant = 0;
    s->player_count = 8;
    /* the original's first loop covers player 0 only (byte offsets below 4) */
    s->words68[0] = 0;
    s->words88[0] = 0;
    for (int32_t i = 1; i < s->player_count; i++) {
      s->words68[i] = 0;
      s->words88[i] = 1;
    }
    break;
  case 1:
  case 2:
  case 3:
  case 4: {
    static const int32_t counts[5] = {0, 2, 3, 4, 1};
    s->variant = 0;
    s->player_count = counts[s->mode];
    for (int32_t i = 0; i < s->player_count; i++) {
      s->words68[i] = 0;
      s->words88[i] = 0;
    }
    break;
  }
  case SETUP_MODE_DEMO:
    s->variant = 0;
    s->course = f32_to_int(random_timer_scaled(F32((float)DEMO_COURSES)));
    s->player_count = g_demo_racer_counts[s->course][s->variant][0];
    for (int32_t i = 0; i < s->player_count; i++)
      s->words68[i] = (uint32_t)i;
    s->words88[0] = 0;
    for (int32_t i = 1; i < s->player_count; i++) {
      uint32_t *word = &s->words88[i];
      *word = fgt(F32(2.0f), random_timer_scaled(F32(3.0f))) ? 0 : 1;
    }
    break;
  }
  return setup;
}
HW_EXPORT(8c01a6b6, RET(race_setup_init(ARG0, (int32_t)ARG1)))

/* ---- the computer players ---- */

#include "save.h" /* save_cup0_complete */

/* the range tables (game/race_setup.c): one before cup 0 is complete, one after; per variant a row of 16 words:
 * word 0 the number of racers, words 8 + i player i's word88 */
#define RANGE_ROWS 5
#define RANGE_WORDS 16
#define RANGE_WORD88 8
#define g_setup_range_tables ((const int32_t (*)[RANGE_ROWS][RANGE_WORDS])hw_ptr(0x8C070758u))

#define CHARACTER_SPECIAL 7 /* a racer drawn as character 7 gets word88 0 */

/* the row of the set-up's variant in the table for the save's progress */
static const int32_t *setup_range_row(const RaceSetup *s) {
  int32_t table = save_cup0_complete() != 0 ? 1 : 0;
  return g_setup_range_tables[table][s->variant];
}

/* racer i draws a character in [0, range); character 7 clears its word88 */
static void racer_draw_character(RaceSetup *s, int32_t i, f32 range) {
  s->words68[i] = (uint32_t)f32_to_int(random_timer_scaled(range));
  if ((int32_t)s->words68[i] == CHARACTER_SPECIAL)
    s->words88[i] = 0;
}

/* 1 when racer i's character equals an earlier racer's (the caller draws racer i again) */
static int racer_character_taken(const RaceSetup *s, int32_t i) {
  for (int32_t j = 0; j < i; j++)
    if (s->words68[j] == s->words68[i])
      return 1;
  return 0;
}

/* was func_8C01AA58: the computer players (players 1 on; 2 on in mode 1) of the set-up's mode. Mode 0: the number
 * of racers and each one's word88 from the range table's row of the variant; variants 0-2 draw each racer a
 * character no earlier racer has (variant 0 in [0, 7.02), 1 and 2 in [0, 7.1)), variant 3 gives them all player 0's
 * character, variant 4 makes player 1 character 7 (word88 0). Mode 1: players 2 on draw different characters in
 * [0, 7.1). Mode 5 (the demo): the count from the table, player 0 a character in [0, 7) with word88 0, the others
 * like mode 0's variant 0 but with the demo's own duplicate test (below). */
void race_setup_pick_racers(gaddr setup) {
  RaceSetup *s = race_setup_at(setup);
  const f32 range_a = F32_BITS(0x40E33333u); /* 7.1 */
  const f32 range_b = F32_BITS(0x40E0A3D7u); /* 7.02 */
  if (s->mode == 0) {
    const int32_t *row = setup_range_row(s);
    s->player_count = row[0];
    switch (s->variant) {
    case 0:
      for (int32_t i = 1; i < s->player_count; i++) {
        s->words88[i] = (uint32_t)row[RANGE_WORD88 + i];
        racer_draw_character(s, i, range_b);
        if (racer_character_taken(s, i))
          i--;
      }
      break;
    case 1:
    case 2:
      for (int32_t i = 1; i < s->player_count; i++) {
        s->words88[i] = (uint32_t)row[RANGE_WORD88 + i];
        racer_draw_character(s, i, range_a);
        if (racer_character_taken(s, i))
          i--;
      }
      break;
    case 4:
      s->words88[1] = 0;
      s->words68[1] = CHARACTER_SPECIAL;
      break;
    case 3:
      for (int32_t i = 1; i < s->player_count; i++) {
        s->words88[i] = (uint32_t)row[RANGE_WORD88 + i];
        s->words68[i] = s->words68[0];
        if ((int32_t)s->words68[i] == CHARACTER_SPECIAL)
          s->words88[i] = 0;
      }
      break;
    }
  } else if (s->mode == 1) {
    for (int32_t i = 2; i < s->player_count; i++) {
      racer_draw_character(s, i, range_a);
      if (racer_character_taken(s, i))
        i--;
    }
  } else if (s->mode == 5) {
    const int32_t *row = setup_range_row(s);
    s->player_count = row[0];
    s->words88[0] = 0;
    s->words68[0] = (uint32_t)f32_to_int(random_timer_scaled(F32(7.0f)));
    for (int32_t i = 1; i < s->player_count; i++) {
      s->words88[i] = (uint32_t)row[RANGE_WORD88 + i];
      racer_draw_character(s, i, range_b);
      /* the demo's test does not stop at a match: each match steps i back once more, and the earlier racers are
       * compared with the racer at the new i, while j < i */
      for (int32_t j = 0; j < i;) {
        uint32_t earlier = s->words68[j];
        uint32_t current = s->words68[i];
        j++;
        if (earlier == current)
          i--;
      }
    }
  }
}
HW_EXPORT(8c01aa58, race_setup_pick_racers(ARG0))
