/* Pen Pen port, S13 (batch b): small methods of the scene's actor and effect classes (0x8C0438E8-0x8C04C7A6):
 * constructors and destructors, record tables, per-view words, bodies at rest. Field names are from what these
 * functions do with them. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/ninja.h"
#include "game.h"

/* ---- a keyframed value: a list of 6-byte records {frame, a, b}, sorted by frame ---- */

/* was func_8C0438E8: the key list (three words: the list and two values) = src */
void keyed_set_keys(gaddr obj, gaddr src) {
  U32_AT(obj + 0x174) = U32_AT(src + 0);
  U32_AT(obj + 0x178) = U32_AT(src + 4);
  U32_AT(obj + 0x17C) = U32_AT(src + 8);
}
HW_EXPORT(8c0438e8, keyed_set_keys(ARG0, ARG1))

/* was func_8C0438FC: the values of the first record whose frame is not before the object's frame (0x38) */
void keyed_update(gaddr obj) {
  gaddr rec = U32_AT(obj + 0x174);
  while ((int32_t)U32_AT(obj + 0x38) > *(int16_t *)hw_ptr(rec)) rec += 6;
  U32_AT(obj + 0x184) = (uint32_t)(int32_t)*(int16_t *)hw_ptr(rec + 2);
  U32_AT(obj + 0x180) = (uint32_t)(int32_t)*(int16_t *)hw_ptr(rec + 4);
}
HW_EXPORT(8c0438fc, keyed_update(ARG0))

/* was func_8C043A90: a mode (0x194), its five slots emptied (-1) and its float at 0x1AC cleared */
void actor_set_mode(gaddr obj, uint32_t mode) {
  U32_AT(obj + 0x194) = mode;
  for (uint32_t off = 0; off < 0x14; off += 4) U32_AT(obj + 0x198 + off) = 0xFFFFFFFFu;
  field_set_f32(obj + 0x1AC, 0, F32(0.0f));
}
HW_EXPORT(8c043a90, actor_set_mode(ARG0, ARG1))

/* was func_8C043D2E: the word at 0x88 */
HW_EXPORT(8c043d2e, RET(field_get(ARG0, 0x88)))

/* was func_8C044424: the word at 8 cleared; returns the object (a constructor) */
gaddr object_clear8(gaddr obj) {
  U32_AT(obj + 8) = 0;
  return obj;
}
HW_EXPORT(8c044424, RET(object_clear8(ARG0)))

/* ---- a group of objects drawn together ---- */

/* was func_8C0446B0: draw each object of the group (count at 0x424, objects from 0x24): the method at 0x10 of
 * the vtable each object holds at 0x24, on the object moved by the method's adjustment */
void group_draw(gaddr group) {
  for (int32_t i = 0; i < (int32_t)U32_AT(group + 0x424); i++) {
    gaddr obj = U32_AT(group + 0x24 + 4u * (uint32_t)i);
    const VMethod *m = (const VMethod *)hw_ptr(U32_AT(obj + 0x24) + 0x10);
    hw_call(m->fn, 1, obj + (uint32_t)m->this_adjust, 0, 0, 0);
  }
}
HW_EXPORT(8c0446b0, group_draw(ARG0))

/* was func_8C044EEA: push the stage's player i (0x1BC-byte records from 0xAC; their Vec3 at 0x140) by v */
void stage_player_push(gaddr obj, int32_t i, gaddr v) {
  gaddr player = U32_AT(obj + 0x38) + 0xAC + (uint32_t)i * 0x1BCu;
  vec3_add(player + 0x140, v);
}
HW_EXPORT(8c044eea, stage_player_push(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C044F1E: the word of view i (-1 = the current view) in the table at 0xCC */
uint32_t actor_view_word(gaddr obj, int32_t view) {
  gaddr table = obj + 0xCC;
  int32_t i = view == -1 ? view_current_index() : view;
  return U32_AT(table + 4u * (uint32_t)i);
}
/* not exported (S13 difftest: differs from the original, to redo): HW_EXPORT(8c044f1e, RET(actor_view_word(ARG0, (int32_t)ARG1))) */

/* ---- destructors ----
 * A deleting destructor runs the class's destructor body, then frees the object when asked; it returns the
 * object. Some bodies are empty. */

static gaddr delete_after(gaddr obj, int32_t free_memory) {
  if (free_memory) syFree(obj);
  return obj;
}

/* func_8C04503C / func_8C047978: destructor bodies (still translated) */
static void destroy_body_8c04503c(gaddr obj) { CALL1(0x8C04503Cu, obj); }
static void destroy_body_8c047978(gaddr obj) { CALL1(0x8C047978u, obj); }

/* was func_8C045050 */
gaddr actor_a_delete(gaddr obj, int32_t free_memory) {
  destroy_body_8c04503c(obj);
  return delete_after(obj, free_memory);
}
HW_EXPORT(8c045050, RET(actor_a_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04798C */
gaddr actor_b_delete(gaddr obj, int32_t free_memory) {
  destroy_body_8c047978(obj);
  return delete_after(obj, free_memory);
}
HW_EXPORT(8c04798c, RET(actor_b_delete(ARG0, (int32_t)ARG1)))

/* was func_8C046070, func_8C046714, func_8C047598, func_8C048498, func_8C049516: the deleting destructors of
 * classes whose body is empty (method_empty, core/inline.c) */
gaddr empty_delete(gaddr obj, int32_t free_memory) {
  method_empty();
  return delete_after(obj, free_memory);
}
HW_EXPORT(8c046070, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c046714, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c047598, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c048498, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c049516, RET(empty_delete(ARG0, (int32_t)ARG1)))

/* the destructor bodies of three two-base classes: their vtables (the second base at `second`) back to the
 * class's own, then the members' destructor (still translated) */
static gaddr two_base_destroy(gaddr obj, gaddr vtable, uint32_t second, gaddr vtable2, gaddr members) {
  U32_AT(obj + 0x24) = vtable;
  U32_AT(obj + second) = vtable2;
  CALL1(members, obj);
  return obj;
}
/* was func_8C0466F2 (members: func_8C0466DE) */
gaddr actor_c_destroy(gaddr obj) { return two_base_destroy(obj, 0x8C23DC8Cu, 0x54, 0x8C23DCACu, 0x8C0466DEu); }
HW_EXPORT(8c0466f2, RET(actor_c_destroy(ARG0)))
/* was func_8C047576 (members: func_8C047562) */
gaddr actor_d_destroy(gaddr obj) { return two_base_destroy(obj, 0x8C23DD0Cu, 0x40, 0x8C23DD2Cu, 0x8C047562u); }
HW_EXPORT(8c047576, RET(actor_d_destroy(ARG0)))
/* was func_8C0494F6 (members: func_8C0494E2) */
gaddr actor_e_destroy(gaddr obj) { return two_base_destroy(obj, 0x8C23DE94u, 0x8C, 0x8C23DEB4u, 0x8C0494E2u); }
HW_EXPORT(8c0494f6, RET(actor_e_destroy(ARG0)))

/* ---- small setters ---- */

/* was func_8C04763E: the angle at 0x38 += 364.09 (two degrees in Ninja units) */
HW_EXPORT(8c04763e, f32_add_to(F32_BITS(0x43B60B60u), ARG0 + 0x38))

/* was func_8C047EAA: a buffer holder: size (4), its two blocks (0x0C, 0x10) empty; returns it */
gaddr buffer_pair_init(gaddr obj, uint32_t size) {
  U32_AT(obj + 0x0C) = 0;
  U32_AT(obj + 0x10) = 0;
  U32_AT(obj + 0x04) = size;
  return obj;
}
HW_EXPORT(8c047eaa, RET(buffer_pair_init(ARG0, ARG1)))

/* was func_8C047ED0: free the holder's two blocks */
void buffer_pair_free(gaddr obj) {
  gaddr p = U32_AT(obj + 0x10);
  if (p != 0) syFree(p);
  p = U32_AT(obj + 0x0C);
  if (p != 0) syFree(p);
  U32_AT(obj + 0x10) = 0;
  U32_AT(obj + 0x0C) = 0;
}
HW_EXPORT(8c047ed0, buffer_pair_free(ARG0))

/* was func_8C04844A / func_8C048450: the words at 0x572C / 0x5730 */
HW_EXPORT(8c04844a, RET(field_get(ARG0, 0x572C)))
HW_EXPORT(8c048450, RET(field_get(ARG0, 0x5730)))

/* was func_8C0499D4, func_8C04AD28: a Body at rest */
HW_EXPORT(8c0499d4, body_reset(ARG0))
HW_EXPORT(8c04ad28, body_reset(ARG0))

/* was func_8C04AEEE: a Body at rest, through the setters that return the vector */
gaddr body_reset_set3(gaddr b) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x00);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x0C);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x18);
  return vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), b + 0x24);
}
HW_EXPORT(8c04aeee, RET(body_reset_set3(ARG0)))

/* was func_8C04B4DE: reset an effect: its counter (0x160) cleared, 0x164 = 0xEC, its table (0x168) the static one,
 * both scales (0x158, 0x15C) the default, and its slot array (0x178) cleared */
#define g_effect_default_scale GUEST_VAR(f32, 0x8C0881C0u)  /* 0.24999999 */
#define g_effect_slots        GUEST_VAR(int32_t, 0x8C0881B0u) /* 32 */
#define EFFECT_STATIC_TABLE   0x8C098F28u
void effect_reset(gaddr fx) {
  U32_AT(fx + 0x160) = 0;
  U32_AT(fx + 0x164) = U32_AT(fx + 0xEC);
  U32_AT(fx + 0x168) = EFFECT_STATIC_TABLE;
  F32_AT(fx + 0x158) = g_effect_default_scale;
  F32_AT(fx + 0x15C) = g_effect_default_scale;
  gaddr slot = U32_AT(fx + 0x178);
  for (int32_t n = g_effect_slots; n > 0; n--) {
    U32_AT(slot) = 0;
    slot += 4;
  }
}
HW_EXPORT(8c04b4de, effect_reset(ARG0))

/* was func_8C04C6F8: a Body at rest at the origin (vec.c's body_init) */
HW_EXPORT(8c04c6f8, body_init(ARG0))

/* func_8C06270C (SHC runtime): destroy an array from operator new[] with the destructor `dtor` */
static void array_delete(gaddr array, gaddr dtor) { CALL2(0x8C06270Cu, array, dtor); }

/* was func_8C04C780: free the object's array of members (4), their destructor being empty */
void member_array_free(gaddr obj) {
  gaddr array = U32_AT(obj + 4);
  if (array == 0) return;
  array_delete(array, 0x8C04C6F4u /* method_empty */);
  syFree(array);
}
HW_EXPORT(8c04c780, member_array_free(ARG0))

/* func_8C062790 (SHC runtime): construct `n` objects of `size` bytes in a block from operator new[] (returns it) */
static gaddr array_construct_new(int32_t n, uint32_t size, gaddr p, gaddr ctor) {
  return CALL4(0x8C062790u, n, size, p, ctor);
}

/* was func_8C04C708: an array holder: its rotation (0x14) the identity, `count` members of 0x48 bytes built with
 * func_8C04C6F8 (a Body at rest) at 4; returns it */
gaddr member_array_init(gaddr obj, int32_t count) {
  F32_AT(obj + 0x14) = F32(0.0f);
  F32_AT(obj + 0x18) = F32(0.0f);
  F32_AT(obj + 0x1C) = F32(0.0f);
  F32_AT(obj + 0x20) = F32(1.0f);
  U32_AT(obj) = (uint32_t)count;
  gaddr block = syMalloc((uint32_t)(count ? count : 1) * 0x48);
  gaddr array = 0;
  if (block != 0) array = array_construct_new(count, 0x48, block, 0x8C04C6F8u);
  U32_AT(obj + 4) = array;
  return obj;
}
HW_EXPORT(8c04c708, RET(member_array_init(ARG0, (int32_t)ARG1)))

/* ---- more group and actor methods ---- */


/* was func_8C044600: tear a group down: destroy its objects (the method at 0 of each one's vtable at 0x24, with
 * "free" = 1), free its 0-terminated block list (0x43C), release and free its texture list (0x438), release the
 * texture list of its data file (0x430) and free the file */
void group_release(gaddr group) {
  for (int32_t i = 0; i < (int32_t)U32_AT(group + 0x424); i++) {
    gaddr obj = U32_AT(group + 0x24 + 4u * (uint32_t)i);
    if (obj == 0) continue;
    const VMethod *m = (const VMethod *)hw_ptr(U32_AT(obj + 0x24));
    hw_call(m->fn, 2, obj + (uint32_t)m->this_adjust, 1, 0, 0);
  }
  for (gaddr p = group + 0x43C; U32_AT(p) != 0; p += 4) syFree(U32_AT(p));
  njReleaseTexture(U32_AT(group + 0x438));
  syFree(U32_AT(group + 0x438));
  njReleaseTexture(U32_AT(U32_AT(group + 0x430) + 4));
  file_free(U32_AT(group + 0x430));
}
HW_EXPORT(8c044600, group_release(ARG0))

/* was func_8C046564: draw the actor's two models: the first at 0x2C turned by the angle at 0x5C, the second at
 * 0x38 turned by the angle of the Body1 at 0x6C */
void actor_pair_draw(gaddr actor) {
  njPushMatrix(0);
  njTranslateV(0, actor + 0x2C);
  njRotateY(0, f32_to_int(F32_AT(actor + 0x5C)));
  nj_ModelL1(U32_AT(actor + 0x7C));
  njPopMatrix(1);
  njPushMatrix(0);
  njTranslateV(0, actor + 0x38);
  matrix_rotate_y(field_get_f32(actor + 0x6C, 0));
  nj_ModelL1(U32_AT(actor + 0x80));
  njPopMatrix(1);
}
HW_EXPORT(8c046564, actor_pair_draw(ARG0))

/* was func_8C046842: write a scrolling strip's texture coordinates: `count` (0x34) quads of (u0, u1) = (0x3C, 0x3E)
 * and v from the scroll (0x44) plus the base (0x40), each quad dv (0x42) high; then the scroll moves by its speed
 * (0x46) and wraps to 0 past 255 */
void strip_scroll(gaddr strip) {
  int16_t *uv = (int16_t *)hw_ptr(U32_AT(strip + 0x38));
#define S16(off) (*(int16_t *)hw_ptr(strip + (off)))
  int32_t v = S16(0x40) + S16(0x44);
  for (int32_t k = 0; k < (uint16_t)S16(0x34); k++) {
    uv[0] = S16(0x3C);
    uv[1] = (int16_t)v;
    uv[2] = S16(0x3C);
    uv[3] = (int16_t)(S16(0x42) + v);
    uv[4] = S16(0x3E);
    uv[5] = (int16_t)v;
    uv[6] = S16(0x3E);
    uv[7] = (int16_t)(S16(0x42) + v);
    uv += 8;
    v += S16(0x42);
  }
  S16(0x44) = (int16_t)(S16(0x44) + S16(0x46));
  if (S16(0x44) > 0xFF) S16(0x44) = 0;
#undef S16
}
HW_EXPORT(8c046842, strip_scroll(ARG0))

/* was func_8C046FA0: kick particle i (0xBC-byte records from 0x38): down by 10, damping 0.98 on its Body and on
 * its Body1 (0x3C), whose value is reset */
void particle_kick(gaddr obj, int32_t i) {
  gaddr p = obj + 0x38 + (uint32_t)i * 0xBCu;
  f32_add_to(F32(-10.0f), p + 0x04);
  vec3_fill(p + 0x24, F32_BITS(0x3F7AE147u)); /* 0.98 */
  field_set_f32(p + 0x3C, 0x00, F32(0.0f));
  field_set_f32(p + 0x3C, 0x0C, F32_BITS(0x3F7AE147u));
}
HW_EXPORT(8c046fa0, particle_kick(ARG0, (int32_t)ARG1))

#define STRIP_MODELS ((const gaddr *)hw_ptr(0x8C098B2Cu)) /* the strip's model (NJS_OBJECT) per kind */

/* was func_8C046778: set a scrolling strip up from its description {?, kind, position}: its position (0x28), its
 * sound / timing words by kind (0x4C, 0x50) and scroll speed (0x46), its model (0x48) and, from the model's first
 * mesh set, the quad count (0x34), the texture coordinates to write (0x38) and their base values; switched on */
void strip_init(gaddr strip, gaddr desc) {
  Vec3 pos = *vec3_at(desc + 8);
  *vec3_at(strip + 0x28) = pos;
  int16_t *s16 = (int16_t *)hw_ptr(strip);
  switch ((int32_t)U32_AT(desc + 4)) {
  case 0:
    U32_AT(strip + 0x4C) = 0x41;
    U32_AT(strip + 0x50) = 0x44;
    s16[0x46 / 2] = 1;
    break;
  case 1:
    U32_AT(strip + 0x4C) = 0x0F;
    U32_AT(strip + 0x50) = 0x13;
    s16[0x46 / 2] = 1;
    break;
  case 2: case 3: case 4: case 5:
    U32_AT(strip + 0x4C) = 0x23;
    U32_AT(strip + 0x50) = 0x24;
    s16[0x46 / 2] = 4;
    break;
  default:
    break;
  }
  gaddr model = STRIP_MODELS[U32_AT(desc + 4)];
  U32_AT(strip + 0x48) = model;
  gaddr meshset = U32_AT(U32_AT(model + 4) + 0x0C);   /* NJS_OBJECT.model -> NJS_MODEL.meshsets */
  s16[0x34 / 2] = *(int16_t *)hw_ptr(meshset + 2);    /* nb_meshes */
  gaddr uv = U32_AT(meshset + 0x14);                  /* vertex_uvs: {u, v} 16-bit pairs */
  U32_AT(strip + 0x38) = uv;
  const int16_t *t = (const int16_t *)hw_ptr(uv);
  s16[0x3C / 2] = t[0];                               /* u0 */
  s16[0x40 / 2] = t[1];                               /* v0 */
  int32_t v1 = t[3];
  s16[0x3E / 2] = t[4];                               /* u1 */
  s16[0x42 / 2] = (int16_t)(v1 - s16[0x40 / 2]);      /* the quad's height */
  s16[0x44 / 2] = 0;                                  /* the scroll */
  U32_AT(strip + 8) = 1;
}
HW_EXPORT(8c046778, strip_init(ARG0, ARG1))

/* was func_8C047F38: an effect's defaults: off, no velocity, gravity -10 (0x20), scales (2, 2, 2) (0x24) and
 * (-2, 1, -2) (0x30), damping 0.98 (0x3C), a spin of -0.0545 (0x40), the shared texture list (0x44) and its
 * texture (0x48), no blocks (0x6C, 0x70) */
void effect_defaults(gaddr fx) {
  U32_AT(fx + 0x00) = 0;
  U32_AT(fx + 0x08) = 0;
  *vec3_at(fx + 0x14) = (Vec3){F32(0.0f), F32(0.0f), F32(0.0f)};
  F32_AT(fx + 0x20) = F32(-10.0f);
  const f32 two = F32_BITS(0x3FFFFFFFu); /* 1.9999999 */
  *vec3_at(fx + 0x24) = (Vec3){two, two, two};
  *vec3_at(fx + 0x30) = (Vec3){F32_BITS(0xBFFFFFFFu), F32_BITS(0x3F7FFFFFu), F32_BITS(0xBFFFFFFFu)};
  F32_AT(fx + 0x3C) = F32_BITS(0x3F7AE147u); /* 0.98 */
  F32_AT(fx + 0x40) = F32_BITS(0xBD5F0122u); /* -0.05444 */
  U32_AT(fx + 0x44) = 0x8C09703Cu;           /* a texture list (static) */
  U32_AT(fx + 0x48) = 0x8C097044u;
  U32_AT(fx + 0x6C) = 0;
  U32_AT(fx + 0x70) = 0;
}
HW_EXPORT(8c047f38, effect_defaults(ARG0))

/* nj_ModelL3 (Ninja, S2 name): draw a chunk model (another light mode than nj_ModelL1) */
static inline void nj_ModelL3(gaddr model) { CALL1(0x8C0D1996u, model); }
/* func_8C046B84: whether something of this size is on the panel's view (still translated) */
static int32_t panel_sees(gaddr panel, uint32_t what) { return (int32_t)CALL2(0x8C046B84u, panel, what); }


/* was func_8C0473C2: draw the 15 pieces (0xBC-byte records from 0x38) that are not gone (state 0x4C < 2) and that
 * the panel sees: placed and turned by their angles (0x30, 0x34, 0x38); a falling piece (state 0) also tips by its
 * Body1 (0x3C) and hangs 10 below; model 0x20 or 0x24 of the model table (0x20) by its flag (0x54) */
void pieces_draw(gaddr obj) {
  gaddr rec = obj + 0x38;
  for (int32_t i = 0; i < 15; i++, rec += 0xBC) {
    if ((int32_t)U32_AT(rec + 0x4C) >= 2) continue;
    uint32_t what = U32_AT(rec + 0x5C);
    if (!panel_sees(panel_get(), what)) continue;
    matrix_push();
    matrix_translate_v(method_self(rec));
    matrix_rotate_x(F32_AT(rec + 0x30));
    matrix_rotate_y(F32_AT(rec + 0x34));
    matrix_rotate_z(F32_AT(rec + 0x38));
    if (U32_AT(rec + 0x4C) == 0) {
      matrix_rotate_x(field_get_f32(rec + 0x3C, 0));
      matrix_translate(F32(0.0f), F32(-10.0f), F32(0.0f));
    }
    gaddr models = U32_AT(obj + 0x20);
    gaddr model = U32_AT(rec + 0x54) != 0 ? U32_AT(models + 0x20) : U32_AT(models + 0x24);
    nj_ModelL1(model);
    matrix_pop();
  }
}
/* not exported (S13 difftest: differs from the original, to redo): HW_EXPORT(8c0473c2, pieces_draw(ARG0)) */

/* was func_8C0485D0: draw a three-part object: part 0 (model 0x2C of the table at 0x20) at 0x34 turned by 0x58, and
 * again turned half a turn and moved 7 along x; parts 1 and 2 (model 0x30) at 0x40 / 0x4C turned by 0x5C / 0x60 */
void triple_draw(gaddr obj) {
  njPushMatrix(0);
  njTranslateV(0, obj + 0x34);
  njRotateY(0, f32_to_int(F32_AT(obj + 0x58)));
  nj_ModelL3(U32_AT(U32_AT(obj + 0x20) + 4u * U32_AT(obj + 0x2C)));
  njRotateY(0, 0x8000);
  njTranslate(0, F32_BITS(0x40DFFFFFu), F32(0.0f), F32(0.0f)); /* 6.9999995 */
  nj_ModelL3(U32_AT(U32_AT(obj + 0x20) + 4u * U32_AT(obj + 0x2C)));
  njPopMatrix(1);
  for (uint32_t k = 1; k <= 2; k++) {
    njPushMatrix(0);
    njTranslateV(0, obj + 0x34 + 12 * k);
    njRotateY(0, f32_to_int(F32_AT(obj + 0x58 + 4 * k)));
    nj_ModelL1(U32_AT(U32_AT(obj + 0x20) + 4u * U32_AT(obj + 0x30)));
    njPopMatrix(1);
  }
}
HW_EXPORT(8c0485d0, triple_draw(ARG0))

#define g_effect_limit GUEST_VAR(f32, 0x8C0881CCu) /* 20.0 */

/* was func_8C04B5AC: step the effect's particles by their state (0x178): 1 = wait out the countdown (0x174), then
 * go on; 2 = count the float (0x17C) up by 1 to the limit, then go on; 3 = off. A moving particle steps its Body
 * (0x184) and its animation frame (0x170, 0..6) */
void effect_update(gaddr fx) {
  gaddr frame = U32_AT(fx + 0x170), countdown = U32_AT(fx + 0x174), state = U32_AT(fx + 0x178);
  gaddr value = U32_AT(fx + 0x17C), body = U32_AT(fx + 0x184);
  for (int32_t n = g_effect_slots; n > 0; n--) {
    int32_t *s = (int32_t *)hw_ptr(state);
    int32_t moving = 0;
    switch (*s) {
    case 1: {
      int32_t *c = (int32_t *)hw_ptr(countdown);
      int32_t left = (*c)--;
      if (left < 0) (*s)++;
      moving = 1;
      break;
    }
    case 2:
      if (fgt(g_effect_limit, F32_AT(value)))
        F32_AT(value) = fadd(F32_AT(value), F32(1.0f));
      else
        (*s)++;
      moving = 1;
      break;
    case 3:
      *s = 0;
      break;
    default:
      break;
    }
    if (moving) {
      body_step(body);
      int32_t *f = (int32_t *)hw_ptr(frame);
      if (++*f > 6) *f = 0;
    }
    frame += 4;
    countdown += 4;
    state += 4;
    value += 4;
    body += 0x30;
  }
}
HW_EXPORT(8c04b5ac, effect_update(ARG0))

/* a Body1 at rest with no damping (the setters' copies, core/inline.c) */
static void body1_clear(gaddr b) {
  field_set_f32(b, 0x00, F32(0.0f));
  field_set(b, 0x04, 0);
  field_set(b, 0x08, 0);
  field_set_f32(b, 0x0C, F32(1.0f));
}

#define CHAIN_LINK_TABLE 0x8C098D60u /* 20 pairs of floats */

/* was func_8C0479EC: set a chain up from its description {?, word, position}: its Body (0x28) at the position, its
 * mover (0x58: state reset, follows the Body, speed 20, rates 1, method func_8C047838), its swing Body1 (0xB8:
 * value 1.5) and 20 links (0x208 / 0x258 from the static table; Body1s from 0xC8 at 20, damping 0.75 - 0.017 k) */
void chain_init(gaddr chain, gaddr desc) {
  gaddr body = chain + 0x28;
  vec3_zero(body + 0x00);
  vec3_zero(body + 0x0C);
  vec3_zero(body + 0x18);
  vec3_fill(body + 0x24, F32(1.0f));
  F32_AT(body + 0x0) = F32_AT(desc + 0x08);
  F32_AT(body + 0x4) = F32_AT(desc + 0x0C);
  F32_AT(body + 0x8) = F32_AT(desc + 0x10);
  F32_AT(body + 0x2C) = F32(1.0f);
  F32_AT(body + 0x28) = F32(1.0f);
  F32_AT(body + 0x24) = F32(1.0f);
  gaddr mover = chain + 0x58;
  field_reset_2c(mover);
  U32_AT(mover + 0x0C) = U32_AT(chain + 4);
  U32_AT(mover + 0x04) = body;
  U32_AT(mover + 0x10) = body + 0x0C;
  F32_AT(mover + 0x08) = F32(20.0f);
  F32_AT(mover + 0x14) = F32(1.0f);
  F32_AT(mover + 0x18) = F32(1.0f);
  U32_AT(mover + 0x1C) = 4;
  U32_AT(mover + 0x24) = 0x8C047838u; /* an empty method */
  U32_AT(chain + 0xB0) = U32_AT(desc + 4);
  F32_AT(chain + 0xB4) = F32(0.0f);
  gaddr swing = chain + 0xB8;
  body1_clear(swing);
  F32_AT(swing + 0x0) = F32_BITS(0x3FBFFFFFu); /* 1.4999999 */
  F32_AT(swing + 0xC) = F32(1.0f);
  F32_AT(swing + 0x4) = F32(0.0f);
  const f32 *table = (const f32 *)hw_ptr(CHAIN_LINK_TABLE);
  for (int32_t k = 0; k < 20; k++) {
    F32_AT(chain + 0x208 + 4u * (uint32_t)k) = *table++;
    F32_AT(chain + 0x258 + 4u * (uint32_t)k) = *table++;
    gaddr link = chain + 0xC8 + 16u * (uint32_t)k;
    body1_clear(link);
    F32_AT(link + 0x0) = F32(20.0f);
    F32_AT(link + 0xC) = fsub(F32(0.75f), fmul(f32_from_int(k), F32_BITS(0x3C8B4395u))); /* 0.017 */
  }
}
HW_EXPORT(8c0479ec, chain_init(ARG0, ARG1))
