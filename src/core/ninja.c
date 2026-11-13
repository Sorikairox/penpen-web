/* Pen Pen port, S13: the game's thin wrappers around Ninja (the SDK's 3D library): inline methods that the compiler
 * copied into every unit (port/symbols/copies.tsv; the copies' exports are in src/copies/core/ninja.c).
 *
 * - jumps to an SDK function (the game links each Ninja call through a local stub: "jmp" with every argument
 *   register as it came, so the C passes them on unchanged);
 * - matrix helpers on Ninja's current matrix (0) with float angles truncated to Ninja's integer angle units;
 * - a small stack of njControl3D flag words (push the current flags, change them, pop back);
 * - fog on or off. */
#include "ninja.h"
#include "sdk.h"

/* ---- stubs: straight jumps to the SDK ---- */

/* was func_8C010BE8 */
HW_EXPORT(8c010be8, RETF(njScalor(ARG0)))
/* was func_8C0122AE */
HW_EXPORT(8c0122ae, RETF(njInnerProduct(ARG0, ARG1)))
/* was func_8C0122C6 */
HW_EXPORT(8c0122c6, RETF(njSqrt(FARG0)))
/* was func_8C013ACC */
HW_EXPORT(8c013acc, RETF(njUnitVector(ARG0)))
/* was func_8C01521E */
HW_EXPORT(8c01521e, njDrawSprite2D(ARG0, (int32_t)ARG1, FARG0, ARG2))
/* was func_8C038A4C */
HW_EXPORT(8c038a4c, njDrawSprite3D(ARG0, (int32_t)ARG1, ARG2))
/* was func_8C01AFD4 */
HW_EXPORT(8c01afd4, RET(nj_ReadChunkFile(ARG0, ARG1, ARG2)))
/* was func_8C01B120 */
HW_EXPORT(8c01b120, nj_ModelL1(ARG0))
/* was func_8C01B126 */
HW_EXPORT(8c01b126, nj_ShapeMotionL1(ARG0, ARG1, ARG2, FARG0))
/* was func_8C01E2B0 */
HW_EXPORT(8c01e2b0, RET(njSetTexture(ARG0)))
/* was func_8C01F5A4 */
HW_EXPORT(8c01f5a4, njReleaseTexture(ARG0))
/* was func_8C01E0F6 */
HW_EXPORT(8c01e0f6, njTranslateV(ARG0, ARG1))
/* was func_8C01E0FE */
HW_EXPORT(8c01e0fe, njTranslate(ARG0, FARG0, FARG1, FARG2))
/* was func_8C030452 */
HW_EXPORT(8c030452, njUnitMatrix(ARG0))
/* was func_8C03880E */
HW_EXPORT(8c03880e, njCalcPoint(ARG0, ARG1, ARG2))

/* ---- the current matrix ---- */

/* was func_8C013B6A */
void matrix_push(void) { njPushMatrix(0); }
HW_EXPORT(8c013b6a, matrix_push())
/* was func_8C013B70 */
void matrix_pop(void) { njPopMatrix(1); }
HW_EXPORT(8c013b70, matrix_pop())
/* was func_8C0227C4 */
void matrix_reset(void) { njUnitMatrix(0); }
HW_EXPORT(8c0227c4, matrix_reset())
/* was func_8C013B76: translate by a Vec3 */
void matrix_translate_v(gaddr v) { njTranslateV(0, v); }
HW_EXPORT(8c013b76, matrix_translate_v(ARG0))
/* was func_8C01E204 */
void matrix_translate(f32 x, f32 y, f32 z) { njTranslate(0, x, y, z); }
HW_EXPORT(8c01e204, matrix_translate(FARG0, FARG1, FARG2))
/* was func_8C013B82 / func_8C013B92 / func_8C013BA2: rotate by an angle held as a float */
void matrix_rotate_x(f32 a) { njRotateX(0, f32_to_int(a)); }
void matrix_rotate_y(f32 a) { njRotateY(0, f32_to_int(a)); }
void matrix_rotate_z(f32 a) { njRotateZ(0, f32_to_int(a)); }
HW_EXPORT(8c013b82, matrix_rotate_x(FARG0))
HW_EXPORT(8c013b92, matrix_rotate_y(FARG0))
HW_EXPORT(8c013ba2, matrix_rotate_z(FARG0))
/* was func_8C030408 / func_8C01E106 / func_8C030434: the same on a given matrix */
HW_EXPORT(8c030408, njRotateX(ARG0, f32_to_int(FARG0)))
HW_EXPORT(8c01e106, njRotateY(ARG0, f32_to_int(FARG0)))
HW_EXPORT(8c030434, njRotateZ(ARG0, f32_to_int(FARG0)))
/* was func_8C02278C */
void matrix_rotate_xyz(f32 x, f32 y, f32 z) { njRotateXYZ(0, f32_to_int(x), f32_to_int(y), f32_to_int(z)); }
HW_EXPORT(8c02278c, matrix_rotate_xyz(FARG0, FARG1, FARG2))
/* S20: was func_8C0304CA (until S20 exported as matrix_rotate_xyz: it jumps to njRotateZXY, not njRotateXYZ; the
 * difference shows only in XMTRX, which the difftest compares since S20): the current matrix rotated by Y, X, Z */
void matrix_rotate_zxy(f32 x, f32 y, f32 z) { nj_rotate_zxy(0, f32_to_int(x), f32_to_int(y), f32_to_int(z)); }
/* was func_8C01B03A */
void matrix_scale(f32 x, f32 y, f32 z) { njScale(0, x, y, z); }
HW_EXPORT(8c01b03a, matrix_scale(FARG0, FARG1, FARG2))
/* was func_8C0227B4: the same factor on the three axes */
void matrix_scale_all(f32 s) { njScale(0, s, s, s); }
HW_EXPORT(8c0227b4, matrix_scale_all(FARG0))
/* was func_8C01E21A */
void matrix_multiply(gaddr m) { njMultiMatrix(0, m); }
HW_EXPORT(8c01e21a, matrix_multiply(ARG0))
/* was func_8C028DBC: out = v, then normalised (its length before is returned) */
f32 vec3_normalized(gaddr v, gaddr out) {
  memmove(hw_ptr(out), hw_ptr(v), sizeof(Vec3));
  return njUnitVector(out);
}
HW_EXPORT(8c028dbc, RETF(vec3_normalized(ARG0, ARG1)))

/* ---- the njControl3D flags stack ---- */
#define g_control3d_top GUEST_VAR(gaddr, 0x8C08A8A0u) /* -> the current flags word of a small stack */

/* was func_8C01B040: push a copy of the current flags */
void control3d_push(void) {
  gaddr top = g_control3d_top + 4;
  uint32_t flags = U32_AT(g_control3d_top);
  g_control3d_top = top;
  U32_AT(g_control3d_top) = flags;
}
HW_EXPORT(8c01b040, control3d_push())
/* was func_8C01B052: back to the flags below, sent to Ninja */
void control3d_pop(void) {
  g_control3d_top -= 4;
  njControl3D(U32_AT(g_control3d_top));
}
HW_EXPORT(8c01b052, control3d_pop())
/* was func_8C01B060: the current flags = these, sent to Ninja */
void control3d_set(uint32_t flags) {
  U32_AT(g_control3d_top) = flags;
  njControl3D(flags);
}
HW_EXPORT(8c01b060, control3d_set(ARG0))

/* ---- fog ---- */

/* was func_8C042442 */
void fog_enable(int32_t on) {
  if (on == 0)
    njFogDisable();
  else
    njFogEnable();
}
HW_EXPORT(8c042442, fog_enable((int32_t)ARG0))
