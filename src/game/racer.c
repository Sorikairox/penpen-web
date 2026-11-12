/* Pen Pen port, S13 (batch b): the racer's helpers (0x8C03044A-0x8C037F50): the player object of a race, its
 * camera and motion pieces, and the small classes that live next to them. S14: the Racer and its FrameBody are
 * declared in game/racer.h, the motion player in game/motion.h. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "../core/file.h"
#include "racer.h"
#include "motion.h"
#include "course.h"
#include "race_scene.h"

/* njCalcVector (Ninja): dst = m * src without the translation */
static inline void njCalcVector(gaddr m, gaddr src, gaddr dst) { CALL3(0x8C0C31C4u, m, src, dst); }

/* ---- matrices and vectors ---- */

/* was func_8C03044A: invert a matrix in place (a jump to njInvertMatrix) */
HW_EXPORT(8c03044a, njInvertMatrix(ARG0))

/* was func_8C03045A: out = a matrix's third row (its z axis; the racer's matrix at 0x3A8) */
HW_EXPORT(8c03045a, vec3_copy(ARG1, member_addr(ARG0, Matrix, m[8])))

/* was func_8C0304CA: rotate the current matrix by three float angles */
HW_EXPORT(8c0304ca, matrix_rotate_xyz(FARG0, FARG1, FARG2))

/* was func_8C030972: a FrameBody's push += v (the racer's, at 0x2B8) */
HW_EXPORT(8c030972, vec3_add(member_addr(ARG0, FrameBody, push), ARG1))

/* was func_8C030994: a FrameBody's push.z += x */
HW_EXPORT(8c030994, f32_add_to(FARG0, member_addr(ARG0, FrameBody, push.z)))

/* was func_8C030AC6: the race scene's float at 0x74 (asked through the racer's scene) */
f32 race_scene_f74(gaddr scene) { return race_scene_at(scene)->f74; }
HW_EXPORT(8c030ac6, RETF(race_scene_f74(ARG0)))

/* was func_8C030AA0: the ground kind of the course's cell i (the low 4 bits of its attribute word) */
int32_t race_scene_cell_kind(gaddr scene, int32_t i) {
  gaddr table = course_at(race_scene_at(scene)->course)->cell_attrs;
  return CELL_KIND(((const uint16_t *)hw_ptr(table))[i]);
}
HW_EXPORT(8c030aa0, RET(race_scene_cell_kind(ARG0, (int32_t)ARG1)))

/* ---- a moving frame (FrameBody, game/racer.h) ---- */

/* was func_8C03090E: at rest at the origin, no damping, no push, no matrices; returns the object */
gaddr frame_body_init(gaddr obj) {
  FrameBody *p = frame_body_at(obj);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(obj, FrameBody, body.position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(obj, FrameBody, body.velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(obj, FrameBody, body.force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(obj, FrameBody, body.damping));
  p->push.z = p->push.y = p->push.x = F32(0.0f);
  p->world = 0;
  p->local = 0;
  return obj;
}
HW_EXPORT(8c03090e, RET(frame_body_init(ARG0)))

/* was func_8C037B16: the two matrices */
void frame_body_set_matrices(gaddr obj, gaddr world, gaddr local) {
  FrameBody *p = frame_body_at(obj);
  p->world = world;
  p->local = local;
}
HW_EXPORT(8c037b16, frame_body_set_matrices(ARG0, ARG1, ARG2))

/* was func_8C037B1E: one step: velocity = world * ((local * force + local * velocity + push) * damping), position
 * += velocity, then force and push are used up (the turned vectors live on the guest stack: Ninja writes them) */
void frame_body_step(gaddr obj) {
  FrameBody *p = frame_body_at(obj);
  gaddr tmp = hw_stack_alloc(2 * sizeof(Vec3));
  gaddr sum = tmp, force = tmp + sizeof(Vec3);
  njCalcVector(p->local, member_addr(obj, FrameBody, body.force), force);
  njCalcVector(p->local, member_addr(obj, FrameBody, body.velocity), sum);
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
  njCalcVector(p->world, sum, member_addr(obj, FrameBody, body.velocity));
  p->body.position.z = fadd(p->body.velocity.z, p->body.position.z);
  p->body.position.y = fadd(p->body.velocity.y, p->body.position.y);
  p->body.position.x = fadd(p->body.velocity.x, p->body.position.x);
  p->body.force.z = p->body.force.y = p->body.force.x = F32(0.0f);
  p->push.z = p->push.y = p->push.x = F32(0.0f);
  hw_stack_free(2 * sizeof(Vec3));
}
HW_EXPORT(8c037b1e, frame_body_step(ARG0))

/* ---- the racer (game/racer.h) ---- */

/* was func_8C032FF0: start action 1 */
HW_EXPORT(8c032ff0, racer_set_action(ARG0, 1))

/* was func_8C0330D8: the racer's contacts: the word at 0x1C cleared, the mass from the character's parameters
 * (through the class-neutral setters) */
void racer_contacts_reset(gaddr racer) {
  field_set(member_addr(racer, Racer, contacts), offsetof(Contacts, unknown1c), 0);
  field_set_f32(member_addr(racer, Racer, contacts), offsetof(Contacts, mass),
                ((const CharacterParams *)hw_ptr(racer_at(racer)->params))->mass);
}
HW_EXPORT(8c0330d8, racer_contacts_reset(ARG0))

/* was func_8C031690: put the camera behind the racer: its springs, its eye (racer + (-25, 20, -200), damping
 * 0.75) and its target */
void racer_camera_reset(gaddr racer) {
  Racer *r = racer_at(racer);
  vec3_set(F32(0.0f), F32(1.25f), F32(0.0f), member_addr(racer, Racer, camera_vec498));
  field_set_f32(member_addr(racer, Racer, camera_spring[0]), offsetof(Body1, value), F32_BITS(0x45088888u)); /* 2184.533 (0x10000 / 30) */
  field_set_f32(member_addr(racer, Racer, camera_spring[1]), offsetof(Body1, value), F32(0.0f));
  field_set_f32(member_addr(racer, Racer, camera_spring[2]), offsetof(Body1, value), F32(0.0f));
  r->camera_distance = F32(-25.0f);
  vec3_set(F32(0.0f), F32(0.0f), r->camera_distance, member_addr(racer, Racer, camera_offset));
  vec3_copy_bytes(member_addr(racer, Racer, camera_eye), method_self(member_addr(racer, Racer, body)));
  vec3_add_xyz(F32(-25.0f), F32(20.0f), F32(-200.0f), member_addr(racer, Racer, camera_eye));
  gaddr damping = hw_stack_alloc(sizeof(Vec3));
  vec3_set(F32(0.75f), F32(0.75f), F32(0.75f), damping);
  vec3_copy_bytes(member_addr(racer, Racer, camera_eye.damping), damping);
  hw_stack_free(sizeof(Vec3));
  field_set_f32(member_addr(racer, Racer, camera_spring[0]), offsetof(Body1, damping), F32(0.125f));
  field_set_f32(member_addr(racer, Racer, camera_spring[1]), offsetof(Body1, damping), F32_BITS(0x3EB33333u)); /* 0.35 */
  field_set_f32(member_addr(racer, Racer, camera_spring[2]), offsetof(Body1, damping), F32(0.125f));
  r->camera_f578 = F32(0.0f);
  r->camera_f57c = F32(0.0f);
  r->camera_f580 = F32(0.0f);
}
HW_EXPORT(8c031690, racer_camera_reset(ARG0))

/* was func_8C03466E: the racer's contacts (S13 took them for the course's gates): against another body of kind < 7
 * that the racer moves into (the other's velocity and the contact normal both along the racer's velocity), the
 * heading turns a twelfth of a turn by the side it hit; a contact of kind >= 7 whose word (0x48 array) is 1..3
 * starts action 3 when the racer is fast enough (strength 2 above 2.78, 1 above 0.926) */
void racer_handle_contacts(gaddr racer) {
  Racer *r = racer_at(racer);
  const Contacts *c = &r->contacts;
  r->had_contacts = 0;
  for (int32_t i = 0; i < c->count; i++) {
    r->had_contacts = 1;
    if (((const int32_t *)hw_ptr(c->other))[i] < 7) {
      gaddr other_velocity = c->velocity + 12u * (uint32_t)i;
      if (!fgt(njInnerProduct(other_velocity, field_addr(member_addr(racer, Racer, body), offsetof(Body, velocity))), F32(0.0f))) continue;
      gaddr normal = c->normal + 12u * (uint32_t)i;
      if (!fgt(njInnerProduct(normal, field_addr(member_addr(racer, Racer, body), offsetof(Body, velocity))), F32(0.0f))) continue;
      normal = c->normal + 12u * (uint32_t)i;
      if (fgt(vec3_cross_y(field_addr(member_addr(racer, Racer, body), offsetof(Body, velocity)), normal), F32(0.0f)))
        f32_add_to(F32_BITS(0x45AAAAAAu), member_addr(racer, Racer, heading.force)); /* +5461.33: 0x10000 / 12 */
      else
        f32_add_to(F32_BITS(0xC5AAAAAAu), member_addr(racer, Racer, heading.force));
    } else {
      int32_t value = ((const int32_t *)hw_ptr(c->unknown48))[i];
      if (value < 1 || value > 3) continue;
      if (fgt(r->speed, F32_BITS(0x4031C71Bu))) { /* 2.7778 */
        r->hit_strength = 2;
        racer_set_action(racer, 3);
      } else if (fgt(r->speed, F32_BITS(0x3F6D097Au))) { /* 0.9259 */
        r->hit_strength = 1;
        racer_set_action(racer, 3);
      }
    }
  }
}
HW_EXPORT(8c03466e, racer_handle_contacts(ARG0))

/* was func_8C036FAC: set the racer's achievement flags: mark_a / mark_b when their values pass a mark (with their
 * switches on), and mark_c when in state 4 (or 5) its motion is 0x10 (0x11) and the motion's frame is in a window
 * (20..35 for the characters 1, 4, 6, else 15..30) */
void racer_check_marks(gaddr racer) {
  Racer *r = racer_at(racer);
  if (r->mark_a_on != 0 && fgt(r->mark_a_value, F32_BITS(0x3EED097Au))) /* 0.463 */
    r->mark_a = 1;
  if (r->mark_b_on != 0 && fgt(r->mark_b_value, F32_BITS(0x3F6D097Au))) /* 0.926 */
    r->mark_b = 1;
  int32_t state = r->state;
  int32_t ground;
  if (state == 4)
    ground = 0x10;
  else if (state == 5)
    ground = 0x11;
  else
    return;
  int32_t kind = r->character;
  f32 low, high;
  if (kind == 1 || kind == 4 || kind == 6) {
    low = F32(20.0f);
    high = F32(35.0f);
  } else {
    low = F32(15.0f);
    high = F32(30.0f);
  }
  if (r->motion != ground) return;
  if (fgt(low, r->motion_frame)) return;
  if (fgt(r->motion_frame, high)) return;
  r->mark_c = 1;
}
HW_EXPORT(8c036fac, racer_check_marks(ARG0))

/* ---- small pieces next to the racer ---- */

#define VARIANT_WORDS ((const uint32_t *)hw_ptr(0x8C0806FCu)) /* a word per variant */

/* was func_8C03735C: when the racer's variant switch is on, take variant n (1 to 5, by the racer's update): its
 * word and number */
void racer_set_variant(gaddr racer, int32_t n) {
  Racer *r = racer_at(racer);
  if (r->variant_on == 0) return;
  r->variant_word = VARIANT_WORDS[n];
  r->variant = n;
}
HW_EXPORT(8c03735c, racer_set_variant(ARG0, (int32_t)ARG1))

/* func_8C037586: the racer's destructor body (a jump to func_8C0309A4) */
static void racer_destroy_body(gaddr racer) { CALL1(0x8C037586u, racer); }

/* was func_8C03758C: the racer's deleting destructor (its vtable's first entry): the body, then the memory when
 * asked */
gaddr racer_delete(gaddr racer, int32_t free_memory) {
  racer_destroy_body(racer);
  if (free_memory) syFree(racer);
  return racer;
}
HW_EXPORT(8c03758c, RET(racer_delete(ARG0, (int32_t)ARG1)))

/* was func_8C037F44: a motion player's destructor body: free the model file it loaded */
void motion_player_free(gaddr player) { file_free(motion_player_at(player)->file); }
HW_EXPORT(8c037f44, motion_player_free(ARG0))
