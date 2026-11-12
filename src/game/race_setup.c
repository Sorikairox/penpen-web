/* Pen Pen port, S15 (b01): the RaceSetup's (game/race_scene.h, mode 7's scene) deleting destructor, and its range
 * table lookup for the computer players. */
#include "hw.h"
#include "sdk.h"
#include "race_scene.h"
#include "save.h"

/* func_8C01A8F4 (core/accessors.c, no header): the destructor body: g_race_setup = 0 */
static void race_setup_destroy(void) { CALL0(0x8C01A8F4u); }

/* was func_8C01A8FC: the deleting destructor: g_race_setup cleared, the set-up freed when asked; returns it */
gaddr race_setup_delete(gaddr setup, int32_t free_memory) {
  race_setup_destroy();
  if (free_memory)
    syFree(setup);
  return setup;
}
HW_EXPORT(8c01a8fc, RET(race_setup_delete(ARG0, (int32_t)ARG1)))

/* the range tables: one before cup 0 is complete, one after; per row (RaceSetup.variant) 16 words */
#define RANGE_ROWS 5
#define RANGE_WORDS 16
#define g_range_tables ((const int32_t (*)[RANGE_ROWS][RANGE_WORDS])hw_ptr(0x8C070758u))

/* was func_8C01AD42: word i of the set-up's row of the range table (word 1 for i = 0) */
int32_t race_setup_range(gaddr setup, int32_t i) {
  int32_t table = save_cup0_complete() != 0 ? 1 : 0;
  const int32_t *row = g_range_tables[table][race_setup_at(setup)->variant];
  if (i == 0)
    return row[1];
  return row[i];
}
HW_EXPORT(8c01ad42, RET(race_setup_range(ARG0, (int32_t)ARG1)))
