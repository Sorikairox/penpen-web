/* Pen Pen port, S16 (b01): the pause screen's four-line menu in the game's language (game/pause_menu.c).
 *
 * The race's pause screen (game/race_hud.c) draws its menu in two places: a fading copy on the pausing player's view
 * (func_8C0175AC, lines from y 0xC0) and another layer at full alpha (func_8C017848, lines from y 0x48). Each line is
 * a string of the game's language centred by its own x; the line under the cursor is drawn highlighted. Languages 0
 * and 1 share their strings; a language past 6 draws nothing. */
#include "hw.h"
#include "sdk.h"

/* the text style (core/sprite_text.c): its scale (also the small sprite's scale) and the third value */
#define g_text_scale_x  GUEST_VAR(f32, 0x8C1B3B28u)
#define g_text_scale_y  GUEST_VAR(f32, 0x8C1B3B2Cu)
#define g_text_style_30 GUEST_VAR(f32, 0x8C1B3B30u)
#define TEXT_SPRITE_SMALL 0x8C1B3AE8u /* NJS_SPRITE (core/ninja.h) */
#include "../core/ninja.h"

#define PAUSE_MENU_LINES 4
#define PAUSE_MENU_LANGUAGES 7 /* game_language() 0..6; 0 and 1 share their table */
#define PAUSE_LINE_HEIGHT 0x20

/* a line: its x and its string */
typedef struct PauseLine {
  int32_t x;
  gaddr string;
} PauseLine;

/* func_8C016BAE (game/text_view.c, not declared in a header): a string at (x, y) of the current view, at `alpha`,
 * highlighted or not */
static void text_draw_in_view(f32 alpha, int32_t highlight, int32_t x, int32_t y, gaddr string) {
  FARGS1(alpha);
  CALL4(0x8C016BAEu, highlight, x, y, string);
}
/* func_8C019860 (game/save.c, not declared in a header): the game's language */
static int32_t game_language(void) { return (int32_t)CALL0(0x8C019860u); }

/* the text style at scale 1, its third value 0 (the order of the original's stores) */
static void pause_text_style(void) {
  NjSprite *small = (NjSprite *)hw_ptr(TEXT_SPRITE_SMALL);
  g_text_scale_x = F32(1.0f);
  small->sx = F32(1.0f);
  g_text_scale_y = F32(1.0f);
  small->sy = F32(1.0f);
  g_text_style_30 = F32(0.0f);
}

/* the four lines at y = top, top + 0x20, ...; line `cursor` highlighted */
static void pause_menu_lines(const PauseLine *lines, int32_t top, int32_t cursor, f32 alpha) {
  for (int32_t i = 0; i < PAUSE_MENU_LINES; i++)
    text_draw_in_view(alpha, cursor == i, lines[i].x, top + i * PAUSE_LINE_HEIGHT, lines[i].string);
}

/* the fading copy's lines, by language */
static const PauseLine PAUSE_MENU_C[PAUSE_MENU_LANGUAGES][PAUSE_MENU_LINES] = {
  {{0xCC, 0x8C06C4FCu}, {0x10D, 0x8C06C514u}, {0xC8, 0x8C06C520u}, {0xEE, 0x8C06C53Cu}},
  {{0xCC, 0x8C06C4FCu}, {0x10D, 0x8C06C514u}, {0xC8, 0x8C06C520u}, {0xEE, 0x8C06C53Cu}},
  {{0xBE, 0x8C06C54Cu}, {0xC8, 0x8C06C568u}, {0x9E, 0x8C06C580u}, {0xC2, 0x8C06C5A0u}},
  {{0xAA, 0x8C06C5BCu}, {0xC8, 0x8C06C5E0u}, {0x9E, 0x8C06C5FCu}, {0x10C, 0x8C06C620u}},
  {{0xDE, 0x8C06C630u}, {0xDA, 0x8C06C644u}, {0xA6, 0x8C06C65Cu}, {0xF0, 0x8C06C680u}},
  {{0xB0, 0x8C06C694u}, {0xF8, 0x8C06C6B8u}, {0xC8, 0x8C06C6C8u}, {0xE6, 0x8C06C6E8u}},
  {{0xB0, 0x8C06C6FCu}, {0x98, 0x8C06C71Cu}, {0xCC, 0x8C06C740u}, {0xCE, 0x8C06C758u}},
};
#define PAUSE_MENU_C_TOP 0xC0

/* the full-alpha layer's lines, by language (the first three strings are the same as above; the fourth differs) */
static const PauseLine PAUSE_MENU_B[PAUSE_MENU_LANGUAGES][PAUSE_MENU_LINES] = {
  {{0xCC, 0x8C06C4FCu}, {0x10D, 0x8C06C514u}, {0xC8, 0x8C06C520u}, {0xDC, 0x8C06C770u}},
  {{0xCC, 0x8C06C4FCu}, {0x10D, 0x8C06C514u}, {0xC8, 0x8C06C520u}, {0xDC, 0x8C06C770u}},
  {{0xBE, 0x8C06C54Cu}, {0xC8, 0x8C06C568u}, {0x9E, 0x8C06C580u}, {0x7E, 0x8C06C788u}},
  {{0xAA, 0x8C06C5BCu}, {0xC8, 0x8C06C5E0u}, {0x9E, 0x8C06C5FCu}, {0xB8, 0x8C06C7B0u}},
  {{0xDE, 0x8C06C630u}, {0xDA, 0x8C06C644u}, {0xA6, 0x8C06C65Cu}, {0xBE, 0x8C06C7D0u}},
  {{0xB0, 0x8C06C694u}, {0xF8, 0x8C06C6B8u}, {0xC8, 0x8C06C6C8u}, {0xD4, 0x8C06C7F0u}},
  {{0xB0, 0x8C06C6FCu}, {0x98, 0x8C06C71Cu}, {0xCC, 0x8C06C740u}, {0xC8, 0x8C06C80Cu}},
};
#define PAUSE_MENU_B_TOP 0x48

/* was func_8C0175AC: the fading copy of the menu at `alpha`, line `cursor` highlighted */
void pause_menu_draw_fading(int32_t cursor, f32 alpha) {
  pause_text_style();
  int32_t language = game_language();
  if (language < 0 || language >= PAUSE_MENU_LANGUAGES)
    return; /* the original's chain of compares with 0..6 */
  pause_menu_lines(PAUSE_MENU_C[language], PAUSE_MENU_C_TOP, cursor, alpha);
}
HW_EXPORT(8c0175ac, pause_menu_draw_fading((int32_t)ARG0, FARG0))

/* was func_8C017848: the menu's other layer, at alpha 1 */
void pause_menu_draw(int32_t cursor) {
  pause_text_style();
  int32_t language = game_language();
  if (language < 0 || language >= PAUSE_MENU_LANGUAGES)
    return; /* the original's chain of compares with 0..6 */
  pause_menu_lines(PAUSE_MENU_B[language], PAUSE_MENU_B_TOP, cursor, F32(1.0f));
}
HW_EXPORT(8c017848, pause_menu_draw((int32_t)ARG0))
