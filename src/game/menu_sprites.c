/* Pen Pen port, S13: methods of two scene classes that draw sprites moved by Bodies (game/menu_sprites.c).
 *
 * The classes are not identified yet (their constructors are still translated): the offsets are those the methods
 * use. One keeps an NJS_SPRITE at 0x17C (position 0x17C, scale 0x188, texture list 0x194) and two Bodies (0x280,
 * 0x2B0: the sprite's position and scale follow them); another places two Bodies at 0x220 / 0x250 from a table;
 * a third draws a model in one of several motions. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"

#define SPRITE         0x17C /* NJS_SPRITE */
#define SPRITE_SX      0x188
#define SPRITE_SY      0x18C
#define SPRITE_TLIST   0x194
#define TEXLIST_160    0x160
#define BODY_POS       0x280 /* Body: the sprite's position (x, y) and priority (z) */
#define BODY_SCALE     0x2B0 /* Body: the sprite's scale (x, y) */
#define BODY_A         0x220
#define BODY_B         0x250
#define ICON_CELLS     0x8C0734D4u /* the texture cell of each icon */
#define ICON_PLACES    0x8C073C3Cu /* 6 floats per entry: x, y, z, dy, dx2, dy2 */
#define DAMPING_HALF   F32(0.5f)
#define DAMPING_PLACE  0x3F47AE14u /* 0.78f */

/* func_8C022618: a Body's position = (x, y, z) (still translated) */
static void body_set_position(gaddr body, f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL1(0x8C022618u, body); }
/* func_8C022904: draws a model in a motion at *frame (still translated; a thunk, like the motion drawers of
 * core/motion.c) */
static void model_draw_motion(gaddr object, gaddr motion, gaddr frame, uint32_t loop) {
  CALL4(0x8C022904u, object, motion, frame, loop);
}

/* the damping of a Body (vec3_fill at 0x24, as func_8C022624) */
static void body_set_damping(gaddr body, f32 d) { vec3_fill(body + 0x24, d); }

/* was func_8C027A54: the sprite's Bodies at rest: position (640, 0, -1), scale 0, both damped by half */
void icon_bodies_init(gaddr self) {
  body_set_position(self + BODY_POS, F32(640.0f), F32(0.0f), F32(-1.0f));
  body_set_position(self + BODY_SCALE, F32(0.0f), F32(0.0f), F32(0.0f));
  body_set_damping(self + BODY_POS, DAMPING_HALF);
  body_set_damping(self + BODY_SCALE, DAMPING_HALF);
}
HW_EXPORT(8c027a54, icon_bodies_init(ARG0))

/* was func_8C027B18: draws icon n where its Bodies are (priority: the position Body's z) */
void icon_draw(gaddr self, int32_t n) {
  F32_AT(self + SPRITE) = field_get_f32(self + BODY_POS, 0);
  F32_AT(self + SPRITE + 4) = field_get_f32(self + BODY_POS, 4);
  F32_AT(self + SPRITE_SX) = field_get_f32(self + BODY_SCALE, 0);
  F32_AT(self + SPRITE_SY) = field_get_f32(self + BODY_SCALE, 4);
  f32 priority = field_get_f32(self + BODY_POS, 8);
  njDrawSprite2D(self + SPRITE, (int32_t)U32_AT(ICON_CELLS + 4u * (uint32_t)n), priority, 0x20);
}
HW_EXPORT(8c027b18, icon_draw(ARG0, (int32_t)ARG1))

/* was func_8C027BD0: places the two Bodies at 0x220 / 0x250 from entry n of the table: (x, y + dy, z) and
 * (x + dx2, y + dy + dy2, z), damping 0.78 */
void icon_place(gaddr self, int32_t n) {
  const f32 *e = &F32_AT(ICON_PLACES + 24u * (uint32_t)n);
  body_set_position(self + BODY_A, e[0], fadd(e[1], e[3]), e[2]);
  body_set_position(self + BODY_B, fadd(e[4], e[0]), fadd(fadd(e[1], e[3]), e[5]), e[2]);
  body_set_damping(self + BODY_A, F32_BITS(DAMPING_PLACE));
  body_set_damping(self + BODY_B, F32_BITS(DAMPING_PLACE));
}
HW_EXPORT(8c027bd0, icon_place(ARG0, (int32_t)ARG1))

/* was func_8C0280BC: the sprite at the origin, scale 1, with the texture list at 0x160: draws cell 0x19 far back
 * (priority -1000), then cell 0x1A in front (-50) */
void backdrop_draw(gaddr self) {
  U32_AT(self + SPRITE_TLIST) = U32_AT(self + TEXLIST_160);
  F32_AT(self + SPRITE) = F32(0.0f);
  F32_AT(self + SPRITE + 4) = F32(0.0f);
  F32_AT(self + SPRITE_SX) = F32(1.0f);
  F32_AT(self + SPRITE_SY) = F32(1.0f);
  njDrawSprite2D(self + SPRITE, 0x19, F32(-1000.0f), 0);
  njDrawSprite2D(self + SPRITE, 0x1A, F32(-50.0f), 0x20);
}
HW_EXPORT(8c0280bc, backdrop_draw(ARG0))

/* was func_8C028802: draws the model of an object with its texture list (0x1B4) at its position (0x1F0), turned by
 * its angle (0x1EC) about y: model and motion number `current` (0x5C) of its tables (0x1B8, 0x1C8; the motion
 * table's entries hold 8-byte motions, 0x1D8 picks one), at the frame at 0x1E0, loop flag 0x1E8 */
void motion_model_draw(gaddr self) {
  njSetTexture(U32_AT(self + 0x1B4));
  matrix_push();
  matrix_translate_v(method_self(self + 0x1F0));
  matrix_rotate_y(F32_AT(self + 0x1EC));
  uint32_t current = U32_AT(self + 0x5C);
  gaddr motion = U32_AT(self + 0x1C8 + 4 * current) + 8 * U32_AT(self + 0x1D8);
  model_draw_motion(U32_AT(self + 0x1B8 + 4 * current), motion, self + 0x1E0, U32_AT(self + 0x1E8));
  matrix_pop();
}
HW_EXPORT(8c028802, motion_model_draw(ARG0))
