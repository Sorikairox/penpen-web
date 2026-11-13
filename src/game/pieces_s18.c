/* Pen Pen port, S18: three pieces of functions whose true entries are not functions of port/symbols/functions.tsv
 * (Ghidra started the "function" past the entry, after the first loads): the translation of the true entry runs
 * on into the piece with the registers it set, so each piece is written as C over those registers (hw.h
 * INHERITED_R0, FARG3 = FR7, FARG4 = FR8). */
#include "hw.h"
#include "fpu.h"
#include "../core/vec.h"

/* was func_8C04CC88 and func_8C055544 (two identical copies; their true entries 0x8C04CC84 / 0x8C055540 load from.x
 * into FR7 and from.y into FR8, leaving r5 at from + 8): out = (to.x - from.y, to.y - from.x, to.z - from.z), stored
 * z, y, x; returns out. (The copy subtracts the components crossed, x from y: as the original.) */
gaddr vec3_diff_crossed_tail(gaddr out, gaddr from_z, gaddr to, f32 from_x, f32 from_y) {
  const Vec3 *t = vec3_at(to);
  f32 z = fsub(t->z, F32_AT(from_z));
  f32 y = fsub(t->y, from_x);
  f32 x = fsub(t->x, from_y);
  Vec3 *o = vec3_at(out);
  o->z = z;
  o->y = y;
  o->x = x;
  return out;
}
HW_EXPORT(8c04cc88, RET(vec3_diff_crossed_tail(ARG0, ARG1, ARG2, FARG3, FARG4)))
HW_EXPORT(8c055544, RET(vec3_diff_crossed_tail(ARG0, ARG1, ARG2, FARG3, FARG4)))

/* was func_8C057698: the last two stores of a setter of four floats (its true entry 0x8C05768C loads the global
 * into r4, stores FR4 at +4 and FR5 at +8 and sets r0 to 12): FR6 at +r0, then FR7 at +0 */
void float4_set_tail(gaddr global, uint32_t offset, f32 c, f32 d) {
  F32_AT(global + offset) = c;
  F32_AT(global) = d;
}
HW_EXPORT(8c057698, float4_set_tail(ARG0, INHERITED_R0, FARG2, FARG3))
