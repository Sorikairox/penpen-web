/* Pen Pen port, S18 b02: the CastScene's update and draw (game/cast_scene.h), and the RacerEffects method the update
 * fires the ribbons with.
 *
 * The update reads the pad once: the main stick (its vertical axis turned so that up is positive), the R trigger,
 * and two shift bits (the triggers' digital bits). START cycles the trap mode (the TrapSet is told; mode 1 freezes
 * the scene). With the main controls the shift bits pick what the d-pad does: nothing held turns the view with the
 * stick, one shift edits the looks, the other picks the penguin and its motion set. The alternate controls use
 * combinations of both shifts and move the camera with the stick while a shift is held. After the alternate
 * controls the ribbons are fired when the motion enters or leaves one of its frame windows. */
#include "hw.h"
#include "../core/pad.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "../core/sprite_text.h"
#include "motion.h"
#include "cast_scene.h"
#include "trap_set.h"
#include "effects.h"

/* the pad's two shift bits (the triggers as digital buttons) */
#define CAST_SHIFT_1 0x20000u
#define CAST_SHIFT_2 0x10000u
enum { CAST_TRAP_MODES = 3, CAST_TRAP_FROZEN = 1 };
enum { CAST_PENGUINS = 8, CAST_MOTIONS = 29 }; /* motion sets 0..28; past them one more per penguin (below) */
/* the looks' ranges */
enum { CAST_LOOK0_COUNT = 27, CAST_LOOK1_COUNT = 9, CAST_LOOK23_COUNT = 13 };

#define CAST_TURN_RATE  F32_BITS(0x43B60B60u) /* 364.08887: Ninja angle units per degree, per unit of stick */
#define CAST_PITCH_MAX  F32_BITS(0x452AAAAAu) /* 2730.6667: 15 degrees */
#define CAST_PITCH_MIN  F32_BITS(0xC52AAAAAu) /* -2730.6667 */
#define CAST_MOVE_RATE  F32_BITS(0x3EFFFFFFu) /* 0.49999997 */
#define CAST_RIBBON_SIZE F32(7.5f)

void ribbon_set_push_at(gaddr ribbons, gaddr position, int32_t flag, f32 size, f32 angle); /* game/effects_ribbon_at.c (func_8C039B14) */

/* was func_8C01F752 (a RacerEffects method, a thunk): its ribbon set's emitter, the arguments going on as they came
 * (a tail jump: the emitter returns nothing, and neither does the thunk; S18: the writer passed r0 on, which the
 * synthetic samples showed is the emitter's leftover) */
void racer_effects_ribbons_emit(gaddr effects, gaddr position, int32_t flag, f32 size, f32 angle) {
  ribbon_set_push_at(racer_effects_at(effects)->ribbons, position, flag, size, angle);
}
HW_EXPORT(8c01f752, racer_effects_ribbons_emit(ARG0, ARG1, (int32_t)ARG2, FARG0, FARG1))

/* ---- the update's pieces ---- */

/* start the motion set again (the penguin shown) */
static void cast_play(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  motion_player_play_set_1(cast_scene_penguin(scene), s->motion);
  s->word58 = 0;
}

/* a new penguin is shown: its motion set started, its looks read (from the last to the first) */
static void cast_take_penguin(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  motion_player_play_set_1(cast_scene_penguin(scene), s->motion);
  for (int32_t i = CAST_LOOKS - 1; i >= 0; i--)
    s->looks[i] = motion_player_look(cast_scene_penguin(scene), i);
}

/* the camera back where it starts */
static void cast_camera_home(gaddr scene) {
  vec3_set(g_cast_start_x, g_cast_start_y, g_cast_start_z, member_addr(scene, CastScene, camera));
}

/* the next motion set: past the 29 common ones, one to four more by penguin (penguin 0 has none), then back to 0 */
static void cast_motion_next(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  s->motion++;
  if (s->motion == CAST_MOTIONS) {
    switch (s->penguin) {
    case 7: s->motion++; /* fall through */
    case 5: s->motion++; /* fall through */
    case 1: case 3: case 4: case 6: s->motion++; /* fall through */
    case 2: s->motion++; break;
    default: break; /* 0 (and any other) */
    }
  } else if (s->motion > CAST_MOTIONS) {
    s->motion = 0;
  }
  cast_play(scene);
}

/* the previous motion set: below 0, the penguin's last one; from past the common ones, back to the last common one */
static void cast_motion_prev(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  s->motion--;
  if (s->motion < 0) {
    switch (s->penguin) {
    case 0: s->motion = 29; break;
    case 2: s->motion = 30; break;
    case 1: case 3: case 4: case 6: s->motion = 31; break;
    case 5: s->motion = 32; break;
    case 7: s->motion = 33; break;
    default: break; /* stays at -1 */
    }
  } else if (s->motion >= CAST_MOTIONS) {
    s->motion = CAST_MOTIONS - 1;
  }
  cast_play(scene);
}

/* the looks: up / down step look 0 / 1, right and left both step looks 2 and 3 (one number), Y toggles the
 * penguin's word14 */
static void cast_edit_looks(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  const Pad *pad = &s->pad;
  if (pad_pressed(pad, PAD_UP)) {
    if (++s->looks[0] >= CAST_LOOK0_COUNT) s->looks[0] = 0;
    motion_player_set_look(cast_scene_penguin(scene), 0, s->looks[0]);
  }
  if (pad_pressed(pad, PAD_DOWN)) {
    if (++s->looks[1] >= CAST_LOOK1_COUNT) s->looks[1] = 0;
    motion_player_set_look(cast_scene_penguin(scene), 1, s->looks[1]);
  }
  if (pad_pressed(pad, PAD_RIGHT)) {
    if (++s->looks[2] >= CAST_LOOK23_COUNT) s->looks[2] = 0;
    motion_player_set_look(cast_scene_penguin(scene), 2, s->looks[2]);
    motion_player_set_look(cast_scene_penguin(scene), 3, s->looks[2]);
  }
  if (pad_pressed(pad, PAD_LEFT)) {
    if (++s->looks[2] >= CAST_LOOK23_COUNT) s->looks[2] = 0;
    motion_player_set_look(cast_scene_penguin(scene), 2, s->looks[2]);
    motion_player_set_look(cast_scene_penguin(scene), 3, s->looks[2]);
  }
  if (pad_pressed(pad, PAD_Y))
    motion_player_toggle_word14(cast_scene_penguin(scene));
}

/* the next penguin (wrapping at 8) */
static void cast_penguin_next(CastScene *s) {
  if (++s->penguin >= CAST_PENGUINS) s->penguin -= CAST_PENGUINS;
}

/* the main controls' penguin and motion picking: up / down the penguin (its camera, view and motion set start
 * again), right / left the motion set, A starts it again */
static void cast_main_pick(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  const Pad *pad = &s->pad;
  int32_t changed = 0;
  if (pad_pressed(pad, PAD_UP)) {
    cast_penguin_next(s);
    changed = 1;
  } else if (pad_pressed(pad, PAD_DOWN)) {
    if (--s->penguin < 0) s->penguin += CAST_PENGUINS;
    changed = 1;
  }
  if (changed) {
    cast_camera_home(scene);
    s->pitch = F32(0.0f);
    s->yaw = F32(0.0f);
    cast_take_penguin(scene);
  }
  if (pad_pressed(pad, PAD_RIGHT))
    cast_motion_next(scene);
  else if (pad_pressed(pad, PAD_LEFT))
    cast_motion_prev(scene);
  if (pad_pressed(pad, PAD_A))
    cast_play(scene);
}

/* the main controls: the shift bits pick the d-pad's job; the penguin plays at speed 1; with no shift the stick
 * turns the view (the pitch kept within +-15 degrees) and X / A move the camera away / closer; B puts the view and
 * the camera's distance back. Then the penguin's frame. */
static void cast_main_controls(gaddr scene, int32_t shift, f32 stick_y, f32 stick_x) {
  CastScene *s = cast_scene_at(scene);
  const Pad *pad = &s->pad;
  if (shift == 1)
    cast_edit_looks(scene);
  if (shift == 2)
    cast_main_pick(scene);
  motion_player_set_speed(cast_scene_penguin(scene), F32(1.0f));
  if (shift == 0) {
    s->pitch = fsub(s->pitch, fmul(stick_y, CAST_TURN_RATE));
    if (fgt(s->pitch, CAST_PITCH_MAX))
      s->pitch = CAST_PITCH_MAX;
    else if (fgt(CAST_PITCH_MIN, s->pitch))
      s->pitch = CAST_PITCH_MIN;
    s->yaw = fmac(CAST_TURN_RATE, stick_x, s->yaw);
    if (pad_held(pad, PAD_X)) s->camera.z = fadd(s->camera.z, CAST_MOVE_RATE);
    if (pad_held(pad, PAD_A)) s->camera.z = fsub(s->camera.z, CAST_MOVE_RATE);
  }
  if (pad_held(pad, PAD_B)) {
    s->pitch = F32(0.0f);
    s->yaw = F32(0.0f);
    s->camera.z = g_cast_start_z;
  }
  motion_player_update(cast_scene_penguin(scene));
}

/* the alternate controls' picking: both shifts: B (unless A) the next penguin; shift 1: right / left the motion
 * set, down starts it again, Y toggles the fixed speed; shift 2: the looks */
static void cast_alt_pick(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  const Pad *pad = &s->pad;
  if (pad_held(pad, CAST_SHIFT_1) && pad_held(pad, CAST_SHIFT_2)) {
    int32_t changed = 0;
    if (!pad_pressed(pad, PAD_A) && pad_pressed(pad, PAD_B)) {
      cast_penguin_next(s);
      changed = 1;
    }
    if (changed) {
      cast_camera_home(scene);
      cast_take_penguin(scene);
    }
  }
  if (pad_held(pad, CAST_SHIFT_1)) {
    if (pad_pressed(pad, PAD_RIGHT))
      cast_motion_next(scene);
    else if (pad_pressed(pad, PAD_LEFT))
      cast_motion_prev(scene);
    if (pad_pressed(pad, PAD_DOWN))
      cast_play(scene);
    if (pad_pressed(pad, PAD_Y))
      s->fixed_speed = s->fixed_speed == 0;
  }
  if (pad_held(pad, CAST_SHIFT_2))
    cast_edit_looks(scene);
}

/* the alternate controls' camera: the speed (1, or the R trigger's); shift 1 held: the stick moves the camera in
 * its plane, X / A away / closer, B home; else the stick turns the view (no limit), X / A move it, B resets the view */
static void cast_alt_camera(gaddr scene, f32 stick_y, f32 stick_x, f32 trigger_r) {
  CastScene *s = cast_scene_at(scene);
  const Pad *pad = &s->pad;
  f32 speed = s->fixed_speed != 0 ? F32(1.0f) : trigger_r;
  motion_player_set_speed(cast_scene_penguin(scene), speed);
  if (pad_held(pad, CAST_SHIFT_1)) {
    s->camera.x = fmac(CAST_MOVE_RATE, stick_x, s->camera.x);
    s->camera.y = fmac(CAST_MOVE_RATE, stick_y, s->camera.y);
    if (pad_held(pad, PAD_X)) s->camera.z = fadd(s->camera.z, CAST_MOVE_RATE);
    if (pad_held(pad, PAD_A)) s->camera.z = fsub(s->camera.z, CAST_MOVE_RATE);
    if (pad_held(pad, PAD_B)) {
      s->camera.x = g_cast_start_x;
      s->camera.y = g_cast_start_y;
      s->camera.z = g_cast_start_z;
    }
  } else {
    s->pitch = fsub(s->pitch, fmul(stick_y, CAST_TURN_RATE));
    s->yaw = fmac(CAST_TURN_RATE, stick_x, s->yaw);
    if (pad_held(pad, PAD_X)) s->camera.z = fadd(s->camera.z, CAST_MOVE_RATE);
    if (pad_held(pad, PAD_A)) s->camera.z = fsub(s->camera.z, CAST_MOVE_RATE);
    if (pad_held(pad, PAD_B)) {
      s->pitch = F32(0.0f);
      s->yaw = F32(0.0f);
    }
  }
}

/* the frame windows of the ribbons: lo < frame < hi (the frame read for each bound), or lo < frame */
static int frame_between(gaddr penguin, f32 lo, f32 hi) {
  return fgt(motion_player_frame_now(penguin), lo) && fgt(hi, motion_player_frame_now(penguin));
}
static int frame_after(gaddr penguin, f32 lo) { return fgt(motion_player_frame_now(penguin), lo); }

/* whether the frame is in a window of motion set 8 for this penguin; -1 for a penguin past 7 (no ribbons) */
static int cast_set8_inside(int32_t which, gaddr p) {
  switch (which) {
  case 0: return frame_between(p, F32(0.0f), F32(15.0f));
  case 1: return frame_after(p, F32(15.0f));
  case 2: return frame_after(p, F32(15.0f));
  case 3: return frame_after(p, F32(20.0f));
  case 4:
  case 6:
    return frame_between(p, F32(10.0f), F32(20.0f)) || frame_between(p, F32(30.0f), F32(40.0f)) ||
           frame_after(p, F32(50.0f));
  case 5:
  case 7:
    return frame_between(p, F32(7.0f), F32(15.0f)) || frame_between(p, F32(22.0f), F32(30.0f)) ||
           frame_between(p, F32(37.0f), F32(45.0f)) || frame_after(p, F32(52.0f));
  default: return -1;
  }
}

/* the ribbons: for the motions that have frame windows (by the request's motion index), fire them when the frame
 * enters a window while they are armed, or leaves it while they are not; firing toggles the arming */
static void cast_ribbons(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  gaddr p = cast_scene_penguin(scene);
  int32_t index = ((const MotionRequest *)hw_ptr(motion_player_request(p)))->index;
  int inside;
  if (index == 21 || index == 22)
    inside = frame_between(p, F32(21.0f), F32(51.0f));
  else if (index == 4 || index == 23)
    inside = frame_between(p, F32(2.0f), F32(64.0f));
  else if (index == 8)
    inside = cast_set8_inside(s->penguin, p);
  else
    inside = -1;
  int32_t fire = 0;
  if (inside > 0) {
    if (g_cast_ribbons_armed != 0) fire = 1;
  } else if (inside == 0) {
    if (g_cast_ribbons_armed == 0) fire = 1;
  }
  if (fire != 0) {
    racer_effects_ribbons_emit(s->effects, member_addr(scene, CastScene, camera), g_cast_ribbons_armed,
                               CAST_RIBBON_SIZE, s->yaw);
    g_cast_ribbons_armed = g_cast_ribbons_armed == 0;
  }
}

/* was func_8C0200E8 (with its tail blocks func_8C02131C and the shared epilogue func_8C02139E): the scene's update
 * (see the top of the file). The R trigger is read into the alternate camera; the L trigger is read and not used. */
void cast_scene_update(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  const Pad *pad = &s->pad;
  f32 stick_y = fneg(pad_axis(pad, 1));
  f32 stick_x = pad_axis(pad, 0);
  f32 trigger_l = pad_axis(pad, 5);
  f32 trigger_r = pad_axis(pad, 4);
  (void)trigger_l;
  int32_t shift = 0;
  if (pad_held(pad, CAST_SHIFT_1)) shift |= 1;
  if (pad_held(pad, CAST_SHIFT_2)) shift |= 2;

  if (pad_pressed(pad, PAD_START)) {
    if (++s->trap_mode >= CAST_TRAP_MODES) s->trap_mode = 0;
    trap_set_set_shown(s->trap_set, s->trap_mode);
  }
  if (s->trap_mode == CAST_TRAP_FROZEN)
    return;
  if (pad_pressed(pad, PAD_Y))
    motion_player_toggle_word14(cast_scene_penguin(scene));

  if (s->controls != 0) {
    cast_main_controls(scene, shift, stick_y, stick_x);
    return;
  }
  cast_alt_pick(scene);
  cast_alt_camera(scene, stick_y, stick_x, trigger_r);
  motion_player_update(cast_scene_penguin(scene));
  cast_ribbons(scene);
}
HW_EXPORT(8c0200e8, cast_scene_update(ARG0))

/* was func_8C0213C8: the scene's draw (none in trap mode 1): the penguin shown, four times (top left, top right,
 * bottom left, bottom right of a square in front of the camera), each turned by the view's yaw and pitch; then the
 * penguin's name and the motion set's name. The four places are locals of the original's frame (kept on the guest
 * stack at the same place: 0x38 bytes, the places at its bottom, the last one first). */
void cast_scene_draw(gaddr scene) {
  CastScene *s = cast_scene_at(scene);
  if (s->trap_mode == CAST_TRAP_FROZEN)
    return;
  gaddr frame = hw_stack_alloc(0x38);
  gaddr places[4];
  for (int32_t i = 0; i < 4; i++) places[i] = frame + (uint32_t)((3 - i) * (int32_t)sizeof(Vec3));
  vec3_set(g_cast_draw_left, g_cast_draw_top, g_cast_draw_depth, places[0]);
  vec3_set(g_cast_draw_right, g_cast_draw_top, g_cast_draw_depth, places[1]);
  vec3_set(g_cast_draw_left, g_cast_draw_bottom, g_cast_draw_depth, places[2]);
  vec3_set(g_cast_draw_right, g_cast_draw_bottom, g_cast_draw_depth, places[3]);
  for (int32_t i = 0; i < 4; i++) {
    matrix_push();
    matrix_translate_v(places[i]);
    matrix_rotate_y(s->yaw);
    matrix_rotate_x(s->pitch);
    motion_player_draw(cast_scene_penguin(scene));
    matrix_pop();
  }
  text_set_scale(F32(32.0f), F32(32.0f), F32(0.0f));
  text_draw(0x40, 0x20, g_cast_penguin_names[s->penguin]);
  text_draw(0x40, 0x48, g_cast_motion_names[s->motion]);
  hw_stack_free(0x38);
}
HW_EXPORT(8c0213c8, cast_scene_draw(ARG0))
