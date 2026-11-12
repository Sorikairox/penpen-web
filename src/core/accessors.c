/* Pen Pen port, S13: more trivial inline methods (core/accessors.c): field getters and setters, operator new, a
 * destructor, thunks. S14: the ones that are not copies (port/symbols/copies.tsv) and serve one class name its field
 * (the menu of func_8C0233EA, RaceScene, Racer); the rest stay class-neutral like core/inline.c. */
#include "hw.h"
#include "sdk.h"
#include "../core/inline.h"
#include "../core/vec.h"
#include "../game/game.h"
#include "../game/sound.h"
#include "../game/race_scene.h"
#include "../game/racer.h"
#include "../game/menu.h"

/* was func_8C010508: pause a task tree (its root task's pause state; the callers pass a task's owner): another copy
 * of tasktree_set_pause (core/task.c) */
HW_EXPORT(8c010508, tasktree_set_pause(tasktree_at(ARG0), (int32_t)ARG1))

/* ---- the RaceSetup's setters (game/race_scene.h; S14 had named them for the SetupMenu, which calls them on its
 * RaceSetup when it leaves for the race: game/house.c) ---- */

/* was func_8C0229E6 / func_8C0229EA: the variant (0x34) / the course (0x30) */
void race_setup_set_variant(gaddr setup, int32_t value) { race_setup_at(setup)->variant = value; }
void race_setup_set_course(gaddr setup, int32_t value) { race_setup_at(setup)->course = value; }
/* not exported (S15 difftest: untested: only game/house.c calls it, from C, and no passing sample of its caller covered it): HW_EXPORT(8c0229e6, race_setup_set_variant(ARG0, (int32_t)ARG1)) */
/* not exported (S15 difftest: untested: only game/house.c calls it, from C, and no passing sample of its caller covered it): HW_EXPORT(8c0229ea, race_setup_set_course(ARG0, (int32_t)ARG1)) */

/* was func_8C0229EE / func_8C0229FA: player i's words at 0x68 / 0x88 (inline.c has the getters) */
void race_setup_set_word68(gaddr setup, int32_t i, uint32_t value) { race_setup_at(setup)->words68[i] = value; }
void race_setup_set_word88(gaddr setup, int32_t i, uint32_t value) { race_setup_at(setup)->words88[i] = value; }
/* not exported (S15 difftest: untested: only game/house.c calls it, from C, and no passing sample of its caller covered it): HW_EXPORT(8c0229ee, race_setup_set_word68(ARG0, (int32_t)ARG1, ARG2)) */
/* not exported (S15 difftest: untested: only game/house.c calls it, from C, and no passing sample of its caller covered it): HW_EXPORT(8c0229fa, race_setup_set_word88(ARG0, (int32_t)ARG1, ARG2)) */

/* was func_8C0292AA: the player view i shows (RaceScene) */
int32_t race_scene_view_player(gaddr scene, int32_t view) { return race_scene_at(scene)->view_player[view]; }
HW_EXPORT(8c0292aa, RET(race_scene_view_player(ARG0, (int32_t)ARG1)))

/* ---- the racer's (game/racer.h) ---- */

/* was func_8C02915C, func_8C029162, func_8C029192: its state, its action, its word at 0x55C */
int32_t racer_state(gaddr racer) { return racer_at(racer)->state; }
int32_t racer_action(gaddr racer) { return racer_at(racer)->action; }
uint32_t racer_word55c(gaddr racer) { return racer_at(racer)->word55c; }
HW_EXPORT(8c02915c, RET(racer_state(ARG0)))
HW_EXPORT(8c029162, RET(racer_action(ARG0)))
HW_EXPORT(8c029192, RET(racer_word55c(ARG0)))
/* was func_8C029120 (its body's velocity), func_8C029198 (its camera's eye), func_8C0291A0, func_8C0291A6 (the
 * camera's Vec3s at 0x4E0, 0x51C): the address of a member */
gaddr racer_velocity(gaddr racer) { return member_addr(racer, Racer, body.body.velocity); }
gaddr racer_camera_eye(gaddr racer) { return member_addr(racer, Racer, camera_eye); }
gaddr racer_camera_vec4e0(gaddr racer) { return member_addr(racer, Racer, camera_vec4e0); }
gaddr racer_camera_vec51c(gaddr racer) { return member_addr(racer, Racer, camera_vec51c); }
HW_EXPORT(8c029120, RET(racer_velocity(ARG0)))
HW_EXPORT(8c029198, RET(racer_camera_eye(ARG0)))
HW_EXPORT(8c0291a0, RET(racer_camera_vec4e0(ARG0)))
HW_EXPORT(8c0291a6, RET(racer_camera_vec51c(ARG0)))

/* constant stores to globals: was func_8C01A8F4 (g_race_setup = 0: the set-up's destructor body), func_8C029026
 * (g_soft_reset = 0) */
void race_setup_clear(void) { g_race_setup = 0; }
HW_EXPORT(8c01a8f4, race_setup_clear())
void soft_reset_clear(void) { g_soft_reset = 0; }
HW_EXPORT(8c029026, soft_reset_clear())

/* was func_8C028E38: operator new (syMalloc) */
gaddr operator_new(uint32_t size) { return syMalloc(size); }
HW_EXPORT(8c028e38, RET(operator_new(ARG0)))

/* was func_8C01DDFC: the deleting destructor of a class whose destructor body is empty: frees the object when
 * asked, returns it */
gaddr object_delete(gaddr obj, int32_t free_memory) {
  method_empty();
  if (free_memory != 0)
    syFree(obj);
  return obj;
}
HW_EXPORT(8c01ddfc, RET(object_delete(ARG0, (int32_t)ARG1)))

/* thunks: was func_8C016A4E (sound_init), func_8C016AD6 (sound_update), func_8C0228FE (nj_ModelL2) */
static void nj_ModelL2(gaddr model) { CALL1(0x8C0D0C22u, model); }
HW_EXPORT(8c016a4e, sound_init())
HW_EXPORT(8c016ad6, sound_update())
HW_EXPORT(8c0228fe, nj_ModelL2(ARG0))

/* was func_8C022B34: plays MIDI sequence `sequence` on a port at full volume, centred */
void sound_play_sequence(int32_t port, int32_t sequence, int32_t priority) {
  sound_midi_request(port, priority, sequence, 0x7F, 0);
}
HW_EXPORT(8c022b34, sound_play_sequence((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* ---- Body (core/vec.h) ---- */

/* was func_8C0225DE, func_8C022D16, func_8C029368: other copies of body_reset (the last two set the vectors with
 * vec3_set) */
HW_EXPORT(8c0225de, body_reset(ARG0))
HW_EXPORT(8c022d16, body_reset(ARG0))
HW_EXPORT(8c029368, body_reset(ARG0))

/* was func_8C0226B2: pulls a Body toward the point (x, y, z): force += (point - position) * rate */
void body_pull_xyz(gaddr body, f32 x, f32 y, f32 z, f32 rate) {
  Body *b = (Body *)hw_ptr(body);
  f32 dx = fmul(fsub(x, b->position.x), rate);
  f32 dy = fmul(fsub(y, b->position.y), rate);
  f32 dz = fmul(fsub(z, b->position.z), rate);
  b->force.z = fadd(dz, b->force.z);
  b->force.y = fadd(dy, b->force.y);
  b->force.x = fadd(dx, b->force.x);
}
HW_EXPORT(8c0226b2, body_pull_xyz(ARG0, FARG0, FARG1, FARG2, FARG3))

/* was func_8C028DFA: out = a - b (vec3_diff's operands the other way round; returns nothing) */
HW_EXPORT(8c028dfa, vec3_diff(ARG0, ARG2, ARG1))
