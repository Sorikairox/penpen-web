/* Pen Pen port, S15 (b01): a string drawn in the current view with one of two colour schemes (game/text_view.c). */
#include "hw.h"
#include "../core/view.h"

#define g_text_color ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b (core/sprite_text.c) */

/* func_8C01529A (core/sprite_text.c, still translated): draws a string at (x, y) with the text style */
static int32_t text_draw(int32_t x, int32_t y, gaddr string) { return (int32_t)CALL3(0x8C01529Au, x, y, string); }

/* was func_8C016BAE: draws `string` at (x, y) from the current view's top-left corner, at alpha `alpha`: yellow
 * (1, 0.9, 0.3) when `highlight`, else pale blue (0.5, 0.8, 1) */
int32_t text_draw_in_view(f32 alpha, int32_t highlight, int32_t x, int32_t y, gaddr string) {
  f32 *color = g_text_color;
  if (highlight != 0) {
    color[1] = F32(1.0f);
    color[2] = F32_BITS(0x3F666666u); /* 0.9 */
    color[3] = F32_BITS(0x3E999999u); /* 0.3 */
  } else {
    color[1] = F32(0.5f);
    color[2] = F32_BITS(0x3F4CCCCCu); /* 0.8 */
    color[3] = F32(1.0f);
  }
  color[0] = alpha;
  View *view = view_at(g_cur_view);
  int32_t left = f32_to_int(view->origin_x);
  int32_t top = f32_to_int(view->origin_y);
  return text_draw(x + left, y + top, string);
}
HW_EXPORT(8c016bae, RET(text_draw_in_view(FARG0, (int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3)))
