/* Pen Pen port, S13 (batch b): the racer's helpers (0x8C03044A-0x8C037F50): the player object of a race, its
 * camera and motion pieces, and the small classes that live next to them.
 *
 * The racer keeps its body (a Body, core/vec.h) at 0x2B8, a few Body1 values (an angle at 0x32C, the camera's
 * springs at 0x4A4, 0x4B4, 0x4C4) and its camera's Body at 0x4EC; the course's checkpoint lists hang off the
 * sub-object at 0x1B8. Field names are from what these functions do with them. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "../core/file.h"

/* njCalcVector (Ninja): dst = m * src without the translation */
static inline void njCalcVector(gaddr m, gaddr src, gaddr dst) { CALL3(0x8C0C31C4u, m, src, dst); }

/* ---- matrices and vectors ---- */

/* was func_8C03044A: invert a matrix in place (a jump to njInvertMatrix) */
HW_EXPORT(8c03044a, njInvertMatrix(ARG0))

/* was func_8C03045A: out = the object's Vec3 at 0x20 */
HW_EXPORT(8c03045a, vec3_copy(ARG1, ARG0 + 0x20))

/* was func_8C0304CA: rotate the current matrix by three float angles */
HW_EXPORT(8c0304ca, matrix_rotate_xyz(FARG0, FARG1, FARG2))

/* was func_8C030972: the object's Vec3 at 0x30 += v */
HW_EXPORT(8c030972, vec3_add(ARG0 + 0x30, ARG1))

/* was func_8C030994: the object's float at 0x38 += x */
HW_EXPORT(8c030994, f32_add_to(FARG0, ARG0 + 0x38))

/* was func_8C030AC6: the object's float at 0x74 */
HW_EXPORT(8c030ac6, RETF(field_get_f32(ARG0, 0x74)))

/* was func_8C030AA0: the ground kind of cell i: the low 4 bits of the stage's 16-bit attribute table (the stage
 * object at 0x38 holds it at 0xB4) */
int32_t stage_cell_kind(gaddr obj, int32_t i) {
  gaddr table = U32_AT(U32_AT(obj + 0x38) + 0xB4);
  return *(uint16_t *)hw_ptr(table + 2u * (uint32_t)i) & 0xF;
}
HW_EXPORT(8c030aa0, RET(stage_cell_kind(ARG0, (int32_t)ARG1)))

/* ---- a moving frame: a Body with an extra push (0x30) and two matrices ---- */

typedef struct FrameBody {
  Body body;      /* 0x00: position, velocity, force, damping */
  Vec3 push;      /* 0x30: added to the velocity on the next step, then cleared */
  gaddr world;    /* 0x3C: the matrix the new velocity is turned by */
  gaddr local;    /* 0x40: the matrix force and velocity are turned by first */
} FrameBody;
HW_OFFSET(FrameBody, push, 0x30);
HW_OFFSET(FrameBody, local, 0x40);

/* was func_8C03090E: at rest at the origin, no damping, no push, no matrices; returns the object */
gaddr frame_body_init(gaddr obj) {
  FrameBody *p = (FrameBody *)hw_ptr(obj);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), obj + 0x00);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), obj + 0x0C);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), obj + 0x18);
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), obj + 0x24);
  p->push.z = p->push.y = p->push.x = F32(0.0f);
  p->world = 0;
  p->local = 0;
  return obj;
}
HW_EXPORT(8c03090e, RET(frame_body_init(ARG0)))

/* was func_8C037B16: the two matrices */
void frame_body_set_matrices(gaddr obj, gaddr world, gaddr local) {
  FrameBody *p = (FrameBody *)hw_ptr(obj);
  p->world = world;
  p->local = local;
}
HW_EXPORT(8c037b16, frame_body_set_matrices(ARG0, ARG1, ARG2))

/* was func_8C037B1E: one step: velocity = world * ((local * force + local * velocity + push) * damping), position
 * += velocity, then force and push are used up (the turned vectors live on the guest stack: Ninja writes them) */
void frame_body_step(gaddr obj) {
  FrameBody *p = (FrameBody *)hw_ptr(obj);
  gaddr tmp = hw_stack_alloc(2 * sizeof(Vec3));
  gaddr sum = tmp, force = tmp + sizeof(Vec3);
  njCalcVector(p->local, obj + 0x18, force);
  njCalcVector(p->local, obj + 0x0C, sum);
  Vec3 *s = vec3_at(sum);
  const Vec3 f = *vec3_at(force);
  s->z = fadd(f.z, s->z);
  s->y = fadd(f.y, s->y);
  s->x = fadd(f.x, s->x);
  s->z = fadd(p->push.z, s->z);
  s->y = fadd(p->push.y, s->y);
  s->x = fadd(p->push.x, s->x);
  s->z = fmul(s->z, p->body.damping.z);
  s->y = fmul(s->y, p->body.damping.y);
  s->x = fmul(s->x, p->body.damping.x);
  njCalcVector(p->world, sum, obj + 0x0C);
  p->body.position.z = fadd(p->body.velocity.z, p->body.position.z);
  p->body.position.y = fadd(p->body.velocity.y, p->body.position.y);
  p->body.position.x = fadd(p->body.velocity.x, p->body.position.x);
  p->body.force.z = p->body.force.y = p->body.force.x = F32(0.0f);
  p->push.z = p->push.y = p->push.x = F32(0.0f);
  hw_stack_free(2 * sizeof(Vec3));
}
HW_EXPORT(8c037b1e, frame_body_step(ARG0))

/* ---- the racer ---- */

#define RACER_BODY 0x2B8u      /* Body */
#define RACER_ANGLE 0x32Cu     /* Body1: the heading */
#define RACER_COURSE 0x1B8u    /* the course's checkpoint lists */
#define RACER_SPEED 0xF0u      /* float */
#define RACER_CAMERA 0x4ECu    /* Body: the camera's eye */

/* was func_8C032FF0: action 1 (the word at 0x27C, its two followers cleared) */
HW_EXPORT(8c032ff0, field_set_and_clear2(ARG0, 1))

/* was func_8C0330D8: the course sub-object's word at 0x1C cleared, its float at 0x14 = the float at 8 of the object
 * at 0x43C */
void racer_course_reset(gaddr racer) {
  field_set(racer + RACER_COURSE, 0x1C, 0);
  field_set_f32(racer + RACER_COURSE, 0x14, F32_AT(U32_AT(racer + 0x43C) + 8));
}
HW_EXPORT(8c0330d8, racer_course_reset(ARG0))

/* was func_8C031690: put the camera behind the racer: its springs, its eye (racer + (-25, 20, -200), damping
 * 0.75) and its target */
void racer_camera_reset(gaddr racer) {
  vec3_set(F32(0.0f), F32(1.25f), F32(0.0f), racer + 0x498);
  field_set_f32(racer + 0x4A4, 0x00, F32_BITS(0x45088888u)); /* 2184.533 (0x10000 / 30) */
  field_set_f32(racer + 0x4B4, 0x00, F32(0.0f));
  field_set_f32(racer + 0x4C4, 0x00, F32(0.0f));
  F32_AT(racer + 0x584) = F32(-25.0f);
  vec3_set(F32(0.0f), F32(0.0f), F32_AT(racer + 0x584), racer + 0x4D4);
  vec3_copy_bytes(racer + RACER_CAMERA, method_self(racer + RACER_BODY));
  vec3_add_xyz(F32(-25.0f), F32(20.0f), F32(-200.0f), racer + RACER_CAMERA);
  gaddr damping = hw_stack_alloc(sizeof(Vec3));
  vec3_set(F32(0.75f), F32(0.75f), F32(0.75f), damping);
  vec3_copy_bytes(racer + RACER_CAMERA + 0x24, damping);
  hw_stack_free(sizeof(Vec3));
  field_set_f32(racer + 0x4A4, 0x0C, F32(0.125f));
  field_set_f32(racer + 0x4B4, 0x0C, F32_BITS(0x3EB33333u)); /* 0.35 */
  field_set_f32(racer + 0x4C4, 0x0C, F32(0.125f));
  F32_AT(racer + 0x578) = F32(0.0f);
  F32_AT(racer + 0x57C) = F32(0.0f);
  F32_AT(racer + 0x580) = F32(0.0f);
}
HW_EXPORT(8c031690, racer_camera_reset(ARG0))

/* was func_8C03466E: the course's gates: for each gate, the racer (moving through a gate of kind < 7 in its
 * direction) is turned a twelfth of a turn by the side it crossed; a gate of kind >= 7 with a value 1..3 starts
 * action 3 when the racer is fast enough (2 above 2.78, 1 above 0.926) */
void racer_check_gates(gaddr racer) {
  gaddr course = racer + RACER_COURSE;
  U32_AT(racer + 0x18C) = 0;
  for (int32_t i = 0; i < (int32_t)U32_AT(course + 0x2C); i++) {
    U32_AT(racer + 0x18C) = 1;
    if ((int32_t)U32_AT(U32_AT(course + 0x30) + 4u * (uint32_t)i) < 7) {
      gaddr gate_dir = U32_AT(course + 0x34) + 12u * (uint32_t)i;
      if (!fgt(njInnerProduct(gate_dir, field_addr(racer + RACER_BODY, 0x0C)), F32(0.0f))) continue;
      gaddr gate_normal = U32_AT(course + 0x40) + 12u * (uint32_t)i;
      if (!fgt(njInnerProduct(gate_normal, field_addr(racer + RACER_BODY, 0x0C)), F32(0.0f))) continue;
      gaddr normal = U32_AT(course + 0x40) + 12u * (uint32_t)i;
      if (fgt(vec3_cross_y(field_addr(racer + RACER_BODY, 0x0C), normal), F32(0.0f)))
        f32_add_to(F32_BITS(0x45AAAAAAu), racer + RACER_ANGLE + 0x08); /* +5461.33: 0x10000 / 12 */
      else
        f32_add_to(F32_BITS(0xC5AAAAAAu), racer + RACER_ANGLE + 0x08);
    } else {
      int32_t value = (int32_t)U32_AT(U32_AT(course + 0x48) + 4u * (uint32_t)i);
      if (value < 1 || value > 3) continue;
      if (fgt(F32_AT(racer + RACER_SPEED), F32_BITS(0x4031C71Bu))) { /* 2.7778 */
        U32_AT(racer + 0x278) = 2;
        field_set_and_clear2(racer, 3);
      } else if (fgt(F32_AT(racer + RACER_SPEED), F32_BITS(0x3F6D097Au))) { /* 0.9259 */
        U32_AT(racer + 0x278) = 1;
        field_set_and_clear2(racer, 3);
      }
    }
  }
}
HW_EXPORT(8c03466e, racer_check_gates(ARG0))

/* was func_8C036FAC: set the racer's achievement flags: 0x148 / 0x14C when the values at 0x270 / 0x26C pass a
 * mark (with the switches at 0x258 / 0x25C on), and 0x150 when in state 4 (or 5) its ground kind is 0x10 (0x11)
 * and the float at 0x54 is in a window (20..35 for the kinds 1, 4, 6 at 0xA4, else 15..30) */
void racer_check_marks(gaddr racer) {
  if (U32_AT(racer + 0x258) != 0 && fgt(F32_AT(racer + 0x270), F32_BITS(0x3EED097Au))) /* 0.463 */
    U32_AT(racer + 0x148) = 1;
  if (U32_AT(racer + 0x25C) != 0 && fgt(F32_AT(racer + 0x26C), F32_BITS(0x3F6D097Au))) /* 0.926 */
    U32_AT(racer + 0x14C) = 1;
  int32_t state = (int32_t)U32_AT(racer + 0x19C);
  int32_t ground;
  if (state == 4)
    ground = 0x10;
  else if (state == 5)
    ground = 0x11;
  else
    return;
  int32_t kind = (int32_t)U32_AT(racer + 0xA4);
  f32 low, high;
  if (kind == 1 || kind == 4 || kind == 6) {
    low = F32(20.0f);
    high = F32(35.0f);
  } else {
    low = F32(15.0f);
    high = F32(30.0f);
  }
  if ((int32_t)U32_AT(racer + 0x50) != ground) return;
  if (fgt(low, F32_AT(racer + 0x54))) return;
  if (fgt(F32_AT(racer + 0x54), high)) return;
  U32_AT(racer + 0x150) = 1;
}
HW_EXPORT(8c036fac, racer_check_marks(ARG0))

/* ---- small pieces next to the racer ---- */

#define CHARACTER_PARAMS ((const uint32_t *)hw_ptr(0x8C0806FCu)) /* a word per character */

/* was func_8C03735C: when the object is on (0x568), take character n: its parameter (0x570) and number (0x574) */
void object_set_character(gaddr obj, int32_t n) {
  if (U32_AT(obj + 0x568) == 0) return;
  U32_AT(obj + 0x570) = CHARACTER_PARAMS[n];
  U32_AT(obj + 0x574) = (uint32_t)n;
}
HW_EXPORT(8c03735c, object_set_character(ARG0, (int32_t)ARG1))

/* func_8C037586: the class's destructor body (a jump to func_8C0309A4) */
static void racer_piece_destroy_body(gaddr obj) { CALL1(0x8C037586u, obj); }

/* was func_8C03758C: the deleting destructor: the body, then the memory when asked */
gaddr racer_piece_delete(gaddr obj, int32_t free_memory) {
  racer_piece_destroy_body(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c03758c, RET(racer_piece_delete(ARG0, (int32_t)ARG1)))

/* was func_8C037F44: free the block the object loaded (0x40) */
HW_EXPORT(8c037f44, file_free(U32_AT(ARG0 + 0x40)))
