/* Pen Pen port, S20: Ninja's transforms of a matrix and of points (njRotateX / Y / Z / XYZ / ZXY, njRotate, njScale(V),
 * njTranslate, njTranslateV, njCalcPoint, njCalcVector).
 *
 * A matrix argument of 0 means the current matrix (XMTRX, ninja_lib.h). Rotations multiply on the right (m = m * R):
 * only the two columns the rotation mixes change. The original computes them with FTRV, for a RAM matrix too: it
 * swaps the RAM matrix's two columns into XMTRX for the FTRV and puts XMTRX's own columns back afterwards, so the
 * other two columns of the products are XMTRX's (times 0). njRotate's RAM body and the translations of a RAM matrix
 * use scalar operations / FIPR instead: each body is kept with its own rounding. */
#include "matrix.h"

/* ---- rotations ---- */

/* m's columns c0 and c1 become m * v0 and m * v4 (two vectors of the rotation's columns). For a RAM matrix the
 * product is taken with XMTRX whose columns c0 and c1 are replaced by m's, as the original does. */
static void rotate_columns(gaddr m, int c0, int c1, f32 v0[4], f32 v4[4]) {
  f32 *x = hw_xmtrx();
  if (m == 0) {
    ftrv(x, v0);
    ftrv(x, v4);
    memcpy(&x[4 * c0], v0, 4 * sizeof(f32));
    memcpy(&x[4 * c1], v4, 4 * sizeof(f32));
    return;
  }
  f32 *a = nj_floats_at(m);
  f32 t[16];
  memcpy(t, x, sizeof t);
  memcpy(&t[4 * c0], &a[4 * c0], 4 * sizeof(f32));
  memcpy(&t[4 * c1], &a[4 * c1], 4 * sizeof(f32));
  ftrv(t, v0);
  ftrv(t, v4);
  memcpy(&a[4 * c0], v0, 4 * sizeof(f32));
  memcpy(&a[4 * c1], v4, 4 * sizeof(f32));
}

/* the rotations by a cosine / sine pair (the columns of the rotation matrix) */
static void rotate_x_cs(gaddr m, f32 c, f32 s) {
  f32 v0[4] = {F32(0.0f), c, s, F32(0.0f)};
  f32 v4[4] = {F32(0.0f), fneg(s), c, F32(0.0f)};
  rotate_columns(m, 1, 2, v0, v4);
}
static void rotate_y_cs(gaddr m, f32 c, f32 s) {
  f32 v0[4] = {c, F32(0.0f), fneg(s), F32(0.0f)};
  f32 v4[4] = {s, F32(0.0f), c, F32(0.0f)};
  rotate_columns(m, 0, 2, v0, v4);
}
static void rotate_z_cs(gaddr m, f32 c, f32 s) {
  f32 v0[4] = {c, s, F32(0.0f), F32(0.0f)};
  f32 v4[4] = {fneg(s), c, F32(0.0f), F32(0.0f)};
  rotate_columns(m, 0, 1, v0, v4);
}

/* was func_8C0C3FE8 (njRotateX): m = m * (rotation about X by angle, Ninja units: 0x10000 = one turn) */
void nj_rotate_x(gaddr m, int32_t angle) {
  f32 s, c;
  fsca((uint32_t)angle, &s, &c);
  rotate_x_cs(m, c, s);
}
HW_EXPORT(8c0c3fe8, nj_rotate_x(ARG0, (int32_t)ARG1))

/* was func_8C0C407C (njRotateY): m = m * (rotation about Y) */
void nj_rotate_y(gaddr m, int32_t angle) {
  f32 s, c;
  fsca((uint32_t)angle, &s, &c);
  rotate_y_cs(m, c, s);
}
HW_EXPORT(8c0c407c, nj_rotate_y(ARG0, (int32_t)ARG1))

/* was func_8C0C42EC (njRotateZ): m = m * (rotation about Z) */
void nj_rotate_z(gaddr m, int32_t angle) {
  f32 s, c;
  fsca((uint32_t)angle, &s, &c);
  rotate_z_cs(m, c, s);
}
HW_EXPORT(8c0c42ec, nj_rotate_z(ARG0, (int32_t)ARG1))

/* was func_8C0C3E14 (njRotateXYZ): m = m * Rz * Ry * Rx, a zero angle (all 32 bits) skipping its rotation */
void nj_rotate_xyz(gaddr m, int32_t x, int32_t y, int32_t z) {
  if (z != 0) nj_rotate_z(m, z);
  if (y != 0) nj_rotate_y(m, y);
  if (x != 0) nj_rotate_x(m, x);
}
HW_EXPORT(8c0c3e14, nj_rotate_xyz(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3))

/* was func_8C0C4118 (njRotateZXY): m = m * Ry * Rx * Rz (the arguments are the X, Y, Z angles), a zero angle (all
 * 32 bits) skipping its rotation */
void nj_rotate_zxy(gaddr m, int32_t x, int32_t y, int32_t z) {
  if (y != 0) nj_rotate_y(m, y);
  if (x != 0) nj_rotate_x(m, x);
  if (z != 0) nj_rotate_z(m, z);
}
HW_EXPORT(8c0c4118, nj_rotate_zxy(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3))

/* njRotate's RAM body: the same rotations by scalar operations on the two columns, element by element */
static void ram_rotate_x(f32 *a, f32 c, f32 s) {
  for (int k = 0; k < 4; k++) {
    f32 p = a[4 + k], q = a[8 + k];
    a[4 + k] = fadd(fmul(q, s), fmul(p, c));
    a[8 + k] = fsub(fmul(q, c), fmul(p, s));
  }
}
static void ram_rotate_y(f32 *a, f32 c, f32 s) {
  for (int k = 0; k < 4; k++) {
    f32 p = a[k], q = a[8 + k];
    a[k] = fsub(fmul(p, c), fmul(q, s));
    a[8 + k] = fadd(fmul(q, c), fmul(p, s));
  }
}
static void ram_rotate_z(f32 *a, f32 c, f32 s) {
  for (int k = 0; k < 4; k++) {
    f32 p = a[k], q = a[4 + k];
    a[k] = fadd(fmul(p, c), fmul(q, s));
    a[4 + k] = fsub(fmul(q, c), fmul(p, s));
  }
}

/* +1 when v > 0, else -1 */
static f32 sign_of(f32 v) { return fgt(v, F32(0.0f)) ? F32(1.0f) : fneg(F32(1.0f)); }

/* was func_8C0C4388 (njRotate): m = m * (rotation by angle about the axis `axis`, given as a vector). The axis is
 * first turned onto Z by a rotation about X (from its y, z normalised) and one about Y (from x and |(y, z)|, not
 * normalised: the original leaves them so), then the rotation about Z, then those two undone. A zero component
 * makes the pair (sign of the other, 0) or (0, sign of the second), the sign being -1 for 0 too. */
void nj_rotate(gaddr m, gaddr axis, int32_t angle) {
  const NjVector *v = nj_vector_at(axis);
  f32 x = v->x, y = v->y, z = v->z;
  f32 yz2 = fmac(z, z, fmul(y, y));
  f32 ny, nz; /* (y, z) normalised: the turn about X has cosine nz and sine -ny */
  if (feq(z, F32(0.0f)) && !feq(y, F32(0.0f))) {
    nz = F32(0.0f);
    ny = sign_of(y);
  } else if (feq(y, F32(0.0f))) {
    ny = F32(0.0f);
    nz = sign_of(z);
  } else {
    f32 r = fsrra(yz2);
    ny = fmul(y, r);
    nz = fmul(z, r);
  }
  f32 yz = fsqrt(yz2);
  f32 tx, tyz; /* the turn about Y: cosine tyz, sine tx */
  if (feq(yz, F32(0.0f)) && !feq(x, F32(0.0f))) {
    tyz = F32(0.0f);
    tx = sign_of(x);
  } else if (feq(x, F32(0.0f))) {
    tx = F32(0.0f);
    tyz = sign_of(yz);
  } else {
    tx = x;
    tyz = yz;
  }
  f32 s, c;
  if (m == 0) {
    rotate_x_cs(0, nz, fneg(ny));
    rotate_y_cs(0, tyz, tx);
    fsca((uint32_t)angle, &s, &c);
    rotate_z_cs(0, c, s);
    rotate_y_cs(0, tyz, fneg(tx));
    rotate_x_cs(0, nz, ny);
    return;
  }
  f32 *a = nj_floats_at(m);
  ram_rotate_x(a, nz, fneg(ny));
  ram_rotate_y(a, tyz, tx);
  fsca((uint32_t)angle, &s, &c);
  ram_rotate_z(a, c, s);
  ram_rotate_y(a, tyz, fneg(tx));
  ram_rotate_x(a, nz, ny);
}
HW_EXPORT(8c0c4388, nj_rotate(ARG0, ARG1, (int32_t)ARG2))

/* ---- scale and translation ---- */

/* was func_8C0C47BC (njScale): m = m * diag(x, y, z, 1): columns 0, 1, 2 scaled */
void nj_scale(gaddr m, f32 x, f32 y, f32 z) {
  f32 *a = m ? nj_floats_at(m) : hw_xmtrx();
  const f32 k[3] = {x, y, z};
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 4; i++) a[4 * j + i] = fmul(a[4 * j + i], k[j]);
}
HW_EXPORT(8c0c47bc, nj_scale(ARG0, FARG0, FARG1, FARG2))

/* was func_8C0C470C (njScaleV): njScale by the vector at v */
void nj_scale_v(gaddr m, gaddr v) {
  const NjVector *k = nj_vector_at(v);
  nj_scale(m, k->x, k->y, k->z);
}
HW_EXPORT(8c0c470c, nj_scale_v(ARG0, ARG1))

/* was func_8C0C4978 (njTranslate): m = m * (translation by x, y, z): column 3 becomes m * (x, y, z, 1) (FTRV for
 * the current matrix, a FIPR per row for a RAM one) */
void nj_translate(gaddr m, f32 x, f32 y, f32 z) {
  f32 v[4] = {x, y, z, F32(1.0f)};
  if (m == 0) {
    f32 *xm = hw_xmtrx();
    ftrv(xm, v);
    memcpy(&xm[12], v, sizeof v);
    return;
  }
  f32 *a = nj_floats_at(m);
  for (int i = 0; i < 4; i++) {
    const f32 row[4] = {a[i], a[4 + i], a[8 + i], a[12 + i]};
    a[12 + i] = fipr(v, row);
  }
}
HW_EXPORT(8c0c4978, nj_translate(ARG0, FARG0, FARG1, FARG2))

/* was func_8C0C4904 (njTranslateV): njTranslate by the vector at v */
void nj_translate_v(gaddr m, gaddr v) {
  const NjVector *t = nj_vector_at(v);
  nj_translate(m, t->x, t->y, t->z);
}
HW_EXPORT(8c0c4904, nj_translate_v(ARG0, ARG1))

/* ---- points and vectors ---- */

/* was func_8C0C3168 (njCalcPoint): dst = m * (src, 1) (m 0: the current matrix). For a RAM matrix each coordinate
 * is stored as soon as the next row is read, as the original does (dst may be m's own memory). */
void nj_calc_point(gaddr m, gaddr src, gaddr dst) {
  const NjVector *s = nj_vector_at(src);
  f32 v[4] = {s->x, s->y, s->z, F32(1.0f)};
  NjVector *d = nj_vector_at(dst);
  if (m == 0) {
    ftrv(hw_xmtrx(), v);
    d->z = v[2];
    d->y = v[1];
    d->x = v[0];
    return;
  }
  const f32 *a = nj_floats_at(m);
  f32 row0[4] = {a[0], a[4], a[8], a[12]};
  f32 rx = fipr(v, row0);
  f32 row1[4] = {a[1], a[5], a[9], a[13]};
  d->x = rx;
  f32 ry = fipr(v, row1);
  f32 row2[4] = {a[2], a[6], a[10], a[14]};
  d->y = ry;
  d->z = fipr(v, row2);
}
HW_EXPORT(8c0c3168, nj_calc_point(ARG0, ARG1, ARG2))

/* was func_8C0C31C4 (njCalcVector): dst = m * (src, 0) (no translation; m 0: the current matrix), normalised (by
 * FSRRA) when njInitMatrix turned the unit-vector mode on */
void nj_calc_vector(gaddr m, gaddr src, gaddr dst) {
  const NjVector *s = nj_vector_at(src);
  f32 v[4] = {s->x, s->y, s->z, F32(0.0f)};
  if (m == 0) {
    ftrv(hw_xmtrx(), v);
    v[3] = F32(0.0f);
  } else {
    const f32 *a = nj_floats_at(m);
    const f32 row0[4] = {a[0], a[4], a[8], F32(0.0f)};
    const f32 row1[4] = {a[1], a[5], a[9], F32(0.0f)};
    const f32 row2[4] = {a[2], a[6], a[10], F32(0.0f)};
    f32 r[4] = {fipr(v, row0), fipr(v, row1), fipr(v, row2), F32(0.0f)};
    memcpy(v, r, sizeof r);
  }
  if (g_nj_unit_vector_mode != 0) {
    f32 k = fsrra(fipr(v, v));
    v[2] = fmul(v[2], k);
    v[1] = fmul(v[1], k);
    v[0] = fmul(v[0], k);
  }
  NjVector *d = nj_vector_at(dst);
  d->z = v[2];
  d->y = v[1];
  d->x = v[0];
}
HW_EXPORT(8c0c31c4, nj_calc_vector(ARG0, ARG1, ARG2))
