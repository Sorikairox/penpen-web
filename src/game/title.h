/* Pen Pen port, S17 (r2): the title scene's class (game/title.c, game/title_draw.c). Moved here from title.c (the
 * lead removes the copy there) so that the draw in title_draw.c can share it. */
#ifndef PENPEN_TITLE_H
#define PENPEN_TITLE_H

#include "../core/task.h"
#include "../core/pad.h"
#include "../core/vec.h"

typedef struct TitleScene {
  Task task;                  /* 0x000 */
  Pad pad;                    /* 0x024: player one */
  int32_t state;              /* 0x028: 0 fade in, 1 logo, 2 (variant 0's intro), 3 menu, 4 leaving */
  int32_t unknown02c;         /* 0x02C */
  int32_t choice;             /* 0x030: the menu's answer */
  int32_t start, a, b;        /* 0x034: pressed this frame (a key repeat lock: 0x050) */
  int32_t right, left, up, down; /* 0x040: pressed this frame */
  int32_t key_lock;           /* 0x050: frames until Start / A / B count again */
  uint8_t unknown054[0x5C - 0x54];
  int32_t logo_phase;         /* 0x05C: 0, or 2 when the logo starts finished (variant 1) */
  int32_t phase_frames;       /* 0x060: frames in the logo's phase */
  int32_t menu_page;          /* 0x064: S17 r3: the intro's menu: 0 the first page (choices 0-2), 1 the players' page
                               * (10-12) */
  int32_t demo_countdown;     /* 0x068: frames left before the attract demo (1800) */
  int32_t controllers;        /* 0x06C: S17 r3: controllers in ports 0, 1, ... up to the first empty port (the intro) */
  uint8_t unknown070[0x74 - 0x70];
  f32 frame[4];               /* 0x074: motion frames: the logo's shape motion, then the back model's two layers
                               * (S17: the draw shows the back model twice, at frame[1] and frame[2]) */
  f32 scroll_a, scroll_b;     /* 0x084: two layers' scroll (0.002 per frame; S17: the draw scales each layer by it
                               * in x and y) */
  f32 blend_a, blend_b;       /* 0x08C: their cross-fade (1 when the logo starts finished; S17: the layers' alpha) */
  f32 fade;                   /* 0x094: 0 to 1 over the fade-in (1/60 per frame) */
  gaddr texlist;              /* 0x098: NJS_TEXLIST (a static one), released with njReleaseTexture */
  gaddr model[3];             /* 0x09C: logo, star, back (chunk models read by nj_ReadChunkFile) */
  gaddr motion[3];            /* 0x0A8: their motions */
  gaddr logo_shape;           /* 0x0B4: the logo's shape motion */
  gaddr data_b8;              /* 0x0B8 */
  Body logo;                  /* 0x0BC: the logo (a screen position) */
  Body eye;                   /* 0x0EC: the camera's position */
  Body target;                /* 0x11C: what the camera looks at */
  Body mover;                 /* 0x14C: pulled toward goal */
  Body1 angle[3];             /* 0x17C: angles pulled toward goal_angle */
  Vec3 goal;                  /* 0x1AC */
  f32 goal_angle[3];          /* 0x1B8 */
} TitleScene;
HW_OFFSET(TitleScene, pad, 0x24);
HW_OFFSET(TitleScene, key_lock, 0x50);
HW_OFFSET(TitleScene, menu_page, 0x64);
HW_OFFSET(TitleScene, demo_countdown, 0x68);
HW_OFFSET(TitleScene, controllers, 0x6C);
HW_OFFSET(TitleScene, frame, 0x74);
HW_OFFSET(TitleScene, texlist, 0x98);
HW_OFFSET(TitleScene, motion, 0xA8);
HW_OFFSET(TitleScene, data_b8, 0xB8);
HW_OFFSET(TitleScene, logo, 0xBC);
HW_OFFSET(TitleScene, mover, 0x14C);
HW_OFFSET(TitleScene, angle, 0x17C);
HW_OFFSET(TitleScene, goal_angle, 0x1B8);
HW_SIZE(TitleScene, 0x1C4);

static inline TitleScene *title_at(gaddr a) { return (TitleScene *)hw_ptr(a); }

/* game/title_draw.c */
void title_draw_logo(gaddr scene);

#endif
