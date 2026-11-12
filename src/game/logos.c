/* Pen Pen port, S15 (b01): Game mode 0's scene: the logos shown at power-on (game/logos.c).
 *
 * The scene (0x38 bytes, constructor func_8C01A3EE -> func_8C01A386, vtable 0x8C23D93C, a Task) shows two pictures
 * in turn on a black background: each fades in, stays 90 frames and fades out (the first picture's texture is
 * replaced by the second's when its fade-out ends); then the game goes to mode 3. core/texture.c declares the
 * scene's texture list as Mode0Scene; this is the whole class. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/view.h"
#include "game.h"

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

#define LOGO_TEXLIST      0x8C08C730u /* the scene's NJS_TEXLIST */
#define LOGO_FIRST_FILE   0x8C070734u /* the first picture's PVM file name */
#define LOGO_SECOND_FILE  0x8C070748u /* the second's */
#define g_logo_fade_step  GUEST_VAR(f32, 0x8C070710u) /* added to / taken from the fade per frame (1/60) */
#define LOGO_BACKGROUND   0xFF000000u /* opaque black */
#define LOGO_SHOW_FRAMES  90

/* the update's steps: the first picture (0-2), the second (3-5), then the next mode */
enum {
  LOGO_FADE_IN = 0, LOGO_SHOW = 1, LOGO_FADE_OUT = 2,
  LOGO2_FADE_IN = 3, LOGO2_SHOW = 4, LOGO2_FADE_OUT = 5,
  LOGO_DONE = 10, LOGO_LEAVE = 99,
};

/* func_8C0120CA (core/texture.c, not in a header): load the PVM file `name` into texlist */
static void texture_load(gaddr name, gaddr texlist, gaddr dir_name) { CALL3(0x8C0120CAu, name, texlist, dir_name); }
/* func_8C010F04 (core/gfx_init.c, no header): the background colour and the fog's near / far distances */
static void background_set(uint32_t argb, f32 fog_near, f32 fog_far) {
  FARGS2(fog_near, fog_far);
  CALL1(0x8C010F04u, argb);
}
/* func_8C01A404 (core/texture.c, no header): the scene's destructor body (its texture list released) */
static void logo_scene_destroy(gaddr scene) { CALL1(0x8C01A404u, scene); }

/* was func_8C01A386: the construction: the Task's words set inline, the V-sync callback's work held while one
 * full-screen view, a black background (fog from 0 to -1) and the first picture are set up */
gaddr logo_scene_init(gaddr scene) {
  Task *t = task_at(scene);
  t->linger = 0;
  t->active = 1;
  t->stops_on_pause = 1;
  t->child = 0;
  t->next = 0;
  t->parent = 0;
  t->owner = 0;
  logo_scene_at(scene)->word24 = 0;
  g_video_running = 0;
  view_set_split(SPLIT_FULL);
  background_set(LOGO_BACKGROUND, F32(0.0f), F32(-1.0f));
  logo_scene_at(scene)->texlist = LOGO_TEXLIST;
  texture_load(LOGO_FIRST_FILE, LOGO_TEXLIST, 0);
  LogoScene *s = logo_scene_at(scene);
  s->step = LOGO_FADE_IN;
  s->timer = 0;
  s->fade = F32(0.0f);
  g_video_running = 1;
  return scene;
}
HW_EXPORT(8c01a386, RET(logo_scene_init(ARG0)))

/* was func_8C01A412: the deleting destructor */
gaddr logo_scene_delete(gaddr scene, int32_t free_memory) {
  logo_scene_destroy(scene);
  if (free_memory)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c01a412, RET(logo_scene_delete(ARG0, (int32_t)ARG1)))

/* was func_8C01A448: the update: fade in to 1, show for 90 frames, fade out to 0, for each picture; then ask the
 * owner (the Game) for mode 3 */
void logo_scene_update(gaddr scene) {
  LogoScene *s = logo_scene_at(scene);
  f32 step = g_logo_fade_step;
  switch (s->step) {
  case LOGO_FADE_IN:
  case LOGO2_FADE_IN:
    s->fade = fadd(s->fade, step);
    if (fgt(s->fade, F32(1.0f))) {
      s->fade = F32(1.0f);
      s->timer = LOGO_SHOW_FRAMES;
      s->step++;
    }
    break;
  case LOGO_SHOW:
  case LOGO2_SHOW:
    s->timer = s->timer - 1;
    if (s->timer < 0) {
      s->timer = 0;
      s->step++;
    }
    break;
  case LOGO_FADE_OUT:
    s->fade = fsub(s->fade, step);
    if (fgt(F32(0.0f), s->fade)) {
      s->fade = F32(0.0f);
      s->step++;
      njReleaseTexture(s->texlist);
      texture_load(LOGO_SECOND_FILE, logo_scene_at(scene)->texlist, 0);
    }
    break;
  case LOGO2_FADE_OUT:
    s->fade = fsub(s->fade, step);
    if (fgt(F32(0.0f), s->fade)) {
      s->fade = F32(0.0f);
      s->step = LOGO_DONE;
    }
    break;
  case LOGO_DONE:
    s->step = LOGO_LEAVE;
    break;
  case LOGO_LEAVE: {
    Game *game = game_at(s->task.owner);
    game->next_mode = MODE_3;
    game->mode_arg = 0;
    break;
  }
  default:
    break;
  }
}
HW_EXPORT(8c01a448, logo_scene_update(ARG0))
