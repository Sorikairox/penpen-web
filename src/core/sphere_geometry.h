/* Pen Pen port, S18 b01: core/sphere_geometry.c */
#ifndef PENPEN_SPHERE_GEOMETRY_H
#define PENPEN_SPHERE_GEOMETRY_H
#include "hw.h"

int32_t line_sphere_near(gaddr line, gaddr sphere);            /* was func_8C012362 */
int32_t line_crosses_triangle(gaddr line, gaddr tri);          /* was func_8C0123B2 */
f32 point_segment_distance(gaddr point, gaddr seg);            /* was func_8C012568 */
f32 point_triangle_distance(gaddr point, gaddr tri);           /* was func_8C012678 */
void point_segment_offset(gaddr point, gaddr seg, gaddr out);  /* was func_8C012960 */
void point_triangle_offset(gaddr point, gaddr tri, gaddr out); /* was func_8C012ACC */

#endif
