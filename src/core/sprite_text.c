/* Pen Pen port, S13: the two shared 2D sprites the game draws numbers and small pictures with
 * (core/sprite_text.c).
 *
 * Two NJS_SPRITE structures in .bss: a small one (its texture animation's cells 0x55 on are the digits) and a big
 * one (cells 0 on); both use the current texture list. Beside them the text style: a scale, a third value set with
 * it, and the colour {a, r, g, b} (inline.c's G_COLOR). The digit helpers pick the cell through an SHC runtime
 * helper called with 10 (the value's last digit, by the look of it: the helper is called as guest code). */
#include "hw.h"
#include "sdk.h"

typedef struct NjSprite {
  f32 x, y, z;     /* 0x00 */
  f32 sx, sy;      /* 0x0C */
  int32_t ang;     /* 0x14 */
  gaddr tlist;     /* 0x18: NJS_TEXLIST */
  gaddr tanim;     /* 0x1C: NJS_TEXANIM cells */
} NjSprite;
HW_OFFSET(NjSprite, tlist, 0x18);
HW_SIZE(NjSprite, 0x20);

#define SPRITE_SMALL      0x8C1B3AE8u
#define SPRITE_BIG        0x8C1B3B08u
#define TANIM_SMALL       0x8C08ADB8u
#define TANIM_BIG         0x8C08C1B8u
#define g_text_scale_x    GUEST_VAR(f32, 0x8C1B3B28u)
#define g_text_scale_y    GUEST_VAR(f32, 0x8C1B3B2Cu)
#define g_text_style_30   GUEST_VAR(f32, 0x8C1B3B30u)
#define g_text_color      ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b */
#define g_current_texlist GUEST_VAR(gaddr, 0x8C1B7E44u)
#define DIGIT_CELL_SMALL  0x55 /* the small sprite's cell of digit 0 */
#define TEXT_PRIORITY     F32(-1.0f)

static inline NjSprite *sprite_at(gaddr a) { return (NjSprite *)hw_ptr(a); }

/* blending for the digits (SDK) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }
/* the last decimal digit, with the dividend's sign: the original calls the SHC runtime's signed remainder
 * (func_8C062518: r1 % r0, truncating; a zero divisor sets errno and gives 0, never with 10) */
static int32_t digit_of(int32_t value) { return value % 10; }

static void text_blend(void) {
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
}

/* was func_8C022A78: the style back to its default: white, scale 1 (both sprites too) */
void text_style_reset(void) {
  f32 *color = g_text_color;
  color[0] = F32(1.0f);
  color[3] = F32(1.0f);
  color[2] = F32(1.0f);
  color[1] = F32(1.0f);
  g_text_scale_y = F32(1.0f);
  g_text_scale_x = F32(1.0f);
  sprite_at(SPRITE_SMALL)->sy = F32(1.0f);
  sprite_at(SPRITE_SMALL)->sx = F32(1.0f);
  sprite_at(SPRITE_BIG)->sy = F32(1.0f);
  sprite_at(SPRITE_BIG)->sx = F32(1.0f);
  g_text_style_30 = F32(0.0f);
}
HW_EXPORT(8c022a78, text_style_reset())

static void sprite_init(gaddr sprite, gaddr tanim) {
  NjSprite *s = sprite_at(sprite);
  s->tlist = g_current_texlist;
  s->tanim = tanim;
  s->ang = 0;
  s->sx = F32(1.0f);
  s->sy = F32(1.0f);
  s->x = F32(0.0f);
  s->y = F32(0.0f);
  s->z = F32(0.0f);
}

/* was func_8C015224: both sprites at the origin with the current texture list, then the default style */
void text_init(void) {
  sprite_init(SPRITE_SMALL, TANIM_SMALL);
  sprite_init(SPRITE_BIG, TANIM_BIG);
  text_style_reset();
}
HW_EXPORT(8c015224, text_init())

/* was func_8C022A58: the scale (in 32-pixel units) and the third style value */
void text_set_scale(f32 sx, f32 sy, f32 v) {
  f32 x = fdiv(sx, F32(32.0f)), y = fdiv(sy, F32(32.0f));
  g_text_scale_x = x;
  sprite_at(SPRITE_SMALL)->sx = x;
  g_text_scale_y = y;
  sprite_at(SPRITE_SMALL)->sy = y;
  g_text_style_30 = v;
}
HW_EXPORT(8c022a58, text_set_scale(FARG0, FARG1, FARG2))

/* was func_8C015756: a digit of value with the small sprite at (x, y), at its current scale */
void text_draw_digit(int32_t x, int32_t y, int32_t value, uint32_t attr) {
  text_blend();
  int32_t digit = digit_of(value);
  NjSprite *s = sprite_at(SPRITE_SMALL);
  s->x = f32_from_int(x);
  s->y = f32_from_int(y);
  njDrawSprite2D(SPRITE_SMALL, digit + DIGIT_CELL_SMALL, TEXT_PRIORITY, attr);
}
HW_EXPORT(8c015756, text_draw_digit((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3))

/* was func_8C015826: the same at a size in pixels: up to 32 with the small sprite, above with the big one
 * (which is 64 pixels) */
void text_draw_digit_sized(int32_t x, int32_t y, int32_t value, int32_t size, uint32_t attr) {
  text_blend();
  int32_t digit = digit_of(value);
  f32 fsize = f32_from_int(size), fy = f32_from_int(y), fx = f32_from_int(x);
  if (size > 32) {
    NjSprite *s = sprite_at(SPRITE_BIG);
    f32 scale = fdiv(fsize, F32(64.0f));
    s->x = fx;
    s->y = fy;
    s->sx = scale;
    s->sy = scale;
    njDrawSprite2D(SPRITE_BIG, digit, TEXT_PRIORITY, attr);
  } else {
    NjSprite *s = sprite_at(SPRITE_SMALL);
    f32 scale = fdiv(fsize, F32(32.0f));
    s->x = fx;
    s->y = fy;
    s->sx = scale;
    s->sy = scale;
    njDrawSprite2D(SPRITE_SMALL, digit + DIGIT_CELL_SMALL, TEXT_PRIORITY, attr);
  }
}
HW_EXPORT(8c015826, text_draw_digit_sized((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, ARG4))

/* was func_8C0292DA: cell n of a sprite at (x, y), scale s (both axes) */
static void sprite_draw_cell(gaddr sprite, int32_t n, uint32_t attr, f32 x, f32 y, f32 scale) {
  NjSprite *s = sprite_at(sprite);
  s->x = x;
  s->y = y;
  s->sx = scale;
  s->sy = scale;
  njDrawSprite2D(sprite, n, TEXT_PRIORITY, attr);
}
void sprite_small_draw(int32_t n, uint32_t attr, f32 x, f32 y, f32 scale) { sprite_draw_cell(SPRITE_SMALL, n, attr, x, y, scale); }
HW_EXPORT(8c0292da, sprite_small_draw((int32_t)ARG0, ARG1, FARG0, FARG1, FARG2))
/* was func_8C029306: the same with the big sprite */
void sprite_big_draw(int32_t n, uint32_t attr, f32 x, f32 y, f32 scale) { sprite_draw_cell(SPRITE_BIG, n, attr, x, y, scale); }
HW_EXPORT(8c029306, sprite_big_draw((int32_t)ARG0, ARG1, FARG0, FARG1, FARG2))
