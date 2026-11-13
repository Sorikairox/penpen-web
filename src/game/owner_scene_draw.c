/* Pen Pen port, S20 (W3): the owner scene's draw method (game/owner_scene.h; vtable 0x8C23DA8C, TaskVtbl.draw),
 * reached only through the vtable, and the small static helpers of its unit that only it calls.
 *
 * The draw: the light of view 0, the result screen's three background sprites, the penguins (each placed by its
 * character's table entry and drawn by its motion player), then the course owner through the course's camera; while
 * the scene is shown (state 1) a tinted panel of three sprites and the course's two caption lines in the game's
 * language on top. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "../core/sprite_text.h"
#include "save.h"
#include "motion.h"
#include "owner_scene.h"

/* ---- guest data ---- */

/* an NJS_ARGB, alpha first */
typedef struct OwnerArgb { f32 a, r, g, b; } OwnerArgb;
HW_SIZE(OwnerArgb, 16);
#define OWNER_MATERIAL_DEFAULT 0x8C0963A4u /* the NJS_ARGB the material helper's local starts as */

/* where a penguin stands in front of the owner, per character and penguin (guest data) */
typedef struct OwnerPenguinPlace {
  Vec3 position;   /* 0x00 */
  Vec3 angle;      /* 0x0C: about x, y, z (Ninja units, as floats) */
} OwnerPenguinPlace;
HW_SIZE(OwnerPenguinPlace, 0x18);
#define g_owner_penguin_places ((const OwnerPenguinPlace (*)[OWNER_PENGUINS])hw_ptr(0x8C07FDD0u))

/* the penguins' NJS_VIEW, then one per course for the owner (the cameras owner_scene_init places the view from) */
#define OWNER_PENGUIN_VIEW 0x8C096574u
#define OWNER_VIEWS   0x8C0965B4u /* an NJS_VIEW (0x40 bytes) per course */
#define NJS_VIEW_SIZE 0x40u

/* the caption of a course: two lines of text (guest strings), per language and course */
#define OWNER_COURSES 4
typedef struct OwnerCaption {
  gaddr lines[2];           /* 0x00 */
  uint8_t unknown08[0x10 - 0x08];
} OwnerCaption;
HW_SIZE(OwnerCaption, 0x10);
#define g_owner_captions ((const OwnerCaption (*)[OWNER_COURSES])hw_ptr(0x8C0963B4u))

#define g_shared_texlist GUEST_VAR(gaddr, 0x8C1B7E40u) /* the parts container's first entry (game/model.c's word) */

#define OWNER_SHOWN 1                 /* OwnerScene.state while the owner is shown */
#define OWNER_LIGHT_VIEW 0
#define OWNER_CONTROL3D 4             /* control3d_set flags of the scene, and of its panel */
#define OWNER_PANEL_CONTROL3D 0x10
#define OWNER_ATTR_AND 0xFFFF9FFFu    /* njSetConstantAttr masks */
#define OWNER_ATTR_OR 0x2000u
#define OWNER_BACK_ATTR 0x20          /* njDrawSprite2D attributes */
#define OWNER_PANEL_ATTR 0x22
#define CAPTION_X1 0x3C
#define CAPTION_Y1 0x172
#define CAPTION_X2 0x2C
#define CAPTION_Y2 0x192

/* ---- calls ---- */

/* func_8C02F79E (a copy of text_style_reset, core/sprite_text.c): the text style back to its default after the
 * panel */
static void owner_panel_draw_rest(void) { text_style_reset(); }

/* ---- static helpers of the unit ---- */

/* was func_8C02F61E: njSetConstantMaterial of {a, r, g, b} = the four floats (r, g, b, a): a local NJS_ARGB, copied
 * from the default one (the SHC block copy) and then filled */
static void owner_material_set(f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(sizeof(OwnerArgb));
  memmove(hw_ptr(argb), hw_ptr(OWNER_MATERIAL_DEFAULT), sizeof(OwnerArgb));
  OwnerArgb *c = (OwnerArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(sizeof(OwnerArgb));
}

/* sprite 'n' of the scene's sprite at (x, y), scale 1 */
static void owner_sprite_draw(OwnerScene *s, gaddr scene, f32 x, f32 y, int32_t n, f32 pri, uint32_t attr) {
  s->sprite.p.x = x;
  s->sprite.p.y = y;
  s->sprite.sx = F32(1.0f);
  s->sprite.sy = F32(1.0f);
  njDrawSprite2D(member_addr(scene, OwnerScene, sprite), n, pri, attr);
}

/* ---- the draw ---- */

/* was func_8C02FE38 (with the gap helpers func_8C02F688 = view_set_light_intensity, func_8C02F6A0 =
 * view_set_light_color, func_8C02F6B8 = view_set_light_dir, func_8C02F648 = a thunk to njSetConstantAttr,
 * func_8C02F61E = owner_material_set): see the top of the file */
void owner_scene_draw(gaddr scene) {
  OwnerScene *s = owner_scene_at(scene);

  /* the light of view 0 */
  view_set_light_intensity(F32(1.0f), F32(1.0f), F32_BITS(0x3ECCCCCCu) /* 0.4 */, OWNER_LIGHT_VIEW);
  view_set_light_dir(F32_BITS(0xBE7551D6u) /* -0.23957 */, F32_BITS(0xBED24617u) /* -0.41069 */,
                     F32_BITS(0xBF613682u) /* -0.87974 */, OWNER_LIGHT_VIEW);
  view_set_light_color(F32(1.0f), F32(1.0f), F32(1.0f), OWNER_LIGHT_VIEW);
  control3d_set(OWNER_CONTROL3D);
  nj_set_constant_attr(OWNER_ATTR_AND, OWNER_ATTR_OR);

  /* the background: sprites 0-2 of the result screen at the origin */
  owner_sprite_draw(s, scene, F32(0.0f), F32(0.0f), 0, F32(-350.0f), OWNER_BACK_ATTR);
  njDrawSprite2D(member_addr(scene, OwnerScene, sprite), 1, F32(-340.0f), OWNER_BACK_ATTR);
  njDrawSprite2D(member_addr(scene, OwnerScene, sprite), 2, F32_BITS(0xBFFFFFFFu) /* -1.9999999 */, OWNER_BACK_ATTR);

  /* the penguins */
  njSetTexture(g_shared_texlist);
  for (int32_t i = 0; i < s->penguin_count; i++) {
    const OwnerPenguinPlace *place = &g_owner_penguin_places[s->penguin_character[i]][i];
    nj_set_view(OWNER_PENGUIN_VIEW);
    matrix_push();
    matrix_translate(place->position.x, place->position.y, place->position.z);
    matrix_translate(F32(0.0f), F32(-0.5f), F32(0.0f));
    matrix_rotate_xyz(place->angle.x, place->angle.y, place->angle.z);
    motion_player_draw(s->penguins[i]);
    matrix_pop();
  }

  /* the owner, through the course's camera */
  nj_set_view(OWNER_VIEWS + (uint32_t)s->course * NJS_VIEW_SIZE);
  owner_scene_draw_owner(scene);
  if (s->state != OWNER_SHOWN)
    return;

  /* the panel, tinted by the course (no tint for another course), and the course's caption */
  control3d_push();
  control3d_set(OWNER_PANEL_CONTROL3D);
  switch (s->course) {
  case 0:
    owner_material_set(F32(1.0f), F32(0.5f), F32(0.7f), F32(0.7f));
    break;
  case 1:
    owner_material_set(F32_BITS(0x3DCCCCCCu) /* 0.1 */, F32_BITS(0x3F199999u) /* 0.6 */,
                       F32_BITS(0x3E999999u) /* 0.3 */, F32(0.7f));
    break;
  case 2:
    owner_material_set(F32(0.0f), F32_BITS(0x3F199999u) /* 0.6 */, F32(1.0f), F32(0.7f));
    break;
  case 3:
    owner_material_set(F32_BITS(0x3E999999u) /* 0.3 */, F32_BITS(0x3E4CCCCCu) /* 0.2 */,
                       F32_BITS(0x3F4CCCCCu) /* 0.8 */, F32(0.7f));
    break;
  }
  owner_sprite_draw(s, scene, F32(24.0f), F32(370.0f), 3, F32(-1.0f), OWNER_PANEL_ATTR);
  owner_sprite_draw(s, scene, F32(72.0f), F32(370.0f), 4, F32(-1.0f), OWNER_PANEL_ATTR);
  owner_sprite_draw(s, scene, F32(440.0f), F32(362.0f), 5, F32(-1.0f), OWNER_PANEL_ATTR);
  owner_panel_draw_rest();
  const OwnerCaption *caption = &g_owner_captions[game_language()][s->course];
  text_draw(CAPTION_X1, CAPTION_Y1, caption->lines[0]);
  text_draw(CAPTION_X2, CAPTION_Y2, caption->lines[1]);
  control3d_pop();
}
HW_EXPORT(8c02fe38, owner_scene_draw(ARG0))
