/* Pen Pen port, S18 b04: the ground below a point (game/course.h): the course's chunk models of lists 0 and 1 at the
 * cells around the point's are asked in turn. */
#include "hw.h"
#include "../core/vec.h"
#include "course.h"

/* func_8C014E20 (still translated): the ground below pos in a chunk model: not 0 when found, its distance in
 * *distance and its normal in *normal */
static int32_t chunk_ground_below(gaddr pos, gaddr chunk, gaddr distance, gaddr normal) {
  return (int32_t)CALL4(0x8C014E20u, pos, chunk, distance, normal);
}

/* entry i of the course's word list `list` is set (not -1) (func_8C041D54 / func_8C041D6C, game/course.c) */
static int course_list_entry_set(const Course *c, int list, int32_t i) {
  return ((const int32_t *)hw_ptr(c->lists[list]))[i] != -1;
}

/* was func_8C0419DA: the ground below pos around `cell`: for cell - 1, cell, cell + 1 (those that are cells of the
 * course) the chunk of list 0, then of list 1, when it has one; 1 at the first found (distance and normal out), else
 * 0 */
int32_t course_ground_below(gaddr course, gaddr pos, int32_t cell, gaddr distance, gaddr normal) {
  const Course *c = course_at(course);
  int32_t i = cell - 1;
  for (int32_t n = 0; n < 3; n++, i++) {
    if (course_cell_valid(course, i) == 0) continue;
    if (course_list_entry_set(c, 0, i) &&
        chunk_ground_below(pos, c->chunks[0] + (uint32_t)((const int32_t *)hw_ptr(c->lists[0]))[i], distance,
                           normal) != 0)
      return 1;
    if (!course_list_entry_set(c, 1, i)) continue;
    if (chunk_ground_below(pos, c->chunks[1] + (uint32_t)((const int32_t *)hw_ptr(c->lists[1]))[i], distance,
                           normal) != 0)
      return 1;
  }
  return 0;
}
HW_EXPORT(8c0419da, RET(course_ground_below(ARG0, ARG1, (int32_t)ARG2, ARG3, ARG4)))

/* was func_8C0419B2: the ground below pos, around the cell found by searching the course for it */
int32_t course_ground_below_search(gaddr course, gaddr pos, gaddr distance, gaddr normal) {
  int32_t cell = course_cell_search(course, pos);
  return course_ground_below(course, pos, cell, distance, normal);
}
HW_EXPORT(8c0419b2, RET(course_ground_below_search(ARG0, ARG1, ARG2, ARG3)))
