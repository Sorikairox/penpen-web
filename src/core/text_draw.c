/* Pen Pen port, S16 (w4): the text drawer (func_8C01529A): a Shift-JIS string drawn a character at a time through
 * the small shared sprite (core/sprite_text.c), in the text style's colour and scale.
 *
 * Each two-byte character picks a cell of the small sprite's texture animation: hiragana, katakana, full-width
 * letters and digits by range, a few punctuation marks by a table; a character the font does not have draws the
 * cell of the character before it. The pen then moves right by the cell's width less 5 (times the text scale), then
 * by the style's extra spacing. */
#include "hw.h"
#include "sdk.h"
#include "../core/ninja.h"
#include "../core/sprite_text.h"

#define SPRITE_SMALL     0x8C1B3AE8u
#define g_text_scale_x   GUEST_VAR(f32, 0x8C1B3B28u)
#define g_text_scale_y   GUEST_VAR(f32, 0x8C1B3B2Cu)
#define g_text_spacing   GUEST_VAR(f32, 0x8C1B3B30u) /* the style's third value: added after each character */
#define TEXT_COLOR       0x8C1B3B34u                 /* the style's colour {a, r, g, b} (an NJS_ARGB) */
#define g_glyph_widths   ((const f32 *)hw_ptr(0x8C066B4Cu)) /* the small sprite's cell widths, in pixels */
#define GLYPH_OVERLAP    F32(-5.0f)                  /* added to each width */
#define TEXT_PRIORITY    F32(-1.0f)
#define TEXT_ATTR        0x22
#define TEXT_CONTROL3D   0x10

static inline NjSprite *sprite_at(gaddr a) { return (NjSprite *)hw_ptr(a); }
/* SDK njColorBlendingMode (0x8C0A23A8) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* Shift-JIS ranges the font has in a row: from `first` on, cell = code - offset (the first matching wins) */
#define KATAKANA_FIRST 0x8341
#define KATAKANA_OFFSET 0x82E0
#define HIRAGANA_FIRST 0x82A0
#define HIRAGANA_OFFSET 0x82A0
#define LETTERS_FIRST 0x8260   /* full-width A on */
#define LETTERS_OFFSET 0x81A0
#define DIGITS_FIRST 0x824F    /* full-width 0 on */
#define DIGITS_OFFSET 0x81FA

/* the cell of a punctuation mark, or `none` when the font has not got it */
static int32_t punctuation_cell(int32_t code, int32_t none) {
  switch (code) {
  case 0x8145: return 0x4E; /* middle dot */
  case 0x8146: return 0x4F; /* : */
  case 0x8147: return 0x52; /* ; */
  case 0x8148: return 0x53; /* ? */
  case 0x8149: return 0x54; /* ! */
  case 0x815B: return 0xB4; /* long vowel mark */
  case 0x815E: return 0x5F; /* / */
  case 0x815F: return 0x60; /* \ */
  case 0x8169: return 0x64; /* ( */
  case 0x816A: return 0x66; /* ) */
  case 0x8175: return 0x68; /* opening corner bracket */
  case 0x8176: return 0x9F; /* closing corner bracket */
  case 0x817B: return 0xB0; /* + */
  case 0x817C: return 0xB1; /* - */
  case 0x817E: return 0xB6; /* times */
  case 0x8180: return 0xB7; /* divided by */
  case 0x8181: return 0xB8; /* = */
  case 0x8190: return 0xAE; /* $ */
  case 0x8193: return 0xBB; /* % */
  case 0x8195: return 0xBC; /* & */
  case 0x819A: return 0xBD; /* star */
  case 0x81A8: return 0xDA; /* arrow right */
  case 0x81A9: return 0xDB; /* arrow left */
  case 0x81AA: return 0xDC; /* arrow up */
  case 0x81AB: return 0xDD; /* arrow down */
  case 0x81F4: return 0xDE; /* note */
  case 0x8197: return 0xBE; /* @ */
  case 0x8141: return 0x03; /* ideographic comma */
  case 0x8142: return 0x05; /* ideographic full stop */
  case 0x8143: return 0x07; /* , */
  case 0x8144: return 0x4C; /* . */
  case 0x8166: return 0xB9; /* ' */
  case 0x8168: return 0xBA; /* " */
  case 0x8194: return 0xBF; /* # */
  default: return none;
  }
}

static int32_t glyph_cell(int32_t code, int32_t prev) {
  if (code >= KATAKANA_FIRST) return code - KATAKANA_OFFSET;
  if (code >= HIRAGANA_FIRST) return code - HIRAGANA_OFFSET;
  if (code >= LETTERS_FIRST) return code - LETTERS_OFFSET;
  if (code >= DIGITS_FIRST) return code - DIGITS_OFFSET;
  return punctuation_cell(code, prev);
}

/* was func_8C01529A: the string at (x, y) (see the top of the file); returns the pen's advance after the last
 * character, before its extra spacing (0 for an empty string) */
int32_t text_draw(int32_t x, int32_t y, gaddr string) {
  NjSprite *sprite = sprite_at(SPRITE_SMALL);
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  control3d_push();
  control3d_set(TEXT_CONTROL3D);
  njSetConstantMaterial(TEXT_COLOR);
  sprite->sx = g_text_scale_x;
  sprite->sy = g_text_scale_y;
  int32_t advance = 0, drawn = 0;
  /* the original never sets the cell before the first character: an unknown first character draws the cell in the
   * caller's r14 (callee-saved: unchanged so far) */
  int32_t cell = (int32_t)hw_cpu->r[14];
  const uint8_t *s = (const uint8_t *)hw_ptr(string);
  while (*s != 0) {
    int32_t code = (uint16_t)((s[0] << 8) + s[1]);
    s += 2;
    cell = glyph_cell(code, cell);
    sprite->p.x = f32_from_int(x + advance);
    sprite->p.y = f32_from_int(y);
    njDrawSprite2D(SPRITE_SMALL, cell, TEXT_PRIORITY, TEXT_ATTR);
    f32 width = fmul(fadd(g_glyph_widths[cell], GLYPH_OVERLAP), g_text_scale_x);
    advance = f32_to_int(fadd(f32_from_int(advance), width));
    drawn = advance;
    advance = f32_to_int(fadd(f32_from_int(advance), g_text_spacing));
  }
  control3d_pop();
  return drawn;
}
HW_EXPORT(8c01529a, RET(text_draw((int32_t)ARG0, (int32_t)ARG1, ARG2)))
