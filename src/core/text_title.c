/* Pen Pen port, S17 b09 (b01's rest): a big red title through the text drawer (core/text_draw.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/sprite_text.h"
#include "../game/save.h" /* game_language */

#define g_text_color     ((f32 *)hw_ptr(0x8C1B3B34u)) /* the text style's colour {a, r, g, b} */
#define TITLE_STRING     0x8C06C35Cu /* a Shift-JIS string (it starts with full-width G, O) */
#define TITLE_X          0x6E
#define TITLE_Y          0x76
#define TITLE_SIZE       F32(120.0f) /* pixels: a text scale of 3.75 */
#define LANGUAGES        7           /* game_language's values 0..6 all draw the same string */

/* was func_8C0171D0: the text style opaque red at a scale of 3.75 with no extra spacing, then, for any of the
 * game's languages, the title string at (110, 118) */
void text_draw_red_title(void) {
  f32 *color = g_text_color;
  color[1] = F32(1.0f); /* r */
  color[2] = F32(0.0f); /* g */
  color[3] = F32(0.0f); /* b */
  color[0] = F32(1.0f); /* a */
  text_set_scale(TITLE_SIZE, TITLE_SIZE, F32(0.0f)); /* its stores, in its order: the original inlines them */
  uint32_t language = (uint32_t)game_language();
  if (language < LANGUAGES) text_draw(TITLE_X, TITLE_Y, TITLE_STRING);
}
HW_EXPORT(8c0171d0, text_draw_red_title())
