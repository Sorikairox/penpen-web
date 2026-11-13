/* Pen Pen port, S16 (w4): a body's contact with a triangle (func_8C013E1C).
 *
 * The body is the sphere of the Contacts (its owner's position, its radius). The triangle (a, b, c) touches it when
 * the plane is nearer than the radius and the sphere's centre lies over the triangle (the contact's normal is the
 * plane's, its depth the radius less the distance to the plane), or when the nearest point of an edge (or vertex) is
 * nearer than the radius (the normal points from that point to the centre). A touching triangle adds a contact of
 * the given kind, unless a contact of the same kind with a normal less than 60 degrees away is there already: that
 * one is deepened (when the new depth is larger) instead. Nothing is added once the list is one short of full. */
#include "hw.h"
#include "sdk.h"
#include "vec.h"
#include "contact.h"

/* njOuterProduct (Ninja): out = a x b (its result unused here) */
static inline void njOuterProduct(gaddr a, gaddr b, gaddr out) { CALL3(0x8C0C3D04u, a, b, out); }

#define SAME_SIDE_COS F32(0.5f) /* normals nearer than 60 degrees are the same contact */

/* the original's locals (all on its stack: Ninja reads and writes them through their addresses) */
typedef struct TriangleFrame {
  Vec3 axis;     /* 0x00: the cross product of an edge test; the unit edge of a nearest-point search */
  Vec3 to_c;     /* 0x0C: the centre less c */
  Vec3 edge_ca;  /* 0x18: a - c */
  Vec3 to_b;     /* 0x24: the centre less b */
  Vec3 to_a;     /* 0x30: the centre less a */
  Vec3 normal;   /* 0x3C: the plane's unit normal, then the contact's normal */
  Vec3 edge_bc;  /* 0x48: c - b */
  Vec3 edge_ab;  /* 0x54: b - a */
} TriangleFrame;
HW_OFFSET(TriangleFrame, normal, 0x3C);
HW_SIZE(TriangleFrame, 0x60);

static void vec3_minus(Vec3 *out, const Vec3 *a, const Vec3 *b) {
  out->x = fsub(a->x, b->x);
  out->y = fsub(a->y, b->y);
  out->z = fsub(a->z, b->z);
}

/* the centre lies outside the edge (the side away from the triangle, seen along the normal) */
static int edge_outside(gaddr frame, gaddr edge, gaddr to_start) {
  njOuterProduct(edge, to_start, member_addr(frame, TriangleFrame, axis));
  return fgt(F32(0.0f), njInnerProduct(member_addr(frame, TriangleFrame, normal),
                                       member_addr(frame, TriangleFrame, axis)));
}

/* the depth of the sphere into the edge from start to end (to_start: the centre less start); the contact normal
 * (unit, from the nearest point toward the centre) is left in frame.normal */
static f32 edge_depth(gaddr frame, const Contacts *c, gaddr edge, gaddr to_start, gaddr end) {
  TriangleFrame *f = (TriangleFrame *)hw_ptr(frame);
  gaddr axis = member_addr(frame, TriangleFrame, axis);
  f->axis = *vec3_at(edge);
  f32 length = njUnitVector(axis);
  f32 along = njInnerProduct(to_start, axis);
  if (!fgt(along, F32(0.0f))) {
    f->normal = *vec3_at(to_start);                    /* before the start: the start vertex */
  } else if (!fgt(length, along)) {
    vec3_minus(&f->normal, vec3_at(c->owner), vec3_at(end)); /* past the end: the end vertex */
  } else {
    f32 back = fneg(along);                           /* the centre less its foot on the edge */
    f->normal.x = fmul(f->axis.x, back);
    f->normal.y = fmul(f->axis.y, back);
    f->normal.z = fmul(f->axis.z, back);
    const Vec3 *s = vec3_at(to_start);
    f->normal.x = fadd(s->x, f->normal.x);
    f->normal.y = fadd(s->y, f->normal.y);
    f->normal.z = fadd(s->z, f->normal.z);
  }
  return fsub(c->radius, njUnitVector(member_addr(frame, TriangleFrame, normal)));
}

/* add the contact (normal from the frame), or deepen the latest one of the same kind facing the same way */
static void contact_note(Contacts *c, const Vec3 *normal, f32 depth, uint32_t kind) {
  uint32_t *kinds = (uint32_t *)hw_ptr(c->unknown48);
  f32 *depths = (f32 *)hw_ptr(c->depth);
  Vec3 *normals = (Vec3 *)hw_ptr(c->normal);
  for (int32_t i = c->count - 1; i >= 0; i--) {
    if (kinds[i] == kind && fgt(njInnerProduct(c->normal + (gaddr)i * sizeof(Vec3), hw_addr(normal)), SAME_SIDE_COS)) {
      if (fgt(depth, depths[i])) {
        depths[i] = depth;
        normals[i] = *normal;
      }
      return;
    }
  }
  depths[c->count] = depth;
  normals[c->count] = *normal;
  kinds[c->count] = kind;
  c->count++;
}

/* was func_8C013E1C: the contact of the Contacts' sphere with the triangle (a, b, c) of the given kind (see the top
 * of the file) */
void contacts_add_triangle(gaddr self, gaddr a, gaddr b, gaddr c, uint32_t kind) {
  Contacts *list = contacts_at(self);
  if (list->count >= list->capacity - 1) return;
  gaddr frame = hw_stack_alloc(sizeof(TriangleFrame));
  TriangleFrame *f = (TriangleFrame *)hw_ptr(frame);
  const Vec3 *owner = vec3_at(list->owner);
  gaddr normal = member_addr(frame, TriangleFrame, normal);
  vec3_minus(&f->edge_ab, vec3_at(b), vec3_at(a));
  vec3_minus(&f->edge_bc, vec3_at(c), vec3_at(b));
  njOuterProduct(member_addr(frame, TriangleFrame, edge_ab), member_addr(frame, TriangleFrame, edge_bc), normal);
  if (fgt(njUnitVector(normal), F32(0.0f))) {
    vec3_minus(&f->to_a, owner, vec3_at(a));
    f32 depth = fsub(list->radius, fabs_(njInnerProduct(normal, member_addr(frame, TriangleFrame, to_a))));
    int touches = fgt(depth, F32(0.0f));
    /* (the original also calls the empty Vec3 constructor on `axis` before each edge test) */
    if (touches) {
      if (edge_outside(frame, member_addr(frame, TriangleFrame, edge_ab), member_addr(frame, TriangleFrame, to_a))) {
        depth = edge_depth(frame, list, member_addr(frame, TriangleFrame, edge_ab),
                           member_addr(frame, TriangleFrame, to_a), b);
        touches = fgt(depth, F32(0.0f));
      } else {
        vec3_minus(&f->to_b, owner, vec3_at(b));
        if (edge_outside(frame, member_addr(frame, TriangleFrame, edge_bc), member_addr(frame, TriangleFrame, to_b))) {
          depth = edge_depth(frame, list, member_addr(frame, TriangleFrame, edge_bc),
                             member_addr(frame, TriangleFrame, to_b), c);
          touches = fgt(depth, F32(0.0f));
        } else {
          vec3_minus(&f->edge_ca, vec3_at(a), vec3_at(c));
          vec3_minus(&f->to_c, owner, vec3_at(c));
          if (edge_outside(frame, member_addr(frame, TriangleFrame, edge_ca),
                           member_addr(frame, TriangleFrame, to_c))) {
            /* past its end, the original copies to_a (the centre less a, the same values) */
            depth = edge_depth(frame, list, member_addr(frame, TriangleFrame, edge_ca),
                               member_addr(frame, TriangleFrame, to_c), a);
            touches = fgt(depth, F32(0.0f));
          }
        }
      }
      if (touches) contact_note(list, &f->normal, depth, kind);
    }
  }
  hw_stack_free(sizeof(TriangleFrame));
}
HW_EXPORT(8c013e1c, contacts_add_triangle(ARG0, ARG1, ARG2, ARG3, ARG4))
