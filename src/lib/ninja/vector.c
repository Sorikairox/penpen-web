/* Pen Pen port, S20: Ninja's 3D vector functions (njInnerProduct, njOuterProduct, njScalor, njScalor2, njUnitVector). Dot
 * products are FIPR over (x, y, z, 0), as the original computes them. */
#include "matrix.h"

static void load4(f32 out[4], gaddr v) {
  const NjVector *p = nj_vector_at(v);
  out[0] = p->x;
  out[1] = p->y;
  out[2] = p->z;
  out[3] = F32(0.0f);
}

/* was func_8C0C3308 (njInnerProduct): a . b */
f32 nj_inner_product(gaddr a, gaddr b) {
  f32 va[4], vb[4];
  load4(va, a);
  load4(vb, b);
  return fipr(vb, va);
}
HW_EXPORT(8c0c3308, RETF(nj_inner_product(ARG0, ARG1)))

/* was func_8C0C3D04 (njOuterProduct): out = a x b (each coordinate one product plus an FMAC); returns |out| */
f32 nj_outer_product(gaddr a, gaddr b, gaddr out) {
  const NjVector *u = nj_vector_at(a), *v = nj_vector_at(b);
  f32 a0 = u->x, a1 = u->y, a2 = u->z, b0 = v->x, b1 = v->y, b2 = v->z;
  NjVector *o = nj_vector_at(out);
  f32 r[4];
  r[0] = fmac(fneg(a2), b1, fmul(a1, b2));
  o->x = r[0];
  r[1] = fmac(a0, fneg(b2), fmul(a2, b0));
  o->y = r[1];
  r[2] = fmac(a1, fneg(b0), fmul(a0, b1));
  o->z = r[2];
  r[3] = F32(0.0f);
  return fsqrt(fipr(r, r));
}
HW_EXPORT(8c0c3d04, RETF(nj_outer_product(ARG0, ARG1, ARG2)))

/* was func_8C0C487C (njScalor): |v| */
f32 nj_scalor(gaddr v) {
  f32 p[4];
  load4(p, v);
  return fsqrt(fipr(p, p));
}
HW_EXPORT(8c0c487c, RETF(nj_scalor(ARG0)))

/* was func_8C0C4868 (njScalor2): |v|^2 */
f32 nj_scalor2(gaddr v) {
  f32 p[4];
  load4(p, v);
  return fipr(p, p);
}
HW_EXPORT(8c0c4868, RETF(nj_scalor2(ARG0)))

/* was func_8C0C4A4C (njUnitVector): scales v to length 1 by FSRRA of its squared length; returns that squared
 * length times the factor (|v|) */
f32 nj_unit_vector(gaddr v) {
  f32 p[4];
  load4(p, v);
  f32 len2 = fipr(p, p);
  f32 k = fsrra(len2);
  NjVector *w = nj_vector_at(v);
  w->z = fmul(p[2], k);
  w->y = fmul(p[1], k);
  w->x = fmul(p[0], k);
  return fmul(len2, k);
}
HW_EXPORT(8c0c4a4c, RETF(nj_unit_vector(ARG0)))
