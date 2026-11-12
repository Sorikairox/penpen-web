/* Pen Pen port, S13 (batch b): the course object's helpers (0x8C04112A-0x8C042D80): its tear-down, its tables
 * (bit sets per row, sorted 6-byte records, -1-terminated word lists), the quad test of the ground and a few
 * setters. S14: Course and CourseEnv are declared in game/course.h. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/file.h"
#include "../core/view.h"
#include "course.h"
#include "racer.h"

/* func_8C062754 (SHC runtime): destroy `n` objects of `size` bytes at `p` with the destructor `dtor` */
static void array_destroy(int32_t n, uint32_t size, gaddr p, gaddr dtor) { CALL4(0x8C062754u, n, size, p, dtor); }

#define COURSE_SHARED_TEXLIST 0x8C09703Cu                      /* a texture list the courses share */
#define COURSE_PART_COUNTS ((const int32_t *)hw_ptr(0x8C0832E4u)) /* part blocks per course */
#define COURSE_ENV_DESTROY 0x8C04278Eu                         /* CourseEnv's destructor (a jump to func_8C042920) */

/* was func_8C04112A: release everything the course loaded: its textures, its chunk models, the part blocks, its two
 * files, and destroy its four CourseEnvs */
void course_release(gaddr course) {
  Course *c = course_at(course);
  njReleaseTexture(c->texlist);
  syFree(c->texlist);
  syFree(c->chunks[0]);
  syFree(c->chunks[1]);
  syFree(c->chunks[2]);
  syFree(c->chunks[3]);
  njReleaseTexture(COURSE_SHARED_TEXLIST);
  njReleaseTexture(c->bg_texlist);
  syFree(c->bg_texlist);
  for (int32_t i = 0; i < COURSE_PART_COUNTS[(uint32_t)c->course]; i++) syFree(c->part_blocks[i]);
  file_free(c->file);
  file_free(c->file2);
  array_destroy(COURSE_ENVS, sizeof(CourseEnv), member_addr(course, Course, envs), COURSE_ENV_DESTROY);
}
HW_EXPORT(8c04112a, course_release(ARG0))

/* was func_8C0417D2: bit `bit` of view `row`'s bit set (Course.view_bits): 1 or 0. (S13 had read the other half of
 * the literal-pool word, 0x1BC, for the table's offset: that was its difftest failure; fixed in S14) */
int32_t course_row_bit(gaddr course, int32_t row, int32_t bit) {
  const uint32_t *bits = course_at(course)->view_bits[row];
  return (bits[bit >> 5] & (1u << (bit & 31))) != 0 ? 1 : 0;
}
HW_EXPORT(8c0417d2, RET(course_row_bit(ARG0, (int32_t)ARG1, (int32_t)ARG2)))

/* was func_8C041806: the first segment whose first cell is >= cell, or the -1 terminator */
gaddr course_find_segment(gaddr course, int32_t cell) {
  gaddr rec = course_at(course)->segments;
  while (((const CourseSegment *)hw_ptr(rec))->first_cell != -1) {
    if (((const CourseSegment *)hw_ptr(rec))->first_cell >= cell) break;
    rec += sizeof(CourseSegment);
  }
  return rec;
}
HW_EXPORT(8c041806, RET(course_find_segment(ARG0, (int32_t)ARG1)))

/* was func_8C041D42: 1 when i is a cell of the course */
int32_t course_cell_valid(gaddr course, int32_t i) { return i >= 0 && i < course_at(course)->cell_count; }
HW_EXPORT(8c041d42, RET(course_cell_valid(ARG0, (int32_t)ARG1)))

/* entry i of the course's word list `list` is set (not -1) */
static int32_t list_entry_set(gaddr course, int list, int32_t i) {
  return ((const int32_t *)hw_ptr(course_at(course)->lists[list]))[i] != -1;
}
/* was func_8C041D54 / func_8C041D6C: entry i of lists 0 / 1 is set */
HW_EXPORT(8c041d54, RET(list_entry_set(ARG0, 0, (int32_t)ARG1)))
HW_EXPORT(8c041d6c, RET(list_entry_set(ARG0, 1, (int32_t)ARG1)))
/* was func_8C041D84 / func_8C041DAA: the same for the optional lists 2 / 3 (0 when there is none) */
int32_t optional_list_entry_set(gaddr course, int list, int32_t i) {
  if (course_at(course)->lists[list] == 0) return 0;
  return list_entry_set(course, list, i);
}
HW_EXPORT(8c041d84, RET(optional_list_entry_set(ARG0, 2, (int32_t)ARG1)))
HW_EXPORT(8c041daa, RET(optional_list_entry_set(ARG0, 3, (int32_t)ARG1)))

/* ---- the ground's quads ---- */

#define g_quad_edges ((const int32_t *)hw_ptr(0x8C096F24u)) /* the corner pairs of a quad's four edges */

/* was func_8C041BD2: whether the point is inside the quad (corners: four entries of the 16-bit index list into the
 * vertex array), seen from above: on the inner side of each edge (y of the cross product not negative). The
 * vectors are made with the Vec3 helpers on the guest stack. */
int32_t quad_contains(gaddr course, gaddr point, gaddr indices, gaddr vertices) {
  gaddr tmp = hw_stack_alloc(3 * sizeof(Vec3));
  gaddr edge = tmp, to_point = tmp + 12, corner = tmp + 24;
  int32_t inside = 1;
  const int32_t *pair = g_quad_edges;
  method_self(edge);
  method_self(to_point);
  method_self(corner);
  for (int32_t k = 0; k < 4; k++) {
    int32_t i0 = ((const int16_t *)hw_ptr(indices))[*pair++];
    int32_t i1 = ((const int16_t *)hw_ptr(indices))[*pair++];
    const Vec3 *v0 = vec3_at(vertices + 12u * (uint32_t)i0);
    vec3_set(v0->x, v0->y, v0->z, corner);
    const Vec3 *v1 = vec3_at(vertices + 12u * (uint32_t)i1);
    vec3_set(v1->x, v1->y, v1->z, edge);
    vec3_sub(edge, corner);
    vec3_copy_bytes(to_point, point);
    vec3_sub(to_point, corner);
    if (fgt(F32(0.0f), vec3_cross_y(edge, to_point))) {
      inside = 0;
      break;
    }
  }
  hw_stack_free(3 * sizeof(Vec3));
  return inside;
}
HW_EXPORT(8c041bd2, RET(quad_contains(ARG0, ARG1, ARG2, ARG3)))

/* ---- per-course values ---- */

#define COURSE_VALUE_TABLES ((const gaddr *)hw_ptr(0x8C096F54u)) /* a float table per course (by 0x2C) */

/* was func_8C042064: the course's value for segment i (-1: the start segment) */
f32 course_value(gaddr course, int32_t i) {
  const Course *c = course_at(course);
  gaddr table = COURSE_VALUE_TABLES[(uint32_t)c->course];
  if (i == -1) i = c->start_segment;
  return ((const f32 *)hw_ptr(table))[i];
}
HW_EXPORT(8c042064, RETF(course_value(ARG0, (int32_t)ARG1)))

/* was func_8C042316 / func_8C04231E: a view's fog on / off (the CourseEnv's set-up calls them on g_views[i]) */
void view_fog_enable(gaddr view) { view_at(view)->fog = 1; }
void view_fog_disable(gaddr view) { view_at(view)->fog = 0; }
HW_EXPORT(8c042316, view_fog_enable(ARG0))
HW_EXPORT(8c04231e, view_fog_disable(ARG0))

/* was func_8C04252E: a Body's force += (x, y, z) (the weather particles') */
HW_EXPORT(8c04252e, vec3_add_xyz(FARG0, FARG1, FARG2, member_addr(ARG0, Body, force)))

/* was func_8C042792: a Body at rest, through the setters that return the vector */
gaddr body_reset_set2(gaddr b) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(b, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(b, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(b, Body, force));
  return vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(b, Body, damping));
}
HW_EXPORT(8c042792, RET(body_reset_set2(ARG0)))

/* was func_8C042920: a CourseEnv's destructor body: free its particles' arrays */
void course_env_free(gaddr env) {
  const CourseEnv *e = course_env_at(env);
  gaddr p = e->particle_anims;
  if (p != 0) syFree(p);
  p = e->particles;
  if (p != 0) syFree(p);
}
HW_EXPORT(8c042920, course_env_free(ARG0))

/* was func_8C042D60: a blend by t in -1..1: s = (t + 1) / 2, a * s + b * (1 - s) */
f32 blend_signed(f32 t, f32 a, f32 b) {
  f32 s = fmul(fadd(t, F32(1.0f)), F32(0.5f));
  return fmac(b, fsub(F32(1.0f), s), fmul(s, a));
}
HW_EXPORT(8c042d60, RETF(blend_signed(FARG0, FARG1, FARG2)))

/* was func_8C03FA44: whether the point is between the two edges of quad `quad` of the racer's lane (a vertex pair
 * per row, quad q from vertex 2q): not right of the first edge (vertex 0 to 1) and not left of the second (2 to
 * 3), seen from above. The vectors are made with the Vec3 helpers on the guest stack. */
int32_t lane_quad_contains(gaddr racer, gaddr point, int32_t quad) {
  gaddr v = ((const LaneRacer *)hw_ptr(racer))->lane_points + (uint32_t)quad * (2 * sizeof(Vec3));
  gaddr tmp = hw_stack_alloc(6 * sizeof(Vec3));
  gaddr a = tmp, b = tmp + 12, c = tmp + 24, d = tmp + 36, edge = tmp + 48, to_point = tmp + 60;
  vec3_copy(a, v);
  vec3_copy(b, v + 12);
  vec3_copy(c, v + 24);
  vec3_copy(d, v + 36);
  int32_t inside = 0;
  vec3_copy(edge, b);
  vec3_sub(edge, a);
  vec3_copy(to_point, point);
  vec3_sub(to_point, a);
  if (!fgt(vec3_cross_y(edge, to_point), F32(0.0f))) {
    vec3_copy_bytes(edge, d);
    vec3_sub(edge, c);
    vec3_copy_bytes(to_point, point);
    vec3_sub(to_point, c);
    if (!fgt(F32(0.0f), vec3_cross_y(edge, to_point))) inside = 1;
  }
  hw_stack_free(6 * sizeof(Vec3));
  return inside;
}
HW_EXPORT(8c03fa44, RET(lane_quad_contains(ARG0, ARG1, (int32_t)ARG2)))

static void quat_set_identity(Quat *q) {
  q->x = F32(0.0f);
  q->y = F32(0.0f);
  q->z = F32(0.0f);
  q->w = F32(1.0f);
}

/* was func_8C042668: CourseEnv's constructor: three rotations the identity, four Bodies at rest (body5c and the
 * light rig), no particle arrays, not active; returns it */
gaddr course_env_construct(gaddr env) {
  CourseEnv *e = course_env_at(env);
  quat_set_identity(&e->rot3c);
  quat_set_identity(&e->rot4c);
  body_reset_set2(member_addr(env, CourseEnv, body5c));
  body_reset_set2(member_addr(env, CourseEnv, lights[0]));
  body_reset_set2(member_addr(env, CourseEnv, lights[1]));
  body_reset_set2(member_addr(env, CourseEnv, lights[2]));
  quat_set_identity(&e->rot1ac);
  e->active = 0;
  e->particles = 0;
  e->particle_anims = 0;
  return env;
}
HW_EXPORT(8c042668, RET(course_env_construct(ARG0)))
