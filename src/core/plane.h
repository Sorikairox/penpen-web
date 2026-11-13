/* Pen Pen port, S18 b01: core/plane.c's plane (a point on it and its unit normal; also a line: a point and its unit
 * direction) and its functions, for their callers (core/sphere_geometry.c). plane.c declares the same struct
 * itself: the lead may make it include this header instead. */
#ifndef PENPEN_PLANE_H
#define PENPEN_PLANE_H
#include "vec.h"

#ifndef PENPEN_PLANE_STRUCT
#define PENPEN_PLANE_STRUCT
typedef struct Plane {
  Vec3 point;  /* 0x00 */
  Vec3 normal; /* 0x0C: unit (a line's direction) */
} Plane;
HW_SIZE(Plane, 0x18);
#endif

/* was func_8C0122D2: the plane through triangle tri[3] */
gaddr plane_from_triangle(gaddr plane, gaddr tri);
/* was func_8C0128DC: out = normal * ((p - point) . normal) - (p - point) */
void plane_point_offset(gaddr p, gaddr plane, gaddr out);
/* was func_8C012A62: out = normal * (normal . (point - p)) */
void plane_point_step(gaddr p, gaddr plane, gaddr out);

#endif
