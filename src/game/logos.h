/* Pen Pen port, S19 (u): Game mode 0's scene class (game/logos.c declared it in the .c; moved here for its draw in
 * game/logos_draw.c: the lead may have logos.c include this header instead of its own copy). */
#ifndef PENPEN_LOGOS_H
#define PENPEN_LOGOS_H
#include "hw.h"
#include "../core/task.h"

typedef struct LogoScene {
  Task task;       /* 0x00 */
  uint32_t word24; /* 0x24: cleared at construction */
  int32_t step;    /* 0x28: LOGO_* */
  int32_t timer;   /* 0x2C: frames left to show the picture */
  gaddr texlist;   /* 0x30: a static NJS_TEXLIST (LOGO_TEXLIST) */
  f32 fade;        /* 0x34: the picture's brightness, 0 to 1 */
} LogoScene;
HW_OFFSET(LogoScene, word24, 0x24);
HW_OFFSET(LogoScene, step, 0x28);
HW_OFFSET(LogoScene, timer, 0x2C);
HW_OFFSET(LogoScene, texlist, 0x30);
HW_OFFSET(LogoScene, fade, 0x34);
HW_SIZE(LogoScene, 0x38);

static inline LogoScene *logo_scene_at(gaddr a) { return (LogoScene *)hw_ptr(a); }

/* S19 u: game/logos_draw.c */
void logo_scene_draw(gaddr scene);

#endif
