/* Pen Pen port, S13: methods of the set-up menu that draw sprites moved by Bodies, and a menu's model drawer
 * (game/menu_sprites.c). S14: the classes are SetupMenu and MenuModel (game/menu.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "menu.h"

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

/* the damping of a Body (vec3_fill, as func_8C022624) */
static void body_set_damping(gaddr body, f32 d) { vec3_fill(member_addr(body, Body, damping), d); }

/* was func_8C027A54: the sprite's Bodies at rest: position (640, 0, -1), scale 0, both damped by half */
void icon_bodies_init(gaddr self) {
  body_set_position(member_addr(self, SetupMenu, icon_position), F32(640.0f), F32(0.0f), F32(-1.0f));
  body_set_position(member_addr(self, SetupMenu, icon_scale), F32(0.0f), F32(0.0f), F32(0.0f));
  body_set_damping(member_addr(self, SetupMenu, icon_position), DAMPING_HALF);
  body_set_damping(member_addr(self, SetupMenu, icon_scale), DAMPING_HALF);
}
HW_EXPORT(8c027a54, icon_bodies_init(ARG0))

/* was func_8C027B18: draws icon n where its Bodies are (priority: the position Body's z) */
void icon_draw(gaddr self, int32_t n) {
  SetupMenu *m = setup_menu_at(self);
  gaddr position = member_addr(self, SetupMenu, icon_position), scale = member_addr(self, SetupMenu, icon_scale);
  m->sprite.p.x = field_get_f32(position, offsetof(Body, position.x));
  m->sprite.p.y = field_get_f32(position, offsetof(Body, position.y));
  m->sprite.sx = field_get_f32(scale, offsetof(Body, position.x));
  m->sprite.sy = field_get_f32(scale, offsetof(Body, position.y));
  f32 priority = field_get_f32(position, offsetof(Body, position.z));
  njDrawSprite2D(member_addr(self, SetupMenu, sprite), (int32_t)U32_AT(ICON_CELLS + 4u * (uint32_t)n), priority, 0x20);
}
HW_EXPORT(8c027b18, icon_draw(ARG0, (int32_t)ARG1))

/* was func_8C027BD0: places the two Bodies place_a / place_b from entry n of the table: (x, y + dy, z) and
 * (x + dx2, y + dy + dy2, z), damping 0.78 */
void icon_place(gaddr self, int32_t n) {
  const f32 *e = &F32_AT(ICON_PLACES + 24u * (uint32_t)n);
  body_set_position(member_addr(self, SetupMenu, place_a), e[0], fadd(e[1], e[3]), e[2]);
  body_set_position(member_addr(self, SetupMenu, place_b), fadd(e[4], e[0]), fadd(fadd(e[1], e[3]), e[5]), e[2]);
  body_set_damping(member_addr(self, SetupMenu, place_a), F32_BITS(DAMPING_PLACE));
  body_set_damping(member_addr(self, SetupMenu, place_b), F32_BITS(DAMPING_PLACE));
}
HW_EXPORT(8c027bd0, icon_place(ARG0, (int32_t)ARG1))

/* was func_8C0280BC: the sprite at the origin, scale 1, with the menu's texture list: draws cell 0x19 far back
 * (priority -1000), then cell 0x1A in front (-50) */
void backdrop_draw(gaddr self) {
  SetupMenu *m = setup_menu_at(self);
  m->sprite.tlist = m->texlist;
  m->sprite.p.x = F32(0.0f);
  m->sprite.p.y = F32(0.0f);
  m->sprite.sx = F32(1.0f);
  m->sprite.sy = F32(1.0f);
  njDrawSprite2D(member_addr(self, SetupMenu, sprite), 0x19, F32(-1000.0f), 0);
  njDrawSprite2D(member_addr(self, SetupMenu, sprite), 0x1A, F32(-50.0f), 0x20);
}
HW_EXPORT(8c0280bc, backdrop_draw(ARG0))

/* was func_8C028802: draws the model of an object with its texture list at its position, turned by its angle about
 * y: model and motion table number `current` (the motion table's entries hold 8-byte motions, `motion` picks one),
 * at its frame, with its loop flag */
void motion_model_draw(gaddr self) {
  const MenuModel *m = (const MenuModel *)hw_ptr(self);
  njSetTexture(m->texlist);
  matrix_push();
  matrix_translate_v(method_self(member_addr(self, MenuModel, position)));
  matrix_rotate_y(m->angle);
  uint32_t current = m->current;
  gaddr motion = m->motion_tables[(int32_t)current] + 8 * m->motion;
  model_draw_motion(m->models[(int32_t)current], motion, member_addr(self, MenuModel, frame), m->loop);
  matrix_pop();
}
HW_EXPORT(8c028802, motion_model_draw(ARG0))
