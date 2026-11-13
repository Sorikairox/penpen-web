/* Pen Pen port, S18 b01: the four-player results' banner (the "B" drawer of game/race_hud.c's results), pulsing
 * in red with the V-blanks; and the fragments of this region of the text and banner drawers whose parent's C does
 * their work. */
#include "hw.h"
#include "sdk.h"
#include "race_text.h"
#include "../core/sprite_text.h"
#include "game.h"
#include "save.h"

#define g_banner_phase   GUEST_VAR(f32, 0x8C08C4F0u) /* the banners' pulse (Ninja angle units, as a float) */
#define g_text_color     ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b (core/sprite_text.c) */
#define BANNER_STEP      F32_BITS(0x44888888u)      /* 1092.27 = 0x10000 / 60: a turn a second */
#define BANNER_GREEN     F32_BITS(0x3E999999u)      /* 0.3 */
#define BANNER_Y         0x60

/* func_8C016B7A (game/text_view_plain.c, not declared in a header): a string at (x, y) from the current view's
 * corner */
static int32_t text_draw_at_view(int32_t x, int32_t y, gaddr string) { return (int32_t)CALL3(0x8C016B7Au, x, y, string); }

/* was func_8C0170F8 (with its tail func_8C01711C): the pulse steps by the V-blanks since the last update; the text
 * style back to its default at scale 32, coloured (|sin pulse|, 0.3, 0) opaque; then the language's banner at its x
 * (0x58; 0x26 and 8 for languages 4 and 5) and y 0x60; nothing for a language above 6 */
void results_draw_banner_four(void) {
  g_banner_phase = fmac(BANNER_STEP, f32_from_int(g_vblanks_pending + 1), g_banner_phase);
  f32 red = fabs_(njSin(f32_to_int(g_banner_phase)));
  text_style_reset();
  text_set_scale(F32(32.0f), F32(32.0f), F32(0.0f));
  f32 *color = g_text_color;
  color[1] = red;
  color[2] = BANNER_GREEN;
  color[3] = F32(0.0f);
  color[0] = F32(1.0f);
  int32_t x = 0x58;
  gaddr text;
  switch (game_language()) {
  case 0:
  case 1: text = 0x8C06C2C8u; break;
  case 2: text = 0x8C06C2DCu; break;
  case 3: text = 0x8C06C2F0u; break;
  case 4: text = 0x8C06C308u; x = 0x26; break;
  case 5: text = 0x8C06C324u; x = 8; break;
  case 6: text = 0x8C06C348u; break;
  default: return;
  }
  text_draw_at_view(x, BANNER_Y, text);
}
HW_EXPORT(8c0170f8, results_draw_banner_four())

/* the rest of func_8C0170F8 after its njSin call (fr0 the sine, r14 and PR on its frame) */
HW_EXPORT_FRAGMENT(8c01711c, 8c0170f8)
/* the tail of func_8C016BAE (game/text_view.c, text_draw_in_view) after its colour choice: r0 the colour's offset,
 * r5 the text colour, fr5 / fr6 its values, its 0x14-byte frame holding x, y and the string at 4..0x0C */
HW_EXPORT_FRAGMENT(8c016bec, 8c016bae)
