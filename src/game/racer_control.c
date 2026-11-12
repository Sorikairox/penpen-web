/* Pen Pen port, S14 (b07): the racer's construction (the two steps above the still-translated base,
 * func_8C030BF4) and its reading of the controls each update (0x8C0374C2-0x8C0377A8). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/task.h"
#include "racer.h"

#define BUTTON_LOCK_UPDATES 15 /* a released button is ignored for this many updates */
#define STICK_DEAD_LENGTH F32_BITS(0x3DCCCCCCu) /* 0.1 */
#define TWO_PI F32_BITS(0x40C90FD7u)            /* 6.2831836 (just under 2 pi) */

/* func_8C030BF4: the racer's base construction (still translated): (racer, a, b, c, f; d on the stack) */
static void racer_base_construct(gaddr racer, uint32_t a, uint32_t b, uint32_t c, f32 f, uint32_t d) {
  FARGS1(f);
  CALL5(0x8C030BF4u, racer, a, b, c, d);
}
/* func_8C02E7AE (game/race_scene.c, not in a header): the packed pad word of player i (the stick's x in the low
 * byte, y in the next, the two buttons at bits 0 and 8) */
static int32_t race_player_pad(gaddr scene, int32_t i) { return (int32_t)CALL2(0x8C02E7AEu, scene, i); }
/* func_8C010B30 (core/pad_analog.c, not in a header): a signed 8-bit stick value as a float in [-1, 1] */
static f32 stick_unit(int32_t value) {
  CALL1(0x8C010B30u, value);
  return hw_fret();
}
/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}
/* func_8C0317D4: the rest of the racer's update (still translated) */
static void racer_update_rest(gaddr racer) { CALL1(0x8C0317D4u, racer); }

/* was func_8C0374C2: the base construction, then the word at 0x588 and the race scene; returns the racer */
gaddr racer_init_base(gaddr racer, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, f32 f) {
  racer_base_construct(racer, a, b, c, f, d);
  word_construct(member_addr(racer, Racer, word588), e);
  racer_at(racer)->scene = scene;
  return racer;
}
HW_EXPORT(8c0374c2, RET(racer_init_base(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, FARG0)))

/* was func_8C03755C: the racer's constructor (vtable 0x8C23DACC; the character's own base pointer) */
gaddr racer_construct(gaddr racer, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, f32 f) {
  Racer *r = racer_at(racer);
  r->task.vtable = RACER_VTABLE;
  r->second_base = member_addr(racer, Racer, second_base_at);
  racer_init_base(racer, scene, a, b, c, d, e, f);
  return racer;
}
HW_EXPORT(8c03755c, RET(racer_construct(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, FARG0)))

/* a button's held state: its bit, unless a lock is still counting down */
static int32_t button_read(int32_t *lock, int32_t pad, int32_t bit) {
  if (--*lock > 0) return 0;
  return (pad & bit) != 0;
}

/* was func_8C0375C2: the racer's update (unless paused): its player's stick (x negated, both damped on request),
 * the two buttons with their presses and releases (a release locks the button for 15 updates), the stick's length
 * and, when pushed, its angle; then the rest of the update */
void racer_read_controls(gaddr racer) {
  if (task_is_paused(task_at(racer))) return;
  Racer *r = racer_at(racer);
  r->button_prev[0] = r->button[0];
  r->button_prev[1] = r->button[1];
  int32_t pad = race_player_pad(r->scene, r->index);
  r->stick_x = fneg(stick_unit(pad));
  int32_t pad16 = (int16_t)pad;
  r->stick_y = stick_unit(pad16 >> 8);
  if (r->stick_damped != 0) {
    r->stick_x = fmul(r->stick_x, F32(0.5f));
    r->stick_y = fmul(r->stick_y, F32(0.75f));
  }
  r->button[0] = button_read(&r->button_lock[0], pad16, 0x001);
  r->button[1] = button_read(&r->button_lock[1], pad16, 0x100);
  for (int b = 0; b < 2; b++) {
    r->edges[b].pressed = r->button[b] != 0 && r->button_prev[b] == 0;
    r->edges[b].released = r->button[b] == 0 && r->button_prev[b] != 0;
  }
  if (r->edges[0].released) r->button_lock[0] = BUTTON_LOCK_UPDATES;
  if (r->edges[1].released) r->button_lock[1] = BUTTON_LOCK_UPDATES;
  f32 sq = f32_square(r->stick_x);
  r->stick_length = njSqrt(fadd(sq, f32_square(r->stick_y)));
  if (fgt(r->stick_length, STICK_DEAD_LENGTH)) {
    f32 a = rt_atan2f(fneg(r->stick_y), r->stick_x);
    r->stick_angle = f32_from_int(f32_to_int(fdiv(fmul(a, F32(65536.0f)), TWO_PI)));
    r->stick_angle = angle_wrap(r->stick_angle);
  }
  racer_update_rest(racer);
}
HW_EXPORT(8c0375c2, racer_read_controls(ARG0))

/* func_8C0373B8: the racer's draw (still translated; takes the racer only) */
static void racer_draw_translated(gaddr racer) { CALL1(0x8C0373B8u, racer); }

/* was func_8C0377E4: a thunk to the racer's draw (func_8C0373B8) */
void racer_draw_thunk(gaddr racer) { racer_draw_translated(racer); }
HW_EXPORT(8c0377e4, racer_draw_thunk(ARG0))
