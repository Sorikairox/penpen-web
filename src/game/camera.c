/* Pen Pen port, S13 (batch b): the camera object's helpers and the current view's lights (0x8C0407B8-0x8C042418).
 * Field names are from what these functions do with them. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "game.h"

/* njSetConstantAttr (Ninja): the constant attribute words (and-mask, or-mask) */
static inline void njSetConstantAttr(uint32_t and_attr, uint32_t or_attr) { CALL2(0x8C0A9672u, and_attr, or_attr); }

/* func_8C0628E8 (C runtime, float): the angle of (y, x) in radians, atan2f by its use (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

#define g_control3d_top GUEST_VAR(gaddr, 0x8C08A8A0u) /* -> the current flags word (core/ninja.c's stack) */

/* was func_8C0407B8: the address of the member at 0xB0 */
HW_EXPORT(8c0407b8, RET(field_addr(ARG0, 0xB0)))

/* was func_8C0407CE: the current view's heading: the angle of its vector (0x70, 0x78) in whole Ninja angle units
 * (0x10000 a turn), as a float */
f32 view_heading(void) {
  gaddr view = g_cur_view;
  f32 radians = rt_atan2f(F32_AT(view + 0x70), F32_AT(view + 0x78));
  f32 units = fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u)); /* 2 pi (6.2831855) */
  return f32_from_int(f32_to_int(units));
}
HW_EXPORT(8c0407ce, RETF(view_heading()))

/* was func_8C040860: add flags to the current njControl3D flags and send them */
void control3d_add(uint32_t flags) {
  gaddr top = g_control3d_top;
  U32_AT(top) |= flags;
  njControl3D(U32_AT(top));
}
HW_EXPORT(8c040860, control3d_add(ARG0))

/* was func_8C040874: a jump to njSetConstantAttr */
HW_EXPORT(8c040874, njSetConstantAttr(ARG0, ARG1))

/* was func_8C040886 / func_8C04089A / func_8C0408AE: the object's Vec3 at 0x08 / 0x20 / 0x2C = v (block copy) */
HW_EXPORT(8c040886, vec3_copy_bytes(ARG0 + 0x08, ARG1))
HW_EXPORT(8c04089a, vec3_copy_bytes(ARG0 + 0x20, ARG1))
HW_EXPORT(8c0408ae, vec3_copy_bytes(ARG0 + 0x2C, ARG1))

/* was func_8C04094E: the word of view i (-1 = the current view) in the table at 0xAC */
uint32_t camera_view_word(gaddr obj, int32_t view) {
  gaddr table = obj + 0xAC;
  int32_t i = view == -1 ? view_current_index() : view;
  return U32_AT(table + 4u * (uint32_t)i);
}
HW_EXPORT(8c04094e, RET(camera_view_word(ARG0, (int32_t)ARG1)))

/* ---- the current view's lights ---- */

/* was func_8C042388: the current view's light intensities = v */
void view_light_intensity_set(gaddr v) {
  F32_AT(g_cur_view + 0x100) = F32_AT(v + 0);
  F32_AT(g_cur_view + 0x104) = F32_AT(v + 4);
  F32_AT(g_cur_view + 0x108) = F32_AT(v + 8);
}
HW_EXPORT(8c042388, view_light_intensity_set(ARG0))

/* was func_8C0423A8: the current view's light colour = v */
void view_light_color_set(gaddr v) {
  F32_AT(g_cur_view + 0x110) = F32_AT(v + 0);
  F32_AT(g_cur_view + 0x114) = F32_AT(v + 4);
  F32_AT(g_cur_view + 0x118) = F32_AT(v + 8);
}
HW_EXPORT(8c0423a8, view_light_color_set(ARG0))

/* was func_8C042400: the current view's light direction = v (block copy) */
void view_light_dir_set(gaddr v) { vec3_copy_bytes(g_cur_view + 0xF4, v); }
HW_EXPORT(8c042400, view_light_dir_set(ARG0))

/* ---- a view's fog and lights over the course ---- */


/* func_8C04241E: fog on for view i (the counterpart of view_fog_off; still translated) */
static void view_fog_on(int32_t index) { CALL1(0x8C04241Eu, index); }

/* was func_8C042E28: follow the course's fog track: the first 0x1C-byte record past the object's position (0x38)
 * gives the mode (0x8C); in modes 0 and 1 the fog is on where the ground's attribute has bit 6 or 7, in modes 2
 * and 3 while the record's height (0x0C) is above the object's; the result goes to 0x188 and the view (word 0) */
void fog_track_update(gaddr obj) {
  gaddr rec = U32_AT(obj + 0x90);
  while ((int32_t)U32_AT(obj + 0x38) >= *(int16_t *)hw_ptr(rec)) rec += 0x1C;
  U32_AT(obj + 0x8C) = (uint32_t)(int32_t)*(int16_t *)hw_ptr(rec + 2);
  switch ((int32_t)U32_AT(obj + 0x8C)) {
  case 0:
  case 1: {
    int32_t cell = (int32_t)U32_AT(obj + 0x38);
    uint16_t attribute = (uint16_t)field_table16_get(U32_AT(panel_get() + 0x38), 0xB4, cell);
    if (attribute & 0xC0) {
      U32_AT(obj + 0x188) = 1;
      view_fog_on((int32_t)U32_AT(obj));
    } else {
      U32_AT(obj + 0x188) = 0;
      view_fog_off((int32_t)U32_AT(obj));
    }
    break;
  }
  case 2:
  case 3:
    if (fgt(F32_AT(rec + 0x0C), F32_AT(obj + 0x0C))) {
      U32_AT(obj + 0x188) = 1;
      view_fog_on((int32_t)U32_AT(obj));
    } else {
      U32_AT(obj + 0x188) = 0;
      view_fog_off((int32_t)U32_AT(obj));
    }
    break;
  default:
    break;
  }
}
HW_EXPORT(8c042e28, fog_track_update(ARG0))

/* func_8C0424C2: v = (x, y, z) (still translated) */
static void vec3_put(gaddr v, f32 x, f32 y, f32 z) {
  FARGS3(x, y, z);
  CALL1(0x8C0424C2u, v);
}

/* was func_8C042EF2: set the view's (word 0) lights, then the object's three Bodies (0x98, 0xC8, 0xF8) at the
 * data's three points (from 4), with damping 0.5 */
void light_rig_init(gaddr obj, gaddr data) {
  int32_t view = (int32_t)U32_AT(obj);
  view_set_light_intensity(F32(1.0f), F32(0.75f), F32_BITS(0x3ECCCCCCu), view); /* 0.39999998 */
  view_set_light_color(F32(1.0f), F32(1.0f), F32(1.0f), (int32_t)U32_AT(obj));
  view_set_light_dir(F32(0.0f), F32_BITS(0xBF333333u), F32_BITS(0xBF333333u), (int32_t)U32_AT(obj)); /* -0.7 */
  U32_AT(obj + 0x128) = data;
  for (uint32_t k = 0; k < 3; k++) {
    gaddr p = U32_AT(obj + 0x128) + 4 + 12 * k;
    vec3_put(obj + 0x98 + 0x30 * k, F32_AT(p), F32_AT(p + 4), F32_AT(p + 8));
  }
  vec3_fill(obj + 0x98 + 0x24, F32(0.5f));
  vec3_fill(obj + 0xC8 + 0x24, F32(0.5f));
  vec3_fill(obj + 0xF8 + 0x24, F32(0.5f));
}
HW_EXPORT(8c042ef2, light_rig_init(ARG0, ARG1))

/* was func_8C042FBE: move the rig's three Bodies toward the points of the track record (0x28 bytes each from the
 * data at 0x128) past the object's position (0x38): rates 0.02, 0.015, 0.02, then one step each (the targets are
 * made with the Vec3 setter on the guest stack) */
void light_rig_update(gaddr obj) {
  gaddr rec = U32_AT(obj + 0x128);
  while ((int32_t)U32_AT(obj + 0x38) >= *(int16_t *)hw_ptr(rec)) rec += 0x28;
  const f32 rate = F32_BITS(0x3CA3D70Au); /* 0.02 */
  gaddr t = hw_stack_alloc(3 * sizeof(Vec3));
  gaddr t0 = t, t1 = t + 12, t2 = t + 24;
  vec3_set(F32_AT(rec + 0x04), F32_AT(rec + 0x08), F32_AT(rec + 0x0C), t2);
  body_pull(rate, obj + 0x98, t2);
  vec3_set(F32_AT(rec + 0x10), F32_AT(rec + 0x14), F32_AT(rec + 0x18), t1);
  body_pull(F32_BITS(0x3C75C28Fu), obj + 0xC8, t1); /* 0.015 */
  vec3_set(F32_AT(rec + 0x1C), F32_AT(rec + 0x20), F32_AT(rec + 0x24), t0);
  body_pull(rate, obj + 0xF8, t0);
  hw_stack_free(3 * sizeof(Vec3));
  body_step(obj + 0x98);
  body_step(obj + 0xC8);
  body_step(obj + 0xF8);
}
HW_EXPORT(8c042fbe, light_rig_update(ARG0))

/* njColorBlendingMode (Ninja): the source (0) / destination (1) blend mode */
static inline void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* the NJS_SPRITE the draw below hands to Ninja (on the guest stack) */
typedef struct NjSprite {
  Vec3 p;        /* 0x00 */
  f32 sx, sy;    /* 0x0C */
  int32_t ang;   /* 0x14 */
  gaddr tlist;   /* 0x18: NJS_TEXLIST */
  gaddr tanim;   /* 0x1C: NJS_TEXANIM */
} NjSprite;
HW_SIZE(NjSprite, 0x20);

/* was func_8C04381A: draw the object's sparkles (count 0x150, 0x30-byte objects from 0x12C, a texture animation
 * number each from 0x130) as 3D sprites of the shared texture, additive when the mode (0x154) is 2 */
void sparkles_draw(gaddr obj) {
  gaddr s = hw_stack_alloc(sizeof(NjSprite));
  NjSprite *sp = (NjSprite *)hw_ptr(s);
  sp->tlist = 0x8C09703Cu;
  sp->tanim = 0x8C097044u;
  sp->ang = 0;
  sp->sx = F32(1.0f);
  sp->sy = F32(1.0f);
  sp->p.x = sp->p.y = sp->p.z = F32(0.0f);
  if (U32_AT(obj + 0x154) == 2) {
    njColorBlendingMode(0, 9);
    njColorBlendingMode(1, 10);
  }
  gaddr item = U32_AT(obj + 0x12C);
  gaddr anim = U32_AT(obj + 0x130);
  for (int32_t i = 0; i < (int32_t)U32_AT(obj + 0x150); i++, item += 0x30) {
    sp->p.x = field_get_f32(item, 0x00);
    sp->p.y = field_get_f32(item, 0x04);
    sp->p.z = field_get_f32(item, 0x08);
    njDrawSprite3D(s, (int32_t)U32_AT(anim), 0x30);
    anim += 4;
  }
  hw_stack_free(sizeof(NjSprite));
}
HW_EXPORT(8c04381a, sparkles_draw(ARG0))
