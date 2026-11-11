/* Pen Pen port, S11: the views (one per player on a split screen).
 *
 * The game keeps four View structures (0x18C bytes each: a layout number, then the camera, screen rectangle and
 * projection that the still-translated camera code fills in). Each frame the scene tree is drawn once per view in
 * use: view_select() makes a view current (its camera and screen go to Ninja), task_draw_tree() draws, then
 * view_finish(). The split mode says how many views there are. */
#ifndef PENPEN_VIEW_H
#define PENPEN_VIEW_H

#include "hw.h"

typedef struct View {
  int32_t layout;               /* 0x000: index into the layout table (screen rectangle, ...) */
  uint8_t camera[0x18C - 4];    /* 0x004: camera, screen and projection (floats; translated code) */
} View;
HW_SIZE(View, 0x18C);

#define VIEW_MAX 4
enum { SPLIT_FULL = 0, SPLIT_TWO = 1, SPLIT_TWO_WITH_PANEL = 2, SPLIT_FOUR = 3 };

#define g_views          ((View *)hw_ptr(0x8C1B34B8u))
#define g_split_mode     GUEST_VAR(int32_t, 0x8C1B34B4u)
#define g_cur_view       GUEST_VAR(gaddr, 0x8C08A898u)
#define g_cur_view_index GUEST_VAR(int32_t, 0x8C08A89Cu)
#define VIEW_INVERSE_MATRIX 0x8C1B3114u /* the current view's inverted camera matrix (NJS_MATRIX) */

void view_frame_begin(void);
void view_frame_end(void);
void view_select(int32_t index);
void view_finish(void);
void views_reset(void);
void view_set_split(int32_t mode);

#endif
