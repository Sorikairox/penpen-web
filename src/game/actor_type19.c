/* Pen Pen port, S16 (b06): step 2 of the actor of placement type 0x19 (ActorType19, game/actor.h; func_8C0512A4,
 * the step type19_update in game/actor_bodies.c runs in state 2).
 *
 * A leaper: it watches the players (in focus order, from the last back to first_player; once the focused player
 * passes cell 0x80 they all are) and when one is on its trigger cell (or past it) it waits 5 to 65 updates, then
 * leaps from its start point (points[0]) in a random direction (points[1], an angle of 4 to 40 degrees, rising)
 * pushed every update, until it falls back below its rest height and starts over. Its second turn follows the
 * direction it moves in. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_setup_get */
#include "scenery.h"      /* race_player_cell, race_scene_view_focus, random_scaled */

#define LEAPER_ALL_CELL   0x81                 /* the focused player on this cell or beyond: every player counts */
#define LEAPER_DELAY      F32(60.0f)           /* the delay is 5 + random 0..60 */
#define LEAPER_DELAY_MIN  5
#define LEAPER_DAMPING    F32_BITS(0x3F199999u) /* 0.59999996 */
#define LEAPER_ANGLE      F32_BITS(0x45CCCCCCu) /* 6553.6: random 0..36 degrees (Ninja angle units) ... */
#define LEAPER_ANGLE_MIN  F32_BITS(0x44360B60u) /* 728.1777: ... plus 4 degrees */
#define LEAPER_RISE       F32_BITS(0x3C23D70Au) /* 0.01: its direction's y grows by it every update */
#define LEAPER_MOVING     F32_BITS(0x3A83126Eu) /* 0.001: |vx| + |vz| above it turns it */
#define g_leaper_rest_y   GUEST_VAR(f32, 0x8C088350u) /* -306.99997: back above it, it lands */
#define g_leaper_turn_rate GUEST_VAR(f32, 0x8C088324u) /* 0.05 */

enum { LEAPER_WAIT = 0, LEAPER_DELAYING = 1, LEAPER_FLY = 2 };

/* func_8C0502EA: the body's damping (a float setter of the class) */
static void leaper_body_damping(gaddr body, f32 d) {
  FARGS1(d);
  CALL1(0x8C0502EAu, body);
}
/* func_8C0502B0: the body reset when it lands */
static void leaper_body_land(gaddr body) { CALL1(0x8C0502B0u, body); }
/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* the leap's delay: 5 + random 0..60, whole updates */
static void leaper_start_delay(ActorType19 *a) {
  a->delay = f32_to_int(random_scaled(LEAPER_DELAY)) + LEAPER_DELAY_MIN;
  a->phase = LEAPER_DELAYING;
}

/* was func_8C0512A4: state 2 of type 0x19 (see the top of the file) */
void type19_step2(gaddr obj) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType19, body);
  int32_t players = race_setup_at(race_setup_get())->player_count;
  if (a->first_player < players) {
    int32_t focus = race_scene_view_focus(panel_get(), 0);
    if ((int32_t)race_player_cell(panel_get(), focus) >= LEAPER_ALL_CELL)
      a->first_player = players;
  }
  switch (a->phase) {
  case LEAPER_WAIT:
    for (int32_t i = players - 1; i >= a->first_player; i--) {
      int32_t focus = race_scene_view_focus(panel_get(), i);
      if ((int32_t)race_player_cell(panel_get(), focus) == a->trigger_cell) {
        leaper_start_delay(a);
        a->first_player = i;
        break;
      }
      if ((int32_t)race_player_cell(panel_get(), focus) > a->trigger_cell) {
        leaper_start_delay(a);
        a->first_player = i + 1;
        break;
      }
    }
    a->waiting = 1;
    break;
  case LEAPER_DELAYING: {
    a->delay--;
    if (a->delay >= 0) break;
    a->phase++;
    a->waiting = 0;
    vec3_copy_bytes(body, member_addr(obj, ActorType19, points[0]));
    leaper_body_damping(body, LEAPER_DAMPING);
    actor_set_cell(obj, body, a->base.cell);
    f32 angle = fadd(random_scaled(LEAPER_ANGLE), LEAPER_ANGLE_MIN);
    f32 c = njCos(f32_to_int(angle));
    f32 s = njSin(f32_to_int(angle));
    vec3_set(s, F32(0.0f), c, member_addr(obj, ActorType19, points[1]));
    break;
  }
  case LEAPER_FLY:
    vec3_add(member_addr(body, Body, force), member_addr(obj, ActorType19, points[1]));
    a->points[1].y = fadd(a->points[1].y, LEAPER_RISE);
    if (fgt(a->body.position.y, g_leaper_rest_y)) {
      a->waiting = 1;
      a->phase = LEAPER_WAIT;
      leaper_body_land(body);
    }
    break;
  default:
    break;
  }
  f32 moving = fadd(f32_abs(a->body.velocity.x), f32_abs(a->body.velocity.z));
  if (fgt(moving, LEAPER_MOVING)) {
    f32 vz = a->body.velocity.z;
    f32 radians = rt_atan2f(a->body.velocity.x, vz);
    /* radians to Ninja angle units, truncated to a whole unit */
    f32 heading = f32_from_int(f32_to_int(fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 2 pi */)));
    body1_pull_angle(heading, g_leaper_turn_rate, member_addr(obj, ActorType19, turns[1]));
  }
  body_step(body);
}
HW_EXPORT(8c0512a4, type19_step2(ARG0))
