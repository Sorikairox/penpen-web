/* Pen Pen port, S17 (r2): the title scene's logo drawn (game/title_draw.c): the logo's shape motion, the sprites under
 * it, the back model's layers, the star, and (until the logo stands) a cover sprite. The scene is game/title.h's
 * TitleScene; title_draw_logo is reached through a tail call of the scene's draw (func_8C01BB0C, still translated). */
#include "hw.h"
#include "sdk.h"
#include "title.h"
#include "../core/ninja.h"
#include "../core/inline.h"

/* the sprites' texture animation table (NJS_TEXANIM of the title's sprites) */
#define TITLE_SPRITE_ANIM 0x8C08C824u
/* the cover sprite drawn over everything until the logo stands, and the flag of its one-time set-up */
#define TITLE_COVER_SPRITE 0x8C08C970u
#define g_title_cover_sprite GUEST_VAR(NjSprite, TITLE_COVER_SPRITE)
#define g_title_cover_ready GUEST_VAR(int32_t, 0x8C1BAAB4u) /* 0 until the first draw gives it the texlist */

/* the logo stands from phase 2 on (title_logo) */
#define TITLE_PHASE_STANDING 2

/* sprite attributes and priorities as the original passes them */
#define SPRITE_ATTR_BLEND 0x22
#define SPRITE_ATTR_PLAIN 0x20
#define SPRITE_ATTR_COVER 2
#define SPRITE_PRI F32(-1.0f)
#define COVER_PRI F32_BITS(0xC77FFF00u) /* -65535 */

/* func_8C01B072: the constant material colour (r, g, b, a: it keeps them as Ninja's {a, r, g, b}) */
static void material_color(f32 r, f32 g, f32 b, f32 a) {
  FARGS4(r, g, b, a);
  CALL0(0x8C01B072u);
}
/* func_8C0111E4 (core/motion.c, not declared in a header): a model in its motion at a frame kept in range
 * (nj_MotionL3b) */
static void motion_draw(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  CALL4(0x8C0111E4u, object, motion, frame, loop);
}

/* the star's place: the mover's position, turned by the three angles (each read through the float getter) */
static void title_star_transform(gaddr scene) {
  matrix_translate_v(method_self(member_addr(scene, TitleScene, mover)));
  matrix_rotate_x(field_get_f32(member_addr(scene, TitleScene, angle[0]), (uint32_t)offsetof(Body1, value)));
  matrix_rotate_y(field_get_f32(member_addr(scene, TitleScene, angle[1]), (uint32_t)offsetof(Body1, value)));
  matrix_rotate_z(field_get_f32(member_addr(scene, TitleScene, angle[2]), (uint32_t)offsetof(Body1, value)));
}

/* one layer of the back model: scaled in x and y by its scroll, at its own frame */
static void title_draw_layer(gaddr scene, f32 scale, gaddr frame) {
  TitleScene *t = title_at(scene);
  matrix_push();
  matrix_scale(scale, scale, F32(1.0f));
  motion_draw(t->model[2], t->motion[2], frame, 1);
  matrix_pop();
}

/* was func_8C01C668: the title's draw. The cover sprite gets the title's texlist the first time. The logo's shape
 * motion at the star's place; then, with the 3D flags 0x10 pushed: three sprites in a row (11, 12, 13, from x = 180,
 * 128 apart, y = 382) at the first layer's alpha; three more (8, 10, 9 at y = 420) blended at that alpha until the
 * logo stands, then plain at alpha 0; the first layer; the star (alpha 1 once the logo stands); the second layer at
 * its alpha once the logo stands. The flags popped, the cover sprite until the logo stands. */
void title_draw_logo(gaddr scene) {
  TitleScene *t = title_at(scene);
  if (g_title_cover_ready == 0) {
    g_title_cover_ready++;
    g_title_cover_sprite.tlist = t->texlist;
  }
  int32_t standing = t->logo_phase >= TITLE_PHASE_STANDING;

  matrix_push();
  title_star_transform(scene);
  nj_ShapeMotionL1(t->model[0], t->motion[0], t->logo_shape, t->frame[0]);
  matrix_pop();

  control3d_push();
  control3d_set(0x10);
  material_color(F32(1.0f), F32(1.0f), F32(1.0f), t->blend_a);

  gaddr sp = hw_stack_alloc(sizeof(NjSprite));
  NjSprite *s = (NjSprite *)hw_ptr(sp);
  s->tlist = t->texlist;
  s->tanim = TITLE_SPRITE_ANIM;
  s->ang = 0;
  s->sx = F32(1.0f);
  s->sy = F32(1.0f);
  s->p.x = F32(180.0f);
  s->p.y = F32(382.0f);
  s->p.z = F32(0.0f);
  njDrawSprite2D(sp, 11, SPRITE_PRI, SPRITE_ATTR_BLEND);
  s->p.x = fadd(s->p.x, F32(128.0f));
  njDrawSprite2D(sp, 12, SPRITE_PRI, SPRITE_ATTR_BLEND);
  s->p.x = fadd(s->p.x, F32(128.0f));
  njDrawSprite2D(sp, 13, SPRITE_PRI, SPRITE_ATTR_BLEND);

  uint32_t attr;
  f32 alpha;
  if (standing) {
    attr = SPRITE_ATTR_PLAIN;
    alpha = F32(0.0f);
  } else {
    attr = SPRITE_ATTR_BLEND;
    alpha = t->blend_a;
  }
  material_color(F32(1.0f), F32(1.0f), F32(1.0f), alpha);
  s->tanim = TITLE_SPRITE_ANIM;
  s->p.x = F32(248.0f);
  s->p.y = F32(420.0f);
  njDrawSprite2D(sp, 8, SPRITE_PRI, attr);
  s->p.x = F32(314.0f);
  s->p.y = F32(420.0f);
  njDrawSprite2D(sp, 10, SPRITE_PRI, attr);
  s->p.x = F32(362.0f);
  s->p.y = F32(420.0f);
  njDrawSprite2D(sp, 9, SPRITE_PRI, attr);
  hw_stack_free(sizeof(NjSprite));

  material_color(F32(1.0f), F32(1.0f), F32(1.0f), t->blend_a);
  title_draw_layer(scene, t->scroll_a, member_addr(scene, TitleScene, frame[1]));

  material_color(F32(1.0f), F32(1.0f), F32(1.0f), standing ? F32(1.0f) : t->blend_a);
  matrix_push();
  title_star_transform(scene);
  nj_ModelL1(t->model[1]);
  matrix_pop();

  if (standing) {
    material_color(F32(1.0f), F32(1.0f), F32(1.0f), t->blend_b);
    title_draw_layer(scene, t->scroll_b, member_addr(scene, TitleScene, frame[2]));
  }
  control3d_pop();
  if (!standing)
    njDrawSprite2D(TITLE_COVER_SPRITE, 0, COVER_PRI, SPRITE_ATTR_COVER);
}
HW_EXPORT(8c01c668, title_draw_logo(ARG0))
