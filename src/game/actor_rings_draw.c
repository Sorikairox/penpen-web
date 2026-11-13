/* Pen Pen port, S18 b05: the draw of the rings (type 0x2C) (game/actor.h: Rings; the rest is in S14's files). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"

/* SDK njColorBlendingMode (0x8C0A23A8) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

#define RING_TEXLIST      0x8C09703Cu           /* the shared NJS_TEXLIST (a static of the binary) */
#define RING_TEXANIM      0x8C097044u           /* its NJS_TEXANIM */
#define RING_SIZE_SCALE   F32(64.0f)            /* a sprite's scale: size / 64 + 0.6 */
#define RING_SIZE_BASE    F32_BITS(0x3F199999u) /* 0.6 */
#define RING_RISE         F32(4.0f)             /* a ring's sprite sits at (size, 0, size / 4) in its frame */
#define RING_CELLS        3                     /* the cells 1..4 by the ring's number */
#define RING_ATTR         0x30u                 /* njDrawSprite3D's attribute */

/* the original's frame (0xBC bytes under ten saved registers and PR): its matrices, vectors and sprite go to
 * Ninja */
typedef struct RingsDrawFrame {
  uint32_t spill;    /* 0x00 */
  Vec3 offset;       /* 0x04 */
  Vec3 point;        /* 0x10 */
  Matrix ring;       /* 0x1C: the plane turned by the ring's turn */
  Matrix plane;      /* 0x5C: the rings' plane */
  NjSprite sprite;   /* 0x9C */
  uint32_t saved[10]; /* 0xBC: PR, fr13-fr15, r9-r14 */
} RingsDrawFrame;
HW_OFFSET(RingsDrawFrame, offset, 0x04);
HW_OFFSET(RingsDrawFrame, ring, 0x1C);
HW_OFFSET(RingsDrawFrame, plane, 0x5C);
HW_OFFSET(RingsDrawFrame, sprite, 0x9C);
HW_SIZE(RingsDrawFrame, 0xE4);

/* was func_8C05C26A: draw the rings: blending modes 9 / 10; the plane turned about x, y, z by its angles; each of
 * the 40 rings a sprite (cell 1-4 by its number) at (size, 0, size / 4) turned about z by its turn in the plane,
 * from the Body, scaled size / 64 + 0.6 */
void rings_draw(gaddr obj) {
  const Rings *r = (const Rings *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(RingsDrawFrame));
  RingsDrawFrame *f = (RingsDrawFrame *)hw_ptr(frame);
  gaddr plane = member_addr(frame, RingsDrawFrame, plane);
  gaddr ring = member_addr(frame, RingsDrawFrame, ring);
  gaddr offset = member_addr(frame, RingsDrawFrame, offset);
  gaddr point = member_addr(frame, RingsDrawFrame, point);
  NjSprite *sp = &f->sprite;
  sp->tlist = RING_TEXLIST;
  sp->tanim = RING_TEXANIM;
  njColorBlendingMode(0, 9);
  njColorBlendingMode(1, 10);
  sp->ang = 0;
  matrix_identity(plane); /* the matrix's inline constructor */
  njUnitMatrix(plane);
  njRotateX(plane, f32_to_int(r->angle.x));
  njRotateY(plane, f32_to_int(r->angle.y));
  njRotateZ(plane, f32_to_int(r->angle.z));
  for (int32_t i = 0; i < RINGS; i++) {
    njSetMatrix(ring, plane);
    njRotateZ(ring, f32_to_int(r->turns[i]));
    f32 size = r->sizes[i].value; /* the unit's inline getter (func_8C05BE58) */
    f32 scale = fadd(fdiv(size, RING_SIZE_SCALE), RING_SIZE_BASE);
    sp->sy = scale;
    sp->sx = scale;
    method_self(point); /* the Vec3's (empty) constructor */
    vec3_set(size, F32(0.0f), fdiv(size, RING_RISE), offset);
    njCalcPoint(ring, offset, point);
    vec3_add(point, method_self(member_addr(obj, Rings, body)));
    sp->p.x = f->point.x;
    sp->p.y = f->point.y;
    sp->p.z = f->point.z;
    njDrawSprite3D(member_addr(frame, RingsDrawFrame, sprite), (i & RING_CELLS) + 1, RING_ATTR);
  }
  hw_stack_free(sizeof(RingsDrawFrame));
}
HW_EXPORT(8c05c26a, rings_draw(ARG0))
