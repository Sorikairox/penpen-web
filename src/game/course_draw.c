/* Pen Pen port, S16 (b05): the course's draw for the current view (func_8C0413CC; the S15 lists cut it in two at
 * func_8C0413FA, the half after the njSin call, which has no entry of its own).
 *
 * The view's CourseEnv learns where the camera is (its position, the cell under it, the camera's flat direction and
 * the course's path point at that cell). The cells drawn depend on how the camera looks along the course: the dot
 * product of its direction with the path point picks one of four ranges (looking ahead, back, or across either
 * way), each from the course's per-cell tables of how many cells are seen. Each drawn cell draws its model from
 * up to three of the course's chunk lists (lists 0 and 2 with the second model call where the cell's attribute
 * has bit 5) and is marked in the view's bit set (Course.view_bits, cleared first). Then the CourseEnv updates
 * (unless the course task is paused) and draws. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "../core/inline.h"   /* method_self, course_cell_attr */
#include "../core/task.h"
#include "game.h"             /* panel_get */
#include "course.h"
#include "race_scene.h"       /* race_scene_view_cell */

#define LOOK_ALONG     F32_BITS(0x3F34FDF3u) /* 0.70700002: cos 45 degrees */
#define LOOK_BACK      F32_BITS(0xBF34FDF3u) /* -0.70700002 */
#define CELL_ATTR_ALT_MODEL 0x20             /* the cell's chunks of lists 0 and 2 draw with nj_ModelL2 */
#define DRAW_CONTROL   0x0C00u               /* njControl3D flags added while the course draws */
#define DRAW_ATTR_OR   0x0200u               /* njSetConstantAttr's or-mask */

/* the ranges of cells drawn, by how the camera looks along the course */
enum { LOOK_AHEAD = 0, LOOK_ACROSS_AHEAD = 1, LOOK_ACROSS_BACK = 2, LOOK_BEHIND = 3 };

/* nj_ModelL1 / nj_ModelL2 (Ninja): draw a chunk model (nj_ModelL1 is in sdk.h) */
static inline void nj_ModelL2(gaddr model) { CALL1(0x8C0D0C22u, model); }
/* njSetConstantAttr (Ninja): the constant attribute words (and-mask, or-mask) */
static inline void njSetConstantAttr(uint32_t and_attr, uint32_t or_attr) { CALL2(0x8C0A9672u, and_attr, or_attr); }
/* func_8C0407CE (game/camera.c, no header): the current view's heading in whole Ninja angle units, as a float */
static f32 view_heading(void) {
  CALL0(0x8C0407CEu);
  return hw_fret();
}
/* func_8C040860 (game/camera.c, no header): add flags to the current njControl3D flags */
static void control3d_add(uint32_t flags) { CALL1(0x8C040860u, flags); }
/* func_8C041D54 (game/course.c, static there): entry i of the course's list 0 is set (not -1) */
static int32_t course_list0_has(gaddr course, int32_t i) { return (int32_t)CALL2(0x8C041D54u, course, i); }
/* func_8C041D84 / func_8C041DAA (game/course.c, no header): the same for the optional lists 2 / 3 */
static int32_t course_list2_has(gaddr course, int32_t i) { return (int32_t)CALL2(0x8C041D84u, course, i); }
static int32_t course_list3_has(gaddr course, int32_t i) { return (int32_t)CALL2(0x8C041DAAu, course, i); }

/* the original's frame (0x50 bytes under PR): the Vec3s njInnerProduct and the CourseEnv setters read */
typedef struct CourseDrawFrame {
  uint8_t pad_00[0x20];  /* 0x00: counters (kept in variables here) */
  Vec3 path;             /* 0x20: the course's path point at the camera's cell */
  uint8_t pad_2c[0x04];  /* 0x2C: the cell */
  Vec3 direction;        /* 0x30: (sin heading, 0, cos heading) */
  f32 heading;           /* 0x3C */
  Vec3 eye;              /* 0x40: the current view's camera position */
  uint8_t rest[0x54 - 0x4C]; /* 0x4C: the course, PR */
} CourseDrawFrame;
HW_OFFSET(CourseDrawFrame, path, 0x20);
HW_OFFSET(CourseDrawFrame, direction, 0x30);
HW_OFFSET(CourseDrawFrame, eye, 0x40);
HW_SIZE(CourseDrawFrame, 0x54);

/* the chunk model of entry `cell` of the course's list `list` (an offset into chunks[list]) */
static gaddr course_chunk(const Course *c, int list, int32_t cell) {
  return c->chunks[list] + ((const uint32_t *)hw_ptr(c->lists[list]))[cell];
}

/* was func_8C0413CC: draw the course's cells seen from the current view, then update and draw its CourseEnv */
void course_draw(gaddr course) {
  Course *c = course_at(course);
  gaddr frame = hw_stack_alloc(sizeof(CourseDrawFrame));
  CourseDrawFrame *f = (CourseDrawFrame *)hw_ptr(frame);
  gaddr eye = member_addr(frame, CourseDrawFrame, eye);
  gaddr direction = member_addr(frame, CourseDrawFrame, direction);
  gaddr path = member_addr(frame, CourseDrawFrame, path);

  vec3_copy(eye, view_screen());
  f->heading = view_heading();
  method_self(direction); /* the local's (empty) constructor */
  f->direction.x = njSin(f32_to_int(f->heading));
  f->direction.y = F32(0.0f);
  f->direction.z = njCos(f32_to_int(f->heading));
  int32_t cell = race_scene_view_cell(panel_get(), -1);
  const Vec3 *point = vec3_at(c->cell_points + (uint32_t)cell * sizeof(Vec3));
  vec3_set(point->x, point->y, point->z, path);
  int32_t view = view_current_index();
  gaddr env = member_addr(course, Course, envs[view]);
  course_env_at(env)->eye = f->eye;          /* func_8C040886: a block copy */
  course_env_at(env)->cell = cell;           /* func_8C0408C2 */
  course_env_at(env)->vec20 = f->direction;  /* func_8C04089A */
  course_env_at(env)->vec2c = f->path;       /* func_8C0408AE */

  int32_t look = LOOK_AHEAD;
  if (course_cell_valid(course, cell)) {
    f32 along = njInnerProduct(direction, path);
    if (fgt(LOOK_ALONG, along)) {
      if (fgt(LOOK_BACK, along))
        look = LOOK_BEHIND;
      else if (fgt(along, F32(0.0f)))
        look = LOOK_ACROSS_AHEAD;
      else
        look = LOOK_ACROSS_BACK;
    }
  }
  /* the first cell, how many, and the direction: entries6c[0] / [1] hold per cell how many cells are seen ahead /
   * behind (signed bytes) */
  int32_t count = 0, first = 0, step = 0;
  switch (look) {
  case LOOK_AHEAD:
    count = ((const int8_t *)hw_ptr(c->entries6c[0]))[cell];
    first = cell - 1;
    step = 1;
    break;
  case LOOK_BEHIND:
    count = ((const int8_t *)hw_ptr(c->entries6c[1]))[cell];
    first = cell + 1;
    step = -1;
    break;
  case LOOK_ACROSS_AHEAD:
    count = ((const int8_t *)hw_ptr(c->entries6c[0]))[cell];
    first = cell - count / 4 - 1;
    count++;
    step = 1;
    break;
  case LOOK_ACROSS_BACK:
    count = ((const int8_t *)hw_ptr(c->entries6c[1]))[cell];
    first = cell + count / 4 + 1;
    count++;
    step = -1;
    break;
  default:
    break;
  }
  for (int32_t w = 0; w < c->cell_count / 32 + 1; w++)
    c->view_bits[view][w] = 0;

  control3d_push();
  control3d_add(DRAW_CONTROL);
  njSetConstantAttr(0xFFFFFFFFu, DRAW_ATTR_OR);
  njSetTexture(c->texlist);
  matrix_push();
  int32_t at = first;
  for (int32_t k = 0; k < count; k++, at += step) {
    if (!course_cell_valid(course, at))
      continue;
    int32_t alt = 0;
    if ((uint16_t)course_cell_attr(course, at) & CELL_ATTR_ALT_MODEL)
      alt = 1;
    if (course_list0_has(course, at)) {
      if (alt)
        nj_ModelL2(course_chunk(c, 0, at));
      else
        nj_ModelL1(course_chunk(c, 0, at));
    }
    if (course_list2_has(course, at)) {
      if (alt)
        nj_ModelL2(course_chunk(c, 2, at));
      else
        nj_ModelL1(course_chunk(c, 2, at));
    }
    if (course_list3_has(course, at))
      nj_ModelL1(course_chunk(c, 3, at));
    c->view_bits[view][at >> 5] |= 1u << (at & 31);
  }
  matrix_pop();
  control3d_pop();
  if (!task_is_paused(task_at(course)))
    course_env_update(env);
  course_env_draw(env);
  hw_stack_free(sizeof(CourseDrawFrame));
}
HW_EXPORT(8c0413cc, course_draw(ARG0))
