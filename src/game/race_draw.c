/* Pen Pen port, S15 (b04): the race scene's drawing over each view (0x8C02B80A-0x8C02BDAE, 0x8C02DF96-0x8C02E4E8).
 *
 * Once per view in use (by the split mode: one, two or four), the scene updates the course cell under the view's
 * camera, places the view's camera (from the course when no player is shown on it, else behind its player) and
 * draws the screen furniture of the game's mode over it:
 *  - mode 0 (one player, full screen): the race time, the item, the results' banner, the pause overlay, results and
 *    ending sprites;
 *  - mode 1 (two views) and 3 (four views): the item of the view's player, the banner, the pause screen, the
 *    results' and ending's sprites (mode 3 also darkens the screen with a polygon); mode 2 is still translated;
 *  - mode 4 (the time trial, full screen): the race time and the course's record time;
 *  - mode 5 (the demo): a caption that fades with the pause screen's fade.
 *
 * The race time is minutes, tens and units of seconds, tenths and hundredths, as digits of the shared sprites with
 * the two separators between them, then a label from the scene's panel textures. In the time trial a time that
 * beats the course's record flashes. */
#include "hw.h"
#include "sdk.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "../core/sprite_text.h"
#include "game.h"
#include "menu.h"
#include "race_scene.h"
#include "racer.h"
#include "course.h"
#include "scenery.h"

/* ---- calls ---- */

/* func_8C02BAEE (still translated): mode 2's drawing over a view */
static void race_draw_view_mode2(gaddr scene) { CALL1(0x8C02BAEEu, scene); }
/* func_8C016D60: a pulsing sprite of the results screen (steps its counter, then draws by its sine) */
static void results_pulse_draw(void) { CALL0(0x8C016D60u); }
/* func_8C02E4E8 (still translated): the time trial's lap times, once the player has finished */
static void race_draw_lap_times(gaddr scene) { CALL1(0x8C02E4E8u, scene); }

/* ---- data ---- */

#define PANEL_TEXLIST     0x8C096070u /* the scene sprite's textures */
#define DARKEN_POLYGON    0x8C0962D4u /* mode 3: four NJS_POLYGON_VTX over the screen, copied to the stack to draw */
#define DARKEN_POLYGON_SIZE 0x40
#define g_record_flash    GUEST_VAR(f32, 0x8C096360u) /* the flash's phase (Ninja angle units, as a float) */
#define VIEW_FOVS         ((const f32 *)hw_ptr(0x8C07FC08u)) /* a camera's field of view by its view's layout */

#define SPRITE_ATTR       0x22
#define TIME_EPSILON      0x38D1B717u /* 0.0001f: keeps a whole number of minutes / tens from flooring one under */
#define FLASH_STEP        0x45088888u /* 2184.533f: 65536 / 30, a flash cycle every 30 frames */
#define F_0_1             0x3DCCCCCCu /* 0.1f */
#define F_0_4             0x3ECCCCCCu /* 0.4f */
#define F_0_6             0x3F199999u /* 0.6f */
#define F_0_7             0x3F333333u /* 0.7f */
#define F_0_8             0x3F4CCCCCu /* 0.8f */

enum { SETUP_MODE_TRIAL = 4, SETUP_MODE_DEMO = 5 };
enum { RACER_STATE_FINISHED = 2 };

static inline gaddr scene_sprite(gaddr scene) { return member_addr(scene, RaceScene, sprite); }
static inline RaceSetup *scene_setup(RaceScene *s) { return race_setup_at(s->setup); }
/* the course's record time (save_time_a by the course and its variant) */
static f32 course_record(RaceScene *s) {
  RaceSetup *setup = scene_setup(s);
  int32_t variant = setup->variant;
  return save_time_a(setup->course, variant);
}

/* ---- the race time ---- */

typedef struct TimeDigits {
  int32_t minutes, tens, units, tenths, hundredths;
} TimeDigits;

/* a time in seconds as five digits; past 9 minutes it shows 9:59.99. (func_8C02DF96 takes the remainders after the
 * clamp too, func_8C02E2B6 only without it: the digits are the same) */
static void time_digits(f32 t, TimeDigits *d) {
  d->minutes = f32_to_int(rt_floor(fadd(fdiv(t, F32(60.0f)), F32_BITS(TIME_EPSILON))));
  d->tens = f32_to_int(rt_floor(fadd(fdiv(t, F32(10.0f)), F32_BITS(TIME_EPSILON))));
  d->units = f32_to_int(t);
  d->tenths = f32_to_int(fmul(t, F32(10.0f)));
  d->hundredths = f32_to_int(fmul(t, F32(100.0f)));
  if (d->minutes > 9) {
    d->minutes = 9;
    d->tens = 5;
    d->units = 9;
    d->tenths = 9;
    d->hundredths = 9;
  } else {
    d->minutes %= 10; /* the SHC runtime's signed remainder (func_8C062518) */
    d->tens %= 6;
    d->units %= 10;
    d->tenths %= 10;
    d->hundredths %= 10;
  }
}

/* the five digits from x (y 90, 32 pixels) with the two separators (cells 0xB9, 0xBA of the small sprite) at
 * sep1 and sep2 */
static void time_draw(const TimeDigits *d, int32_t x, f32 sep1, f32 sep2) {
  text_draw_digit_sized(x, 0x5A, d->minutes, 0x20, SPRITE_ATTR);
  sprite_small_draw(0xB9, SPRITE_ATTR, sep1, F32(83.0f), F32(1.0f));
  text_draw_digit_sized(x + 0x18, 0x5A, d->tens, 0x20, SPRITE_ATTR);
  text_draw_digit_sized(x + 0x2C, 0x5A, d->units, 0x20, SPRITE_ATTR);
  sprite_small_draw(0xBA, SPRITE_ATTR, sep2, F32(80.0f), F32(1.0f));
  text_draw_digit_sized(x + 0x48, 0x5A, d->tenths, 0x20, SPRITE_ATTR);
  text_draw_digit_sized(x + 0x5C, 0x5A, d->hundredths, 0x20, SPRITE_ATTR);
}

/* the scene's sprite at (x, 36), scale 1 */
static void label_place(RaceScene *s, f32 x) {
  s->sprite.p.x = x;
  s->sprite.p.y = F32(36.0f);
  s->sprite.p.z = F32(0.0f);
  s->sprite.sx = F32(1.0f);
  s->sprite.sy = F32(1.0f);
  s->sprite.ang = 0;
}

/* was func_8C02DF96: the race time at the top left. While the results or the ending show (state 3 or 2) it is the
 * scene's time (f74), or in the time trial, once player 0 has finished, its time at times298[word1a8 - 3], tinted
 * (1, 0.4, 0.6). Else the running time (f78 once set, else f74): in the time trial, while f78 does not exceed the
 * course's record, its green pulses with a sine (one cycle per 30 frames); else the same tint. Then the label
 * (cell 1) in (1, 0.7, 0.1). */
void race_draw_time(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  f32 t;
  if (s->state == RACE_STATE_RESULTS || s->state == 2) {
    if (racer_state(s->players[0]) == RACER_STATE_FINISHED && scene_setup(s)->mode == SETUP_MODE_TRIAL) {
      Racer *r = racer_at(s->players[0]);
      t = r->times298[r->word1a8 - 3];
    } else {
      t = s->f74;
    }
    material_set_2(F32(1.0f), F32_BITS(F_0_4), F32_BITS(F_0_6), F32(1.0f));
  } else {
    t = feq(s->f78, F32(0.0f)) ? s->f74 : s->f78;
    if (scene_setup(s)->mode == SETUP_MODE_TRIAL && !fgt(s->f78, course_record(s))) {
      g_record_flash = fadd(g_record_flash, F32_BITS(FLASH_STEP));
      f32 pulse = njSin(f32_to_int(g_record_flash));
      material_set_2(F32(1.0f), pulse, F32_BITS(F_0_6), F32(1.0f));
    } else {
      material_set_2(F32(1.0f), F32_BITS(F_0_4), F32_BITS(F_0_6), F32(1.0f));
    }
  }
  TimeDigits d;
  time_digits(t, &d);
  text_style_reset();
  time_draw(&d, 0x16, F32(40.0f), F32(83.0f));
  njSetTexture(PANEL_TEXLIST);
  label_place(s, F32(37.0f));
  material_set_2(F32(1.0f), F32_BITS(F_0_7), F32_BITS(F_0_1), F32(1.0f));
  njDrawSprite2D(scene_sprite(scene), 1, F32(-1.0f), SPRITE_ATTR);
}
HW_EXPORT(8c02df96, race_draw_time(ARG0))

/* was func_8C02E2B6: the time trial's course record at the top right, tinted (1, 0.5, 0.1), then its label (cell 0)
 * in (0.8, 1, 0.1) */
void race_draw_record(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  TimeDigits d;
  time_digits(course_record(s), &d);
  text_style_reset();
  material_set_2(F32(1.0f), F32(0.5f), F32_BITS(F_0_1), F32(1.0f));
  time_draw(&d, 0x1EE, F32(512.0f), F32(556.0f));
  label_place(s, F32(480.0f));
  njSetTexture(PANEL_TEXLIST);
  material_set_2(F32_BITS(F_0_8), F32(1.0f), F32_BITS(F_0_1), F32(1.0f));
  njDrawSprite2D(scene_sprite(scene), 0, F32(-1.0f), SPRITE_ATTR);
}
HW_EXPORT(8c02e2b6, race_draw_record(ARG0))

/* ---- per mode, over the current view ---- */

/* was func_8C02B9E8: mode 0: the time, the item of the view's player (or of the player the course shows when the
 * view has none), then in the ending the results' pulse, else the banner, pause overlay and results; the ending's
 * sprites */
void race_draw_view_full(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  control3d_push();
  control3d_set(0x10);
  int32_t player = race_scene_view_player(scene, view_current_index());
  race_draw_time(scene);
  if (player >= 0)
    race_item_draw_full(scene, player);
  else
    race_item_draw_full(scene, course_at(s->course)->shown_view);
  if (s->word_ca8 != 2) {
    race_results_draw_banner(scene, 0);
    race_pause_draw_overlay(scene);
    race_results_draw_full(scene);
  } else if (s->state == RACE_STATE_RESULTS) {
    results_pulse_draw();
  }
  race_ending_draw_full(scene);
  control3d_pop();
}
HW_EXPORT(8c02b9e8, race_draw_view_full(ARG0))

/* was func_8C02BA96: mode 1 (two views): the view's player's item and banner, the pause screen, the results'
 * sprites and the ending's banner */
void race_draw_view_two(gaddr scene) {
  njSetTexture(PANEL_TEXLIST);
  control3d_push();
  control3d_set(0x10);
  int32_t player = race_scene_view_player(scene, view_current_index());
  race_item_draw_two(scene, player);
  race_results_draw_banner(scene, player);
  race_pause_draw_b(scene);
  race_results_draw_two(scene);
  race_ending_draw_a(scene);
  control3d_pop();
}
HW_EXPORT(8c02ba96, race_draw_view_two(ARG0))

/* was func_8C02BBCA (with its tail func_8C02BC24, which Ghidra split off at the block copy's return): mode 3 (four
 * views): as mode 1 with the four-view drawers, then the darkening polygon (copied to the stack) */
void race_draw_view_four(gaddr scene) {
  gaddr poly = hw_stack_alloc(DARKEN_POLYGON_SIZE); /* the original's 0x40-byte frame at r15 */
  njSetTexture(PANEL_TEXLIST);
  control3d_push();
  control3d_set(0x10);
  int32_t player = race_scene_view_player(scene, view_current_index());
  race_item_draw_four(scene, player);
  race_results_draw_banner(scene, player);
  race_pause_draw_a(scene);
  race_results_draw_four(scene);
  race_ending_draw_b(scene);
  memmove(hw_ptr(poly), hw_ptr(DARKEN_POLYGON), DARKEN_POLYGON_SIZE);
  njDrawPolygon(poly, 4, 0);
  control3d_pop();
  hw_stack_free(DARKEN_POLYGON_SIZE);
}
HW_EXPORT(8c02bbca, race_draw_view_four(ARG0))

/* was func_8C02BC3E: mode 4 (the time trial): the time and the record; then in the ending the results' pulse, else
 * (on the view of player 0, once it has finished) its lap times, the pause overlay and the results; the other
 * ending sprites */
void race_draw_view_trial(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  control3d_push();
  control3d_set(0x10);
  int32_t player = race_scene_view_player(scene, view_current_index());
  race_draw_time(scene);
  race_draw_record(scene);
  if (s->word_ca8 != 2) {
    if (player == 0 && racer_state(s->players[0]) == RACER_STATE_FINISHED)
      race_draw_lap_times(scene);
    race_pause_draw_overlay(scene);
    race_results_draw_full(scene);
  } else if (s->state == RACE_STATE_RESULTS) {
    results_pulse_draw();
  }
  race_ending_draw_other(scene);
  control3d_pop();
}
HW_EXPORT(8c02bc3e, race_draw_view_trial(ARG0))

/* was func_8C02BD18: mode 5 (the demo): the caption (cells 0x13 to 0x15, 128 pixels apart from (180, 382)) in white
 * at the pause screen's fade */
void race_draw_view_demo(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  gaddr sprite = scene_sprite(scene);
  control3d_push();
  control3d_set(0x10);
  material_set_2(F32(1.0f), F32(1.0f), F32(1.0f), s->pause_fade);
  njSetTexture(PANEL_TEXLIST);
  s->sprite.p.x = F32(180.0f);
  s->sprite.p.y = F32(382.0f);
  s->sprite.p.z = F32(0.0f);
  s->sprite.sx = F32(1.0f);
  s->sprite.sy = F32(1.0f);
  njDrawSprite2D(sprite, 0x13, F32(-1.0f), SPRITE_ATTR);
  s->sprite.p.x = fadd(s->sprite.p.x, F32(128.0f));
  njDrawSprite2D(sprite, 0x14, F32(-1.0f), SPRITE_ATTR);
  s->sprite.p.x = fadd(s->sprite.p.x, F32(128.0f));
  njDrawSprite2D(sprite, 0x15, F32(-1.0f), SPRITE_ATTR);
  control3d_pop();
}
HW_EXPORT(8c02bd18, race_draw_view_demo(ARG0))

/* ---- every view ---- */

/* the camera a view gets from the course when no player is shown on it (the original's 0x20-byte frame: the
 * course writes all four through pointers) */
typedef struct CourseCameraFrame {
  f32 roll;     /* 0x00 */
  f32 fov;      /* 0x04 */
  Vec3 target;  /* 0x08 */
  Vec3 eye;     /* 0x14 */
} CourseCameraFrame;
HW_OFFSET(CourseCameraFrame, target, 0x08);
HW_OFFSET(CourseCameraFrame, eye, 0x14);
HW_SIZE(CourseCameraFrame, 0x20);

/* the drawing of the game's mode over the current view (mode 2 still translated) */
static void race_draw_view_by_mode(gaddr scene, int32_t mode) {
  if (mode == 0)
    race_draw_view_full(scene);
  else if (mode == 1)
    race_draw_view_two(scene);
  else if (mode == 2)
    race_draw_view_mode2(scene);
  else if (mode == 3)
    race_draw_view_four(scene);
  else if (mode == SETUP_MODE_TRIAL)
    race_draw_view_trial(scene);
  else if (mode == SETUP_MODE_DEMO)
    race_draw_view_demo(scene);
}

/* was func_8C02B80A: per view in use (1, 2 or 4 by the split mode): the course cell under its camera, then its
 * camera and its drawing. A view without a player gets the course's camera and draws only the full-screen modes
 * (0 and 4) and the demo, those only once the ending's word_ca8 is 2 (the demo always); a view with a player looks
 * through that player's camera (eye, target, roll) with the field of view of the view's layout. */
void race_draw_views(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  int32_t views = 0;
  if (g_split_mode == SPLIT_FULL)
    views = 1;
  else if (g_split_mode == SPLIT_TWO || g_split_mode == SPLIT_TWO_WITH_PANEL)
    views = 2;
  else if (g_split_mode == SPLIT_FOUR)
    views = 4;
  for (int32_t k = 0; k < views; k++) {
    s->view_cell[k] = race_scene_find_cell(scene, view_screen(), s->view_cell[k]);
    int32_t player = s->view_player[k];
    if (player == -1) {
      gaddr frame = hw_stack_alloc(sizeof(CourseCameraFrame));
      CourseCameraFrame *f = (CourseCameraFrame *)hw_ptr(frame);
      race_scene_course_camera(scene, member_addr(frame, CourseCameraFrame, eye),
                               member_addr(frame, CourseCameraFrame, target), member_addr(frame, CourseCameraFrame, roll),
                               member_addr(frame, CourseCameraFrame, fov));
      house_view_camera(k, member_addr(frame, CourseCameraFrame, eye), member_addr(frame, CourseCameraFrame, target),
                        f->roll, f->fov);
      hw_stack_free(sizeof(CourseCameraFrame));
      if (s->word_ca8 == 2) {
        int32_t mode = scene_setup(s)->mode;
        if (mode == 0)
          race_draw_view_full(scene);
        else if (mode == SETUP_MODE_TRIAL)
          race_draw_view_trial(scene);
        else if (mode == SETUP_MODE_DEMO)
          race_draw_view_demo(scene);
      } else if (scene_setup(s)->mode == SETUP_MODE_DEMO) {
        race_draw_view_demo(scene);
      }
    } else {
      gaddr racer = s->players[player];
      f32 fov = VIEW_FOVS[g_views[k].layout];
      f32 roll = racer_at(racer)->camera_f51c;
      house_view_camera(k, member_addr(racer, Racer, camera_eye), member_addr(racer, Racer, camera_vec4e0), roll, fov);
      race_draw_view_by_mode(scene, scene_setup(s)->mode);
    }
  }
}
HW_EXPORT(8c02b80a, race_draw_views(ARG0))
