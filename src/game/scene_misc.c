/* Pen Pen port, S15 b09: small methods left from batch b02: a Task's owner (the good-night scene asks it for the
 * Game) and the set-up menu's grid cursor reset. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "motion.h"
#include "menu.h"
#include "racer.h" /* save_byte44 (game/save.c) */

/* was func_8C01D03E: a Task's owner (the good-night scene's update asks it for the Game, game/good_night.c) */
gaddr task_owner(gaddr task) { return task_at(task)->owner; }
HW_EXPORT(8c01d03e, RET(task_owner(ARG0)))

/* was func_8C025CD8: the set-up menu's grid cursor back to the first cell (and word1a4 cleared), then the icon's
 * Bodies at rest (the original jumps to icon_bodies_init with 2 in r5, which it does not read) */
void setup_menu_grid_reset(gaddr self) {
  SetupMenu *m = setup_menu_at(self);
  m->word1a4 = 0;
  m->grid_row = 0;
  m->grid_column = 0;
  icon_bodies_init(self);
}
HW_EXPORT(8c025cd8, setup_menu_grid_reset(ARG0))
