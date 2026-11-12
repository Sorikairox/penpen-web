/* Pen Pen port, S15 (b01): a view's layout (core/view_layout.c): its rectangle, origin and screen from the layout
 * tables, a fresh Ninja camera and view, and the default lights and fog. */
#include "hw.h"
#include "view.h"

/* the layout tables, one entry per layout number (View.layout) */
#define LAYOUT_CLIPS    0x8C066944u /* 16 bytes: the njClip2D rectangle */
#define LAYOUT_ORIGINS  0x8C0669E4u /* 16 bytes: origin_x, origin_y and the extent */
#define LAYOUT_SCREENS  0x8C066A84u /* NjScreen */
#define LAYOUT_CLIP_SIZE   16u
#define LAYOUT_ORIGIN_SIZE 16u

/* Ninja (SDK) */
static void nj_InitCamera(gaddr camera) { CALL1(0x8C0D1A10u, camera); }
static void njInitView(gaddr view) { CALL1(0x8C0CF4ACu, view); }

/* the SHC runtime's block copy (r0 bytes from r2 to r1), here in two 8-byte halves as the original does */
static void copy_bytes(void *to, gaddr from, uint32_t n) { memmove(to, hw_ptr(from), n); }

/* was func_8C011AFC: sets view's layout: the rectangle, origin and screen of the layout, the camera and view
 * initialised, fog off, the light from (0, 0, -1) with intensities (0, 1, 0.25) and white colours, and the
 * per-light records at 0x120 / 0x150 / 0x174 to their defaults */
void view_init_layout(View *view, int32_t layout) {
  gaddr v = hw_addr(view);
  gaddr clip = LAYOUT_CLIPS + (uint32_t)layout * LAYOUT_CLIP_SIZE;
  view->layout = layout;
  copy_bytes(&view->clip[0], clip, 8);
  copy_bytes(&view->clip[8], clip + 8, 8);
  gaddr origin = LAYOUT_ORIGINS + (uint32_t)layout * LAYOUT_ORIGIN_SIZE;
  copy_bytes(&view->origin_x, origin, 8);
  copy_bytes(&view->extent[0], origin + 8, 8);
  copy_bytes(&view->screen, LAYOUT_SCREENS + (uint32_t)layout * sizeof(NjScreen), sizeof(NjScreen));
  nj_InitCamera(member_addr(v, View, camera));
  njInitView(member_addr(v, View, request));

  view->fog = 0;
  view->light_dir.z = F32(-1.0f);
  view->light_dir.y = F32(0.0f);
  view->light_dir.x = F32(0.0f);
  view->light_intensity.x = F32(0.0f);
  view->light_intensity.y = F32(1.0f);
  view->light_intensity.z = F32(0.25f);
  view->light_10c = F32(1.0f);
  view->light_color.x = F32(1.0f);
  view->light_color.y = F32(1.0f);
  view->light_color.z = F32(1.0f);
  view->word11c = 1;
  for (int i = 0; i < 3; i++) {
    view->colors120[i].rgb[2] = F32(1.0f);
    view->colors120[i].rgb[1] = F32(1.0f);
    view->colors120[i].rgb[0] = F32(1.0f);
  }
  for (int i = 0; i < 3; i++) {
    view->vec150[i].z = F32(0.0f);
    view->vec150[i].y = F32(0.0f);
    view->vec150[i].x = F32(0.0f);
  }
  for (int i = 0; i < 3; i++) {
    view->ranges174[i].a = F32(1.0f);
    view->ranges174[i].b = F32(100.0f);
  }
}
HW_EXPORT(8c011afc, view_init_layout((View *)hw_ptr(ARG0), (int32_t)ARG1))
