/* Pen Pen port, S18 b01: a two-digit number drawn with the shared text sprites (core/sprite_text.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/ninja.h"
#include "sprite_number.h"

#define SPRITE_SMALL     0x8C1B3AE8u /* NJS_SPRITE (core/sprite_text.c): its cells 0x55 on are the digits */
#define SPRITE_BIG       0x8C1B3B08u /* the big one (64 pixels), its cells 0 on */
#define DIGIT_CELL_SMALL 0x55
#define NUMBER_ATTR      0x20u
#define NUMBER_PRIORITY  F32(-1.0f)

static inline NjSprite *sprite_at(gaddr a) { return (NjSprite *)hw_ptr(a); }
/* SDK njColorBlendingMode */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* a digit cell of `sprite` at (x, y), scale s */
static void number_digit(gaddr sprite, int32_t cell, f32 x, f32 y, f32 scale) {
  NjSprite *s = sprite_at(sprite);
  s->p.x = x;
  s->p.y = y;
  s->sx = scale;
  s->sy = scale;
  njDrawSprite2D(sprite, cell, NUMBER_PRIORITY, NUMBER_ATTR);
}

/* was func_8C0158CA: the two digits of `value` (tens, then ones: C's truncating / and %) at (x, y), `size` pixels
 * high: above 32 with the big sprite, else the small one; the ones at x + size less a quarter of the size (the
 * scale times 16, or times 8, truncated) */
void number_draw(int32_t x, int32_t y, int32_t value, int32_t size) {
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  int32_t tens = value / 10;
  int32_t ones = value % 10;
  f32 fsize = f32_from_int(size), fy = f32_from_int(y), fx = f32_from_int(x);
  if (size > 32) {
    f32 scale = fdiv(fsize, F32(64.0f));
    int32_t step = f32_to_int(fmul(scale, F32(16.0f)));
    number_digit(SPRITE_BIG, tens, fx, fy, scale);
    number_digit(SPRITE_BIG, ones, f32_from_int(x + size - step), fy, scale);
  } else {
    f32 scale = fdiv(fsize, F32(32.0f));
    int32_t step = f32_to_int(fmul(scale, F32(8.0f)));
    number_digit(SPRITE_SMALL, tens + DIGIT_CELL_SMALL, fx, fy, scale);
    number_digit(SPRITE_SMALL, ones + DIGIT_CELL_SMALL, f32_from_int(x + size - step), fy, scale);
  }
}
HW_EXPORT(8c0158ca, number_draw((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3))
