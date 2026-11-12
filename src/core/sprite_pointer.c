/* Pen Pen port, S15 (b01): a pointer drawn with the big text sprite (core/sprite_text.c's SPRITE_BIG): its cell 15
 * at a place, and cell 16 twice, set 4 pixels from two anchor points in the direction of a target. */
#include "hw.h"
#include "sdk.h"
#include "../core/ninja.h"

#define SPRITE_BIG        0x8C1B3B08u /* NJS_SPRITE (core/sprite_text.c) */
#define POINTER_CELL      15
#define POINTER_DOT_CELL  16
#define POINTER_ATTR      0x20
#define POINTER_PRIORITY  F32(-1.0f)
#define POINTER_RADIUS    F32(4.0f)  /* the dots' distance from their anchors */
#define ANGLE_UNITS       F32(65536.0f) /* a Ninja angle turn */
#define TWO_PI            F32_BITS(0x40C90FD7u) /* 6.2831845 (the original's constant, a hair below 2 pi) */

/* the anchors (pixels from the pointer's place) */
#define g_pointer_aim_dy  GUEST_VAR(int32_t, 0x8C066F50u) /* 6: the aim's and the dots' y */
#define g_pointer_aim_dx  GUEST_VAR(int32_t, 0x8C066F54u) /* 8: the aim's x */
#define g_pointer_dot1_dx GUEST_VAR(int32_t, 0x8C066F58u) /* 0: the first dot's x */
#define g_pointer_dot2_dx GUEST_VAR(int32_t, 0x8C066F5Cu) /* 17: the second dot's x */

static inline NjSprite *sprite_at(gaddr a) { return (NjSprite *)hw_ptr(a); }

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* was func_8C015AA8: the pointer at (x, y) (scale 1), and its two dots turned toward (target_x, target_y) as seen
 * from the aim point (x + 8, y + 6) */
void sprite_pointer_draw(int32_t x, int32_t y, int32_t target_x, int32_t target_y) {
  NjSprite *s = sprite_at(SPRITE_BIG);
  s->sy = F32(1.0f);
  s->sx = F32(1.0f);
  s->p.x = f32_from_int(x);
  s->p.y = f32_from_int(y);
  njDrawSprite2D(SPRITE_BIG, POINTER_CELL, POINTER_PRIORITY, POINTER_ATTR);

  f32 dx = f32_from_int(target_x - g_pointer_aim_dx - x);
  f32 dy = f32_from_int(target_y - g_pointer_aim_dy - y);
  f32 radians = rt_atan2f(dy, dx);
  int32_t angle = f32_to_int(fdiv(fmul(radians, ANGLE_UNITS), TWO_PI));
  angle = f32_to_int(f32_from_int(angle)); /* the original goes through a float again (exact) */
  int32_t off_x = f32_to_int(fmac(POINTER_RADIUS, njCos(angle), F32(0.5f)));
  int32_t off_y = f32_to_int(fmac(POINTER_RADIUS, njSin(angle), F32(0.5f)));

  f32 dot_y = f32_from_int(g_pointer_aim_dy + y + off_y);
  s->p.x = f32_from_int(g_pointer_dot1_dx + x + off_x);
  s->p.y = dot_y;
  njDrawSprite2D(SPRITE_BIG, POINTER_DOT_CELL, POINTER_PRIORITY, POINTER_ATTR);

  dot_y = f32_from_int(y + g_pointer_aim_dy + off_y);
  s->p.x = f32_from_int(x + g_pointer_dot2_dx + off_x);
  s->p.y = dot_y;
  njDrawSprite2D(SPRITE_BIG, POINTER_DOT_CELL, POINTER_PRIORITY, POINTER_ATTR);
}
HW_EXPORT(8c015aa8, sprite_pointer_draw((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3))
