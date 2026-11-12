/* Pen Pen port, S13: the vector, transform and scalar helpers (core/vec.h). Each exported function is the first of
 * a group of identical copies; tools/copies.py exports the others (src/copies/core/vec.c). The addresses named in
 * the "was" comments are the copies written here; where two groups of copies do the same thing with different code
 * (one returns the object, one does not; different registers), both are exported from the same C function. S14:
 * the copies that work on a member of a Body (velocity 0x0C, force 0x18, damping 0x24: Body's inline methods, applied
 * to the Bodies of many classes) name it; the others stay class-neutral offsets. */
#include "vec.h"
#include "sdk.h"

/* ---- construction ---- */

/* was func_8C01657A (returns v) and func_8C012198 (void): v = (x, y, z) */
gaddr vec3_set(f32 x, f32 y, f32 z, gaddr v) {
  Vec3 *p = vec3_at(v);
  p->z = z;
  p->y = y;
  p->x = x;
  return v;
}
HW_EXPORT(8c01657a, RET(vec3_set(FARG0, FARG1, FARG2, ARG0)))
HW_EXPORT(8c012198, vec3_set(FARG0, FARG1, FARG2, ARG0))
HW_EXPORT(8c01af44, vec3_set(FARG0, FARG1, FARG2, ARG0)) /* was func_8C01AF44 */
/* was func_8C01E19A and func_8C01E1B6: an object's Vec3 at 0x0C / at 0x24 = (x, y, z) (a Body's velocity and
 * damping) */
HW_EXPORT(8c01e19a, vec3_set(FARG0, FARG1, FARG2, member_addr(ARG0, Body, velocity)))
HW_EXPORT(8c01e1b6, vec3_set(FARG0, FARG1, FARG2, member_addr(ARG0, Body, damping)))

/* was func_8C012278: dst = src (the three loads first: dst may be src) */
gaddr vec3_copy(gaddr dst, gaddr src) {
  Vec3 v = *vec3_at(src);
  *vec3_at(dst) = v;
  return dst;
}
HW_EXPORT(8c012278, RET(vec3_copy(ARG0, ARG1)))
/* was func_8C03C480: out = the object's Vec3 at 0x30 */
HW_EXPORT(8c03c480, vec3_copy(ARG1, ARG0 + 0x30))

/* was func_8C0306CE: dst = src through the C runtime's block copy (12 bytes); func_8C0306E0 / func_8C0306F4:
 * into the object's Vec3 at 0x0C / 0x24 */
void vec3_copy_bytes(gaddr dst, gaddr src) { memmove(hw_ptr(dst), hw_ptr(src), sizeof(Vec3)); }
HW_EXPORT(8c0306ce, vec3_copy_bytes(ARG0, ARG1))
HW_EXPORT(8c0306e0, vec3_copy_bytes(member_addr(ARG0, Body, velocity), ARG1))
HW_EXPORT(8c0306f4, vec3_copy_bytes(member_addr(ARG0, Body, damping), ARG1))

/* was func_8C0121A2: v = (0, 0, 0); func_8C01D058 / func_8C01D064: the object's Vec3 at 0x0C / 0x18 */
void vec3_zero(gaddr v) {
  Vec3 *p = vec3_at(v);
  p->z = p->y = p->x = F32(0.0f);
}
HW_EXPORT(8c0121a2, vec3_zero(ARG0))
HW_EXPORT(8c01d058, vec3_zero(member_addr(ARG0, Body, velocity)))
HW_EXPORT(8c01d064, vec3_zero(member_addr(ARG0, Body, force)))

/* was func_8C01AF50: v = (s, s, s) for the object's Vec3 at 0x24 (a Body's damping); func_8C01D070: (1, 1, 1)
 * there */
void vec3_fill(gaddr v, f32 s) {
  Vec3 *p = vec3_at(v);
  p->z = p->y = p->x = s;
}
HW_EXPORT(8c01af50, vec3_fill(member_addr(ARG0, Body, damping), FARG0))
HW_EXPORT(8c01d070, vec3_fill(member_addr(ARG0, Body, damping), F32(1.0f)))

/* was func_8C019D44 (returns q) and func_8C01AEAC / func_8C022D56 (void): q = (0, 0, 0, 1) */
gaddr quat_identity(gaddr q) {
  Quat *p = (Quat *)hw_ptr(q);
  p->x = p->y = p->z = F32(0.0f);
  p->w = F32(1.0f);
  return q;
}
HW_EXPORT(8c019d44, RET(quat_identity(ARG0)))
HW_EXPORT(8c01aeac, quat_identity(ARG0))
HW_EXPORT(8c022d56, quat_identity(ARG0))

/* was func_8C019D5A: at rest at the origin, no damping */
gaddr body_init(gaddr t) {
  Body *p = (Body *)hw_ptr(t);
  p->position.z = p->position.y = p->position.x = F32(0.0f);
  p->velocity.z = p->velocity.y = p->velocity.x = F32(0.0f);
  p->force.z = p->force.y = p->force.x = F32(0.0f);
  p->damping.z = p->damping.y = p->damping.x = F32(1.0f);
  return t;
}
HW_EXPORT(8c019d5a, RET(body_init(ARG0)))

/* was func_8C01AF0A: the same through the Vec3 setters */
void body_reset(gaddr t) {
  Body *p = (Body *)hw_ptr(t);
  vec3_zero(hw_addr(&p->position));
  vec3_zero(hw_addr(&p->velocity));
  vec3_zero(hw_addr(&p->force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), hw_addr(&p->damping));
}
HW_EXPORT(8c01af0a, body_reset(ARG0))

/* was func_8C01E0CA: the 4x4 identity, written from the last element down */
gaddr matrix_identity(gaddr m) {
  Matrix *p = (Matrix *)hw_ptr(m);
  for (int i = 15; i >= 0; i--)
    p->m[i] = i % 5 == 0 ? F32(1.0f) : F32(0.0f);
  return m;
}
HW_EXPORT(8c01e0ca, RET(matrix_identity(ARG0)))

/* ---- arithmetic ---- */

/* was func_8C010BC0: out = to - from */
gaddr vec3_diff(gaddr out, gaddr from, gaddr to) {
  const Vec3 a = *vec3_at(from), b = *vec3_at(to);
  Vec3 *o = vec3_at(out);
  o->z = fsub(b.z, a.z);
  o->y = fsub(b.y, a.y);
  o->x = fsub(b.x, a.x);
  return out;
}
HW_EXPORT(8c010bc0, RET(vec3_diff(ARG0, ARG1, ARG2)))

/* was func_8C01E09E: out = a + b */
gaddr vec3_sum(gaddr out, gaddr a, gaddr b) {
  const Vec3 u = *vec3_at(a), v = *vec3_at(b);
  Vec3 *o = vec3_at(out);
  o->z = fadd(u.z, v.z);
  o->y = fadd(u.y, v.y);
  o->x = fadd(u.x, v.x);
  return out;
}
HW_EXPORT(8c01e09e, vec3_sum(ARG0, ARG1, ARG2))

/* was func_8C016594 and func_8C013B24: v += w; func_8C013B44 / func_8C04250C / func_8C0165FC: the object's Vec3 at
 * 0x0C / 0x18 / 0x140 += w */
void vec3_add(gaddr v, gaddr w) {
  const Vec3 d = *vec3_at(w);
  Vec3 *p = vec3_at(v);
  p->z = fadd(d.z, p->z);
  p->y = fadd(d.y, p->y);
  p->x = fadd(d.x, p->x);
}
HW_EXPORT(8c016594, vec3_add(ARG0, ARG1))
HW_EXPORT(8c013b24, vec3_add(ARG0, ARG1))
HW_EXPORT(8c013b44, vec3_add(member_addr(ARG0, Body, velocity), ARG1))
HW_EXPORT(8c04250c, vec3_add(member_addr(ARG0, Body, force), ARG1))
HW_EXPORT(8c0165fc, vec3_add(ARG0 + 0x140, ARG1))

/* was func_8C030798: v += (x, y, z) */
void vec3_add_xyz(f32 x, f32 y, f32 z, gaddr v) {
  Vec3 *p = vec3_at(v);
  p->x = fadd(p->x, x);
  p->y = fadd(p->y, y);
  p->z = fadd(p->z, z);
}
HW_EXPORT(8c030798, vec3_add_xyz(FARG0, FARG1, FARG2, ARG0))

/* was func_8C01AE22: v -= w */
void vec3_sub(gaddr v, gaddr w) {
  const Vec3 d = *vec3_at(w);
  Vec3 *p = vec3_at(v);
  p->z = fsub(p->z, d.z);
  p->y = fsub(p->y, d.y);
  p->x = fsub(p->x, d.x);
}
HW_EXPORT(8c01ae22, vec3_sub(ARG0, ARG1))

/* was func_8C013AD2: v *= s */
void vec3_scale(f32 s, gaddr v) {
  Vec3 *p = vec3_at(v);
  p->z = fmul(p->z, s);
  p->y = fmul(p->y, s);
  p->x = fmul(p->x, s);
}
HW_EXPORT(8c013ad2, vec3_scale(FARG0, ARG0))

/* was func_8C013AF0: out = v * s */
void vec3_scaled(f32 s, gaddr out, gaddr v) {
  const Vec3 a = *vec3_at(v);
  Vec3 *o = vec3_at(out);
  o->z = fmul(a.z, s);
  o->y = fmul(a.y, s);
  o->x = fmul(a.x, s);
}
HW_EXPORT(8c013af0, vec3_scaled(FARG0, ARG0, ARG1))

/* was func_8C013AA4: the y component of a x b (a.z b.x - a.x b.z) */
f32 vec3_cross_y(gaddr a, gaddr b) {
  const Vec3 *u = vec3_at(a), *v = vec3_at(b);
  return fsub(fmul(u->z, v->x), fmul(v->z, u->x));
}
HW_EXPORT(8c013aa4, RETF(vec3_cross_y(ARG0, ARG1)))

/* was func_8C03D9EE: |b - a| (the difference on the guest stack, Ninja's njScalor) */
f32 vec3_distance(gaddr a, gaddr b) {
  gaddr d = hw_stack_alloc(sizeof(Vec3));
  vec3_diff(d, a, b);
  f32 r = njScalor(d);
  hw_stack_free(sizeof(Vec3));
  return r;
}
HW_EXPORT(8c03d9ee, RETF(vec3_distance(ARG0, ARG1)))

/* was func_8C01AF68: pull a Body toward a point: force += (target - position) * rate */
void body_pull(f32 rate, gaddr t, gaddr target) {
  Body *p = (Body *)hw_ptr(t);
  Vec3 d = *vec3_at(target);
  d.x = fmul(fsub(d.x, p->position.x), rate);
  d.y = fmul(fsub(d.y, p->position.y), rate);
  d.z = fmul(fsub(d.z, p->position.z), rate);
  p->force.x = fadd(d.x, p->force.x);
  p->force.y = fadd(d.y, p->force.y);
  p->force.z = fadd(d.z, p->force.z);
}
HW_EXPORT(8c01af68, body_pull(FARG0, ARG0, ARG1))

/* was func_8C0121FE: one step of a Body (z, y, x in that order, as the original) */
void body_step(gaddr b) {
  Body *p = (Body *)hw_ptr(b);
  p->velocity.z = fadd(p->force.z, p->velocity.z);
  p->velocity.y = fadd(p->force.y, p->velocity.y);
  p->velocity.x = fadd(p->force.x, p->velocity.x);
  p->velocity.z = fmul(p->velocity.z, p->damping.z);
  p->velocity.y = fmul(p->velocity.y, p->damping.y);
  p->velocity.x = fmul(p->velocity.x, p->damping.x);
  p->position.z = fadd(p->velocity.z, p->position.z);
  p->position.y = fadd(p->velocity.y, p->position.y);
  p->position.x = fadd(p->velocity.x, p->position.x);
  vec3_zero(hw_addr(&p->force));
}
HW_EXPORT(8c0121fe, body_step(ARG0))

/* ---- scalars ---- */

/* was func_8C0122C0: x * x */
f32 f32_square(f32 x) { return fmul(x, x); }
HW_EXPORT(8c0122c0, RETF(f32_square(FARG0)))

/* was func_8C0122CC: |x| */
f32 f32_abs(f32 x) { return fabs_(x); }
HW_EXPORT(8c0122cc, RETF(f32_abs(FARG0)))

/* was func_8C013B12: 1 / x */
f32 f32_reciprocal(f32 x) { return fdiv(F32(1.0f), x); }
HW_EXPORT(8c013b12, RETF(f32_reciprocal(FARG0)))

/* was func_8C01AE66: an angle as a signed 16-bit Ninja angle (-0x8000 .. 0x7FFF: half a turn each way) */
f32 angle_wrap(f32 a) { return f32_from_int((int16_t)f32_to_int(a)); }
HW_EXPORT(8c01ae66, RETF(angle_wrap(FARG0)))

/* was func_8C0305F4: *p += x; func_8C038946 / func_8C0305A0 / func_8C01AF5E / func_8C01D080 / func_8C038982: the
 * object's float at 4 / 8 / 0x18 / 0x1C / 0x20 += x; func_8C0493D0: the float at 0x38 += 1 */
void f32_add_to(f32 x, gaddr p) { F32_AT(p) = fadd(F32_AT(p), x); }
HW_EXPORT(8c0305f4, f32_add_to(FARG0, ARG0))
HW_EXPORT(8c038946, f32_add_to(FARG0, ARG0 + 0x04))
HW_EXPORT(8c0305a0, f32_add_to(FARG0, ARG0 + 0x08))
HW_EXPORT(8c01af5e, f32_add_to(FARG0, ARG0 + 0x18))
HW_EXPORT(8c01d080, f32_add_to(FARG0, ARG0 + 0x1C))
HW_EXPORT(8c038982, f32_add_to(FARG0, ARG0 + 0x20))
HW_EXPORT(8c0493d0, f32_add_to(F32(1.0f), ARG0 + 0x38))

/* was func_8C0225BA: pull a Body1 toward a value: force += (target - value) * rate */
void body1_pull(f32 target, f32 rate, gaddr p) {
  f32 *v = (f32 *)hw_ptr(p);
  v[2] = fmac(rate, fsub(target, v[0]), v[2]);
}
HW_EXPORT(8c0225ba, body1_pull(FARG0, FARG1, ARG0))

/* was func_8C03065C: the same for an angle: the difference is taken the short way round (as a 16-bit angle) */
void body1_pull_angle(f32 target, f32 rate, gaddr p) {
  f32 *v = (f32 *)hw_ptr(p);
  f32 d = angle_wrap(fsub(target, v[0]));
  v[2] = fmac(d, rate, v[2]);
}
HW_EXPORT(8c03065c, body1_pull_angle(FARG0, FARG1, ARG0))

/* was func_8C0121AE: one step of a Body1 */
void body1_step(gaddr s) {
  f32 *v = (f32 *)hw_ptr(s);
  f32 speed = fadd(v[1], v[2]);
  v[1] = speed;
  speed = fmul(speed, v[3]);
  v[1] = speed;
  v[0] = fadd(v[0], speed);
  v[2] = F32(0.0f);
}
HW_EXPORT(8c0121ae, body1_step(ARG0))
