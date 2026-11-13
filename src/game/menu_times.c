/* Pen Pen port, S16 (w4): the variant panel of mode 4 on the course selection (func_8C028882): the record time of
 * each of the course's five variants, as M'SS"cc in 32-pixel digits, the chosen variant's in yellow, the others in
 * pale blue, each followed by its rank's sprite. The rows step 34 pixels down and 26 pixels left. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "../core/inline.h"
#include "../core/sprite_text.h"  /* text_draw (S16 w4: core/text_draw.c), text_draw_digit_sized */
#include "menu.h"
#include "race_scene.h"   /* save_time_a */

/* functions of other files that no header declares yet */
void material_set(f32 r, f32 g, f32 b, f32 a);           /* game/house.c */
void text_set_scale(f32 sx, f32 sy, f32 v);              /* core/sprite_text.c */
int32_t save_rank(int32_t cup, int32_t course);          /* game/save.c */

#define TIME_VARIANTS 5
#define TIME_FIRST_X 0x1B5          /* the first row's left edge */
#define TIME_FIRST_Y 0x108
#define TIME_ROW_DX (-0x1A)
#define TIME_ROW_DY 0x22
#define DIGIT_SIZE 0x20             /* pixels */
#define DIGIT_ATTR 0x22
#define DIGIT_STEP 0x18             /* from a digit to the next */
#define MARK_STEP 0x14              /* from a ' or " mark to the next digit */
#define MARK_DY (-6)
#define STR_MINUTES 0x8C07FBF0u     /* the minutes mark ' (Shift-JIS) */
#define STR_SECONDS 0x8C07FBF4u     /* the seconds mark " */
#define RANK_DX 0x20                /* the rank sprite: right of the time's last digit */
#define RANK_COUNT 8                /* ranks with a sprite */
#define g_rank_cells ((const int32_t *)hw_ptr(0x8C073CB4u)) /* the map sprite's cell per rank */
#define RANK_PRIORITY F32(-10.0f)
#define TIME_ROUNDING F32_BITS(0x38D1B717u) /* 0.0001: added before flooring */
#define TIME_SCALE F32(32.0f)
#define CHOSEN_G F32_BITS(0x3F666666u)  /* 0.9 */
#define CHOSEN_B F32_BITS(0x3E999999u)  /* 0.3 */
#define OTHER_G F32_BITS(0x3F4CCCCCu)   /* 0.8 */

/* the text colour and the digits' material */
static void time_color(f32 r, f32 g, f32 b) {
  control3d_set(0x10);
  material_set(r, g, b, F32(1.0f));
}

/* a time's digits: minutes, tens of seconds, seconds, tenths, hundredths */
typedef struct TimeDigits {
  int32_t minutes, tens, seconds, tenths, hundredths;
} TimeDigits;

static TimeDigits time_digits(f32 t) {
  TimeDigits d;
  d.minutes = f32_to_int(rt_floor(fadd(fdiv(t, F32(60.0f)), TIME_ROUNDING)));
  d.tens = f32_to_int(rt_floor(fadd(fdiv(t, F32(10.0f)), TIME_ROUNDING)));
  d.seconds = f32_to_int(t);
  d.tenths = f32_to_int(fmul(t, F32(10.0f)));
  d.hundredths = f32_to_int(fmul(t, F32(100.0f)));
  if (d.minutes > 9) {        /* 9'59"99 at most */
    d.minutes = 9;
    d.tens = 5;
    d.hundredths = 9;
    d.tenths = 9;
    d.seconds = 9;
  } else {
    d.minutes %= 10;          /* the SHC runtime's signed remainder (func_8C062518) */
    d.tens %= 6;
    d.seconds %= 10;
    d.tenths %= 10;
    d.hundredths %= 10;
  }
  return d;
}

/* M'SS"cc from x at y, in the colour (r, g, b); returns the last digit's x */
static int32_t time_draw(int32_t x, int32_t y, const TimeDigits *d, f32 r, f32 g, f32 b) {
  global_color_set(r, g, b, F32(1.0f));
  time_color(r, g, b);
  text_draw_digit_sized(x, y, d->minutes, DIGIT_SIZE, DIGIT_ATTR);
  x += DIGIT_STEP;
  text_draw(x, y + MARK_DY, STR_MINUTES);
  x += MARK_STEP;
  time_color(r, g, b);
  text_draw_digit_sized(x, y, d->tens, DIGIT_SIZE, DIGIT_ATTR);
  x += DIGIT_STEP;
  text_draw_digit_sized(x, y, d->seconds, DIGIT_SIZE, DIGIT_ATTR);
  x += DIGIT_STEP;
  text_draw(x, y + MARK_DY, STR_SECONDS);
  x += MARK_STEP;
  time_color(r, g, b);
  text_draw_digit_sized(x, y, d->tenths, DIGIT_SIZE, DIGIT_ATTR);
  x += DIGIT_STEP;
  text_draw_digit_sized(x, y, d->hundredths, DIGIT_SIZE, DIGIT_ATTR);
  return x;
}

/* was func_8C028882: mode 4's variant panel (see the top of the file) */
void variant_panel_draw_mode4(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  int32_t row_x = TIME_FIRST_X;
  int32_t y = TIME_FIRST_Y;
  for (int32_t variant = 0; variant < TIME_VARIANTS; variant++) {
    TimeDigits d = time_digits(save_time_a(m->course, variant));
    int32_t x;
    control3d_push();
    text_set_scale(TIME_SCALE, TIME_SCALE, F32(0.0f));
    if (m->variant == variant)
      x = time_draw(row_x, y, &d, F32(1.0f), CHOSEN_G, CHOSEN_B);
    else
      x = time_draw(row_x, y, &d, F32(0.5f), OTHER_G, F32(1.0f));
    x += RANK_DX;
    m->sprite.p.x = f32_from_int(x);
    m->sprite.p.y = f32_from_int(y);
    m->sprite.sy = F32(1.0f);
    m->sprite.sx = F32(1.0f);
    int32_t rank = save_rank(m->course, variant);
    if (rank < RANK_COUNT)
      njDrawSprite2D(member_addr(menu, SetupMenu, sprite), g_rank_cells[rank], RANK_PRIORITY, 0x20);
    control3d_pop();
    row_x += TIME_ROW_DX;
    y += TIME_ROW_DY;
  }
}
HW_EXPORT(8c028882, variant_panel_draw_mode4(ARG0))
