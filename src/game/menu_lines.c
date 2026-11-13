/* Pen Pen port, S18 b01: the pause menu's and the race ending's menus (game/race_hud.c's pause_draw_menu_a and
 * ending_draw_result_a/_b/_c, game/race_scene.c's ending_draw_result / ending_draw_result2): three or four lines of
 * the game's language, each at its place for that language, opaque, the line at the cursor highlighted
 * (text_draw_in_view's yellow), the others pale blue; nothing for a language above 6. */
#include "hw.h"
#include "sdk.h"
#include "race_text.h"
#include "../core/sprite_text.h"
#include "save.h"

#define MENU_LINES_MAX 4
#define LANGUAGES      7

/* func_8C016BAE (game/text_view.c, not declared in a header): a string at (x, y) of the current view, at `alpha`,
 * yellow when `highlight`, else pale blue */
static int32_t text_draw_in_view(f32 alpha, int32_t highlight, int32_t x, int32_t y, gaddr string) {
  FARGS1(alpha);
  return (int32_t)CALL4(0x8C016BAEu, highlight, x, y, string);
}

typedef struct MenuLine {
  int32_t x;
  gaddr text;
} MenuLine;
typedef struct MenuLines {
  int32_t count;
  int32_t y[MENU_LINES_MAX];
  MenuLine lang[LANGUAGES][MENU_LINES_MAX]; /* languages 0 and 1 share their lines */
} MenuLines;

/* the text scale (style value 0), then the language's lines in order */
static void menu_lines_draw(int32_t cursor, f32 scale, const MenuLines *m) {
  text_set_scale(scale, scale, F32(0.0f));
  int32_t language = game_language();
  if ((uint32_t)language >= LANGUAGES)
    return;
  for (int32_t i = 0; i < m->count; i++) {
    const MenuLine *line = &m->lang[language][i];
    text_draw_in_view(F32(1.0f), cursor == i, line->x, m->y[i], line->text);
  }
}

/* was func_8C017AC8: the pause menu's four lines (y 0x48 to 0xA8) at scale 28; the line at the cursor highlighted */
void pause_draw_menu_a(int32_t cursor) {
  static const MenuLines lines = {
    4, {0x48, 0x68, 0x88, 0xA8},
    {
      {{0x3A, 0x8C06C4FCU}, {0x6E, 0x8C06C514U}, {0x37, 0x8C06C520U}, {0x46, 0x8C06C770U}},
      {{0x3A, 0x8C06C4FCU}, {0x6E, 0x8C06C514U}, {0x37, 0x8C06C520U}, {0x46, 0x8C06C770U}},
      {{0x2D, 0x8C06C54CU}, {0x36, 0x8C06C568U}, {0x12, 0x8C06C580U}, {0x0, 0x8C06C788U}},
      {{0x1A, 0x8C06C5BCU}, {0x38, 0x8C06C5E0U}, {0x16, 0x8C06C5FCU}, {0x29, 0x8C06C7B0U}},
      {{0x4A, 0x8C06C630U}, {0x44, 0x8C06C644U}, {0x18, 0x8C06C65CU}, {0x2C, 0x8C06C7D0U}},
      {{0x22, 0x8C06C694U}, {0x5C, 0x8C06C6B8U}, {0x2D, 0x8C06C6C8U}, {0x3E, 0x8C06C7F0U}},
      {{0x22, 0x8C06C6FCU}, {0x14, 0x8C06C71CU}, {0x40, 0x8C06C740U}, {0x3C, 0x8C06C80CU}},
    },
  };
  menu_lines_draw(cursor, F32(28.0f), &lines);
}
HW_EXPORT(8c017ac8, pause_draw_menu_a((int32_t)ARG0))

/* was func_8C017D40: an ending menu's four lines (y 0xB0 to 0x110) at scale 32, first set; the line at the cursor highlighted */
void ending_draw_result_a(int32_t cursor) {
  static const MenuLines lines = {
    4, {0xB0, 0xD0, 0xF0, 0x110},
    {
      {{0xA2, 0x8C06C824U}, {0xB2, 0x8C06C1A0U}, {0xC8, 0x8C06C520U}, {0xEE, 0x8C06C53CU}},
      {{0xA2, 0x8C06C824U}, {0xB2, 0x8C06C1A0U}, {0xC8, 0x8C06C520U}, {0xEE, 0x8C06C53CU}},
      {{0xAC, 0x8C06C844U}, {0xC8, 0x8C06C1C0U}, {0x9E, 0x8C06C580U}, {0xC2, 0x8C06C5A0U}},
      {{0xCE, 0x8C06C864U}, {0x110, 0x8C06C1D8U}, {0x9E, 0x8C06C5FCU}, {0x10C, 0x8C06C620U}},
      {{0x94, 0x8C06C87CU}, {0xFE, 0x8C06C1E4U}, {0xA6, 0x8C06C65CU}, {0xF0, 0x8C06C680U}},
      {{0xCE, 0x8C06C8A0U}, {0x100, 0x8C06C1F4U}, {0xC8, 0x8C06C6C8U}, {0xE6, 0x8C06C6E8U}},
      {{0xAC, 0x8C06C8B8U}, {0xB2, 0x8C06C1A0U}, {0xCC, 0x8C06C740U}, {0xCE, 0x8C06C758U}},
    },
  };
  menu_lines_draw(cursor, F32(32.0f), &lines);
}
HW_EXPORT(8c017d40, ending_draw_result_a((int32_t)ARG0))

/* was func_8C018010: an ending menu's four lines (y 0xB0 to 0x110) at scale 32, second set; the line at the cursor highlighted */
void ending_draw_result_b(int32_t cursor) {
  static const MenuLines lines = {
    4, {0xB0, 0xD0, 0xF0, 0x110},
    {
      {{0xB2, 0x8C06C1A0U}, {0x106, 0x8C06C514U}, {0xC8, 0x8C06C520U}, {0xEE, 0x8C06C53CU}},
      {{0xB2, 0x8C06C1A0U}, {0x106, 0x8C06C514U}, {0xC8, 0x8C06C520U}, {0xEE, 0x8C06C53CU}},
      {{0xC8, 0x8C06C1C0U}, {0xC8, 0x8C06C568U}, {0x9E, 0x8C06C580U}, {0xC2, 0x8C06C5A0U}},
      {{0x110, 0x8C06C1D8U}, {0xC8, 0x8C06C5E0U}, {0x9E, 0x8C06C5FCU}, {0x10C, 0x8C06C620U}},
      {{0xFE, 0x8C06C1E4U}, {0xDA, 0x8C06C644U}, {0xA6, 0x8C06C65CU}, {0xF0, 0x8C06C680U}},
      {{0x100, 0x8C06C1F4U}, {0xF8, 0x8C06C6B8U}, {0xC8, 0x8C06C6C8U}, {0xE6, 0x8C06C6E8U}},
      {{0xB0, 0x8C06C1A0U}, {0x98, 0x8C06C71CU}, {0xCC, 0x8C06C740U}, {0xCE, 0x8C06C758U}},
    },
  };
  menu_lines_draw(cursor, F32(32.0f), &lines);
}
HW_EXPORT(8c018010, ending_draw_result_b((int32_t)ARG0))

/* was func_8C0182D4: an ending menu's four lines (y 0xB0 to 0x110) at scale 32, third set; the line at the cursor highlighted */
void ending_draw_result_c(int32_t cursor) {
  static const MenuLines lines = {
    4, {0xB0, 0xD0, 0xF0, 0x110},
    {
      {{0xB2, 0x8C06C1A0U}, {0xDC, 0x8C06C8D8U}, {0xC8, 0x8C06C520U}, {0xEE, 0x8C06C53CU}},
      {{0xB2, 0x8C06C1A0U}, {0xDC, 0x8C06C8D8U}, {0xC8, 0x8C06C520U}, {0xEE, 0x8C06C53CU}},
      {{0xC8, 0x8C06C1C0U}, {0xB8, 0x8C06C8F0U}, {0x9E, 0x8C06C580U}, {0xC2, 0x8C06C5A0U}},
      {{0x110, 0x8C06C1D8U}, {0xF8, 0x8C06C910U}, {0x9E, 0x8C06C5FCU}, {0x10C, 0x8C06C620U}},
      {{0xFE, 0x8C06C1E4U}, {0xB4, 0x8C06C920U}, {0xA6, 0x8C06C65CU}, {0xF0, 0x8C06C680U}},
      {{0x100, 0x8C06C1F4U}, {0xC8, 0x8C06C940U}, {0xC8, 0x8C06C6C8U}, {0xE6, 0x8C06C6E8U}},
      {{0xB2, 0x8C06C1A0U}, {0xDC, 0x8C06C95CU}, {0xCC, 0x8C06C740U}, {0xCE, 0x8C06C758U}},
    },
  };
  menu_lines_draw(cursor, F32(32.0f), &lines);
}
HW_EXPORT(8c0182d4, ending_draw_result_c((int32_t)ARG0))

/* was func_8C01859C: the ending's three-line menu (y 0x48 to 0x88) at scale 32; the line at the cursor highlighted */
void ending_draw_result(int32_t cursor) {
  static const MenuLines lines = {
    3, {0x48, 0x68, 0x88},
    {
      {{0xDC, 0x8C06C8D8U}, {0xC8, 0x8C06C520U}, {0xDC, 0x8C06C770U}},
      {{0xDC, 0x8C06C8D8U}, {0xC8, 0x8C06C520U}, {0xDC, 0x8C06C770U}},
      {{0xB8, 0x8C06C8F0U}, {0x9E, 0x8C06C580U}, {0x7E, 0x8C06C788U}},
      {{0xF8, 0x8C06C910U}, {0x9E, 0x8C06C5FCU}, {0xB8, 0x8C06C7B0U}},
      {{0xB4, 0x8C06C920U}, {0xA6, 0x8C06C65CU}, {0xBE, 0x8C06C7D0U}},
      {{0xC8, 0x8C06C940U}, {0xC8, 0x8C06C6C8U}, {0xD4, 0x8C06C7F0U}},
      {{0xDC, 0x8C06C95CU}, {0xCC, 0x8C06C740U}, {0xC8, 0x8C06C80CU}},
    },
  };
  menu_lines_draw(cursor, F32(32.0f), &lines);
}
HW_EXPORT(8c01859c, ending_draw_result((int32_t)ARG0))

/* was func_8C018774: the ending's other three-line menu (y 0x48 to 0x88) at scale 28; the line at the cursor highlighted */
void ending_draw_result2(int32_t cursor) {
  static const MenuLines lines = {
    3, {0x48, 0x68, 0x88},
    {
      {{0x46, 0x8C06C8D8U}, {0x37, 0x8C06C520U}, {0x46, 0x8C06C770U}},
      {{0x46, 0x8C06C8D8U}, {0x37, 0x8C06C520U}, {0x46, 0x8C06C770U}},
      {{0x2A, 0x8C06C8F0U}, {0x12, 0x8C06C580U}, {0x0, 0x8C06C788U}},
      {{0x64, 0x8C06C910U}, {0x16, 0x8C06C5FCU}, {0x29, 0x8C06C7B0U}},
      {{0x24, 0x8C06C920U}, {0x18, 0x8C06C65CU}, {0x2C, 0x8C06C7D0U}},
      {{0x34, 0x8C06C940U}, {0x2D, 0x8C06C6C8U}, {0x3E, 0x8C06C7F0U}},
      {{0x3C, 0x8C06C95CU}, {0x40, 0x8C06C740U}, {0x3C, 0x8C06C80CU}},
    },
  };
  menu_lines_draw(cursor, F32(28.0f), &lines);
}
HW_EXPORT(8c018774, ending_draw_result2((int32_t)ARG0))
