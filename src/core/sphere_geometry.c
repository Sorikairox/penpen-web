/* Pen Pen port, S18 b01: distances and tests between a point (a sphere's centre), a line (a point and a unit
 * direction: core/plane.h's Plane), a segment (two Vec3s) and a triangle (three Vec3s). Their vectors are locals of
 * the original's frames, handed to Ninja's vector functions: each function keeps its frame (hw_stack_alloc) with the
 * locals at the original's offsets. */
#include "hw.h"
#include "sdk.h"
#include "sphere_geometry.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "plane.h"

/* SDK njOuterProduct (0x8C0C3D04): out = a x b */
static inline void njOuterProduct(gaddr a, gaddr b, gaddr out) { CALL3(0x8C0C3D04u, a, b, out); }

/* a sphere as these read it (core/sphere_triangle.c's layout) */
typedef struct GeoSphere {
  Vec3 center; /* 0x00 */
  f32 radius;  /* 0x0C */
} GeoSphere;
HW_OFFSET(GeoSphere, radius, 0x0C);

/* out = a - b, written z, y, x */
static void vec3_sub_to(Vec3 *out, const Vec3 *a, const Vec3 *b) {
  f32 x = fsub(a->x, b->x), y = fsub(a->y, b->y), z = fsub(a->z, b->z);
  out->z = z;
  out->y = y;
  out->x = x;
}

/* ---- a line and a sphere ---- */

typedef struct LineSphereFrame {
  uint32_t spill; /* 0x00 */
  Vec3 d;         /* 0x04 */
} LineSphereFrame;
HW_SIZE(LineSphereFrame, 0x10);

/* was func_8C012362: 1 when the sphere's radius squared is above |sphere.center|^2 - (direction . d)^2 (absolute),
 * d = sphere.center - line.point. (The original measures the centre's own length, not d's, there: kept.) */
int32_t line_sphere_near(gaddr line, gaddr sphere) {
  gaddr frame = hw_stack_alloc(sizeof(LineSphereFrame));
  gaddr d = member_addr(frame, LineSphereFrame, d);
  vec3_diff(d, line, sphere);
  f32 square = njScalor2(sphere);
  f32 along = njInnerProduct(member_addr(line, Plane, normal), d);
  f32 dist2 = f32_abs(fsub(square, f32_square(along)));
  f32 r = ((const GeoSphere *)hw_ptr(sphere))->radius;
  hw_stack_free(sizeof(LineSphereFrame));
  return fgt(fmul(r, r), dist2);
}
HW_EXPORT(8c012362, RET(line_sphere_near(ARG0, ARG1)))

/* ---- a line and a triangle ---- */

typedef struct LineTriangleFrame {
  uint32_t args[3]; /* 0x00: spills */
  Vec3 n;           /* 0x0C: edge x d */
  Vec3 d;           /* 0x18: the line's point from the edge's start */
  Vec3 edge;        /* 0x24 */
} LineTriangleFrame;
HW_OFFSET(LineTriangleFrame, n, 0x0C);
HW_OFFSET(LineTriangleFrame, edge, 0x24);
HW_SIZE(LineTriangleFrame, 0x30);

/* one edge (from corner a to corner b): is the line's direction on the inner side (edge x (point - a)) . direction
 * not below 0? */
static int line_inside_edge(gaddr line, gaddr frame, const Vec3 *a, const Vec3 *b) {
  LineTriangleFrame *f = (LineTriangleFrame *)hw_ptr(frame);
  vec3_sub_to(&f->edge, b, a);
  vec3_sub_to(&f->d, vec3_at(line), a);
  method_self(member_addr(frame, LineTriangleFrame, n));
  njOuterProduct(member_addr(frame, LineTriangleFrame, edge), member_addr(frame, LineTriangleFrame, d),
                 member_addr(frame, LineTriangleFrame, n));
  f32 dot = njInnerProduct(member_addr(line, Plane, normal), member_addr(frame, LineTriangleFrame, n));
  return !fgt(F32(0.0f), dot);
}

/* was func_8C0123B2: 1 when the line passes through the triangle's three edges' inner sides (edges 0-1, 1-2, 2-0 in
 * turn; the first that fails gives 0) */
int32_t line_crosses_triangle(gaddr line, gaddr tri) {
  const Vec3 *t = vec3_at(tri);
  gaddr frame = hw_stack_alloc(sizeof(LineTriangleFrame));
  int32_t hit = line_inside_edge(line, frame, &t[0], &t[1]) && line_inside_edge(line, frame, &t[1], &t[2]) &&
                line_inside_edge(line, frame, &t[2], &t[0]);
  hw_stack_free(sizeof(LineTriangleFrame));
  return hit;
}
HW_EXPORT(8c0123b2, RET(line_crosses_triangle(ARG0, ARG1)))

/* ---- a point and a segment ---- */

typedef struct PointSegmentFrame {
  uint32_t spill; /* 0x00 */
  Vec3 d;         /* 0x04 */
  Plane line;     /* 0x10: the segment's start and direction */
  Vec3 dir;       /* 0x28: the segment's direction (made unit: its length the result) */
  Vec3 pd;        /* 0x34: the point from the segment's start */
} PointSegmentFrame;
HW_OFFSET(PointSegmentFrame, line, 0x10);
HW_OFFSET(PointSegmentFrame, dir, 0x28);
HW_OFFSET(PointSegmentFrame, pd, 0x34);
HW_SIZE(PointSegmentFrame, 0x40);

/* was func_8C012568: the distance from point to the segment seg[2]: to its start when the point projects before
 * it, to its end when past it, else to its line (sqrt of |d|^2 - (d . direction)^2, absolute) */
f32 point_segment_distance(gaddr point, gaddr seg) {
  const Vec3 *s = vec3_at(seg);
  gaddr frame = hw_stack_alloc(sizeof(PointSegmentFrame));
  PointSegmentFrame *f = (PointSegmentFrame *)hw_ptr(frame);
  gaddr d = member_addr(frame, PointSegmentFrame, d);
  gaddr dir = member_addr(frame, PointSegmentFrame, dir);
  f32 result;
  vec3_sub_to(&f->pd, vec3_at(point), &s[0]);
  vec3_sub_to(&f->dir, &s[1], &s[0]);
  f32 length = njUnitVector(dir);
  f32 t = njInnerProduct(member_addr(frame, PointSegmentFrame, pd), dir);
  if (!fgt(t, F32(0.0f))) {
    vec3_diff(d, point, seg);
    result = njScalor(d);
  } else if (!fgt(length, t)) {
    vec3_diff(d, point, seg + sizeof(Vec3));
    result = njScalor(d);
  } else {
    gaddr line = member_addr(frame, PointSegmentFrame, line);
    vec3_copy(line, seg);
    vec3_copy(member_addr(line, Plane, normal), dir);
    vec3_diff(d, line, point);
    f32 square = njScalor2(d);
    f32 along = njInnerProduct(member_addr(line, Plane, normal), d);
    result = njSqrt(f32_abs(fsub(square, f32_square(along))));
  }
  hw_stack_free(sizeof(PointSegmentFrame));
  return result;
}
HW_EXPORT(8c012568, RETF(point_segment_distance(ARG0, ARG1)))

typedef struct SegmentOffsetFrame {
  uint32_t spill; /* 0x00 */
  Plane line;     /* 0x04: the segment's start and direction */
  Vec3 dir;       /* 0x1C */
  Vec3 pd;        /* 0x28 */
} SegmentOffsetFrame;
HW_OFFSET(SegmentOffsetFrame, line, 0x04);
HW_OFFSET(SegmentOffsetFrame, dir, 0x1C);
HW_OFFSET(SegmentOffsetFrame, pd, 0x28);
HW_SIZE(SegmentOffsetFrame, 0x34);

/* was func_8C012960: out = the step from point to the nearest point of the segment seg[2]: to its start when the
 * point projects before it, to its end when past it, else to its line (plane_point_offset) */
void point_segment_offset(gaddr point, gaddr seg, gaddr out) {
  const Vec3 *s = vec3_at(seg);
  gaddr frame = hw_stack_alloc(sizeof(SegmentOffsetFrame));
  SegmentOffsetFrame *f = (SegmentOffsetFrame *)hw_ptr(frame);
  gaddr dir = member_addr(frame, SegmentOffsetFrame, dir);
  vec3_sub_to(&f->pd, vec3_at(point), &s[0]);
  vec3_sub_to(&f->dir, &s[1], &s[0]);
  f32 length = njUnitVector(dir);
  f32 t = njInnerProduct(member_addr(frame, SegmentOffsetFrame, pd), dir);
  if (fgt(F32(0.0f), t)) {
    vec3_sub_to(vec3_at(out), &s[0], vec3_at(point));
  } else if (fgt(t, length)) {
    vec3_sub_to(vec3_at(out), &s[1], vec3_at(point));
  } else {
    gaddr line = member_addr(frame, SegmentOffsetFrame, line);
    vec3_copy(line, seg);
    vec3_copy(member_addr(line, Plane, normal), dir);
    plane_point_offset(point, line, out);
  }
  hw_stack_free(sizeof(SegmentOffsetFrame));
}
HW_EXPORT(8c012960, point_segment_offset(ARG0, ARG1, ARG2))

/* ---- a point and a triangle ---- */

typedef struct PointTriangleFrame {
  uint32_t args[3]; /* 0x00: spills */
  Vec3 seg[2];      /* 0x0C: the edge the point lies outside of; seg[1] is also each edge's cross product */
  Vec3 d2, d1, d0;  /* 0x24: the point from corners 2, 1, 0 */
  Vec3 e3, e2, e1;  /* 0x48: the edges 2-0, 1-2, 0-1 */
  Plane plane;      /* 0x6C: the triangle's */
} PointTriangleFrame;
HW_OFFSET(PointTriangleFrame, seg, 0x0C);
HW_OFFSET(PointTriangleFrame, d2, 0x24);
HW_OFFSET(PointTriangleFrame, d0, 0x3C);
HW_OFFSET(PointTriangleFrame, e3, 0x48);
HW_OFFSET(PointTriangleFrame, e1, 0x60);
HW_OFFSET(PointTriangleFrame, plane, 0x6C);
HW_SIZE(PointTriangleFrame, 0x84);

/* the triangle's plane, its edges and the point from each corner; then the first edge (0-1, 1-2, 2-0) the point
 * lies outside of ((edge x d) . normal below 0) is copied to seg: its index (0-2), or -1 when inside all three */
static int point_triangle_edge(gaddr point, gaddr tri, gaddr frame) {
  const Vec3 *t = vec3_at(tri);
  const Vec3 *p = vec3_at(point);
  PointTriangleFrame *f = (PointTriangleFrame *)hw_ptr(frame);
  gaddr cross = member_addr(frame, PointTriangleFrame, seg[1]);
  gaddr normal = member_addr(frame, PointTriangleFrame, plane.normal);
  plane_from_triangle(member_addr(frame, PointTriangleFrame, plane), tri);
  vec3_sub_to(&f->e1, &t[1], &t[0]);
  vec3_sub_to(&f->e2, &t[2], &t[1]);
  vec3_sub_to(&f->e3, &t[0], &t[2]);
  vec3_sub_to(&f->d0, p, &t[0]);
  vec3_sub_to(&f->d1, p, &t[1]);
  vec3_sub_to(&f->d2, p, &t[2]);
  static const struct { uint32_t edge, d; } pairs[3] = {
    {offsetof(PointTriangleFrame, e1), offsetof(PointTriangleFrame, d0)},
    {offsetof(PointTriangleFrame, e2), offsetof(PointTriangleFrame, d1)},
    {offsetof(PointTriangleFrame, e3), offsetof(PointTriangleFrame, d2)},
  };
  for (int i = 0; i < 3; i++) {
    method_self(cross);
    njOuterProduct(frame + pairs[i].edge, frame + pairs[i].d, cross);
    if (fgt(F32(0.0f), njInnerProduct(normal, cross))) {
      gaddr a = tri + (uint32_t)i * sizeof(Vec3), b = tri + (uint32_t)((i + 1) % 3) * sizeof(Vec3);
      gaddr seg = member_addr(frame, PointTriangleFrame, seg);
      vec3_copy(seg, a);
      vec3_copy(seg + sizeof(Vec3), b);
      return i;
    }
  }
  return -1;
}

/* was func_8C012678: the distance from point to the triangle tri[3]: to the edge it lies outside of
 * (point_segment_distance), else to the triangle's plane (|normal . (point - plane point)|) */
f32 point_triangle_distance(gaddr point, gaddr tri) {
  gaddr frame = hw_stack_alloc(sizeof(PointTriangleFrame));
  f32 result;
  if (point_triangle_edge(point, tri, frame) >= 0) {
    result = point_segment_distance(point, member_addr(frame, PointTriangleFrame, seg));
  } else {
    gaddr d = member_addr(frame, PointTriangleFrame, seg[1]);
    vec3_diff(d, member_addr(frame, PointTriangleFrame, plane), point);
    result = f32_abs(njInnerProduct(member_addr(frame, PointTriangleFrame, plane.normal), d));
  }
  hw_stack_free(sizeof(PointTriangleFrame));
  return result;
}
HW_EXPORT(8c012678, RETF(point_triangle_distance(ARG0, ARG1)))

/* was func_8C012ACC: out = the step from point to the nearest point of the triangle tri[3]: to the edge it lies
 * outside of (point_segment_offset), else straight to the triangle's plane (plane_point_step) */
void point_triangle_offset(gaddr point, gaddr tri, gaddr out) {
  gaddr frame = hw_stack_alloc(sizeof(PointTriangleFrame));
  if (point_triangle_edge(point, tri, frame) >= 0)
    point_segment_offset(point, member_addr(frame, PointTriangleFrame, seg), out);
  else
    plane_point_step(point, member_addr(frame, PointTriangleFrame, plane), out);
  hw_stack_free(sizeof(PointTriangleFrame));
}
HW_EXPORT(8c012acc, point_triangle_offset(ARG0, ARG1, ARG2))
