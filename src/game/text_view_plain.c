/* Pen Pen port, S17 (b01): a string drawn in the current view with the text colour as it is (game/text_view.c
 * has the coloured one). */
#include "hw.h"
#include "../core/sprite_text.h"
#include "../core/view.h"

/* was func_8C016B7A: draws `string` at (x, y) from the current view's top-left corner; text_draw's result */
int32_t text_draw_at_view(int32_t x, int32_t y, gaddr string) {
  View *view = view_at(g_cur_view);
  int32_t left = f32_to_int(view->origin_x);
  int32_t top = f32_to_int(view->origin_y);
  return text_draw(x + left, y + top, string);
}
HW_EXPORT(8c016b7a, RET(text_draw_at_view((int32_t)ARG0, (int32_t)ARG1, ARG2)))
