/* Pen Pen port, S13: more trivial inline methods (core/accessors.c): field getters and setters, operator new, a
 * destructor, thunks. Class-neutral like core/inline.c (their class is not declared yet); the merge may move them
 * there. */
#include "hw.h"
#include "sdk.h"
#include "../core/inline.h"
#include "../core/vec.h"
#include "../game/game.h"
#include "../game/sound.h"

/* the word at 0x0C of the object an object's first word points to */
void field_set_indirect_0c(gaddr obj, uint32_t value) { U32_AT(U32_AT(obj) + 0x0C) = value; }
/* was func_8C010508 */
HW_EXPORT(8c010508, field_set_indirect_0c(ARG0, ARG1))

/* setters of a word: was func_8C0229E6 (0x34), func_8C0229EA (0x30) */
HW_EXPORT(8c0229e6, field_set(ARG0, 0x34, ARG1))
HW_EXPORT(8c0229ea, field_set(ARG0, 0x30, ARG1))

/* element i of a word array member = value: was func_8C0229EE (0x68), func_8C0229FA (0x88) (inline.c has the
 * getters) */
void field_array_set(gaddr obj, uint32_t offset, int32_t index, uint32_t value) {
  U32_AT(obj + offset + 4u * (uint32_t)index) = value;
}
HW_EXPORT(8c0229ee, field_array_set(ARG0, 0x68, (int32_t)ARG1, ARG2))
HW_EXPORT(8c0229fa, field_array_set(ARG0, 0x88, (int32_t)ARG1, ARG2))
/* getter: was func_8C0292AA (0x9C) */
HW_EXPORT(8c0292aa, RET(field_array_get(ARG0, 0x9C, (int32_t)ARG1, 4)))

/* getters of a word: was func_8C02915C (0x19C), func_8C029162 (0x27C), func_8C029192 (0x55C) */
HW_EXPORT(8c02915c, RET(field_get(ARG0, 0x19C)))
HW_EXPORT(8c029162, RET(field_get(ARG0, 0x27C)))
HW_EXPORT(8c029192, RET(field_get(ARG0, 0x55C)))
/* the address of a member: was func_8C029120 (0x2C4: 0x0C into the member at 0x2B8), func_8C029198 (0x4EC),
 * func_8C0291A0 (0x4E0), func_8C0291A6 (0x51C) */
HW_EXPORT(8c029120, RET(field_addr(ARG0, 0x2C4)))
HW_EXPORT(8c029198, RET(field_addr(ARG0, 0x4EC)))
HW_EXPORT(8c0291a0, RET(field_addr(ARG0, 0x4E0)))
HW_EXPORT(8c0291a6, RET(field_addr(ARG0, 0x51C)))

/* constant stores to globals: was func_8C01A8F4 (0x8C08C794 = 0), func_8C029026 (g_soft_reset = 0) */
#define g_word_8c08c794 GUEST_VAR(int32_t, 0x8C08C794u)
void global_8c08c794_clear(void) { g_word_8c08c794 = 0; }
HW_EXPORT(8c01a8f4, global_8c08c794_clear())
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
