/* Pen Pen port, S18 b06: the time trial's lap time against the record (func_8C02E4E8, race_draw.c's
 * race_draw_lap_times: drawn once player 0 has finished). */
#include "hw.h"
#include "sdk.h"
#include "../core/sprite_text.h"
#include "menu.h"       /* material_set_2, rt_floor */
#include "save.h"       /* save_time_b */
#include "race_scene.h"
#include "racer.h"      /* racer_time */
#include "race_lap_time.h"

#define LAP_FIRST_PROGRESS 3          /* the progress word of the first lap's time */
#define LAP_ATTR           0x22
#define LAP_Y              120        /* the digits' y */
#define LAP_DIGIT_SIZE     32
#define LAP_SIGN_X         F32(270.0f)
#define LAP_SIGN_Y         F32(120.0f)
#define LAP_SEP_Y          F32(110.0f)
#define LAP_X              0x122      /* the first digit's x (the minutes, when shown) */
#define LAP_X_AFTER_MINUTES 0x140
#define LAP_MINUTES_SEP_X  F32(310.0f)
#define CELL_MINUS         0xB1       /* the small sprite's cells: the signs and the two separators */
#define CELL_PLUS          0xB0
#define CELL_MINUTES_SEP   0xB9
#define CELL_SECONDS_SEP   0xBA
#define TIME_EPSILON       0x38D1B717u /* 0.0001f: keeps a whole number of minutes / tens from flooring one under */
#define F_0_1              0x3DCCCCCCu /* 0.1f */

/* func_8C02C828 (game/race_scene.c, no header): player i's progress word (Racer.word1a8) */
static int32_t race_player_progress(gaddr scene, int32_t i) { return (int32_t)CALL2(0x8C02C828u, scene, i); }

/* was func_8C02E4E8: player 0's time of the lap it is on (the one before the progress's first lap is lap 0) against
 * the course's lap record: a minus sign tinted yellow when it is faster (the difference made positive), else a
 * plus sign in red; then the difference's digits as m:ss.hh (the minutes only when there are some; past 9 minutes
 * it shows 9:59.99) */
void race_draw_lap_times(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  int32_t lap = race_player_progress(scene, 0) - LAP_FIRST_PROGRESS;
  if (!(lap >= 0)) lap = 0;
  f32 record = save_time_b(race_setup_at(s->setup)->course, lap);
  f32 diff = racer_time(s->players[0], lap);
  text_style_reset();
  diff = fsub(diff, record);
  if (fgt(F32(0.0f), diff)) {
    material_set_2(F32(1.0f), F32(1.0f), F32_BITS(F_0_1), F32(1.0f));
    sprite_small_draw(CELL_MINUS, LAP_ATTR, LAP_SIGN_X, LAP_SIGN_Y, F32(1.0f));
    diff = fneg(diff);
  } else {
    material_set_2(F32(1.0f), F32(0.25f), F32_BITS(F_0_1), F32(1.0f));
    sprite_small_draw(CELL_PLUS, LAP_ATTR, LAP_SIGN_X, LAP_SIGN_Y, F32(1.0f));
  }

  /* the digits (the SHC runtime's signed remainder, func_8C062518, for each) */
  int32_t minutes = f32_to_int(rt_floor(fadd(fdiv(diff, F32(60.0f)), F32_BITS(TIME_EPSILON))));
  int32_t tens = f32_to_int(rt_floor(fadd(fdiv(diff, F32(10.0f)), F32_BITS(TIME_EPSILON))));
  int32_t units = f32_to_int(diff);
  int32_t tenths = f32_to_int(fmul(diff, F32(10.0f)));
  int32_t hundredths = f32_to_int(fmul(diff, F32(100.0f)));
  if (minutes > 9) {
    minutes = 9;
    tenths = 9;
    hundredths = 9;
    tens = 5;
    units = 9;
  } else {
    minutes %= 10;
    tens %= 6;
    units %= 10;
    tenths %= 10;
    hundredths %= 10;
  }

  int32_t x = LAP_X;
  if (minutes != 0) {
    text_draw_digit_sized(x, LAP_Y, minutes, LAP_DIGIT_SIZE, LAP_ATTR);
    sprite_small_draw(CELL_MINUTES_SEP, LAP_ATTR, LAP_MINUTES_SEP_X, LAP_SEP_Y, F32(1.0f));
    x = LAP_X_AFTER_MINUTES;
  }
  text_draw_digit_sized(x, LAP_Y, tens, LAP_DIGIT_SIZE, LAP_ATTR);
  x += 20;
  text_draw_digit_sized(x, LAP_Y, units, LAP_DIGIT_SIZE, LAP_ATTR);
  x += 20;
  sprite_small_draw(CELL_SECONDS_SEP, LAP_ATTR, f32_from_int(x), LAP_SEP_Y, F32(1.0f));
  x += 10;
  text_draw_digit_sized(x, LAP_Y, tenths, LAP_DIGIT_SIZE, LAP_ATTR);
  x += 20;
  text_draw_digit_sized(x, LAP_Y, hundredths, LAP_DIGIT_SIZE, LAP_ATTR);
}
HW_EXPORT(8c02e4e8, race_draw_lap_times(ARG0))
