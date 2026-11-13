/* Pen Pen port, S17 (b02): small functions the compiler left in several units of 0x8C018000-0x8C01F800: copies
 * of the text scale setter, the constant material of four floats (an
 * NJS_ARGB on the stack copied from a default first), and the destructor wrappers that run a scene's destructor
 * body and give `this` back. */
#include "hw.h"
#include "sdk.h"
#include "../core/sprite_text.h"
#include "../core/inline.h"
#include "inline_b02.h"

/* ---- the text scale ---- */

/* was func_8C018DB0 and func_8C01F5C8: copies of text_set_scale (func_8C022A58) */
HW_EXPORT(8c018db0, text_set_scale(FARG0, FARG1, FARG2))
HW_EXPORT(8c01f5c8, text_set_scale(FARG0, FARG1, FARG2))

/* ---- the constant material ---- */

typedef struct NjArgb {
  f32 a, r, g, b;
} NjArgb;
HW_SIZE(NjArgb, 16);

#define MATERIAL_DEFAULT_B072 0x8C08C7B0u /* the default NJS_ARGB of func_8C01B072's unit */
#define MATERIAL_DEFAULT_E258 0x8C08CD3Cu /* the default NJS_ARGB of func_8C01E258's unit */

/* njSetConstantMaterial of {a, r, g, b}: a local NJS_ARGB first copied from the unit's default, then filled */
static void material_set_from(gaddr defaults, f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(20); /* the original's local, 16 bytes below its saved PR */
  memmove(hw_ptr(argb), hw_ptr(defaults), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(20);
}

/* was func_8C01B072: the constant material (r, g, b, a) */
void material_set_b072(f32 r, f32 g, f32 b, f32 a) { material_set_from(MATERIAL_DEFAULT_B072, r, g, b, a); }
HW_EXPORT(8c01b072, material_set_b072(FARG0, FARG1, FARG2, FARG3))

/* was func_8C01E258: the same in another unit */
void material_set_e258(f32 r, f32 g, f32 b, f32 a) { material_set_from(MATERIAL_DEFAULT_E258, r, g, b, a); }
HW_EXPORT(8c01e258, material_set_e258(FARG0, FARG1, FARG2, FARG3))

/* ---- destructor wrappers: the destructor body, then `this` back ---- */

/* func_8C01A404 (core/texture.c, no header): mode 0's scene's destructor body */
static void texture_release_member(gaddr scene) { CALL1(0x8C01A404u, scene); }
/* func_8C01A8F4 (core/accessors.c, no header): the race set-up's destructor body (it takes no argument; the
 * original passes `this` in r4) */
static void race_setup_clear(gaddr setup) { CALL1(0x8C01A8F4u, setup); }
/* func_8C01B790 (game/title.c, static there): the title scene's destructor body */
static void title_release(gaddr scene) { CALL1(0x8C01B790u, scene); }
/* func_8C01D4E0 (game/good_night.c, no header): the good-night scene's destructor body */
static void good_night_destroy(gaddr scene) { CALL1(0x8C01D4E0u, scene); }

/* was func_8C01A436: mode 0's scene destroyed (its texture list released); returns it */
gaddr mode0_scene_destruct(gaddr scene) {
  texture_release_member(scene);
  return scene;
}
HW_EXPORT(8c01a436, RET(mode0_scene_destruct(ARG0)))

/* was func_8C01A920: the race set-up destroyed; returns it */
gaddr race_setup_destruct(gaddr setup) {
  race_setup_clear(setup);
  return setup;
}
HW_EXPORT(8c01a920, RET(race_setup_destruct(ARG0)))

/* was func_8C01B812: the title scene destroyed (without the free); returns it */
gaddr title_destruct(gaddr scene) {
  title_release(scene);
  return scene;
}
HW_EXPORT(8c01b812, RET(title_destruct(ARG0)))

/* was func_8C01D53E: the good-night scene destroyed; returns it */
gaddr good_night_destruct(gaddr scene) {
  good_night_destroy(scene);
  return scene;
}
HW_EXPORT(8c01d53e, RET(good_night_destruct(ARG0)))

/* was func_8C01DE20: a class whose destructor body is empty (func_8C01DDF8, a copy of method_empty); returns
 * `this` */
gaddr empty_destruct(gaddr obj) {
  method_empty();
  return obj;
}
HW_EXPORT(8c01de20, RET(empty_destruct(ARG0)))
