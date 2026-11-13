/* Pen Pen port, S18 b01: the race ending's words (game/race_scene.c's ending_draw_banner / ending_draw_item): one
 * string of the game's language, in the text style at scale 48, drawn in the current view at its place for that
 * language. */
#include "hw.h"
#include "sdk.h"
#include "race_text.h"
#include "../core/sprite_text.h"
#include "save.h"

#define g_text_color ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b (core/sprite_text.c) */
#define ENDING_TEXT_Y 0x58

/* func_8C016B7A (game/text_view_plain.c, not declared in a header): a string at (x, y) from the current view's
 * corner */
static int32_t text_draw_at_view(int32_t x, int32_t y, gaddr string) { return (int32_t)CALL3(0x8C016B7Au, x, y, string); }

static void text_color(f32 r, f32 g, f32 b, f32 a) {
  f32 *color = g_text_color;
  color[1] = r;
  color[2] = g;
  color[3] = b;
  color[0] = a;
}

/* a string and its x for each of the 7 languages (0 and 1 share one) */
typedef struct LangText {
  gaddr text[7];
  int32_t x[7];
} LangText;

/* draws the language's entry; nothing for a language above 6 */
static void draw_lang_text(const LangText *t) {
  int32_t language = game_language();
  if ((uint32_t)language > 6)
    return;
  text_draw_at_view(t->x[language], ENDING_TEXT_Y, t->text[language]);
}

/* was func_8C01723E: the ending's banner at scale 48 (spacing 4): for the player (0) in orange (1, 0.3, 0), for the
 * other player in violet (0.5, 0.3, 1); each with its own strings and places */
void ending_draw_banner(int32_t other_player) {
  static const LangText mine = {
    {0x8C06C368u, 0x8C06C368u, 0x8C06C378u, 0x8C06C39Cu, 0x8C06C3ACu, 0x8C06C3BCu, 0x8C06C3D0u},
    {0xD5, 0xD5, 0x23, 0xE9, 0xDF, 0xCB, 0x23},
  };
  static const LangText other = {
    {0x8C06C3F4u, 0x8C06C3F4u, 0x8C06C408u, 0x8C06C42Cu, 0x8C06C43Cu, 0x8C06C450u, 0x8C06C464u},
    {0xCA, 0xCA, 0x24, 0xDE, 0xD4, 0xC0, 0x24},
  };
  text_set_scale(F32(48.0f), F32(48.0f), F32(4.0f));
  if (other_player == 0) {
    text_color(F32(1.0f), F32_BITS(0x3E999999u) /* 0.3 */, F32(0.0f), F32(1.0f));
    draw_lang_text(&mine);
  } else {
    text_color(F32(0.5f), F32_BITS(0x3E999999u) /* 0.3 */, F32(1.0f), F32(1.0f));
    draw_lang_text(&other);
  }
}
HW_EXPORT(8c01723e, ending_draw_banner((int32_t)ARG0))

/* was func_8C0173E8: the ending's item `item` (0-3; nothing for another) at scale 48 (spacing 5) in red: items 0-2
 * one string for every language, item 3 one per language */
void ending_draw_item(int32_t item) {
  static const LangText item3 = {
    {0x8C06C4A0u, 0x8C06C4A0u, 0x8C06C4ACu, 0x8C06C4BCu, 0x8C06C4CCu, 0x8C06C4D8u, 0x8C06C4E8u},
    {0x47, 0x47, 0x2F, 0x2F, 0x47, 0x47, 0x17},
  };
  static const struct { gaddr text; int32_t x; } items[3] = {
    {0x8C06C488u, 0x71}, {0x8C06C490u, 0x69}, {0x8C06C498u, 0x6A},
  };
  text_set_scale(F32(48.0f), F32(48.0f), F32(5.0f));
  if (item == 0 || item == 1 || item == 2) {
    text_color(F32(1.0f), F32(0.0f), F32(0.0f), F32(1.0f));
    if ((uint32_t)game_language() > 6)
      return;
    text_draw_at_view(items[item].x, ENDING_TEXT_Y, items[item].text);
  } else if (item == 3) {
    text_color(F32(1.0f), F32(0.0f), F32(0.0f), F32(1.0f));
    draw_lang_text(&item3);
  }
}
HW_EXPORT(8c0173e8, ending_draw_item((int32_t)ARG0))
