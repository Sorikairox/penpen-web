/* Pen Pen port, S13 (batch b): model and motion helpers (0x8C03851E-0x8C039232): a model's motion player, small
 * setters of the drawing classes, a shared data file loaded on first use.
 *
 * The motion player (a member of the animated models) plays motion `index` of its motion table (0x48: 8-byte
 * entries whose first word is the NJS_MOTION) from a request {next request, start frame, ...}; when it switches
 * motion it keeps the previous one and its frame (0x34 / 0x38) to blend from. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/file.h"

/* nj_ObjectB (Ninja, S2 name): draw a basic model tree */
static inline void nj_ObjectB(gaddr object) { CALL1(0x8C0C30A8u, object); }

/* ---- the motion player ---- */

typedef struct MotionRequest {
  gaddr next;          /* 0x00: the request that follows (0 = none: no blend) */
  f32 blend_speed;     /* 0x04 */
  f32 start;           /* 0x08: the first frame; below 0, counted from the motion's end */
  uint32_t unknown0c;  /* 0x0C */
  int32_t index;       /* 0x10: the motion */
  uint32_t flags;      /* 0x14 */
} MotionRequest;
HW_OFFSET(MotionRequest, flags, 0x14);

typedef struct MotionPlayer {
  uint32_t unknown00;  /* 0x00 */
  gaddr next;          /* 0x04: the request's follower */
  f32 blend;           /* 0x08: 0 = the previous motion, 1 = this one */
  f32 blend_speed;     /* 0x0C */
  uint8_t unknown10[0x20 - 0x10];
  gaddr request;       /* 0x20: MotionRequest */
  uint32_t flags;      /* 0x24 */
  uint32_t unknown28;  /* 0x28 */
  int32_t index;       /* 0x2C: the motion playing */
  f32 frame;           /* 0x30 */
  int32_t prev_index;  /* 0x34: the motion blended from */
  f32 prev_frame;      /* 0x38 */
  uint8_t unknown3c[0x48 - 0x3C];
  gaddr motions;       /* 0x48: {NJS_MOTION *, ...} per motion */
} MotionPlayer;
HW_OFFSET(MotionPlayer, request, 0x20);
HW_OFFSET(MotionPlayer, prev_frame, 0x38);
HW_OFFSET(MotionPlayer, motions, 0x48);

/* was func_8C03851E: start a request: with a follower, blend from the motion that was playing; the start frame
 * (counted back from the end when negative) */
void motion_player_start(gaddr player, gaddr request) {
  MotionPlayer *p = (MotionPlayer *)hw_ptr(player);
  MotionRequest *r = (MotionRequest *)hw_ptr(request);
  p->request = request;
  p->next = r->next;
  if (r->next != 0) {
    p->prev_index = p->index;
    p->index = r->index;
    p->prev_frame = p->frame;
    p->blend = F32(0.0f);
    p->blend_speed = r->blend_speed;
  } else {
    p->index = r->index;
    p->prev_index = r->index;
  }
  f32 frame;
  if (fgt(F32(0.0f), r->start)) {
    gaddr motion = U32_AT(p->motions + 8u * (uint32_t)p->index);
    frame = fadd(f32_from_uint(U32_AT(motion + 4)), r->start); /* nb_frames + start */
  } else {
    frame = r->start;
  }
  p->frame = frame;
  p->flags = r->flags;
}
HW_EXPORT(8c03851e, motion_player_start(ARG0, ARG1))

/* ---- small inits and setters ---- */

/* was func_8C038788: a 0x18-byte record cleared (five words, two halfwords); returns it */
gaddr record18_clear(gaddr rec) {
  for (uint32_t off = 0; off < 0x14; off += 4) U32_AT(rec + off) = 0;
  *(uint16_t *)hw_ptr(rec + 0x14) = 0;
  *(uint16_t *)hw_ptr(rec + 0x16) = 0;
  return rec;
}
HW_EXPORT(8c038788, RET(record18_clear(ARG0)))

/* was func_8C0388AC: a Body at rest (the Vec3 helpers' copies) */
HW_EXPORT(8c0388ac, body_reset(ARG0))

/* was func_8C03890C / func_8C038918: the object's float at 0x24 / 0x2C */
HW_EXPORT(8c03890c, field_set_f32(ARG0, 0x24, FARG0))
HW_EXPORT(8c038918, field_set_f32(ARG0, 0x2C, FARG0))
/* was func_8C03895A / func_8C038964: the object's float at 0x0C / 0x14 += x */
HW_EXPORT(8c03895a, f32_add_to(FARG0, ARG0 + 0x0C))
HW_EXPORT(8c038964, f32_add_to(FARG0, ARG0 + 0x14))

/* was func_8C038FF0: draw the model tree the object points to */
void model_draw(gaddr obj) { nj_ObjectB(U32_AT(obj)); }
HW_EXPORT(8c038ff0, model_draw(ARG0))

/* was func_8C0391F2: a Body at rest, through the setters that return the vector */
gaddr body_reset_set(gaddr b) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x00);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x0C);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), b + 0x18);
  return vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), b + 0x24);
}
HW_EXPORT(8c0391f2, RET(body_reset_set(ARG0)))

/* ---- a data file shared by the objects that use it, loaded on first use ---- */

#define g_shared_data      GUEST_VAR(gaddr, 0x8C096D1Cu) /* the container (0 = not loaded) */
#define g_shared_entry0    GUEST_VAR(gaddr, 0x8C096D14u) /* its first entry's data */
#define g_shared_entry1    GUEST_VAR(gaddr, 0x8C096D18u) /* its second entry's data */
#define g_shared_word      GUEST_VAR(uint32_t, 0x8C096D10u)
#define g_word_8c1b7e40    GUEST_VAR(uint32_t, 0x8C1B7E40u)
#define SHARED_DATA_NAME   0x8C083128u                   /* the file's name (in the game's data) */

/* was func_8C038AFC: load the shared container if it is not yet, and note its two entries */
void shared_data_load(void) {
  if (g_shared_data != 0) return;
  gaddr data = container_load(SHARED_DATA_NAME, 0);
  g_shared_data = data;
  g_shared_entry0 = U32_AT(data + 4);
  g_shared_entry1 = U32_AT(g_shared_data + 0x10 + 4);
  g_shared_word = g_word_8c1b7e40;
}
HW_EXPORT(8c038afc, shared_data_load())

/* ---- ribbons: a trail of point pairs (two edges) that follows a moving matrix ----
 *
 * The points live in an array of 4 * RIBBON_LENGTH Vec3 (the array is the first word of the object the ribbon's
 * first word points to); new pairs go in at the head and the trail moves back by a pair each frame. */

#define g_ribbon_length GUEST_VAR(int32_t, 0x8C082FACu) /* pairs per half (32) */
#define g_ribbon_height GUEST_VAR(f32, 0x8C082FA8u)     /* 3.5: the edges' height above the matrix's origin */
#define VEC3_CONSTRUCT 0x8C03898Cu                      /* Vec3's empty constructor (for the array helper) */

/* func_8C062800 (SHC runtime): construct `n` objects of `size` bytes at `p` with the constructor `ctor` */
static void array_construct(int32_t n, uint32_t size, gaddr p, gaddr ctor) { CALL4(0x8C062800u, n, size, p, ctor); }
/* func_8C062790 (SHC runtime): the same for a block from operator new[] (returns it, or 0) */
static gaddr array_construct_new(int32_t n, uint32_t size, gaddr p, gaddr ctor) {
  return CALL4(0x8C062790u, n, size, p, ctor);
}

static gaddr ribbon_points(gaddr ribbon) { return U32_AT(U32_AT(ribbon)); }
static Vec3 *ribbon_point(gaddr points, int32_t i) { return vec3_at(points + 12u * (uint32_t)i); }

/* the pair at the matrix: left and right of the origin at (+-5, y, z), turned by the matrix (on the guest stack:
 * Ninja writes the results); out[0] = left, out[1] = right */
static void ribbon_edge_pair(gaddr m, f32 y, f32 z, Vec3 out[2]) {
  gaddr tmp = hw_stack_alloc(4 * sizeof(Vec3));
  gaddr src = tmp, dst = tmp + 2 * sizeof(Vec3);
  array_construct(2, sizeof(Vec3), src, VEC3_CONSTRUCT);
  array_construct(2, sizeof(Vec3), dst, VEC3_CONSTRUCT);
  *vec3_at(src) = (Vec3){F32(5.0f), y, z};
  *vec3_at(src + sizeof(Vec3)) = (Vec3){F32(-5.0f), y, z};
  njCalcPoint(m, src, dst);
  njCalcPoint(m, src + sizeof(Vec3), dst + sizeof(Vec3));
  out[0] = *vec3_at(dst);
  out[1] = *vec3_at(dst + sizeof(Vec3));
  hw_stack_free(4 * sizeof(Vec3));
}

/* was func_8C038FFE: move the trail back by a pair and put the pair at the object's matrix at the head (height
 * from the object's floats at 0x70 / 0x74, just behind the origin) */
void ribbon_push(gaddr ribbon, gaddr obj) {
  gaddr points = ribbon_points(ribbon);
  int32_t last = 4 * g_ribbon_length - 1;
  for (int32_t i = last; i >= 2; i--) *ribbon_point(points, i) = *ribbon_point(points, i - 2);
  f32 y = fadd(fsub(F32_AT(obj + 0x74), F32_AT(obj + 0x70)), F32_BITS(0x3EFFFFFFu)); /* + 0.49999997 */
  Vec3 pair[2];
  ribbon_edge_pair(obj, y, F32_BITS(0xBF7FFFFFu), pair);                              /* z = -0.99999994 */
  *ribbon_point(points, 1) = pair[1];
  *ribbon_point(points, 0) = pair[0];
}
HW_EXPORT(8c038ffe, ribbon_push(ARG0, ARG1))

/* was func_8C039114: fill the whole trail with the pair at the matrix (edges at height 3.5 - drop, 5 ahead) */
void ribbon_reset(gaddr ribbon, f32 drop, gaddr m) {
  gaddr points = ribbon_points(ribbon);
  Vec3 pair[2];
  ribbon_edge_pair(m, fsub(g_ribbon_height, drop), F32(5.0f), pair);
  int32_t i = 4 * g_ribbon_length - 1;
  for (int32_t k = 2 * g_ribbon_length - 1; k >= 0; k--) {
    *ribbon_point(points, i--) = pair[1];
    *ribbon_point(points, i--) = pair[0];
  }
}
HW_EXPORT(8c039114, ribbon_reset(ARG0, FARG0, ARG1))

/* ---- the ribbon set: per-ribbon arrays and their bodies ---- */

#define g_ribbon_kind      GUEST_VAR(int32_t, 0x8C08305Cu) /* which entry of the shared data's table */
#define g_ribbon_count     GUEST_VAR(int32_t, 0x8C083060u) /* 8 */
#define RIBBON_SET_DATA    0x8C096BE0u                     /* a static record the set points to */

/* was func_8C039232: build the set: its model and texture from the shared data's table, eight word arrays and the
 * bodies (one per ribbon), the third array cleared; returns the set */
gaddr ribbon_set_construct(gaddr set) {
  U32_AT(set + 0x24) = U32_AT(g_shared_entry1 + 12u * (uint32_t)g_ribbon_kind);
  U32_AT(set + 0x28) = U32_AT(g_shared_entry1 + 12u * (uint32_t)g_ribbon_kind + 4);
  U32_AT(set + 0x2C) = (uint32_t)g_ribbon_count;
  for (uint32_t off = 0x34; off <= 0x50; off += 4) {
    int32_t n = (int32_t)U32_AT(set + 0x2C);
    U32_AT(set + off) = syMalloc((uint32_t)(n ? n : 1) * 4);
  }
  int32_t n = (int32_t)U32_AT(set + 0x2C);
  gaddr block = syMalloc((uint32_t)(n ? n : 1) * 0x30);
  gaddr bodies = 0;
  if (block != 0) bodies = array_construct_new((int32_t)U32_AT(set + 0x2C), 0x30, block, 0x8C0391F2u /* body_reset_set */);
  U32_AT(set + 0x54) = bodies;
  U32_AT(set + 0x14) = 0;
  U32_AT(set + 0x18) = g_shared_word;
  U32_AT(set + 0x1C) = RIBBON_SET_DATA;
  gaddr words = U32_AT(set + 0x3C);
  for (int32_t k = (int32_t)U32_AT(set + 0x2C); k > 0; k--) {
    U32_AT(words) = 0;
    words += 4;
  }
  return set;
}
HW_EXPORT(8c039232, RET(ribbon_set_construct(ARG0)))

/* free the blocks at these offsets of an object (each when not 0), in this order */
static void free_blocks(gaddr obj, const uint16_t *offsets, int n) {
  for (int i = 0; i < n; i++) {
    gaddr p = U32_AT(obj + offsets[i]);
    if (p != 0) syFree(p);
  }
}

/* was func_8C0393C2: free the set's arrays and bodies */
void ribbon_set_free(gaddr set) {
  static const uint16_t offsets[] = {0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54};
  free_blocks(set, offsets, 9);
}
HW_EXPORT(8c0393c2, ribbon_set_free(ARG0))

/* was func_8C03AF80: build an object set of `count` objects (freed by func_8C03B10A): the shared data's words, seven
 * word arrays (0x3C, 0x34, 0x38, 0x40 to 0x4C) and the bodies (0x50), the array at 0x38 cleared; returns the set */
gaddr object_set_b_construct(gaddr set, int32_t count) {
  U32_AT(set + 0x14) = 0;
  U32_AT(set + 0x18) = g_shared_word;
  U32_AT(set + 0x1C) = RIBBON_SET_DATA;
  U32_AT(set + 0x2C) = (uint32_t)count;
  U32_AT(set + 0x30) = 1;
  static const uint16_t arrays[] = {0x3C, 0x34, 0x38, 0x40, 0x44, 0x48, 0x4C};
  for (int i = 0; i < 7; i++) {
    int32_t n = (int32_t)U32_AT(set + 0x2C);
    U32_AT(set + arrays[i]) = syMalloc((uint32_t)(n ? n : 1) * 4);
  }
  int32_t n = (int32_t)U32_AT(set + 0x2C);
  gaddr block = syMalloc((uint32_t)(n ? n : 1) * 0x30);
  gaddr bodies = 0;
  if (block != 0) bodies = array_construct_new((int32_t)U32_AT(set + 0x2C), 0x30, block, 0x8C0391F2u /* body_reset_set */);
  U32_AT(set + 0x50) = bodies;
  gaddr words = U32_AT(set + 0x38);
  for (int32_t k = (int32_t)U32_AT(set + 0x2C); k > 0; k--) {
    U32_AT(words) = 0;
    words += 4;
  }
  return set;
}
HW_EXPORT(8c03af80, RET(object_set_b_construct(ARG0, (int32_t)ARG1)))
