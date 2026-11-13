/* Pen Pen port, S18 c44: the results screen's top banner (0x8C016C44-0x8C016D5F), a sibling of
 * game/results_text.c's pulsing lines (same pulse phase, same inlined default style).
 *
 * It steps the shared pulse phase by the frame's V-blanks, sets the default text style (white, scale 1:
 * text_style_reset and a 32-pixel scale, both inlined in the original), colours the text a pale yellow
 * (r 1, g 0.9, b 0.1) faded in and out by |sin| of the phase, and draws the line of the game's language 8 pixels
 * below the view's top. A language out of 0-6 draws nothing (the original then returns the language it got). */
#include "hw.h"
#include "sdk.h"
#include "../core/sprite_text.h"
#include "text_view_plain.h"
#include "game.h"
#include "save.h"

/* the results' pulse phase, in Ninja angle units kept as a float (also stepped by game/results_text.c) */
#define g_results_pulse_phase GUEST_VAR(f32, 0x8C08C4F0u)
/* the text style's colour {a, r, g, b} (core/sprite_text.c) */
#define g_text_color          ((f32 *)hw_ptr(0x8C1B3B34u))
#define PULSE_STEP            0x44088888u /* 546.1333f: 32768 / 60, half a turn a second (S18: the writer had 0x44888888) */
#define F_0_1                 0x3DCCCCCCu /* 0.1f */
#define F_0_9                 0x3F666666u /* 0.9f */
#define TEXT_SIZE_DEFAULT     F32(32.0f)  /* the scale in 32-pixel units: 1 */

/* the game's languages (game_language) */
enum { LANG_0 = 0, LANG_1 = 1, LANG_2 = 2, LANG_3 = 3, LANG_4 = 4, LANG_5 = 5, LANG_6 = 6 };

/* steps the phase by the frame's V-blanks (+1) and gives |sin| of it */
static f32 results_pulse_step(void) {
  f32 phase = fmac(F32_BITS(PULSE_STEP), f32_from_int(g_vblanks_pending + 1), g_results_pulse_phase);
  g_results_pulse_phase = phase;
  return fabs_(njSin(f32_to_int(phase)));
}

/* the default style (white, scale 1, no spacing) */
static void results_text_style(void) {
  text_style_reset();
  text_set_scale(TEXT_SIZE_DEFAULT, TEXT_SIZE_DEFAULT, F32(0.0f));
}

static void text_color_set(f32 a, f32 r, f32 g, f32 b) {
  f32 *color = g_text_color;
  color[1] = r;
  color[2] = g;
  color[3] = b;
  color[0] = a;
}

/* was func_8C016C44: the results' top banner, pale yellow faded in and out, at y 8 from the view's corner
 * (languages 0, 1, 2 and 6 share one line) */
int32_t results_draw_banner_top(void) {
  f32 fade = results_pulse_step();
  results_text_style();
  text_color_set(fade, F32(1.0f), F32_BITS(F_0_9) /* 0.9 */, F32_BITS(F_0_1) /* 0.1 */);
  int32_t language = game_language();
  int32_t y = 8;
  switch (language) {
  case LANG_0:
  case LANG_1:
  case LANG_2:
  case LANG_6:
    return text_draw_at_view(0xA8, y, 0x8C06C150u);
  case LANG_3:
    return text_draw_at_view(y + 0x78, y, 0x8C06C15Cu);
  case LANG_4:
    return text_draw_at_view(y + 0x78, y, 0x8C06C174u);
  case LANG_5:
    return text_draw_at_view(0x6C, y, 0x8C06C188u);
  default:
    return language;
  }
}
HW_EXPORT(8c016c44, RET(results_draw_banner_top()))
