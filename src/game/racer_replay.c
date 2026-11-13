/* Pen Pen port, S17 b04: two more racer classes beside the Racer (game/racer.h, vtable 0x8C23DACC):
 *  - the plain racer (vtable 0x8C23DAA4, constructor func_8C030EDE, deleting destructor func_8C0309E0): the base
 *    construction only; its character's base word is its own word at 0x588;
 *  - the replay racer (vtable 0x8C23DAF4, constructor func_8C037816 -> func_8C0377E8, deleting destructor
 *    func_8C037842, update func_8C037878): built like the Racer but without the word at 0x588 (its base word), it
 *    reads its controls from the race scene's recording (RaceScene.marks_a: a packed pad word per frame,
 *    word_cac the frame) instead of a pad, and is not held by the pause.
 * And the Racer's own destructor that returns the racer (func_8C0375B0). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/contact.h"
#include "../core/pad.h"
#include "racer.h"
#include "race_scene.h"
#include "motion.h"

#define PLAIN_RACER_VTABLE 0x8C23DAA4u
#define REPLAY_RACER_VTABLE 0x8C23DAF4u

#define BUTTON_LOCK_UPDATES 15 /* a released button is ignored for this many updates */
#define STICK_DEAD_LENGTH F32_BITS(0x3DCCCCCCu) /* 0.1 */
#define TWO_PI F32_BITS(0x40C90FD7u)            /* 6.2831836 (just under 2 pi) */

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* ---- the Racer ---- */

/* was func_8C0375B0: the Racer's destructor body, returning the racer */
gaddr racer_destruct(gaddr racer) {
  racer_body_destroy(racer);
  return racer;
}
HW_EXPORT(8c0375b0, RET(racer_destruct(ARG0)))

/* ---- the plain racer ---- */

/* was func_8C030EDE: the plain racer's constructor: its vtable, its base word, the base construction */
gaddr plain_racer_construct(gaddr racer, uint32_t index, gaddr unused_b, int32_t character, f32 unused_f,
                            int32_t costume) {
  Racer *r = racer_at(racer);
  r->task.vtable = PLAIN_RACER_VTABLE;
  r->second_base = member_addr(racer, Racer, word588);
  racer_base_init(racer, index, unused_b, character, unused_f, costume);
  return racer;
}
HW_EXPORT(8c030ede, RET(plain_racer_construct(ARG0, ARG1, ARG2, (int32_t)ARG3, FARG0, (int32_t)ARG4)))

/* was func_8C0309E0: the plain racer's deleting destructor: the second Contacts' arrays, the collider, the
 * character's model file, then the memory when asked */
gaddr plain_racer_delete(gaddr racer, int32_t free_memory) {
  contacts_free(member_addr(racer, Racer, contacts2));
  contacts_holder_destroy_self(member_addr(racer, Racer, collider_data));
  motion_player_free(member_addr(racer, Racer, second_base));
  method_empty(); /* (func_8C0304A0 on the racer) */
  if (free_memory) syFree(racer);
  return racer;
}
HW_EXPORT(8c0309e0, RET(plain_racer_delete(ARG0, (int32_t)ARG1)))

/* ---- the replay racer ---- */

/* was func_8C0377E8: the base construction (index, b, character, f, costume), then the race scene */
gaddr replay_racer_init(gaddr racer, gaddr scene, uint32_t index, gaddr unused_b, int32_t character,
                        int32_t costume, f32 unused_f) {
  racer_base_init(racer, index, unused_b, character, unused_f, costume);
  racer_at(racer)->scene = scene;
  return racer;
}
HW_EXPORT(8c0377e8, RET(replay_racer_init(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4, (int32_t)ARG5, FARG0)))

/* was func_8C037816: the replay racer's constructor: its vtable, its base word (0x588), then the construction */
gaddr replay_racer_construct(gaddr racer, gaddr scene, uint32_t index, gaddr unused_b, int32_t character,
                             int32_t costume, f32 unused_f) {
  Racer *r = racer_at(racer);
  r->task.vtable = REPLAY_RACER_VTABLE;
  r->second_base = member_addr(racer, Racer, word588);
  replay_racer_init(racer, scene, index, unused_b, character, costume, unused_f);
  return racer;
}
HW_EXPORT(8c037816, RET(replay_racer_construct(ARG0, ARG1, ARG2, ARG3, (int32_t)ARG4, (int32_t)ARG5, FARG0)))

/* was func_8C037842: the replay racer's deleting destructor: the racer's destructor body, then the memory when
 * asked */
gaddr replay_racer_delete(gaddr racer, int32_t free_memory) {
  racer_body_destroy(racer);
  if (free_memory) syFree(racer);
  return racer;
}
HW_EXPORT(8c037842, RET(replay_racer_delete(ARG0, (int32_t)ARG1)))

/* was func_8C037866: the replay racer's destructor body, returning the racer */
gaddr replay_racer_destruct(gaddr racer) {
  racer_body_destroy(racer);
  return racer;
}
HW_EXPORT(8c037866, RET(replay_racer_destruct(ARG0)))

/* a button's held state: its bit, unless a lock is still counting down */
static int32_t button_read(int32_t *lock, int32_t pad, int32_t bit) {
  if (--*lock > 0) return 0;
  return (pad & bit) != 0;
}

/* was func_8C037878: the replay racer's update: the controls as racer_read_controls (game/racer_control.c) reads
 * them, from the recording's pad word of the scene's current frame (no pause check); then the rest of the update */
void replay_racer_update(gaddr racer) {
  Racer *r = racer_at(racer);
  r->button_prev[0] = r->button[0];
  r->button_prev[1] = r->button[1];
  RaceScene *s = race_scene_at(r->scene);
  int32_t pad = ((const int16_t *)hw_ptr(s->marks_a))[s->word_cac];
  r->stick_x = fneg(stick_to_unit(pad));
  r->stick_y = stick_to_unit(pad >> 8);
  if (r->stick_damped != 0) {
    r->stick_x = fmul(r->stick_x, F32(0.5f));
    r->stick_y = fmul(r->stick_y, F32(0.75f));
  }
  r->button[0] = button_read(&r->button_lock[0], pad, 0x001);
  r->button[1] = button_read(&r->button_lock[1], pad, 0x100);
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
  racer_base_update(racer);
}
HW_EXPORT(8c037878, replay_racer_update(ARG0))
