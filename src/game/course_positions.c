/* Pen Pen port, S17 b05: the racers' positions at the course's finish and (S18 b04) at its start (func_8C041F70
 * enters the shared body below in its middle, with the start segment). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "course.h"
#include "race_scene.h"
#include "actor_b05.h"     /* S17: the declarations of this file's functions (its copies call them) */

/* per course (Course.course): a Vec3 per segment, the point the racers line up around */
#define g_course_segment_points ((const gaddr *)hw_ptr(0x8C096F44u))

#define LINE_SIDE   F32(25.0f) /* odd positions to the left (-), even ones to the right */
#define LINE_DEPTH  F32(20.0f) /* each position this much further back */

/* the shared body: position i around segment `segment`'s point, turned by the course value there (an angle) */
static void course_segment_position(gaddr course, int32_t i, int32_t segment, gaddr out) {
  const Course *c = course_at(course);
  Vec3 point = vec3_at(g_course_segment_points[c->course])[segment];
  f32 angle = course_value(course, segment);
  f32 side = (i & 1) ? F32(-25.0f) : LINE_SIDE;
  f32 depth = fmul(f32_from_int(i), LINE_DEPTH);
  int32_t a = f32_to_int(angle);
  f32 s = njSin(a);
  f32 co = njCos(a);
  Vec3 offset;
  offset.x = fsub(fmul(side, co), fmul(depth, s));
  offset.z = fsub(fneg(fmul(side, s)), fmul(depth, co));
  offset.y = F32(0.0f);
  Vec3 sum;
  sum.x = fadd(point.x, offset.x);
  sum.y = fadd(point.y, offset.y);
  sum.z = fadd(point.z, offset.z);
  memmove(hw_ptr(out), &sum, sizeof(Vec3));
}

/* was func_8C041F8C: racer position i at the course's finish segment into *out */
void course_finish_position(gaddr course, int32_t i, gaddr out) {
  course_segment_position(course, i, course_at(course)->finish_segment, out);
}
HW_EXPORT(8c041f8c, course_finish_position(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C041F70: racer position i at the course's start segment into *out (S18 b04; it jumps into the shared
 * body, func_8C041F9C, with the start segment) */
void course_start_position(gaddr course, int32_t i, gaddr out) {
  course_segment_position(course, i, course_at(course)->start_segment, out);
}
HW_EXPORT(8c041f70, course_start_position(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C03C670: the same for the race scene's course */
void race_scene_finish_position(gaddr scene, int32_t i, gaddr out) {
  course_finish_position(race_scene_at(scene)->course, i, out);
}
HW_EXPORT(8c03c670, race_scene_finish_position(ARG0, (int32_t)ARG1, ARG2))

/* was func_8C041DD0: two corners of cell `cell`'s ground quad: corner 0 into *a, corner 2 into *b (block copies of
 * the ground's vertices) */
void course_cell_corners(gaddr course, int32_t cell, gaddr a, gaddr b) {
  const CourseGround *g = (const CourseGround *)hw_ptr(course_at(course)->entry64);
  const CellQuad *q = &((const CellQuad *)hw_ptr(((const CourseGroundStrip *)hw_ptr(g->strip))->quads))[cell];
  const Vec3 *vertices = vec3_at(g->vertices);
  memmove(hw_ptr(a), &vertices[q->corner[0]], sizeof(Vec3));
  memmove(hw_ptr(b), &vertices[q->corner[2]], sizeof(Vec3));
}
HW_EXPORT(8c041dd0, course_cell_corners(ARG0, (int32_t)ARG1, ARG2, ARG3))
