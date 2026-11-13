/* Pen Pen port, S17 b04: the menu after a race (retry, next, quit ...): the line under the cursor
 * (RaceScene.menu_line) moves up and down with the controller of the player it belongs to, wrapping
 * over the menu's lines; A or Start plays the line's sound and sets the scene's next state. The menu's lines and
 * its last line depend on the game's mode, on the ending (RaceScene.ending_kind) and on the set-up's words at
 * 0x3C / 0x40. */
#include "hw.h"
#include "sdk.h"
#include "../core/pad.h"
#include "../core/inline.h"
#include "sound.h"
#include "race_scene.h"

/* a line of the menu: the sound it plays and the scene state it goes to (as the pause menu's, game/race_hud.c) */
typedef struct MenuLine {
  int32_t sound;      /* 0x00 */
  int32_t next_state; /* 0x04 */
} MenuLine;
HW_SIZE(MenuLine, 8);
#define g_menu_last_line_one   GUEST_VAR(int32_t, 0x8C07FC74u) /* one player: the last line's index (3) */
#define g_menu_last_line_split GUEST_VAR(int32_t, 0x8C07FC78u) /* split screens: (2) */
#define MENU_LINES_CUP        0x8C07FC7Cu /* one player, no ending, the set-up's word 0x3C or 0x40 set */
#define MENU_LINES_ONE        0x8C07FC9Cu /* one player, no ending, neither set */
#define MENU_LINES_ENDING     0x8C07FCBCu /* one player after an ending, and the time trial */
#define MENU_LINES_SPLIT      0x8C07FCDCu /* modes 1 to 3 */

#define MENU_SOUND_PORT 7
#define MENU_MOVE_SOUND 5
#define BUTTON_UP     0x10
#define BUTTON_DOWN   0x20
#define BUTTON_A      0x08
#define BUTTON_START  0x04

/* the original's locals live in its frame (in another mode than 0-4 they are read without being set, so they
 * keep their place on the stack): the line under the cursor, the last line, the port; then its saved `scene`
 * and PR */
typedef struct MenuFrame {
  gaddr line;          /* 0x00: &lines[cursor] (taken before the cursor moves) */
  int32_t last;        /* 0x04 */
  int32_t port;        /* 0x08 */
  uint8_t saved[0x14 - 0x0C]; /* 0x0C */
} MenuFrame;
HW_SIZE(MenuFrame, 0x14);

/* was func_8C02D150: the menu's update (see the top of the file); the line confirmed is the one under the cursor
 * before this update's move */
void race_results_menu_update(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  gaddr frame = hw_stack_alloc(sizeof(MenuFrame));
  MenuFrame *f = (MenuFrame *)hw_ptr(frame);
  RaceSetup *setup = race_setup_at(s->setup);
  switch (setup->mode) {
  case 0:
    f->port = 0;
    f->last = g_menu_last_line_one;
    if (s->ending_kind != 0)
      f->line = MENU_LINES_ENDING + (uint32_t)s->menu_line * sizeof(MenuLine);
    else if (race_setup_word3c(s->setup) != 0 || race_setup_word40(s->setup) != 0)
      f->line = MENU_LINES_CUP + (uint32_t)s->menu_line * sizeof(MenuLine);
    else
      f->line = MENU_LINES_ONE + (uint32_t)s->menu_line * sizeof(MenuLine);
    break;
  case 4:
    f->port = 0;
    f->last = g_menu_last_line_one;
    f->line = MENU_LINES_ENDING + (uint32_t)s->menu_line * sizeof(MenuLine);
    break;
  case 1:
  case 2:
  case 3:
    f->port = s->focus[0];
    f->last = g_menu_last_line_split;
    f->line = MENU_LINES_SPLIT + (uint32_t)s->menu_line * sizeof(MenuLine);
    break;
  }
  int32_t *cursor = &s->menu_line;
  if (port_pressed(f->port, BUTTON_UP)) {
    sound_play(MENU_SOUND_PORT, MENU_MOVE_SOUND, 0);
    if (--*cursor < 0) *cursor = f->last;
  }
  if (port_pressed(f->port, BUTTON_DOWN)) {
    sound_play(MENU_SOUND_PORT, MENU_MOVE_SOUND, 0);
    if (++*cursor > f->last) *cursor = 0;
  }
  if (port_pressed(f->port, BUTTON_A) || port_pressed(f->port, BUTTON_START)) {
    const MenuLine *line = (const MenuLine *)hw_ptr(f->line);
    sound_play(MENU_SOUND_PORT, line->sound, 0);
    field_set_and_clear(scene, (uint32_t)((const MenuLine *)hw_ptr(f->line))->next_state);
  }
  hw_stack_free(sizeof(MenuFrame));
}
HW_EXPORT(8c02d150, race_results_menu_update(ARG0))
