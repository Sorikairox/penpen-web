/* Pen Pen port, S13 (batch b): the course object's helpers (0x8C04112A-0x8C042D80): its tear-down, its tables
 * (bit sets per row, sorted 6-byte records, -1-terminated word lists), the quad test of the ground and a few
 * setters. Field names are from what these functions do with them. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/file.h"

/* func_8C062754 (SHC runtime): destroy `n` objects of `size` bytes at `p` with the destructor `dtor` */
static void array_destroy(int32_t n, uint32_t size, gaddr p, gaddr dtor) { CALL4(0x8C062754u, n, size, p, dtor); }

#define COURSE_SHARED_TEXLIST 0x8C09703Cu                      /* a texture list the courses share */
#define COURSE_PART_COUNTS ((const int32_t *)hw_ptr(0x8C0832E4u)) /* blocks at 0xA4 per course (by 0x2C) */
#define COURSE_PART_DESTROY 0x8C04278Eu                        /* the parts' destructor (a jump to func_8C042920) */

/* was func_8C04112A: release everything the course loaded: its textures, its arrays, the per-part blocks, its two
 * files, and destroy its four parts (0x1BC bytes each at 0x4EE4) */
void course_release(gaddr course) {
  njReleaseTexture(U32_AT(course + 0x50));
  syFree(U32_AT(course + 0x50));
  syFree(U32_AT(course + 0x40));
  syFree(U32_AT(course + 0x44));
  syFree(U32_AT(course + 0x48));
  syFree(U32_AT(course + 0x4C));
  njReleaseTexture(COURSE_SHARED_TEXLIST);
  njReleaseTexture(U32_AT(course + 0xA0));
  syFree(U32_AT(course + 0xA0));
  for (int32_t i = 0; i < COURSE_PART_COUNTS[U32_AT(course + 0x2C)]; i++) syFree(U32_AT(course + 0xA4 + 4u * (uint32_t)i));
  file_free(U32_AT(course + 0x38));
  file_free(U32_AT(course + 0x3C));
  array_destroy(4, 0x1BC, course + 0x4EE4, COURSE_PART_DESTROY);
}
HW_EXPORT(8c04112a, course_release(ARG0))

/* was func_8C0417D2: bit `bit` of row `row` of the bit table at 0x1BC (40 bytes a row): 1 or 0 */
int32_t course_row_bit(gaddr course, int32_t row, int32_t bit) {
  uint32_t word = U32_AT(course + 0x1BC + 40u * (uint32_t)row + 4u * (uint32_t)(bit >> 5));
  return (word & (1u << (bit & 31))) != 0 ? 1 : 0;
}
/* not exported (S13 difftest: differs from the original, to redo): HW_EXPORT(8c0417d2, RET(course_row_bit(ARG0, (int32_t)ARG1, (int32_t)ARG2))) */

/* was func_8C041806: the first 6-byte record of the list at 0x84 whose (signed 16-bit) key is >= key, or the -1
 * terminator */
gaddr course_find_record(gaddr course, int32_t key) {
  gaddr rec = U32_AT(course + 0x84);
  while (*(int16_t *)hw_ptr(rec) != -1) {
    if (*(int16_t *)hw_ptr(rec) >= key) break;
    rec += 6;
  }
  return rec;
}
HW_EXPORT(8c041806, RET(course_find_record(ARG0, (int32_t)ARG1)))

/* was func_8C041D42: 1 when 0 <= i < the count at 0x30 */
int32_t course_index_valid(gaddr course, int32_t i) { return i >= 0 && i < (int32_t)U32_AT(course + 0x30); }
HW_EXPORT(8c041d42, RET(course_index_valid(ARG0, (int32_t)ARG1)))

/* entry i of the word list the member at `offset` points to is set (not -1) */
static int32_t list_entry_set(gaddr course, uint32_t offset, int32_t i) {
  return (int32_t)U32_AT(U32_AT(course + offset) + 4u * (uint32_t)i) != -1;
}
/* was func_8C041D54 / func_8C041D6C: entry i of the lists at 0x54 / 0x58 is set */
HW_EXPORT(8c041d54, RET(list_entry_set(ARG0, 0x54, (int32_t)ARG1)))
HW_EXPORT(8c041d6c, RET(list_entry_set(ARG0, 0x58, (int32_t)ARG1)))
/* was func_8C041D84 / func_8C041DAA: the same for the optional lists at 0x5C / 0x60 (0 when there is none) */
int32_t optional_list_entry_set(gaddr course, uint32_t offset, int32_t i) {
  if (U32_AT(course + offset) == 0) return 0;
  return list_entry_set(course, offset, i);
}
HW_EXPORT(8c041d84, RET(optional_list_entry_set(ARG0, 0x5C, (int32_t)ARG1)))
HW_EXPORT(8c041daa, RET(optional_list_entry_set(ARG0, 0x60, (int32_t)ARG1)))

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

/* was func_8C042064: the course's value i (-1: the current one, 0x572C) */
f32 course_value(gaddr course, int32_t i) {
  gaddr table = COURSE_VALUE_TABLES[U32_AT(course + 0x2C)];
  if (i == -1) i = (int32_t)U32_AT(course + 0x572C);
  return F32_AT(table + 4u * (uint32_t)i);
}
HW_EXPORT(8c042064, RETF(course_value(ARG0, (int32_t)ARG1)))

/* was func_8C042316 / func_8C04231E: the switch at 0xF0 on / off */
HW_EXPORT(8c042316, field_set(ARG0, 0xF0, 1))
HW_EXPORT(8c04231e, field_set(ARG0, 0xF0, 0))

/* was func_8C04252E: the object's Vec3 at 0x18 += (x, y, z) */
HW_EXPORT(8c04252e, vec3_add_xyz(FARG0, FARG1, FARG2, ARG0 + 0x18))

/* was func_8C042792: a Body at rest, through the setters that return the vector */
gaddr body_reset_set2(gaddr b) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x00);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x0C);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x18);
  return vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), b + 0x24);
}
HW_EXPORT(8c042792, RET(body_reset_set2(ARG0)))

/* was func_8C042920: a course part's destructor body: free its two blocks (0x130, 0x12C) */
void course_part_free(gaddr part) {
  gaddr p = U32_AT(part + 0x130);
  if (p != 0) syFree(p);
  p = U32_AT(part + 0x12C);
  if (p != 0) syFree(p);
}
HW_EXPORT(8c042920, course_part_free(ARG0))

/* was func_8C042D60: a blend by t in -1..1: s = (t + 1) / 2, a * s + b * (1 - s) */
f32 blend_signed(f32 t, f32 a, f32 b) {
  f32 s = fmul(fadd(t, F32(1.0f)), F32(0.5f));
  return fmac(b, fsub(F32(1.0f), s), fmul(s, a));
}
HW_EXPORT(8c042d60, RETF(blend_signed(FARG0, FARG1, FARG2)))

/* was func_8C03FA44: whether the point is between the two edges of quad `quad` of the object's strip (0x61C: a
 * vertex pair per row, quad q from vertex 2q): not right of the first edge (vertex 0 to 1) and not left of the
 * second (2 to 3), seen from above. The vectors are made with the Vec3 helpers on the guest stack. */
int32_t strip_quad_contains(gaddr obj, gaddr point, int32_t quad) {
  gaddr v = U32_AT(obj + 0x61C) + (uint32_t)quad * 24u;
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
HW_EXPORT(8c03fa44, RET(strip_quad_contains(ARG0, ARG1, (int32_t)ARG2)))

static void quat_set_identity(gaddr q) {
  F32_AT(q + 0x0) = F32(0.0f);
  F32_AT(q + 0x4) = F32(0.0f);
  F32_AT(q + 0x8) = F32(0.0f);
  F32_AT(q + 0xC) = F32(1.0f);
}

/* was func_8C042668: a course part's constructor: three rotations (0x3C, 0x4C, 0x1AC) the identity, four Bodies
 * (0x5C, 0x98, 0xC8, 0xF8) at rest, no blocks (0x12C, 0x130) and the word at 4 cleared; returns the part */
gaddr course_part_construct(gaddr part) {
  quat_set_identity(part + 0x3C);
  quat_set_identity(part + 0x4C);
  body_reset_set2(part + 0x5C);
  body_reset_set2(part + 0x98);
  body_reset_set2(part + 0xC8);
  body_reset_set2(part + 0xF8);
  quat_set_identity(part + 0x1AC);
  U32_AT(part + 0x04) = 0;
  U32_AT(part + 0x12C) = 0;
  U32_AT(part + 0x130) = 0;
  return part;
}
HW_EXPORT(8c042668, RET(course_part_construct(ARG0)))
