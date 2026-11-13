/* Pen Pen port, S18 b06: the results screen's pulsing lines of text (0x8C016D60-0x8C0170F7).
 *
 * Each drawer steps the shared pulse phase (a float angle in Ninja units, 65536 a turn) by the V-blanks of the
 * frame, sets the text style to its default (white, scale 1: text_style_reset and a 32-pixel scale, both inlined
 * in the original), then colours the text with |sin| of the phase in one channel and draws the line of the game's
 * language at that language's place. A language out of 0-6 draws nothing (the original then returns the
 * language it got). */
#include "hw.h"
#include "sdk.h"
#include "../core/sprite_text.h"
#include "text_view_plain.h"
#include "game.h"
#include "save.h"
#include "results_text.h"

/* the results' pulse phase, in Ninja angle units kept as a float (stepped by the drawers below) */
#define g_results_pulse_phase GUEST_VAR(f32, 0x8C08C4F0u)
/* the text style's colour {a, r, g, b} (core/sprite_text.c) */
#define g_text_color          ((f32 *)hw_ptr(0x8C1B3B34u))
#define PULSE_STEP            0x44888888u /* 1092.2667f: 65536 / 60, a turn a second */
#define F_0_3                 0x3E999999u /* 0.3f */
#define F_0_4                 0x3ECCCCCCu /* 0.4f */
#define F_0_6                 0x3F199999u /* 0.6f */
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

/* was func_8C016D60: the pulsing line of the results (faded in and out, pink), at y 410 from the view's corner */
int32_t results_pulse_draw(void) {
  f32 fade = results_pulse_step();
  results_text_style();
  text_color_set(fade, F32(1.0f), F32_BITS(F_0_4) /* 0.4 */, F32_BITS(F_0_6) /* 0.6 */);
  int32_t language = game_language();
  int32_t y = 0x19A;
  switch (language) {
  case LANG_0:
  case LANG_1:
  case LANG_6:
    return text_draw_at_view(0x110, y, 0x8C06C1A0u);
  case LANG_2:
    return text_draw_at_view(0x19E, y, 0x8C06C1C0u);
  case LANG_3:
    return text_draw_at_view(0x1D0, y, 0x8C06C1D8u);
  case LANG_4:
    return text_draw_at_view(0x1D0, y, 0x8C06C1E4u);
  case LANG_5:
    return text_draw_at_view(0x1D0, y, 0x8C06C1F4u);
  default:
    return language;
  }
}
HW_EXPORT(8c016d60, RET(results_pulse_draw()))

/* was func_8C016E98 (with its tail func_8C016F20): the one-player results' banner: red pulsing (r = |sin|, g 0.3,
 * b 0), drawn at y 224 on the screen (text_draw, not from the view's corner) */
int32_t results_draw_banner_one(void) {
  f32 red = results_pulse_step();
  results_text_style();
  text_color_set(F32(1.0f), red, F32_BITS(F_0_3) /* 0.3 */, F32(0.0f));
  int32_t language = game_language();
  int32_t y = 0xD0 + 0x10;
  switch (language) {
  case LANG_0:
  case LANG_1:
    return text_draw(0xD0, y, 0x8C06C204u);
  case LANG_2:
    return text_draw(0xF3, y, 0x8C06C220u);
  case LANG_3:
    return text_draw(0x94, y, 0x8C06C234u);
  case LANG_4:
    return text_draw(0xD0, y, 0x8C06C25Cu);
  case LANG_5:
    return text_draw(0x94, y, 0x8C06C278u);
  case LANG_6:
    return text_draw(0xD0, y, 0x8C06C2A4u);
  default:
    return language;
  }
}
HW_EXPORT(8c016e98, RET(results_draw_banner_one()))
/* func_8C016F20: the second half of func_8C016E98 past its njSin call (the last colour stores, the language call
 * and the switch): it inherits the colour's base in r4, fr0 = |sin|, fr4 = 1, fr5 = 0.3, fr6 = 0, r12 = 208,
 * r13 = text_draw, r14 = 208 and the frame of r12-r14 and PR */
HW_EXPORT_FRAGMENT(8c016f20, 8c016e98)

/* was func_8C016FD4 (with its tail func_8C017054): the split-screen results' banner, the same red pulse (r =
 * |sin|, g 0.3, b 0), at y 96 from the view's corner */
int32_t results_draw_banner_split(void) {
  f32 red = results_pulse_step();
  results_text_style();
  text_color_set(F32(1.0f), red, F32_BITS(F_0_3) /* 0.3 */, F32(0.0f));
  int32_t language = game_language();
  int32_t x = 0xEC, y = 0x60;
  switch (language) {
  case LANG_0:
  case LANG_1:
    return text_draw_at_view(x, y, 0x8C06C2C8u);
  case LANG_2:
    return text_draw_at_view(x, y, 0x8C06C2DCu);
  case LANG_3:
    return text_draw_at_view(x, y, 0x8C06C2F0u);
  case LANG_4:
    return text_draw_at_view(x, y, 0x8C06C308u);
  case LANG_5:
    return text_draw_at_view(0x60 + 0x50, y, 0x8C06C324u);
  case LANG_6:
    return text_draw_at_view(x, y, 0x8C06C348u);
  default:
    return language;
  }
}
HW_EXPORT(8c016fd4, RET(results_draw_banner_split()))
/* func_8C017054: the second half of func_8C016FD4 past its njSin call (the last colour stores, the language call
 * and the switch): it inherits the colour's base in r4, fr4 = 1, fr6 = 0, r1 = the language getter, r14 = the
 * line's x, and the frame of r14 and PR */
HW_EXPORT_FRAGMENT(8c017054, 8c016fd4)
