/* Pen Pen port, S17 (b01): two more drawers of the big text sprite (core/sprite_text.c's SPRITE_BIG): a string of
 * full-width digits and the pointer with its dots in their rest place (core/sprite_pointer.c turns them). */
#include "hw.h"
#include "sdk.h"
#include "../core/ninja.h"

#define SPRITE_BIG        0x8C1B3B08u /* NJS_SPRITE (core/sprite_text.c) */
#define BIG_PRIORITY      F32(-1.0f)
#define BIG_ATTR          0x20
#define BIG_DIGITS_X      F32(64.0f)  /* added to the string's x */
#define BIG_DIGIT_FIRST   0x824F      /* Shift-JIS full-width 0: the big sprite's cell 0 */
#define POINTER_CELL      15
#define POINTER_DOT_CELL  16

/* the pointer's anchors (core/sprite_pointer.c) */
#define g_pointer_aim_dy  GUEST_VAR(int32_t, 0x8C066F50u) /* 6: the dots' y */
#define g_pointer_dot1_dx GUEST_VAR(int32_t, 0x8C066F58u) /* 0: the first dot's x */
#define g_pointer_dot2_dx GUEST_VAR(int32_t, 0x8C066F5Cu) /* 17: the second dot's x */

static inline NjSprite *sprite_at(gaddr a) { return (NjSprite *)hw_ptr(a); }
/* SDK njColorBlendingMode (0x8C0A23A8) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* was func_8C0156DA: a Shift-JIS string of full-width digits through the big sprite at (x + 64, y): each
 * two-byte character picks the cell code - 0x824F; the pen does not move (every character at the same place) */
void text_draw_big(int32_t x, int32_t y, gaddr string) {
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  f32 fx = fadd(f32_from_int(x), BIG_DIGITS_X);
  while (*(const int8_t *)hw_ptr(string) != 0) {
    const int8_t *ch = (const int8_t *)hw_ptr(string); /* a two-byte character */
    int32_t hi = ch[0];
    int32_t lo = (uint8_t)ch[1];
    string += 2;
    int32_t cell = (int32_t)(uint16_t)((hi << 8) + lo) - BIG_DIGIT_FIRST;
    NjSprite *s = sprite_at(SPRITE_BIG);
    s->p.x = fx;
    s->p.y = f32_from_int(y);
    njDrawSprite2D(SPRITE_BIG, cell, BIG_PRIORITY, BIG_ATTR);
  }
}
HW_EXPORT(8c0156da, text_draw_big((int32_t)ARG0, (int32_t)ARG1, ARG2))

/* was func_8C0159EA: the pointer at (x, y) with its two dots in their rest place (the sprite's scale as it is) */
void sprite_pointer_draw_still(int32_t x, int32_t y) {
  NjSprite *s = sprite_at(SPRITE_BIG);
  s->p.x = f32_from_int(x);
  s->p.y = f32_from_int(y);
  njDrawSprite2D(SPRITE_BIG, POINTER_CELL, BIG_PRIORITY, BIG_ATTR);
  s->p.x = f32_from_int(g_pointer_dot1_dx + x);
  s->p.y = f32_from_int(g_pointer_aim_dy + y);
  njDrawSprite2D(SPRITE_BIG, POINTER_DOT_CELL, BIG_PRIORITY, BIG_ATTR);
  s->p.x = f32_from_int(x + g_pointer_dot2_dx);
  s->p.y = f32_from_int(y + g_pointer_aim_dy);
  njDrawSprite2D(SPRITE_BIG, POINTER_DOT_CELL, BIG_PRIORITY, BIG_ATTR);
}
HW_EXPORT(8c0159ea, sprite_pointer_draw_still((int32_t)ARG0, (int32_t)ARG1))
