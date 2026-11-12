/* Pen Pen port, S11: the views (one per player on a split screen).
 *
 * The game keeps four View structures (0x18C bytes each: a layout number, then the camera, screen rectangle and
 * projection that the still-translated camera code fills in). Each frame the scene tree is drawn once per view in
 * use: view_select() makes a view current (its camera and screen go to Ninja), task_draw_tree() draws, then
 * view_finish(). The split mode says how many views there are. */
#ifndef PENPEN_VIEW_H
#define PENPEN_VIEW_H

#include "hw.h"
#include "vec.h"

/* the camera as a view was asked for it (S13, core/view_setup.c: view_set_camera) */
typedef struct ViewCamera {
  Vec3 position;  /* 0x00 */
  Vec3 direction; /* 0x0C: toward the target, normalised by Ninja; (0, 0, -1) when the target is the position */
  int32_t roll;   /* 0x18: Ninja angle units */
  int32_t fov;    /* 0x1C */
} ViewCamera;
HW_SIZE(ViewCamera, 0x20);

/* NJS_SCREEN */
typedef struct NjScreen {
  f32 dist;       /* 0x00: the distance to the screen (from the field of view) */
  f32 w, h;       /* 0x04 */
  f32 cx, cy;     /* 0x0C */
} NjScreen;
HW_SIZE(NjScreen, 0x14);

typedef struct View {
  int32_t layout;               /* 0x000: index into the layout table (screen rectangle, ...) */
  uint8_t clip[0x20];           /* 0x004: the njClip2D rectangle */
  uint8_t camera[0x40];         /* 0x024: the Ninja camera (NJS_CAMERA) */
  struct {
    Vec3 position, direction;
    int32_t roll;
  } camera_copy;                /* 0x064: the first seven words of `request`, copied by view_set_camera */
  uint8_t unknown80[0x9C - 0x80];
  NjScreen screen;              /* 0x09C */
  ViewCamera request;           /* 0x0B0 */
  uint8_t unknownd0[0xF0 - 0xD0];
  int32_t fog;                  /* 0x0F0: fog on */
  Vec3 light_dir;               /* 0x0F4: the lights' direction (turned by the camera) */
  Vec3 light_intensity;         /* 0x100: (unused, diffuse, ambient): nj_Light*Intensity */
  uint32_t unknown10c;          /* 0x10C */
  Vec3 light_color;             /* 0x110: nj_Light*Color */
  uint8_t unknown11c[0x150 - 0x11C];
  Vec3 vec150[3];               /* 0x150: three members with empty constructors */
  uint8_t unknown174[0x18C - 0x174];
} View;
HW_OFFSET(View, camera, 0x24);
HW_OFFSET(View, camera_copy, 0x64);
HW_OFFSET(View, screen, 0x9C);
HW_OFFSET(View, request, 0xB0);
HW_OFFSET(View, fog, 0xF0);
HW_OFFSET(View, light_color, 0x110);
HW_OFFSET(View, vec150, 0x150);
HW_SIZE(View, 0x18C);

static inline View *view_at(gaddr a) { return (View *)hw_ptr(a); }

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
void view_set_light_dir(f32 x, f32 y, f32 z, int32_t index);
void view_set_light_intensity(f32 a, f32 b, f32 c, int32_t index);
void view_set_light_color(f32 r, f32 g, f32 b, int32_t index);
void view_fog_off(int32_t index);
void view_apply_fog(void);
gaddr view_screen(void);
int32_t view_current_index(void);

/* S14 */
void view_fog_on(int32_t index);
void view_current_set_light_color(f32 r, f32 g, f32 b);
void view_current_set_light_dir(f32 x, f32 y, f32 z);

#endif
