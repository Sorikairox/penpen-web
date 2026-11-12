/* Pen Pen port, S13: the game's trivial inline methods: field getters and setters, "return this", empty bodies.
 *
 * SHC compiled each C++ inline accessor into every unit that used it, and accessors of different classes that touch
 * the same offset compile to the same bytes: one copy here stands for all of them (port/symbols/copies.tsv; the
 * copies' exports are in src/copies/core/inline.c, which tools/copies.py takes from this file first). Since a copy may
 * belong to any class, the names say what the code does to the object, not which field it is: a class that is
 * declared later gets its own named accessor, and the copies stay class-neutral. S14: the groups whose every copy is
 * called on one class (by their callers in the call graph) say that class's field; they are at the end. */
#include "inline.h"
#include "sdk.h"
#include "../game/racer.h"
#include "../game/race_scene.h"
#include "../game/course.h"

/* an object's word / float / address at an offset */
uint32_t field_get(gaddr obj, uint32_t offset) { return U32_AT(obj + offset); }
void field_set(gaddr obj, uint32_t offset, uint32_t value) { U32_AT(obj + offset) = value; }
f32 field_get_f32(gaddr obj, uint32_t offset) { return F32_AT(obj + offset); }
void field_set_f32(gaddr obj, uint32_t offset, f32 value) { F32_AT(obj + offset) = value; }
gaddr field_addr(gaddr obj, uint32_t offset) { return obj + offset; }

/* was func_8C010BEE: an empty method (default constructors and destructors, hooks a class does not use) */
void method_empty(void) {}
HW_EXPORT(8c010bee, method_empty())

/* was func_8C010BBC: returns its argument (an inline conversion or a `return this`) */
gaddr method_self(gaddr obj) { return obj; }
HW_EXPORT(8c010bbc, RET(method_self(ARG0)))

/* was func_8C010A4E: *p = value */
HW_EXPORT(8c010a4e, field_set(ARG0, 0, ARG1))
/* was func_8C019D0E: *p = value, returns p (a one-word constructor) */
gaddr word_construct(gaddr obj, uint32_t value) {
  U32_AT(obj) = value;
  return obj;
}
HW_EXPORT(8c019d0e, RET(word_construct(ARG0, ARG1)))
/* was func_8C044DD2: *p = 0.0f */
HW_EXPORT(8c044dd2, field_set_f32(ARG0, 0, F32(0.0f)))

/* setters of a word: was func_8C01F6B0 (8), func_8C01F6A8 (0x0C), func_8C01F694 (0x10), func_8C01F698 (0x14),
 * func_8C01F69C (0x18), func_8C01F6A0 (0x1C), func_8C030844 (0x20), func_8C030848 (0x24), func_8C0229C6 (0x28),
 * func_8C02905C (0x38) */
HW_EXPORT(8c01f6b0, field_set(ARG0, 0x08, ARG1))
HW_EXPORT(8c01f6a8, field_set(ARG0, 0x0C, ARG1))
HW_EXPORT(8c01f694, field_set(ARG0, 0x10, ARG1))
HW_EXPORT(8c01f698, field_set(ARG0, 0x14, ARG1))
HW_EXPORT(8c01f69c, field_set(ARG0, 0x18, ARG1))
HW_EXPORT(8c01f6a0, field_set(ARG0, 0x1C, ARG1))
HW_EXPORT(8c030844, field_set(ARG0, 0x20, ARG1))
HW_EXPORT(8c030848, field_set(ARG0, 0x24, ARG1))
HW_EXPORT(8c0229c6, field_set(ARG0, 0x28, ARG1))
HW_EXPORT(8c02905c, field_set(ARG0, 0x38, ARG1))
/* constant stores: was func_8C030590 (0 at 4), func_8C0305EC (0 at 8), func_8C016306 (0 at 0xF0), func_8C030598
 * (1.0f at 0x0C) */
HW_EXPORT(8c030590, field_set(ARG0, 0x04, 0))
HW_EXPORT(8c0305ec, field_set(ARG0, 0x08, 0))
HW_EXPORT(8c016306, field_set(ARG0, 0xF0, 0))
HW_EXPORT(8c030598, field_set_f32(ARG0, 0x0C, F32(1.0f)))

/* was func_8C0229BE: a value at 0x2C and its follower at 0x34 cleared */
void field_set_and_clear(gaddr obj, uint32_t value) {
  U32_AT(obj + 0x2C) = value;
  U32_AT(obj + 0x34) = 0;
}
HW_EXPORT(8c0229be, field_set_and_clear(ARG0, ARG1))
/* was func_8C030850: 0 at 0x2C, 4 at 0x54 */
void field_reset_2c(gaddr obj) {
  U32_AT(obj + 0x2C) = 0;
  U32_AT(obj + 0x54) = 4;
}
HW_EXPORT(8c030850, field_reset_2c(ARG0))

/* setters of a float: was func_8C01E136 (0), func_8C01E13A (4), func_8C01E140 (8), func_8C01AEA6 (0x0C),
 * func_8C030744 (0x14), func_8C03074A (0x18) */
HW_EXPORT(8c01e136, field_set_f32(ARG0, 0x00, FARG0))
HW_EXPORT(8c01e13a, field_set_f32(ARG0, 0x04, FARG0))
HW_EXPORT(8c01e140, field_set_f32(ARG0, 0x08, FARG0))
HW_EXPORT(8c01aea6, field_set_f32(ARG0, 0x0C, FARG0))
HW_EXPORT(8c030744, field_set_f32(ARG0, 0x14, FARG0))
HW_EXPORT(8c03074a, field_set_f32(ARG0, 0x18, FARG0))

/* getters of a word: was func_8C0229CA (0x2C), func_8C02903A (0x30), func_8C029036 (0x34), func_8C02903E (0x64) */
HW_EXPORT(8c0229ca, RET(field_get(ARG0, 0x2C)))
HW_EXPORT(8c02903a, RET(field_get(ARG0, 0x30)))
HW_EXPORT(8c029036, RET(field_get(ARG0, 0x34)))
HW_EXPORT(8c02903e, RET(field_get(ARG0, 0x64)))
/* getters of a float: was func_8C01AEA2 (0), func_8C01AF04 (4), func_8C0225D8 (8) */
HW_EXPORT(8c01aea2, RETF(field_get_f32(ARG0, 0x00)))
HW_EXPORT(8c01af04, RETF(field_get_f32(ARG0, 0x04)))
HW_EXPORT(8c0225d8, RETF(field_get_f32(ARG0, 0x08)))
/* the address of a member: was func_8C013B1E (0x0C) */
HW_EXPORT(8c013b1e, RET(field_addr(ARG0, 0x0C)))

/* an element of an array member: was func_8C0229CE (words at 0x68), func_8C0229DA (0x88), func_8C0291EA (0xFC); func_8C03DB00 (the first word of 8-byte elements at 0x11C) */
uint32_t field_array_get(gaddr obj, uint32_t offset, int32_t index, uint32_t stride) {
  return U32_AT(obj + offset + (uint32_t)index * stride);
}
HW_EXPORT(8c0229ce, RET(field_array_get(ARG0, 0x68, (int32_t)ARG1, 4)))
HW_EXPORT(8c0229da, RET(field_array_get(ARG0, 0x88, (int32_t)ARG1, 4)))
HW_EXPORT(8c0291ea, RET(field_array_get(ARG0, 0xFC, (int32_t)ARG1, 4)))
HW_EXPORT(8c03db00, RET(field_array_get(ARG0, 0x11C, (int32_t)ARG1, 8)))
/* element i of the 16-bit table a member points to, sign-extended */
int32_t field_table16_get(gaddr obj, uint32_t offset, int32_t index) {
  gaddr table = U32_AT(obj + offset);
  return *(int16_t *)hw_ptr(table + 2u * (uint32_t)index);
}

/* a float ARGB colour {a, r, g, b} at 0x8C1B3B34 (its reader is still translated) */
#define G_COLOR 0x8C1B3B34u
/* was func_8C018D9E: set it from (r, g, b, a) */
void global_color_set(f32 r, f32 g, f32 b, f32 a) {
  f32 *c = (f32 *)hw_ptr(G_COLOR);
  c[1] = r;
  c[2] = g;
  c[3] = b;
  c[0] = a;
}
HW_EXPORT(8c018d9e, global_color_set(FARG0, FARG1, FARG2, FARG3))

/* ---- groups that serve one class (S14: every copy's callers pass that class) ---- */

/* the Racer's (game/racer.h; also called on the racer class of func_8C03E150 / func_8C03FC68, LaneRacer, which has
 * the same fields there) */

/* was func_8C030A20: start an action, its two arguments cleared */
void racer_set_action(gaddr racer, uint32_t action) {
  Racer *r = racer_at(racer);
  r->action = (int32_t)action;
  r->action_arg[0] = 0;
  r->action_arg[1] = 0;
}
HW_EXPORT(8c030a20, racer_set_action(ARG0, ARG1))
/* was func_8C02914A: its course cell (its contacts' cell) */
uint32_t racer_cell(gaddr racer) { return field_get(racer, offsetof(Racer, contacts.cell)); }
HW_EXPORT(8c02914a, RET(racer_cell(ARG0)))
/* was func_8C029134: its heading */
f32 racer_heading(gaddr racer) { return field_get_f32(racer, offsetof(Racer, heading.value)); }
HW_EXPORT(8c029134, RETF(racer_heading(ARG0)))
/* was func_8C029118 / func_8C030A40: the address of its moving body / of its matrix's z axis (the third row) */
gaddr racer_body(gaddr racer) { return field_addr(racer, offsetof(Racer, body)); }
gaddr racer_matrix_axis_z(gaddr racer) { return field_addr(racer, offsetof(Racer, matrix3a8.m[8])); }
HW_EXPORT(8c029118, RET(racer_body(ARG0)))
HW_EXPORT(8c030a40, RET(racer_matrix_axis_z(ARG0)))

/* the RaceScene's (game/race_scene.h), through its course (game/course.h) */

/* was func_8C02E8E2: the attribute word of the course's cell i, sign-extended */
int32_t race_scene_cell_attr(gaddr scene, int32_t i) {
  return field_table16_get(race_scene_at(scene)->course, offsetof(Course, cell_attrs), i);
}
HW_EXPORT(8c02e8e2, RET(race_scene_cell_attr(ARG0, (int32_t)ARG1)))
/* was func_8C029202: the address of the course's 12-byte record of cell i */
gaddr race_scene_cell_point(gaddr scene, int32_t i) {
  gaddr array = course_at(race_scene_at(scene)->course)->cell_points;
  return array + (uint32_t)i * 12;
}
HW_EXPORT(8c029202, RET(race_scene_cell_point(ARG0, (int32_t)ARG1)))

/* the Course's */

/* was func_8C03DACC: the attribute word of cell i, sign-extended */
int32_t course_cell_attr(gaddr course, int32_t i) { return field_table16_get(course, offsetof(Course, cell_attrs), i); }
HW_EXPORT(8c03dacc, RET(course_cell_attr(ARG0, (int32_t)ARG1)))

/* S14: more class-neutral copies: was func_8C038900 (and 4 copies): the float at 0x10 = x; func_8C046A50 (and a
 * copy): the float at 0x0C; func_8C046A5C (and a copy): the float at 0x14 */
HW_EXPORT(8c038900, field_set_f32(ARG0, 0x10, FARG0))
HW_EXPORT(8c046a50, RETF(field_get_f32(ARG0, 0x0C)))
HW_EXPORT(8c046a5c, RETF(field_get_f32(ARG0, 0x14)))
