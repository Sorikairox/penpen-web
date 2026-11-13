/* Pen Pen port, S17 b03: the RaceSetup's remaining setters and getters (game/race_scene.h), and a few one-line
 * methods of the RaceScene (its course's finish, its build-and-reset for state 1, its destructor wrapper) and of
 * the SetupMenu (its destructor wrapper). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../game/race_scene.h"
#include "../game/course.h"
#include "../game/menu.h"

/* ---- RaceSetup ---- */

/* was func_8C0229E6 / func_8C0229EA / func_8C0229EE / func_8C0229FA: the setters core/accessors.c defines
 * (race_setup_set_variant, _course, _word68, _word88; their exports there are commented out): exported here, with
 * the C of core/accessors.c (the index is not checked, as in the original) */
HW_EXPORT(8c0229e6, race_setup_set_variant(ARG0, (int32_t)ARG1))
HW_EXPORT(8c0229ea, race_setup_set_course(ARG0, (int32_t)ARG1))
HW_EXPORT(8c0229ee, race_setup_set_word68(ARG0, (int32_t)ARG1, ARG2))
HW_EXPORT(8c0229fa, race_setup_set_word88(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C029060 / func_8C029064: get / set the word at 0x3C */
uint32_t race_setup_word3c(gaddr setup) { return race_setup_at(setup)->word3c; }
void race_setup_set_word3c(gaddr setup, uint32_t value) { race_setup_at(setup)->word3c = value; }
HW_EXPORT(8c029060, RET(race_setup_word3c(ARG0)))
HW_EXPORT(8c029064, race_setup_set_word3c(ARG0, ARG1))

/* was func_8C029068 / func_8C02906E: get / set the word at 0x40 */
uint32_t race_setup_word40(gaddr setup) { return race_setup_at(setup)->word40; }
void race_setup_set_word40(gaddr setup, uint32_t value) { race_setup_at(setup)->word40 = value; }
HW_EXPORT(8c029068, RET(race_setup_word40(ARG0)))
HW_EXPORT(8c02906e, race_setup_set_word40(ARG0, ARG1))

/* was func_8C029074: player i's word at 0x44 (no bound check, as in the original) */
void race_setup_set_word44(gaddr setup, int32_t i, int32_t value) { race_setup_at(setup)->words44[i] = value; }
HW_EXPORT(8c029074, race_setup_set_word44(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* ---- RaceScene ---- */

/* was func_8C029288: the view the course shows (Course.shown_view) */
int32_t race_scene_shown_view(gaddr scene) { return course_at(race_scene_at(scene)->course)->shown_view; }
HW_EXPORT(8c029288, RET(race_scene_shown_view(ARG0)))

/* func_8C029BD0 (game/race_scene_build.c, not declared in a header): builds the scene's race */
static void race_scene_build_race(gaddr scene) { CALL1(0x8C029BD0u, scene); }
/* func_8C0297D4 (game/race_scene.c, not declared in a header): the scene's destructor body */
static void race_scene_destroy_body(gaddr scene) { CALL1(0x8C0297D4u, scene); }

/* was func_8C029ED2: the scene's state 1: build the race, then reset the recording counters and hide view 0's
 * player */
void race_scene_run_state1(gaddr scene) {
  RaceScene *s;
  race_scene_build_race(scene);
  s = race_scene_at(scene);
  s->word_ca8 = 2;
  s->word_cac = 0;
  s->word_cb8 = 0;
  s->word_cbc = 0;
  s->view_player[0] = -1;
}
HW_EXPORT(8c029ed2, race_scene_run_state1(ARG0))

/* was func_8C0298A8: the destructor called without freeing: runs the body, returns the scene */
gaddr race_scene_destroy_self(gaddr scene) {
  race_scene_destroy_body(scene);
  return scene;
}
HW_EXPORT(8c0298a8, RET(race_scene_destroy_self(ARG0)))

/* ---- SetupMenu (game/menu.h) ---- */

/* was func_8C0233D8: the same for the SetupMenu (setup_menu_destroy, game/house.c) */
gaddr setup_menu_destroy_self(gaddr menu) {
  setup_menu_destroy(menu);
  return menu;
}
HW_EXPORT(8c0233d8, RET(setup_menu_destroy_self(ARG0)))
