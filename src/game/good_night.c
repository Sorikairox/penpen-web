/* Pen Pen port, S14 (b02): PenPen House's GOOD NIGHT (Game mode 4's scene, game/good_night.c), and the opening
 * movie's scene (mode 3).
 *
 * The good-night scene (0x88 bytes, constructor func_8C01D4CA -> func_8C01D2C4, vtable 0x8C23D984 {func_8C01D51A,
 * func_8C01D58C, func_8C01DBB8}) shows a penguin (a MotionPlayer, game/motion.h) and pages of labels in the console's
 * language, faded in and out; it leaves to another mode through its owner, the Game. The movie scene (0x28 bytes,
 * constructor func_8C01DDE2 -> func_8C01DD7A) plays the Sofdec opening movie. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/ninja.h"
#include "game.h"
#include "sound.h"
#include "motion.h"
#include "menu.h"
#include "../core/inline.h"
#include "../core/pad.h"

/* ---- Game (game/game.h) ---- */

/* was func_8C01D1D2: switch the game to `mode` with its argument (the scenes call it on their owner) */
void game_set_next_mode(gaddr game, int32_t mode, int32_t arg) {
  Game *g = game_at(game);
  g->next_mode = mode;
  g->mode_arg = arg;
}
HW_EXPORT(8c01d1d2, game_set_next_mode(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* ---- the good-night scene ---- */

typedef struct GoodNightScene {
  Task task;          /* 0x00 */
  uint32_t word24;    /* 0x24 */
  gaddr file;         /* 0x28: its model file (file_free) */
  gaddr penguin;      /* 0x2C: MotionPlayer, or 0 */
  gaddr texlist;      /* 0x30: a static NJS_TEXLIST (0x8C08C9C4) */
  int32_t character;  /* 0x34: the penguin shown (score table a's best entry, game/save.c); it picks the camera and the
                       * placements too */
  int32_t choice;     /* 0x38: 0 or 1 (left / right on the last page) */
  Body cursor;        /* 0x3C */
  gaddr model;        /* 0x6C */
  int32_t step;       /* 0x70: the update's state */
  int32_t next_step;  /* 0x74 */
  int32_t timer;      /* 0x78 */
  int32_t page;       /* 0x7C: the labels shown: 0, 1, 2 (another value: none) */
  f32 alpha;          /* 0x80: the labels' alpha (fade / 60) */
  f32 fade;           /* 0x84: frames into the fade */
} GoodNightScene;
HW_OFFSET(GoodNightScene, file, 0x28);
HW_OFFSET(GoodNightScene, character, 0x34);
HW_OFFSET(GoodNightScene, cursor, 0x3C);
HW_OFFSET(GoodNightScene, model, 0x6C);
HW_OFFSET(GoodNightScene, page, 0x7C);
HW_OFFSET(GoodNightScene, fade, 0x84);
HW_SIZE(GoodNightScene, 0x88);

static inline GoodNightScene *good_night_at(gaddr a) { return (GoodNightScene *)hw_ptr(a); }

/* the scene's cameras (0x40-byte entries, given to njSetView by the draw; the update reads their first words) */
#define GOOD_NIGHT_CAMERAS  0x8C08C9E0u
#define NJS_VIEW_SIZE       0x40u
typedef struct GoodNightCamera {
  Vec3 eye;             /* 0x00 */
  Vec3 target;          /* 0x0C */
  int32_t roll;         /* 0x18 */
  int32_t fov;          /* 0x1C */
} GoodNightCamera;

/* the labels: per language (7) nine places and nine strings; page 0 shows labels 0-3, page 1 4-6, page 2 7-8 */
#define LABELS 9
typedef struct LabelPlace {
  int32_t x, y;
} LabelPlace;
HW_SIZE(LabelPlace, 8);
#define g_label_places  ((const LabelPlace (*)[LABELS])hw_ptr(0x8C070CE8u))
#define g_label_strings ((const gaddr (*)[LABELS])hw_ptr(0x8C08CBE0u))
static const struct { int32_t first, count; } label_pages[3] = {{0, 4}, {4, 3}, {7, 2}};

/* the text style of core/sprite_text.c (its .bss) */
#define TEXT_SPRITE_SMALL 0x8C1B3AE8u /* NJS_SPRITE */
#define g_text_scale_x    GUEST_VAR(f32, 0x8C1B3B28u)
#define g_text_scale_y    GUEST_VAR(f32, 0x8C1B3B2Cu)
#define g_text_style_30   GUEST_VAR(f32, 0x8C1B3B30u)
#define g_text_color      ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b */
#define LABEL_GREEN       0x3F547AE1u /* 0.83f */
#define LABEL_BLUE        0x3ECCCCCCu /* 0.4f */

/* still translated */
static int32_t game_language(void) { return (int32_t)CALL0(0x8C019860u); } /* func_8C019860: 0-6 */
/* func_8C01529A: draws a string at (x, y) with the text style */
static int32_t text_draw(int32_t x, int32_t y, gaddr string) { return (int32_t)CALL3(0x8C01529Au, x, y, string); }

/* ---- construction (S15) ---- */

/* functions of other files (no header yet) */
void loading_screen_show(int32_t picture);                    /* game/loading.c */
int32_t save_best_a(void);                                    /* game/save.c */
void texture_load(gaddr name, gaddr texlist, gaddr dir_name); /* core/texture.c */
void view_set_camera(gaddr view, gaddr position, gaddr target, f32 roll, f32 fov, int32_t apply); /* core/view_setup.c */

#define GOOD_NIGHT_FILE     0x8C071434u /* the penguins' and the room's model container */
#define GOOD_NIGHT_TEXLIST  0x8C08C9C4u /* a static NJS_TEXLIST */
#define PENGUIN_SIZE        0xF8u       /* a menu penguin (game/menu_penguin.c) */
#define GN_PENGUIN_ENTRIES  3           /* the container's entries per character, from the first */
#define GN_MODEL_ENTRY      24          /* the room's model */
#define GN_FOV              0x462AAAAAu /* 10922.67: 60 degrees in Ninja angle units */
/* the labels' textures per language (0-6; 0 and 1 share English) */
static const gaddr label_textures[7] = {0x8C071448u, 0x8C071448u, 0x8C07145Cu, 0x8C071470u,
                                        0x8C071484u, 0x8C071498u, 0x8C0714ACu};

/* the original's frame below its saved registers: view_set_camera reads the eye and the target from it */
typedef struct GoodNightInitFrame {
  gaddr memory;       /* 0x00: the penguin's allocation (kept there by the original) */
  Vec3 target;        /* 0x04: (0, 0, -1) */
  Vec3 eye;           /* 0x10: the origin */
  uint32_t saved[6];  /* 0x1C: r14, r13, r12, fr15, fr14, PR */
} GoodNightInitFrame;
HW_OFFSET(GoodNightInitFrame, target, 0x04);
HW_OFFSET(GoodNightInitFrame, eye, 0x10);
HW_SIZE(GoodNightInitFrame, 0x34);

/* was func_8C01D2C4: the construction: the Task's words set inline, the cursor at rest, the loading screen (its
 * picture by the character: it ends frames, so this is resumable), the cursor placed at the first frame with
 * damping 0.78, the model file and the labels' textures in the console's language, the penguin, the room's model,
 * the sound set, the music (track 10, repeated), one full-screen view, and the camera at the origin looking down z */
gaddr good_night_init(gaddr scene) {
  gaddr frame = 0;
  HW_RESUMABLE(0x8C01D2C4u, scene, frame);
  HW_RESUME_AT(0);
  frame = hw_stack_alloc(sizeof(GoodNightInitFrame));
  {
    Task *t = task_at(scene);
    t->linger = 0;
    t->active = 1;
    t->stops_on_pause = 1;
    t->child = 0;
    t->next = 0;
    t->parent = 0;
    t->owner = 0;
    good_night_at(scene)->word24 = 0;
    body_init(member_addr(scene, GoodNightScene, cursor));
    good_night_at(scene)->character = save_best_a();
  }
  HW_YIELDING_RET(0, loading_screen_show(good_night_at(scene)->character + 10), 0);
  GoodNightScene *s = good_night_at(scene);
  s->step = 0;
  s->next_step = 0;
  s->timer = 60;
  s->choice = 0;
  gaddr cursor = member_addr(scene, GoodNightScene, cursor);
  body_reset(cursor);
  s->cursor.damping.z = s->cursor.damping.y = s->cursor.damping.x = F32_BITS(0x3F47AE14u); /* 0.78 */
  s->cursor.position.x = F32(186.0f);
  s->cursor.position.y = F32(322.0f); /* GN_CURSOR_Y */
  s->cursor.position.z = F32(0.0f);
  s->file = container_load(GOOD_NIGHT_FILE, 0);
  s->texlist = GOOD_NIGHT_TEXLIST;
  int32_t language = game_language();
  if (language >= 0 && language <= 6)
    texture_load(label_textures[language], s->texlist, 0);
  gaddr memory = syMalloc(PENGUIN_SIZE);
  s->penguin = memory ? menu_penguin_construct(memory, s->character, 0,
                                               s->file + (uint32_t)s->character * GN_PENGUIN_ENTRIES *
                                                             sizeof(ContainerEntry), 1)
                      : 0;
  s->model = (gaddr)((const ContainerEntry *)hw_ptr(s->file))[GN_MODEL_ENTRY].offset;
  sound_select_set(0);
  music_play(10, 0, 1);
  view_set_split(0);
  GoodNightInitFrame *f = (GoodNightInitFrame *)hw_ptr(frame);
  f->target.z = F32(-1.0f);
  f->target.y = f->target.x = F32(0.0f);
  f->eye.z = f->eye.y = f->eye.x = F32(0.0f);
  view_set_camera(hw_addr(&g_views[0]), member_addr(frame, GoodNightInitFrame, eye),
                  member_addr(frame, GoodNightInitFrame, target), F32(0.0f), F32_BITS(GN_FOV), 0);
  hw_stack_free(sizeof(GoodNightInitFrame));
  return scene;
}
HW_EXPORT_RESUMABLE(8c01d2c4, RET(good_night_init(ARG0)))

/* was func_8C01D4CA: the constructor (the vtable, then the construction: resumable) */
gaddr good_night_construct(gaddr scene) {
  HW_RESUMABLE(0x8C01D4CAu, scene);
  HW_RESUME_AT(0);
  task_at(scene)->vtable = 0x8C23D984u;
  HW_YIELDING_RET(0, good_night_init(scene), 0);
  return scene;
}
HW_EXPORT_RESUMABLE(8c01d4ca, RET(good_night_construct(ARG0)))

/* was func_8C01D4E0: the destructor's body: the texture list released, the penguin deleted, the model file freed,
 * the music stopped; then it waits while the drive is busy (status 3) */
void good_night_destroy(gaddr scene) {
  GoodNightScene *s = good_night_at(scene);
  njReleaseTexture(s->texlist);
  gaddr penguin = s->penguin;
  if (penguin)
    motion_player_delete(penguin, 1);
  file_free(good_night_at(scene)->file);
  music_stop();
  while (gdFsGetDrvStat() == 3) {
  }
}
HW_EXPORT(8c01d4e0, good_night_destroy(ARG0))

/* was func_8C01D51A: the deleting destructor */
gaddr good_night_delete(gaddr scene, int32_t free_memory) {
  good_night_destroy(scene);
  if (free_memory)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c01d51a, RET(good_night_delete(ARG0, (int32_t)ARG1)))

/* was func_8C01DA44: draws the labels of the current page in the console's language: scale 1, a pale yellow at the
 * scene's alpha */
void good_night_draw_labels(gaddr scene) {
  NjSprite *small = (NjSprite *)hw_ptr(TEXT_SPRITE_SMALL);
  g_text_scale_x = F32(1.0f);
  small->sx = F32(1.0f);
  g_text_scale_y = F32(1.0f);
  small->sy = F32(1.0f);
  g_text_style_30 = F32(0.0f);
  f32 *color = g_text_color;
  color[1] = F32(1.0f);
  color[2] = F32_BITS(LABEL_GREEN);
  color[3] = F32_BITS(LABEL_BLUE);
  color[0] = good_night_at(scene)->alpha;
  int32_t language = game_language();
  int32_t page = good_night_at(scene)->page;
  if (page < 0 || page > 2)
    return;
  for (int32_t k = label_pages[page].first; k < label_pages[page].first + label_pages[page].count; k++) {
    const LabelPlace *at = &g_label_places[language][k];
    text_draw(at->x, at->y, g_label_strings[language][k]);
  }
}
HW_EXPORT(8c01da44, good_night_draw_labels(ARG0))

/* the update's steps */
enum {
  GN_WAIT, GN_FADE_IN, GN_SHOW, GN_FADE_OUT, GN_PAUSE, GN_CHOOSE, GN_SAVE, GN_NO_SAVE, GN_LAST,
};
#define GN_FADE_FRAMES  F32(60.0f)
#define g_gn_push       GUEST_VAR(f32, 0x8C070B64u) /* 8.0: the push on the cursor when the choice changes */
#define GN_CURSOR_RATE  0x3DCCCCCCu                 /* 0.1f */
#define GN_CURSOR_Y     322                         /* the frame's bottom edge (and the cursor's target y) */

/* still translated: func_8C018F7C, the VMU save (nonzero once it has saved) */
static int32_t save_to_vmu(void) { return (int32_t)CALL0(0x8C018F7Cu); }
/* func_8C015AA8: the frame drawn between two corners (in pixels) */
static void frame_draw(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { CALL4(0x8C015AA8u, x0, y0, x1, y1); }

/* a fade starts: from `fade`, nothing counted, page `page`, through `step` to `next` */
static void gn_start(GoodNightScene *s, f32 fade, int32_t page, int32_t step, int32_t next) {
  s->fade = fade;
  s->timer = 0;
  s->page = page;
  s->step = step;
  s->next_step = next;
}

/* was func_8C01D58C: the update: a wait, page 0 faded in, shown (A skips once 240 frames are left), faded out; a
 * pause, page 1 (the choice: left / right moves a cursor between two frames, A takes it: save to the VMU or not),
 * faded out; the save (or a pause), page 2 shown until its time is up or A (once 540 frames are left), then back to
 * the title (mode 2). Every frame the scene's camera, and the penguin's motion */
void good_night_update(gaddr scene) {
  GoodNightScene *s = good_night_at(scene);
  switch (s->step) {
  case GN_WAIT:
    if (--s->timer < 0)
      gn_start(s, F32(0.0f), 0, GN_FADE_IN, GN_SHOW);
    break;
  case GN_FADE_IN:
    s->fade = fadd(s->fade, F32(1.0f));
    s->alpha = fdiv(s->fade, GN_FADE_FRAMES);
    good_night_draw_labels(scene);
    if (fgt(s->alpha, F32(1.0f))) {
      s->fade = GN_FADE_FRAMES;
      s->alpha = F32(1.0f);
      s->step = s->next_step;
      s->timer = s->next_step == GN_LAST ? 600 : 240;
    }
    break;
  case GN_SHOW:
    good_night_draw_labels(scene);
    if (--s->timer < 0 || (s->timer < 240 && port_pressed(0, PAD_A)))
      gn_start(s, GN_FADE_FRAMES, 0, GN_FADE_OUT, GN_PAUSE);
    break;
  case GN_FADE_OUT:
    s->fade = fsub(s->fade, F32(1.0f));
    s->alpha = fdiv(s->fade, GN_FADE_FRAMES);
    good_night_draw_labels(scene);
    if (!fgt(s->alpha, F32(0.0f))) {
      s->timer = 60;
      s->fade = F32(0.0f);
      s->alpha = F32(0.0f);
      s->step = s->next_step;
    }
    break;
  case GN_PAUSE:
    if (--s->timer < 0) {
      gn_start(s, F32(0.0f), 1, GN_FADE_IN, GN_CHOOSE);
      f32_add_to(g_gn_push, member_addr(scene, GoodNightScene, cursor.force.y));
    }
    break;
  case GN_CHOOSE: {
    gaddr target = hw_stack_alloc(sizeof(Vec3));
    method_self(target); /* the Vec3's empty constructor */
    good_night_draw_labels(scene);
    if (port_pressed(0, PAD_LEFT)) {
      if (s->choice != 0) {
        s->choice = 0;
        f32_add_to(g_gn_push, member_addr(scene, GoodNightScene, cursor.force.y));
        sound_play_port_sound(0);
      }
    } else if (port_pressed(0, PAD_RIGHT) && s->choice != 1) {
      s->choice = 1;
      f32_add_to(fneg(g_gn_push), member_addr(scene, GoodNightScene, cursor.force.y));
      sound_play_port_sound(0);
    }
    int32_t right;
    if (s->choice == 0) {
      vec3_set(F32(186.0f), F32(322.0f), F32(0.0f), target);
      right = 218;
    } else {
      vec3_set(F32(300.0f), F32(322.0f), F32(0.0f), target);
      right = 332;
    }
    gaddr cursor = member_addr(scene, GoodNightScene, cursor);
    body_pull(F32_BITS(GN_CURSOR_RATE), cursor, target);
    body_step(cursor);
    int32_t y = f32_to_int(s->cursor.position.y);
    int32_t x = f32_to_int(s->cursor.position.x);
    frame_draw(x, y, right, GN_CURSOR_Y);
    hw_stack_free(sizeof(Vec3));
    if (port_pressed(0, PAD_A)) {
      s->fade = GN_FADE_FRAMES;
      s->timer = 0;
      s->page = 1;
      s->step = GN_FADE_OUT;
      if (s->choice == 0) {
        s->next_step = GN_SAVE;
        sound_play_port_sound(2);
      } else {
        s->next_step = GN_NO_SAVE;
        sound_play_port_sound(1);
      }
    }
    break;
  }
  case GN_SAVE:
    if (save_to_vmu() == 0)
      return; /* (no camera, no motion this frame) */
    gn_start(s, F32(0.0f), 2, GN_FADE_IN, GN_LAST);
    break;
  case GN_NO_SAVE:
    if (--s->timer < 0)
      gn_start(s, F32(0.0f), 2, GN_FADE_IN, GN_LAST);
    break;
  default: /* GN_LAST */
    good_night_draw_labels(scene);
    if (--s->timer < 0 || (s->timer < 540 && port_pressed(0, PAD_A)))
      game_set_next_mode(task_at(scene)->owner, MODE_TITLE, 0);
    break;
  }

  /* the camera of the scene's NJS_VIEW: its position, its direction as the target, its roll and field of view
   * (integers there) */
  const GoodNightCamera *cam = (const GoodNightCamera *)hw_ptr(GOOD_NIGHT_CAMERAS + (uint32_t)s->character * NJS_VIEW_SIZE);
  f32 fov = f32_from_int(cam->fov), roll = f32_from_int(cam->roll);
  gaddr frame = hw_stack_alloc(2 * sizeof(Vec3));
  gaddr target = frame, eye = frame + (gaddr)sizeof(Vec3);
  vec3_set(cam->target.x, cam->target.y, cam->target.z, target);
  vec3_set(cam->eye.x, cam->eye.y, cam->eye.z, eye);
  house_view_camera(0, eye, target, roll, fov);
  hw_stack_free(2 * sizeof(Vec3));
  motion_player_update(good_night_at(scene)->penguin);
}
HW_EXPORT(8c01d58c, good_night_update(ARG0))

/* the draw's data: the background sprite, the cameras, and per camera a placement of the penguin and of the model
 * (a translation, then three angles kept as floats) */
#define GOOD_NIGHT_SPRITE   0x8C08CCDCu /* an NJS_SPRITE copied to the stack (its texture list replaced) */
#define g_shared_texlist    GUEST_VAR(gaddr, 0x8C1B7E40u)
typedef struct Placement {
  f32 x, y, z;          /* 0x00 */
  f32 ax, ay, az;       /* 0x0C: Ninja angle units */
} Placement;
HW_SIZE(Placement, 0x18);
#define g_penguin_places ((const Placement *)hw_ptr(0x8C070B68u))
#define g_model_places   ((const Placement *)hw_ptr(0x8C070C28u))
#define LIGHT_DIR        0xBF13CD35u /* -0.577f: down, away and left, each axis the same */
#define LIGHT_RED        0x3F018937u /* 0.506f */
#define LIGHT_GREEN      0x3F4BC6A7u /* 0.796f */
#define LIGHT_AMBIENT    0x3ECCCCCCu /* 0.4f */

static inline void njSetView(gaddr view) { CALL1(0x8C0CF4ECu, view); } /* SDK (Ninja) */

static void place_matrix(const Placement *p) {
  njPushMatrix(0);
  njTranslate(0, p->x, p->y, p->z);
  njRotateXYZ(0, f32_to_int(p->ax), f32_to_int(p->ay), f32_to_int(p->az));
}

/* was func_8C01DBB8 (with its body at func_8C01DBD2, the dump's second piece of it): the draw: the background
 * sprite with the scene's textures, view 0's light (intensity, direction, colour), the scene's camera, then the
 * penguin and the model each at the camera's placement */
void good_night_draw(gaddr scene) {
  gaddr sprite = hw_stack_alloc(sizeof(NjSprite));
  memmove(hw_ptr(sprite), hw_ptr(GOOD_NIGHT_SPRITE), sizeof(NjSprite));
  ((NjSprite *)hw_ptr(sprite))->tlist = good_night_at(scene)->texlist;
  njDrawSprite2D(sprite, 0, F32(-2.0f), 0x20);
  hw_stack_free(sizeof(NjSprite));

  View *v = &g_views[0];
  v->light_intensity.x = F32(1.0f);
  v->light_intensity.y = F32(1.0f);
  v->light_intensity.z = F32_BITS(LIGHT_AMBIENT);
  vec3_set(F32_BITS(LIGHT_DIR), F32_BITS(LIGHT_DIR), F32_BITS(LIGHT_DIR), hw_addr(&v->light_dir));
  v->light_color.x = F32_BITS(LIGHT_RED);
  v->light_color.y = F32_BITS(LIGHT_GREEN);
  v->light_color.z = F32(1.0f);
  njSetView(GOOD_NIGHT_CAMERAS + (uint32_t)good_night_at(scene)->character * NJS_VIEW_SIZE);
  njSetTexture(g_shared_texlist);

  place_matrix(&g_penguin_places[good_night_at(scene)->character]);
  motion_player_draw(good_night_at(scene)->penguin);
  place_matrix(&g_model_places[good_night_at(scene)->character]);
  nj_ModelL1(good_night_at(scene)->model);
  njPopMatrix(1);
  matrix_pop();
}
HW_EXPORT(8c01dbb8, good_night_draw(ARG0))

/* ---- the movie scene (mode 3) ---- */

typedef struct MovieScene {
  Task task;          /* 0x00 */
  uint32_t word24;    /* 0x24 */
} MovieScene;
HW_SIZE(MovieScene, 0x28);

/* mwPlyInitSofdec's parameters (on the stack) */
typedef struct MwSofdecInit {
  uint32_t display_mode;      /* 0x00: the game's sbInitSystem mode */
  uint32_t frame_buffer_mode; /* 0x04 */
  uint32_t frame_count_mode;  /* 0x08 */
  uint32_t word0c;            /* 0x0C: 2 */
} MwSofdecInit;
HW_SIZE(MwSofdecInit, 16);

static inline void mwPlyInitSofdec(gaddr init) { CALL1(0x8C09BD64u, init); } /* SDK (CRI) */
/* func_8C010F04: the background colour and fog range (as the game sets them for a scene) */
static void background_set(uint32_t argb, int32_t mode, f32 fog_near, f32 fog_far) {
  FARGS2(fog_near, fog_far);
  CALL2(0x8C010F04u, argb, mode);
}

/* was func_8C01DD7A: the movie scene's construction: the Task's words set inline (active, stops on pause, no
 * links), the V-sync work stopped meanwhile, one full-screen view on black, then Sofdec initialised with the
 * game's video modes */
gaddr movie_init(gaddr scene) {
  Task *t = task_at(scene);
  t->linger = 0;
  t->active = 1;
  t->stops_on_pause = 1;
  t->child = 0;
  t->next = 0;
  t->parent = 0;
  t->owner = 0;
  g_video_running = 0;
  view_set_split(0);
  background_set(0xFF000000u, 0, F32(0.0f), F32(-1.0f));
  ((MovieScene *)hw_ptr(scene))->word24 = 0;
  gaddr init = hw_stack_alloc(sizeof(MwSofdecInit));
  MwSofdecInit *in = (MwSofdecInit *)hw_ptr(init);
  in->display_mode = (uint32_t)g_display_mode;
  in->frame_buffer_mode = (uint32_t)g_frame_buffer_mode;
  in->frame_count_mode = (uint32_t)g_frame_count_mode;
  in->word0c = 2;
  mwPlyInitSofdec(init);
  hw_stack_free(sizeof(MwSofdecInit));
  g_video_running = 1;
  return scene;
}
HW_EXPORT(8c01dd7a, RET(movie_init(ARG0)))

/* ---- the movie's playback (S15) ---- */

/* a Sofdec player (CRI middleware): an object whose first word is its method table */
typedef struct MwPlyVtbl {
  gaddr methods0[5];  /* 0x00 */
  gaddr destroy;      /* 0x14: (ply) */
  gaddr start_fname;  /* 0x18: (ply, file name) */
  gaddr stop;         /* 0x1C: (ply) */
  gaddr get_stat;     /* 0x20: (ply) -> MWE_PLY_STAT_* */
} MwPlyVtbl;
HW_OFFSET(MwPlyVtbl, destroy, 0x14);
HW_OFFSET(MwPlyVtbl, get_stat, 0x20);
#define MWE_PLY_STAT_PLAYEND 3
#define MWE_PLY_STAT_ERROR   4
typedef struct MwPly {
  gaddr vtable;       /* 0x00: MwPlyVtbl */
} MwPly;
static inline const MwPlyVtbl *mw_ply_vtbl(gaddr ply) { return (const MwPlyVtbl *)hw_ptr(((const MwPly *)hw_ptr(ply))->vtable); }

/* the player's creation parameters (mwPlyCreateSofdec) */
typedef struct MwCreateParams {
  int32_t ftype;      /* 0x00: 1 */
  int32_t max_bps;    /* 0x04 */
  int32_t max_width;  /* 0x08 */
  int32_t max_height; /* 0x0C */
  int32_t nfrm_pool;  /* 0x10: 3 */
  gaddr work;         /* 0x14 */
  int32_t work_size;  /* 0x18 */
  int32_t word1c;     /* 0x1C: 1 */
} MwCreateParams;
HW_SIZE(MwCreateParams, 0x20);

/* the original's frame (with its saved registers): the player's handle is kept in its first word, and that
 * word's address is the error callback's argument */
typedef struct MoviePlayFrame {
  gaddr ply;              /* 0x00 */
  gaddr name;             /* 0x04 */
  MwCreateParams params;  /* 0x08 */
  uint32_t saved[8];      /* 0x28: r14-r8, PR */
} MoviePlayFrame;
HW_OFFSET(MoviePlayFrame, params, 0x08);
HW_SIZE(MoviePlayFrame, 0x48);

#define MOVIE_MAX_BPS   0x384000
#define MOVIE_EMPTY_CALLBACK 0x8C01DD76u /* func_8C01DD76: returns at once */
#define MOVIE_NAME      0x8C0714C0u      /* the opening movie's file name */
#define PAD_START       0x0008u          /* PDD_DGT_ST */

static inline int32_t mwPlyCalcWorkCprmSfd(int32_t ftype, int32_t max_bps, int32_t width, int32_t height,
                                           int32_t nfrm) {
  return (int32_t)CALL5(0x8C09C392u, ftype, max_bps, width, height, nfrm);
}
static inline gaddr mwPlyCreateSofdec(gaddr params) { return CALL1(0x8C09B09Cu, params); }
static inline void mwPlySetOutVol(gaddr ply, int32_t volume) { CALL2(0x8C09C9BAu, ply, volume); }
static inline void mwPlyFinishSofdec(void) { CALL0(0x8C09BEC0u); }
/* middleware thunks the game calls around the playback (func_8C09AFDE / 8C09AFD8 / 8C09AFE4: jumps into the SDK) */
static inline void mw_set_error_callback(gaddr fn, gaddr arg) { CALL2(0x8C09AFDEu, fn, arg); }
static inline void mw_after_vsync(void) { CALL0(0x8C09AFD8u); }
static inline void mw_idle(void) { CALL0(0x8C09AFE4u); }

/* was func_8C01DEF8: play movie file `name` until it ends (1), Start is pressed (2), or the player fails or reports
 * an error (3): the player is created with work memory of the size it asks for, each frame ends in njWaitVSync,
 * then stopped, one more frame, destroyed, and its memory freed. Resumable (it ends frames) */
int32_t movie_play(gaddr scene, gaddr name) {
  gaddr frame = 0;
  int32_t result = 0;
  HW_RESUMABLE(0x8C01DEF8u, frame, result);
  HW_RESUME_AT(0, 1);
  frame = hw_stack_alloc(sizeof(MoviePlayFrame));
  {
    MoviePlayFrame *f = (MoviePlayFrame *)hw_ptr(frame);
    f->name = name;
    f->params.ftype = 1;
    f->params.word1c = 1;
    f->params.max_bps = MOVIE_MAX_BPS;
    f->params.max_width = 320;
    f->params.max_height = 480;
    f->params.nfrm_pool = 3;
    f->params.work_size = mwPlyCalcWorkCprmSfd(1, MOVIE_MAX_BPS, 320, 480, 3);
    f->params.work = syMalloc((uint32_t)f->params.work_size);
    f->ply = mwPlyCreateSofdec(member_addr(frame, MoviePlayFrame, params));
    if (f->ply == 0) {
      hw_stack_free(sizeof(MoviePlayFrame));
      return 3; /* (the work memory stays allocated: the original's leak) */
    }
    mw_set_error_callback(MOVIE_EMPTY_CALLBACK, frame);
    mwPlySetOutVol(f->ply, 1);
    CALL2(mw_ply_vtbl(f->ply)->start_fname, f->ply, f->name);
  }
  for (;;) {
    HW_YIELDING_RET(0, njWaitVSync(), 0);
    mw_after_vsync();
    if (((const PdPeripheral *)hw_ptr(pdGetPeripheral(0)))->press & PAD_START) {
      result = 2;
      break;
    }
    gaddr ply = ((MoviePlayFrame *)hw_ptr(frame))->ply;
    int32_t stat = (int32_t)CALL1(mw_ply_vtbl(ply)->get_stat, ply);
    if (stat == MWE_PLY_STAT_PLAYEND) {
      result = 1;
      break;
    }
    if (stat == MWE_PLY_STAT_ERROR) {
      result = 3;
      break;
    }
    mw_idle();
  }
  {
    gaddr ply = ((MoviePlayFrame *)hw_ptr(frame))->ply;
    CALL1(mw_ply_vtbl(ply)->stop, ply);
  }
  HW_YIELDING_RET(1, njWaitVSync(), 0);
  {
    MoviePlayFrame *f = (MoviePlayFrame *)hw_ptr(frame);
    CALL1(mw_ply_vtbl(f->ply)->destroy, f->ply);
    syFree(f->params.work);
  }
  hw_stack_free(sizeof(MoviePlayFrame));
  return result;
}
HW_EXPORT_RESUMABLE(8c01def8, RET(movie_play(ARG0, ARG1)))

/* was func_8C01DE32: the movie scene's update: plays the opening movie until it ends, is skipped or fails (a
 * result of 0 plays it again; movie_play never gives another), closes Sofdec, and asks the Game for the next mode: the title (mode 1) after the
 * whole movie, mode 2 when it was skipped or failed. Resumable */
void movie_update(gaddr scene) {
  int32_t outcome = 0;
  HW_RESUMABLE(0x8C01DE32u, scene, outcome);
  HW_RESUME_AT(0);
  while (outcome == 0) {
    int32_t r;
    HW_YIELDING(0, r = movie_play(scene, MOVIE_NAME));
    if (r == 1)
      outcome = 1;
    else if (r == 2 || r == 3)
      outcome = 2;
  }
  mwPlyFinishSofdec();
  game_set_next_mode(task_at(scene)->owner, outcome == 1 ? 1 : 2, 0);
}
HW_EXPORT_RESUMABLE(8c01de32, movie_update(ARG0))
