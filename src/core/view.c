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

/* ---- the per-view light and fog (read by the view's camera set-up, func_8C011C7C) ---- */

/* was func_8C01B10C */
void view_set_light_dir(f32 x, f32 y, f32 z, int32_t index) { vec3_set(x, y, z, hw_addr(&g_views[index].light_dir)); }
HW_EXPORT(8c01b10c, view_set_light_dir(FARG0, FARG1, FARG2, (int32_t)ARG0))
/* was func_8C01B0DC */
void view_set_light_intensity(f32 a, f32 b, f32 c, int32_t index) {
  View *v = &g_views[index];
  v->light_intensity.x = a;
  v->light_intensity.y = b;
  v->light_intensity.z = c;
}
HW_EXPORT(8c01b0dc, view_set_light_intensity(FARG0, FARG1, FARG2, (int32_t)ARG0))
/* was func_8C01B0F4 */
void view_set_light_color(f32 r, f32 g, f32 b, int32_t index) {
  View *v = &g_views[index];
  v->light_color.x = r;
  v->light_color.y = g;
  v->light_color.z = b;
}
HW_EXPORT(8c01b0f4, view_set_light_color(FARG0, FARG1, FARG2, (int32_t)ARG0))
/* was func_8C01F5B0 */
void view_fog_off(int32_t index) { g_views[index].fog = 0; }
HW_EXPORT(8c01f5b0, view_fog_off((int32_t)ARG0))
/* was func_8C042452: fog as the current view wants it */
void view_apply_fog(void) {
  if (((View *)hw_ptr(g_cur_view))->fog == 0)
    njFogDisable();
  else
    njFogEnable();
}
HW_EXPORT(8c042452, view_apply_fog())
/* was func_8C028E90: the address of the current view's 0x64 (S14: the copy of its camera request, see View) */
gaddr view_screen(void) { return hw_addr(&((View *)hw_ptr(g_cur_view))->camera_copy); }
HW_EXPORT(8c028e90, RET(view_screen()))
/* was func_8C0165E2 */
int32_t view_current_index(void) { return g_cur_view_index; }
HW_EXPORT(8c0165e2, RET(view_current_index()))

/* was func_8C04241E: fog on for view i (the counterpart of view_fog_off) */
void view_fog_on(int32_t index) { g_views[index].fog = 1; }
/* not exported (S14 difftest: untested: since the menu code that called it is hand-written, it is entered only from C, and no passing sample of its callers covered it): HW_EXPORT(8c04241e, view_fog_on((int32_t)ARG0)) */

/* was func_8C01E282 (and its copy func_8C0228D0): the current view's light colour = (r, g, b), the view pointer read
 * again for each component as the original does */
void view_current_set_light_color(f32 r, f32 g, f32 b) {
  ((View *)hw_ptr(g_cur_view))->light_color.x = r;
  ((View *)hw_ptr(g_cur_view))->light_color.y = g;
  ((View *)hw_ptr(g_cur_view))->light_color.z = b;
}
HW_EXPORT(8c01e282, view_current_set_light_color(FARG0, FARG1, FARG2))
/* was func_8C01E298 (and its copy func_8C0228E6): the current view's light direction = (x, y, z), stored z first */
void view_current_set_light_dir(f32 x, f32 y, f32 z) {
  View *v = (View *)hw_ptr(g_cur_view);
  v->light_dir.z = z;
  v->light_dir.y = y;
  v->light_dir.x = x;
}
HW_EXPORT(8c01e298, view_current_set_light_dir(FARG0, FARG1, FARG2))
