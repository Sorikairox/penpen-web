/* Pen Pen port, S17 (b01): a plane (a point on it and its unit normal) built from a triangle. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

typedef struct Plane {
  Vec3 point;  /* 0x00 */
  Vec3 normal; /* 0x0C */
} Plane;
HW_OFFSET(Plane, normal, 0x0C);

/* the original's frame: a spill word, the two edges, the saved r14 and PR */
typedef struct PlaneFrame {
  uint32_t spill;    /* 0x00 */
  Vec3 edge2;        /* 0x04: c - a */
  Vec3 edge1;        /* 0x10: b - a */
  uint32_t saved[2]; /* 0x1C */
} PlaneFrame;
HW_OFFSET(PlaneFrame, edge1, 0x10);
HW_SIZE(PlaneFrame, 0x24);

static void njOuterProduct(gaddr a, gaddr b, gaddr out) { CALL3(0x8C0C3D04u, a, b, out); }

/* was func_8C0122D2: the plane through triangle tri[3]: its first corner, and the normalised cross product of its
 * two edges from that corner. Returns the plane. */
gaddr plane_from_triangle(gaddr plane, gaddr tri) {
  Plane *p = (Plane *)hw_ptr(plane);
  const Vec3 *t = (const Vec3 *)hw_ptr(tri);
  gaddr frame = hw_stack_alloc(sizeof(PlaneFrame));
  PlaneFrame *f = (PlaneFrame *)hw_ptr(frame);
  Vec3 a = t[0];
  p->point.z = a.z;
  p->point.y = a.y;
  p->point.x = a.x;
  f->edge1.z = fsub(t[1].z, p->point.z);
  f->edge1.y = fsub(t[1].y, p->point.y);
  f->edge1.x = fsub(t[1].x, p->point.x);
  f->edge2.z = fsub(t[2].z, p->point.z);
  f->edge2.y = fsub(t[2].y, p->point.y);
  f->edge2.x = fsub(t[2].x, p->point.x);
  njOuterProduct(member_addr(frame, PlaneFrame, edge1), member_addr(frame, PlaneFrame, edge2),
                 member_addr(plane, Plane, normal));
  njUnitVector(member_addr(plane, Plane, normal));
  hw_stack_free(sizeof(PlaneFrame));
  return plane;
}
HW_EXPORT(8c0122d2, RET(plane_from_triangle(ARG0, ARG1)))

/* the frame of the two below: a spill word, the difference vector, the saved r14, fr15 and PR */
typedef struct PlaneOffsetFrame {
  uint32_t spill;    /* 0x00 */
  Vec3 d;            /* 0x04 */
  uint32_t saved[3]; /* 0x10 */
} PlaneOffsetFrame;
HW_OFFSET(PlaneOffsetFrame, d, 0x04);
HW_SIZE(PlaneOffsetFrame, 0x1C);

/* was func_8C0128DC: with d = p - plane.point: out = normal * (d . normal) - d (minus d's part along the plane) */
void plane_point_offset(gaddr p, gaddr plane, gaddr out) {
  const Vec3 *pt = vec3_at(p);
  const Plane *pl = (const Plane *)hw_ptr(plane);
  Vec3 *o = vec3_at(out);
  gaddr frame = hw_stack_alloc(sizeof(PlaneOffsetFrame));
  PlaneOffsetFrame *f = (PlaneOffsetFrame *)hw_ptr(frame);
  Vec3 a = pl->point;
  f->d.z = fsub(pt->z, a.z);
  f->d.y = fsub(pt->y, a.y);
  f->d.x = fsub(pt->x, a.x);
  f32 t = njInnerProduct(member_addr(frame, PlaneOffsetFrame, d), member_addr(plane, Plane, normal));
  Vec3 n = pl->normal;
  o->z = fmul(n.z, t);
  o->y = fmul(n.y, t);
  o->x = fmul(n.x, t);
  Vec3 d = f->d;
  o->z = fsub(o->z, d.z);
  o->y = fsub(o->y, d.y);
  o->x = fsub(o->x, d.x);
  hw_stack_free(sizeof(PlaneOffsetFrame));
}
HW_EXPORT(8c0128dc, plane_point_offset(ARG0, ARG1, ARG2))

/* was func_8C012A62: the step from p to the plane along its normal: out = normal * (normal . (plane.point - p)) */
void plane_point_step(gaddr p, gaddr plane, gaddr out) {
  const Vec3 *pt = vec3_at(p);
  const Plane *pl = (const Plane *)hw_ptr(plane);
  Vec3 *o = vec3_at(out);
  gaddr frame = hw_stack_alloc(sizeof(PlaneOffsetFrame));
  PlaneOffsetFrame *f = (PlaneOffsetFrame *)hw_ptr(frame);
  Vec3 q = *pt;
  f->d.z = fsub(pl->point.z, q.z);
  f->d.y = fsub(pl->point.y, q.y);
  f->d.x = fsub(pl->point.x, q.x);
  f32 t = njInnerProduct(member_addr(plane, Plane, normal), member_addr(frame, PlaneOffsetFrame, d));
  Vec3 n = pl->normal;
  o->z = fmul(n.z, t);
  o->y = fmul(n.y, t);
  o->x = fmul(n.x, t);
  hw_stack_free(sizeof(PlaneOffsetFrame));
}
HW_EXPORT(8c012a62, plane_point_step(ARG0, ARG1, ARG2))
