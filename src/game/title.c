/* Pen Pen port, S12, S13: the title scene (mode 2's scene; mode 1 builds the same one with variant 0). A Task
 * subclass of 0x1C4 bytes whose construction loads the title's data (it can end frames: the fade it starts waits for
 * V-blanks). S13: the construction, the struct; func_8C01B812 (the destructor without the free) is never called. */
#include "game.h"
#include "sound.h"
#include "sdk.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "../core/file.h"

#define TITLE_VTABLE 0x8C23D96Cu /* {destroy = title_destroy, update, draw} */


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
  int32_t unknown064;         /* 0x064 */
  int32_t demo_countdown;     /* 0x068: frames left before the attract demo (1800) */
  uint8_t unknown06c[0x74 - 0x6C];
  f32 frame[4];               /* 0x074: motion frames (logo, star, back, ...) */
  f32 scroll_a, scroll_b;     /* 0x084: two layers' scroll (0.002 per frame) */
  f32 blend_a, blend_b;       /* 0x08C: their cross-fade (1 when the logo starts finished) */
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
HW_OFFSET(TitleScene, demo_countdown, 0x68);
HW_OFFSET(TitleScene, frame, 0x74);
HW_OFFSET(TitleScene, texlist, 0x98);
HW_OFFSET(TitleScene, motion, 0xA8);
HW_OFFSET(TitleScene, data_b8, 0xB8);
HW_OFFSET(TitleScene, logo, 0xBC);
HW_OFFSET(TitleScene, mover, 0x14C);
HW_OFFSET(TitleScene, angle, 0x17C);
HW_OFFSET(TitleScene, goal_angle, 0x1B8);
HW_SIZE(TitleScene, 0x1C4);
#define TITLE_ARRAY 3

static inline TitleScene *title_at(gaddr a) { return (TitleScene *)hw_ptr(a); }
#define FIELD(scene, field) ((scene) + (gaddr)__builtin_offsetof(TitleScene, field))

/* data of the original */
#define TITLE_TEXLIST     0x8C08C81Cu /* the title's NJS_TEXLIST */
#define TITLE_FILES       0x8C08C7C4u /* 7 file names: logo .nj .njm .njs, star .nj .njm, back .nj .njm */
/* the texture archive (its name in the original's data) for each language: 0 and 1 share the first (ER, GR, FR,
 * SP, IT, DU); another value loads none */
static const gaddr title_textures[] = {0x8C070AECu, 0x8C070AECu, 0x8C070B00u, 0x8C070B14u, 0x8C070B28u,
                                       0x8C070B3Cu, 0x8C070B50u};
#define TITLE_FLOAT_0_6   0x8C0709E8u /* 0.6f */
#define TITLE_FLOAT_0_002 0x8C0709ECu /* 0.002f: the layers' scroll per frame */
#define TITLE_FLOAT_8     0x8C0709D8u /* 8.0f: Start's push on the logo */
#define TITLE_FLOAT_300   0x8C070A1Cu /* 300.0f: frames before the logo comes in */
#define TITLE_FLOAT_50    0x8C070A20u /* 50.0f: frames of the logo's fade-in */
#define g_title_hold_frames GUEST_VAR(int32_t, 0x8C0709F0u) /* 15: frames a layer holds, minus one */
#define g_title_fade_frames GUEST_VAR(int32_t, 0x8C0709F4u) /* frames of a cross-fade, minus one (the sine's quarter turn
                                                              is 100 frames) */
#define TITLE_CAMERA_FOV  0x8C0709DCu /* 60 degrees in Ninja angle units, as a float */
#define LANGUAGE_COUNT    7

/* still translated */
static void screen_fade(int32_t on) { CALL1(0x8C01630Eu, on); }          /* func_8C01630E: ends frames */
static int32_t game_language(void) { return (int32_t)CALL0(0x8C019860u); } /* func_8C019860: 0-6 */
static void texture_load(gaddr name, gaddr texlist, int32_t flags) { CALL3(0x8C0120CAu, name, texlist, flags); } /* func_8C0120CA */
/* func_8C010F04: the background colour and fog range (as the game sets them for a scene) */
static void background_set(uint32_t argb, int32_t mode, f32 fog_near, f32 fog_far) {
  FARGS2(fog_near, fog_far);
  CALL2(0x8C010F04u, argb, mode);
}
/* func_8C011F50: a view's camera from an eye and a target point (roll and field of view in angle units) */
static void view_look_at(gaddr view, gaddr eye, gaddr target, int32_t apply, f32 roll, f32 fov) {
  FARGS2(roll, fov);
  CALL4(0x8C011F50u, view, eye, target, apply);
}

/* was func_8C01B23C: the title's construction: the parts, the fade (variant 1, back from a game: the logo starts
 * finished), the texture archive of the console's language, the seven model files, the camera and lights */
gaddr title_init(gaddr scene, int32_t variant) {
  HW_RESUMABLE(0x8C01B23Cu, scene, variant);
  HW_RESUME_AT(0);
  TitleScene *t = title_at(scene);
  task_construct(scene, 0);
  word_construct(FIELD(scene, pad), 0);
  body_init(FIELD(scene, logo));
  body_init(FIELD(scene, eye));
  body_init(FIELD(scene, target));
  body_init(FIELD(scene, mover));
  for (int i = 0; i < TITLE_ARRAY; i++)
    quat_identity(FIELD(scene, angle[i]));
  method_self(FIELD(scene, goal)); /* Vec3's empty constructor */
  if (variant)
    HW_YIELDING_RET(0, screen_fade(1), 0);
  t = title_at(scene);
  video_pause();
  for (int i = 0; i < TITLE_ARRAY; i++)
    t->model[i] = 0;
  for (int i = 0; i < TITLE_ARRAY; i++)
    t->motion[i] = 0;
  t->data_b8 = 0;
  t->frame[0] = t->frame[1] = t->frame[2] = t->frame[3] = F32(0.0f);

  /* the textures: one archive per language (0 and 1 share the first) */
  t->texlist = TITLE_TEXLIST;
  int32_t language = game_language();
  if (language >= 0 && language < LANGUAGE_COUNT)
    texture_load(title_textures[language], t->texlist, 0);

  /* the models: each file into the fixed area, then read into its own memory */
  static const uint16_t slot[7] = {0x9C, 0xA8, 0xB4, 0xA0, 0xAC, 0xA4, 0xB0};
  gaddr out = hw_stack_alloc(8); /* nj_ReadChunkFile's two out-words (the first must be 0 on the way in) */
  for (int i = 0; i < 7; i++) {
    U32_AT(out) = 0;
    gaddr data = file_load_fixed(GUEST_VAR(gaddr, TITLE_FILES + 4u * (uint32_t)i), 0);
    U32_AT(scene + slot[i]) = nj_ReadChunkFile(data, out, out + 4);
  }
  hw_stack_free(8);

  view_set_split(SPLIT_FULL);
  background_set(0xFF000000u, 0, F32(0.0f), F32(-1.0f));
  t->b = t->a = t->start = 0;
  t->right = t->left = t->up = t->down = 0;
  t->key_lock = 15;
  t->blend_a = t->blend_b = F32(0.0f);
  t->scroll_a = F32_AT(TITLE_FLOAT_0_6);
  t->scroll_b = F32_AT(TITLE_FLOAT_0_6);

  body_reset(FIELD(scene, logo));
  vec3_fill(FIELD(scene, logo.damping), F32_BITS(0x3F47AE14u)); /* 0.78 */
  vec3_set(F32(298.0f), F32(260.0f), F32(0.0f), FIELD(scene, logo.position));
  body_reset(FIELD(scene, eye));
  vec3_fill(FIELD(scene, eye.damping), F32_BITS(0x3F333333u)); /* 0.7 */
  vec3_set(F32(0.0f), F32_BITS(0xC11EF360u), F32(750.0f), FIELD(scene, eye.position));
  body_reset(FIELD(scene, target));
  vec3_fill(FIELD(scene, target.damping), F32_BITS(0x3F4CCCCCu)); /* 0.8 */
  vec3_set(F32(0.0f), F32_BITS(0xC2F9F3B6u), F32_BITS(0x41C75604u), FIELD(scene, target.position));
  body_reset(FIELD(scene, mover));
  vec3_fill(FIELD(scene, mover.damping), F32_BITS(0x3F4CCCCCu));
  for (int i = 0; i < TITLE_ARRAY; i++) {
    quat_identity(FIELD(scene, angle[i]));
    field_set_f32(FIELD(scene, angle[i]), 0x0C, F32_BITS(0x3F4CCCCCu)); /* damping 0.8 */
  }
  vec3_zero(FIELD(scene, goal));
  t->goal_angle[0] = t->goal_angle[1] = t->goal_angle[2] = F32(0.0f);
  sound_select_set(0);
  t->demo_countdown = 1800;

  music_play(5, 0, 1);
  if (variant) {
    /* back from a game: the logo starts at its last frame */
    const NjMotion *m = (const NjMotion *)hw_ptr(t->motion[0]);
    t->frame[0] = f32_from_uint((uint32_t)(m->nb_frames - 1));
    t->logo_phase = 2;
    t->blend_a = F32(1.0f);
    t->state = 1;
  } else {
    t->state = 0;
    t->logo_phase = 0;
  }
  t->phase_frames = 0;
  t->choice = 0;
  t->unknown064 = 0;
  t->fade = F32(0.0f);
  view_set_light_intensity(F32(1.0f), F32(1.0f), F32(1.0f), 0);
  view_set_light_dir(F32(0.0f), F32_BITS(0xBF3504F6u), F32_BITS(0xBF3504F6u), 0); /* down and away, 45 degrees */
  view_set_light_color(F32(1.0f), F32(1.0f), F32(1.0f), 0);
  view_look_at(g_cur_view, method_self(FIELD(scene, eye)), method_self(FIELD(scene, target)), 1, F32(0.0f),
               F32_AT(TITLE_CAMERA_FOV));
  video_resume();
  return scene;
}
HW_EXPORT_RESUMABLE(8c01b23c, RET(title_init(ARG0, (int32_t)ARG1)))

/* was func_8C01B77A: the constructor (the vtable first, then the scene's construction: it can end frames) */
gaddr title_construct(gaddr scene, int32_t variant) {
  HW_RESUMABLE(0x8C01B77Au, scene, variant);
  HW_RESUME_AT(0);
  task_at(scene)->vtable = TITLE_VTABLE;
  HW_YIELDING_RET(0, title_init(scene, variant), 0);
  return scene;
}
HW_EXPORT_RESUMABLE(8c01b77a, RET(title_construct(ARG0, (int32_t)ARG1)))

/* ---- the update (vtable slot 1) ---- */

#define g_title_demo_next GUEST_VAR(int32_t, 0x8C08C7C0u) /* the attract sequence alternates: demo / mode 3 */

/* still translated: variant 0's intro (state 2), the save-file check of the VMU
 * (state 3: poll, then read the save; 1 when finished) */
static void title_intro(gaddr scene) { CALL1(0x8C01BEF8u, scene); }    /* func_8C01BEF8 */
static void save_poll(void) { CALL0(0x8C018E10u); }                    /* func_8C018E10 */
static int32_t save_load(void) { return (int32_t)CALL0(0x8C018E7Cu); } /* func_8C018E7C */

static void title_next_mode(gaddr scene, int32_t mode, int32_t arg) {
  Game *game = game_at(task_at(scene)->owner);
  game->next_mode = mode;
  game->mode_arg = arg;
}

/* as func_8C01BB52 (stays translated: only mode 1, which no route visits, starts at state 0): state 0, the fade-in:
 * one second, then the logo */
static void title_fade_in(gaddr scene) {
  TitleScene *t = title_at(scene);
  t->fade = fadd(t->fade, F32_BITS(0x3C888888u)); /* 1/60 */
  if (fgt(t->fade, F32(1.0f)))
    t->state = 1;
}

/* the logo motion's last frame (frames - 1, as an unsigned count) */
static f32 title_last_frame(const TitleScene *t) {
  const NjMotion *m = (const NjMotion *)hw_ptr(t->motion[0]);
  return f32_from_uint((uint32_t)(m->nb_frames - 1));
}
/* sin(n / 100 of a quarter turn): the cross-fade's curve over a phase */
static f32 title_wave(int32_t n) {
  return njSin(f32_to_int(fdiv(fmul(f32_from_int(n), F32(16384.0f)), F32(100.0f))));
}

/* was func_8C01BB70: state 1, the logo. Start skips ahead: during the logo's entry (phases 0-1) it jumps to its last
 * frame (sound 6 on port 1, the music again); once the logo stands (phase 2 on) it starts variant 0's intro (sound 7
 * on port 2, the camera goals). Otherwise the logo animates: phase 0 waits 300 frames, phase 1 plays the logo's
 * motion and fades it in over 50 frames, then phases 2-5 loop: a layer holds (16 frames), the two layers cross-fade
 * along a sine, the other holds, they cross-fade back. */
void title_logo(gaddr scene) {
  TitleScene *t = title_at(scene);
  const f32 step = F32_AT(TITLE_FLOAT_0_002);
  if (t->start) {
    t->logo.force.x = fadd(t->logo.force.x, fneg(F32_AT(TITLE_FLOAT_8)));
    if (t->logo_phase < 2) {
      sound_play(1, 6, 0);
      music_play(5, 0, 1);
      t->frame[0] = title_last_frame(t);
      t->logo_phase++;
      t->phase_frames = 0;
      t->blend_a = F32(1.0f);
      t->demo_countdown = 1800;
    } else {
      sound_play(2, 7, 0);
      game_at(task_at(scene)->owner)->reset_exits = 0;
      t->state = 2;
      t->unknown064 = 0;
      t->frame[0] = F32(0.0f);
      vec3_set(F32(-176.0f), F32(40.0f), F32(-6.0f), FIELD(scene, goal));
      t->goal_angle[0] = F32(-122.5f);
      t->goal_angle[1] = F32(8350.0f);
      t->goal_angle[2] = F32(0.0f);
    }
    return;
  }
  t->frame[1] = fadd(t->frame[1], F32(1.0f));
  t->frame[2] = fadd(t->frame[2], F32(1.0f));
  switch (t->logo_phase) {
  case 0: /* wait */
    t->frame[0] = fadd(t->frame[0], F32(1.0f));
    if (!fgt(F32_AT(TITLE_FLOAT_300), t->frame[0])) {
      t->logo_phase++;
      t->phase_frames = 0;
      t->frame[1] = F32(0.0f);
      music_play(5, 0, 1);
    }
    break;
  case 1: /* the logo's motion, fading in */
    t->scroll_a = fadd(t->scroll_a, step);
    t->blend_a = fdiv(f32_from_int(t->phase_frames), F32_AT(TITLE_FLOAT_50));
    t->phase_frames++;
    t->frame[0] = fadd(t->frame[0], F32(1.0f));
    if (fgt(t->frame[0], title_last_frame(t))) {
      t->frame[0] = title_last_frame(t);
      t->logo_phase++;
      t->phase_frames = 0;
      t->blend_a = F32(1.0f);
    }
    break;
  case 2:
    t->scroll_a = fadd(t->scroll_a, step);
    if (t->phase_frames++ > g_title_hold_frames) {
      t->phase_frames = 0;
      t->logo_phase++;
      t->scroll_b = F32_AT(TITLE_FLOAT_0_6);
      t->frame[3] = F32(0.0f);
      t->frame[2] = F32(0.0f);
    }
    t->blend_a = F32(1.0f);
    t->blend_b = F32(0.0f);
    break;
  case 3: {
    t->scroll_a = fadd(t->scroll_a, step);
    t->scroll_b = fadd(t->scroll_b, F32_AT(TITLE_FLOAT_0_002));
    f32 s = title_wave(t->phase_frames);
    t->blend_b = s;
    t->blend_a = fadd(fneg(t->blend_b), F32(1.0f));
    t->frame[3] = fadd(t->frame[3], F32(1.0f));
    if (t->phase_frames++ > g_title_fade_frames) {
      t->logo_phase++;
      t->phase_frames = 0;
      t->blend_a = F32(0.0f);
      t->blend_b = F32(1.0f);
    }
    break;
  }
  case 4:
    t->scroll_b = fadd(t->scroll_b, step);
    if (t->phase_frames++ > g_title_hold_frames) {
      t->phase_frames = 0;
      t->logo_phase++;
      t->scroll_a = F32_AT(TITLE_FLOAT_0_6);
      t->frame[1] = F32(0.0f);
      t->frame[3] = F32(0.0f);
    }
    t->blend_a = F32(0.0f);
    t->blend_b = F32(1.0f);
    break;
  case 5: {
    t->scroll_a = fadd(t->scroll_a, step);
    t->scroll_b = fadd(t->scroll_b, F32_AT(TITLE_FLOAT_0_002));
    f32 s = title_wave(t->phase_frames);
    t->blend_a = s;
    t->blend_b = fadd(fneg(t->blend_a), F32(1.0f));
    t->frame[3] = fadd(t->frame[3], F32(1.0f));
    if (t->phase_frames++ > g_title_fade_frames) {
      t->logo_phase = 2;
      t->phase_frames = 0;
      t->blend_a = F32(1.0f);
      t->blend_b = F32(0.0f);
    }
    break;
  }
  default:
    t->logo_phase = 0;
    break;
  }
}
HW_EXPORT(8c01bb70, title_logo(ARG0))

/* was func_8C01B858: the buttons (Start, A and B with a 15-frame lock after one of them), the camera bodies pulled
 * toward their goals, then the state: fade in, logo (until the attract time-out: the demo or mode 3, in turn), the
 * intro, the save check, and the menu's answer as the next mode */
void title_update(gaddr scene) {
  TitleScene *t = title_at(scene);
  const Pad *pad = &t->pad;
  if (--t->key_lock < 0) {
    t->start = pad_pressed(pad, PAD_START);
    t->a = pad_pressed(pad, PAD_A);
    t->b = pad_pressed(pad, PAD_B);
    if (t->start || t->a || t->b)
      t->key_lock = 15;
  } else {
    t->b = t->a = t->start = 0;
  }
  t->right = pad_pressed(pad, PAD_RIGHT);
  t->left = pad_pressed(pad, PAD_LEFT);
  t->up = pad_pressed(pad, PAD_UP);
  t->down = pad_pressed(pad, PAD_DOWN);

  const f32 k = F32_BITS(0x3D4CCCCCu); /* 0.05 */
  Vec3 pull = t->goal;
  pull.z = fmul(fsub(pull.z, t->mover.position.z), k);
  pull.y = fmul(fsub(pull.y, t->mover.position.y), k);
  pull.x = fmul(fsub(pull.x, t->mover.position.x), k);
  t->mover.force.z = fadd(pull.z, t->mover.force.z);
  t->mover.force.y = fadd(pull.y, t->mover.force.y);
  t->mover.force.x = fadd(pull.x, t->mover.force.x);
  body_step(FIELD(scene, mover));
  for (int i = 0; i < TITLE_ARRAY; i++) {
    Body1 *a = &t->angle[i];
    a->force = fmac(k, angle_wrap(fsub(t->goal_angle[i], a->value)), a->force);
    body1_step(FIELD(scene, angle[i]));
  }

  switch (t->state) {
  case 0:
    title_fade_in(scene);
    break;
  case 1:
    title_logo(scene);
    t = title_at(scene);
    if (t->logo_phase >= 2 && --t->demo_countdown < 0) {
      title_next_mode(scene, g_title_demo_next ? MODE_DEMO : MODE_3, 0);
      g_title_demo_next = !g_title_demo_next;
      t->state = 3;
    }
    break;
  case 2:
    title_intro(scene);
    break;
  case 3:
    save_poll();
    switch (t->choice) {
    case 0:
    case 1:
      if (save_load())
        t->state = 4;
      break;
    case 10:
      title_next_mode(scene, MODE_7, 1);
      break;
    case 11:
      title_next_mode(scene, MODE_7, 2);
      break;
    case 12:
      title_next_mode(scene, MODE_7, 3);
      break;
    case 3:
      if (save_load()) {
        t->state = 4;
        title_next_mode(scene, MODE_12, 0);
      }
      break;
    }
    break;
  case 4:
    switch (t->choice) {
    case 0:
      title_next_mode(scene, MODE_7, 0);
      break;
    case 1:
      title_next_mode(scene, MODE_7, 4);
      break;
    case 3:
      title_next_mode(scene, MODE_12, 0);
      break;
    }
    break;
  }
}
HW_EXPORT(8c01b858, title_update(ARG0))

/* was func_8C01B790: free what the scene loaded, stop the music, and wait until the drive is no longer playing */
static void title_release(gaddr scene) {
  TitleScene *title = title_at(scene);
  syFree(title->data_b8);
  for (int i = 0; i < TITLE_ARRAY; i++)
    syFree(title->motion[i]);
  for (int i = 0; i < TITLE_ARRAY; i++)
    syFree(title->model[i]);
  syFree(title->logo_shape);
  njReleaseTexture(title->texlist);
  music_stop();
  while (gdFsGetDrvStat() == GDD_DRVSTAT_PLAY) {
  }
}
HW_EXPORT(8c01b790, title_release(ARG0))

/* was func_8C01B7EE: the virtual destructor (vtable slot 0): release, then free the object when asked */
gaddr title_destroy(gaddr scene, int32_t free_memory) {
  title_release(scene);
  if (free_memory)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c01b7ee, RET(title_destroy(ARG0, (int32_t)ARG1)))
