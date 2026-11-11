/* Pen Pen port, S13: the game's trivial inline methods: field getters and setters, "return this", empty bodies.
 *
 * SHC compiled each C++ inline accessor into every unit that used it, and accessors of different classes that touch
 * the same offset compile to the same bytes: one copy here stands for all of them (port/symbols/copies.tsv; the
 * copies' exports are in src/copies/core/inline.c, which tools/copies.py takes from this file first). Since a copy may
 * belong to any class, the names say what the code does to the object, not which field it is: a class that is
 * declared later gets its own named accessor, and the copies stay class-neutral. */
#include "inline.h"
#include "sdk.h"

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
/* was func_8C030A20: a value at 0x27C, the two words after it cleared */
void field_set_and_clear2(gaddr obj, uint32_t value) {
  U32_AT(obj + 0x27C) = value;
  U32_AT(obj + 0x280) = 0;
  U32_AT(obj + 0x284) = 0;
}
HW_EXPORT(8c030a20, field_set_and_clear2(ARG0, ARG1))

/* setters of a float: was func_8C01E136 (0), func_8C01E13A (4), func_8C01E140 (8), func_8C01AEA6 (0x0C),
 * func_8C030744 (0x14), func_8C03074A (0x18) */
HW_EXPORT(8c01e136, field_set_f32(ARG0, 0x00, FARG0))
HW_EXPORT(8c01e13a, field_set_f32(ARG0, 0x04, FARG0))
HW_EXPORT(8c01e140, field_set_f32(ARG0, 0x08, FARG0))
HW_EXPORT(8c01aea6, field_set_f32(ARG0, 0x0C, FARG0))
HW_EXPORT(8c030744, field_set_f32(ARG0, 0x14, FARG0))
HW_EXPORT(8c03074a, field_set_f32(ARG0, 0x18, FARG0))

/* getters of a word: was func_8C0229CA (0x2C), func_8C02903A (0x30), func_8C029036 (0x34), func_8C02903E (0x64),
 * func_8C02914A (0x1C4) */
HW_EXPORT(8c0229ca, RET(field_get(ARG0, 0x2C)))
HW_EXPORT(8c02903a, RET(field_get(ARG0, 0x30)))
HW_EXPORT(8c029036, RET(field_get(ARG0, 0x34)))
HW_EXPORT(8c02903e, RET(field_get(ARG0, 0x64)))
HW_EXPORT(8c02914a, RET(field_get(ARG0, 0x1C4)))
/* getters of a float: was func_8C01AEA2 (0), func_8C01AF04 (4), func_8C0225D8 (8), func_8C029134 (0x32C) */
HW_EXPORT(8c01aea2, RETF(field_get_f32(ARG0, 0x00)))
HW_EXPORT(8c01af04, RETF(field_get_f32(ARG0, 0x04)))
HW_EXPORT(8c0225d8, RETF(field_get_f32(ARG0, 0x08)))
HW_EXPORT(8c029134, RETF(field_get_f32(ARG0, 0x32C)))
/* the address of a member: was func_8C013B1E (0x0C), func_8C029118 (0x2B8), func_8C030A40 (0x3C8) */
HW_EXPORT(8c013b1e, RET(field_addr(ARG0, 0x0C)))
HW_EXPORT(8c029118, RET(field_addr(ARG0, 0x2B8)))
HW_EXPORT(8c030a40, RET(field_addr(ARG0, 0x3C8)))

/* an element of an array member: was func_8C0229CE (words at 0x68), func_8C0229DA (0x88), func_8C0291EA (0xFC); func_8C03DB00 (the first word of 8-byte elements at 0x11C) */
uint32_t field_array_get(gaddr obj, uint32_t offset, int32_t index, uint32_t stride) {
  return U32_AT(obj + offset + (uint32_t)index * stride);
}
HW_EXPORT(8c0229ce, RET(field_array_get(ARG0, 0x68, (int32_t)ARG1, 4)))
HW_EXPORT(8c0229da, RET(field_array_get(ARG0, 0x88, (int32_t)ARG1, 4)))
HW_EXPORT(8c0291ea, RET(field_array_get(ARG0, 0xFC, (int32_t)ARG1, 4)))
HW_EXPORT(8c03db00, RET(field_array_get(ARG0, 0x11C, (int32_t)ARG1, 8)))
/* was func_8C03DACC: element i of the 16-bit table the member at 0xB4 points to, sign-extended */
int32_t field_table16_get(gaddr obj, uint32_t offset, int32_t index) {
  gaddr table = U32_AT(obj + offset);
  return *(int16_t *)hw_ptr(table + 2u * (uint32_t)index);
}
HW_EXPORT(8c03dacc, RET(field_table16_get(ARG0, 0xB4, (int32_t)ARG1)))
/* was func_8C02E8E2: the same through the member object at 0x38 */
HW_EXPORT(8c02e8e2, RET(field_table16_get(U32_AT(ARG0 + 0x38), 0xB4, (int32_t)ARG1)))
/* was func_8C029202: the address of the 12-byte element i of the array the member object at 0x38 holds at 0x68 */
gaddr member_array_element(gaddr obj, int32_t index) {
  gaddr array = U32_AT(U32_AT(obj + 0x38) + 0x68);
  return array + (uint32_t)index * 12;
}
HW_EXPORT(8c029202, RET(member_array_element(ARG0, (int32_t)ARG1)))

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
