/* Pen Pen port, S18 (b03): the players at the start of a mode-2 race (three human players on the four-way split),
 * beside the other modes of game/race_players.c (which see). */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/view.h"
#include "../core/vec.h"
#include "race_scene.h"
#include "racer.h"
#include "course.h"

#define THREE_PLAYERS 3

/* func_8C041F70 (game/course.c, still translated): start position i of the course */
static void course_start_position(gaddr course, int32_t i, gaddr out) { CALL3(0x8C041F70u, course, i, out); }

/* the racer's virtual reset (RacerVtbl.reset): at `position`, word 0, facing `heading` */
static void racer_vreset(gaddr racer, gaddr position, f32 heading) {
  const RacerVtbl *vt = (const RacerVtbl *)hw_ptr(racer_at(racer)->task.vtable);
  FARGS1(heading);
  hw_call(vt->reset.fn, 3, racer + (uint32_t)vt->reset.this_adjust, position, 0, 0);
}

static const int32_t THREE_VIEWS[4] = {0, 1, 2, -1};
static const int32_t PLAYERS_SHOWN[RACE_RACERS] = {0, 1, 2, -1, -1, -1, -1, -1};

/* was func_8C02C1AA: mode 2: players 0-2 (Racers, a new one for an empty slot, with its two words of the set-up)
 * at start positions 0-2, facing the course's start heading; four views, the fourth showing no one */
void race_start_three(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  gaddr position = hw_stack_alloc(sizeof(Vec3)); /* the original's frame: the position at 0 (constructor
                                                  * func_8C028D5C does nothing) */
  f32 heading = course_value(s->course, -1);
  for (int32_t i = 0; i < THREE_PLAYERS; i++) {
    course_start_position(s->course, i, position);
    if (s->players[i] == 0) {
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
    racer_vreset(s->players[i], position, heading);
  }
  hw_stack_free(sizeof(Vec3));
  view_set_split(SPLIT_FOUR);
  for (int k = 0; k < 4; k++)
    s->view_player[k] = THREE_VIEWS[k];
  for (int k = 0; k < RACE_RACERS; k++)
    s->player_view[k] = PLAYERS_SHOWN[k];
}
HW_EXPORT(8c02c1aa, race_start_three(ARG0))
