/* Pen Pen port, S11: the views (core/view.h). */
#include "view.h"
#include "sdk.h"

/* still translated: the camera maths of a view (floats) and a view's layout set-up */
static void view_apply_camera(View *view) { CALL1(0x8C011C7Cu, hw_addr(view)); }      /* func_8C011C7C */
static void view_set_layout(View *view, int32_t layout) { CALL2(0x8C011AFCu, hw_addr(view), layout); } /* func_8C011AFC */

/* was func_8C010DD6 and func_8C010DDA: empty in this build (frame hooks around the views) */
void view_frame_begin(void) {}
void view_frame_end(void) {}
HW_EXPORT(8c010dd6, view_frame_begin())
HW_EXPORT(8c010dda, view_frame_end())

/* was func_8C011F48 and func_8C011F4C: empty per-view hooks (after drawing a view; when the views are reset) */
static void view_after_draw(View *view) { (void)view; }
static void view_reset(View *view) { (void)view; }
HW_EXPORT(8c011f48, view_after_draw((View *)hw_ptr(ARG0)))
HW_EXPORT(8c011f4c, view_reset((View *)hw_ptr(ARG0)))

/* was func_8C010DDE: make a view current: its camera goes to Ninja, and its inverse is kept for the game's
 * screen-to-world maths. */
void view_select(int32_t index) {
  g_cur_view_index = index;
  g_cur_view = hw_addr(&g_views[index]);
  view_apply_camera(&g_views[index]);
  njGetMatrix(VIEW_INVERSE_MATRIX);
  njInvertMatrix(VIEW_INVERSE_MATRIX);
}
HW_EXPORT(8c010dde, view_select((int32_t)ARG0))

/* was func_8C010E10 */
void view_finish(void) { view_after_draw((View *)hw_ptr(g_cur_view)); }
HW_EXPORT(8c010e10, view_finish())

/* was func_8C010E18 */
void views_reset(void) {
  for (View *v = g_views; v < g_views + VIEW_MAX; v++)
    view_reset(v);
}
HW_EXPORT(8c010e18, views_reset())

/* was func_8C010E40: the screen layouts of each split mode (layout numbers index the original's table) */
void view_set_split(int32_t mode) {
  g_split_mode = mode;
  switch (mode) {
  case SPLIT_FULL:
    view_set_layout(&g_views[0], 0);
    break;
  case SPLIT_TWO:
    view_set_layout(&g_views[0], 1);
    view_set_layout(&g_views[1], 2);
    break;
  case SPLIT_TWO_WITH_PANEL:
    view_set_layout(&g_views[0], 3);
    view_set_layout(&g_views[1], 4);
    break;
  case SPLIT_FOUR:
    view_set_layout(&g_views[0], 5);
    view_set_layout(&g_views[1], 6);
    view_set_layout(&g_views[2], 7);
    view_set_layout(&g_views[3], 8);
    break;
  }
}
HW_EXPORT(8c010e40, view_set_split((int32_t)ARG0))
