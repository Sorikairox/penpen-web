/* Pen Pen port, S17 b05: copies of the menus' penguin constructor and destructors (game/motion.h) kept by the
 * racer's unit, and the player's destructor that returns it. */
#include "hw.h"
#include "sdk.h"
#include "motion.h"

/* ---- the motion player of the menus' penguins (game/motion.h) ---- */

/* was func_8C037F26: a copy of menu_penguin_construct (the first word points at the part at 0xDC, then the
 * MotionPlayer's construction with the same arguments) */
HW_EXPORT(8c037f26, RET(menu_penguin_construct(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3, ARG4)))

/* was func_8C037F50: a copy of motion_player_delete (the player freed, then the memory when asked) */
HW_EXPORT(8c037f50, RET(motion_player_delete(ARG0, (int32_t)ARG1)))

/* was func_8C037F74: the player freed; returns it */
gaddr motion_player_destroy_self(gaddr player) {
  motion_player_free(player);
  return player;
}
HW_EXPORT(8c037f74, RET(motion_player_destroy_self(ARG0)))
