/* Pen Pen port, S19 (u): out-of-line copies of small inline methods that Ghidra's function table missed (they sit in
 * gaps between listed functions; entered by bsr or by pointer): Vec3 / Body setters, "return this", a Vec3 copy,
 * the setters of a Contacts' words that the scenery units use, and the text style's colour.
 * Where an existing helper does the same stores in the same order (or on distinct words, with no other effect), the
 * copy is exported from it. */
#include "hw.h"
#include "vec.h"
#include "inline.h"
#include "contact.h"

/* ---- Vec3 and Body ---- */

/* was func_8C046A86 (Pieces' unit): a Body's damping = (1, 1, 1) (func_8C01D070's code) */
HW_EXPORT(8c046a86, vec3_fill(member_addr(ARG0, Body, damping), F32(1.0f)))
/* was func_8C049A1A, func_8C04F942, func_8C0502EA, func_8C05EC5A: a Body's damping = (s, s, s), written z, y, x
 * (func_8C01AF50's code) */
HW_EXPORT(8c049a1a, vec3_fill(member_addr(ARG0, Body, damping), FARG0))
HW_EXPORT(8c04f942, vec3_fill(member_addr(ARG0, Body, damping), FARG0))
HW_EXPORT(8c0502ea, vec3_fill(member_addr(ARG0, Body, damping), FARG0))
HW_EXPORT(8c05ec5a, vec3_fill(member_addr(ARG0, Body, damping), FARG0))
/* was func_8C049A0E and func_8C04F936: v = (x, y, z), no result. The original stores x, y, z in that order
 * (vec3_set: z first; distinct words, no other effect) */
HW_EXPORT(8c049a0e, vec3_set(FARG0, FARG1, FARG2, ARG0))
HW_EXPORT(8c04f936, vec3_set(FARG0, FARG1, FARG2, ARG0))
/* was func_8C057506: a Body's velocity = (x, y, z), no result: it stores x at 0x0C, then runs on into the stores of
 * y and z at 0x10 / 0x14 of the code after it (x, y, z order; vec3_set: z first, distinct words) */
HW_EXPORT(8c057506, vec3_set(FARG0, FARG1, FARG2, member_addr(ARG0, Body, velocity)))
/* was func_8C0502B0 (type 0x19's unit): its copy of body_reset (the three Vec3s cleared, the damping (1, 1, 1)
 * through the unit's setters) */
HW_EXPORT(8c0502b0, body_reset(ARG0))

/* was func_8C0476E4: returns its argument (Vec3's empty constructor, `return this`) */
HW_EXPORT(8c0476e4, RET(method_self(ARG0)))
/* was func_8C0476E8: dst = src (the three loads first), returns dst (func_8C012278's work) */
HW_EXPORT(8c0476e8, RET(vec3_copy(ARG0, ARG1)))

/* ---- a Contacts' words (the scenery units' inline setters: type 0x33's unit) ---- */

/* was func_8C04F950: the radius */
void contacts_set_radius(gaddr c, f32 radius) { contacts_at(c)->radius = radius; }
HW_EXPORT(8c04f950, contacts_set_radius(ARG0, FARG0))
/* was func_8C04F956: the course cell */
void contacts_set_cell(gaddr c, int32_t cell) { contacts_at(c)->cell = cell; }
HW_EXPORT(8c04f956, contacts_set_cell(ARG0, (int32_t)ARG1))
/* was func_8C04F95A: the mass */
void contacts_set_mass(gaddr c, f32 mass) { contacts_at(c)->mass = mass; }
HW_EXPORT(8c04f95a, contacts_set_mass(ARG0, FARG0))
/* was func_8C04F960: the float at 0x18 */
void contacts_set_unknown18(gaddr c, f32 v) { contacts_at(c)->unknown18 = v; }
HW_EXPORT(8c04f960, contacts_set_unknown18(ARG0, FARG0))
/* was func_8C04F966: the word at 0x1C */
void contacts_set_unknown1c(gaddr c, int32_t v) { contacts_at(c)->unknown1c = v; }
HW_EXPORT(8c04f966, contacts_set_unknown1c(ARG0, (int32_t)ARG1))
/* was func_8C04F96A: the contacts emptied (count 0) and the word after them (contacts_word in the actor classes) set
 * to 4 (func_8C030850's code) */
HW_EXPORT(8c04f96a, field_reset_2c(ARG0))

/* ---- the text style's colour ---- */

typedef struct TextColor {
  f32 a, r, g, b; /* NJS_ARGB */
} TextColor;
HW_SIZE(TextColor, 16);
#define g_text_color GUEST_VAR(TextColor, 0x8C1B3B34u) /* the text style's colour (core/sprite_text.c) */

/* was func_8C05768C: the text's colour = (r, g, b, a): r, g, b, then (through the code after it, func_8C057698) a.
 * Its argument register is not read (the global's address replaces it) */
void text_set_color(f32 r, f32 g, f32 b, f32 a) {
  TextColor *c = &g_text_color;
  c->r = r;
  c->g = g;
  c->b = b;
  c->a = a;
}
HW_EXPORT(8c05768c, text_set_color(FARG0, FARG1, FARG2, FARG3))
