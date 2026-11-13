/* Pen Pen port, S17 b05: constructors and destructors of the scenery actors and of other classes of the race
 * (game/actor_b05.h).
 *
 * SHC emits, per class, a deleting destructor (the body, then syFree when asked; the vtables point at it) and a
 * destructor that runs the body and returns the object (for the class's members and arrays); several units keep
 * their own copies of a class's. The constructors of the actor classes store both vtables, then build the base. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "effects.h"
#include "racer.h"
#include "course.h"
#include "actor.h"
#include "actor_b05.h"

/* ---- destructors that run a body and return the object ---- */

/* was func_8C038CD8: a racer's effects freed (racer_effects_free) */
gaddr racer_effects_destroy_self(gaddr fx) {
  racer_effects_free(fx);
  return fx;
}
HW_EXPORT(8c038cd8, RET(racer_effects_destroy_self(ARG0)))

/* was func_8C03DC0E: a lane racer's destructor body (lane_racer_destroy) */
gaddr lane_racer_destroy_self(gaddr self) {
  lane_racer_destroy(self);
  return self;
}
HW_EXPORT(8c03dc0e, RET(lane_racer_destroy_self(ARG0)))

/* was func_8C041210: the course released (course_release) */
gaddr course_destroy_self(gaddr course) {
  course_release(course);
  return course;
}
HW_EXPORT(8c041210, RET(course_destroy_self(ARG0)))

/* was func_8C04442C: the Actor constructor run again on the object (its result dropped); returns the object */
gaddr actor_construct_self(gaddr actor) {
  actor_construct(actor);
  return actor;
}
HW_EXPORT(8c04442c, RET(actor_construct_self(ARG0)))

/* was func_8C04469E: the actor group released (actor_group_release) */
gaddr actor_group_destroy_self(gaddr group) {
  actor_group_release(group);
  return group;
}
HW_EXPORT(8c04469e, RET(actor_group_destroy_self(ARG0)))

/* was func_8C045074 and func_8C048CAC (whose body is func_8C048C74, a copy of actor_a_destroy): the Contacts at
 * 0x74 freed */
gaddr actor_a_destroy_self(gaddr obj) {
  actor_a_destroy(obj);
  return obj;
}
HW_EXPORT(8c045074, RET(actor_a_destroy_self(ARG0)))
HW_EXPORT(8c048cac, RET(actor_a_destroy_self(ARG0)))

/* was func_8C0459A8: type 2's destructor body (actor_type02_destroy) */
gaddr actor_type02_destroy_self(gaddr obj) {
  actor_type02_destroy(obj);
  return obj;
}
HW_EXPORT(8c0459a8, RET(actor_type02_destroy_self(ARG0)))

/* was func_8C046094, func_8C046738, func_8C0475BC, func_8C0484BC, func_8C048806, func_8C0492CE, func_8C04953A: the
 * classes whose destructor body is empty (method_empty, core/inline.c, and its copies) */
gaddr empty_destroy_self(gaddr obj) {
  method_empty();
  return obj;
}
HW_EXPORT(8c046094, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c046738, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c0475bc, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c0484bc, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c048806, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c0492ce, RET(empty_destroy_self(ARG0)))
HW_EXPORT(8c04953a, RET(empty_destroy_self(ARG0)))

/* was func_8C0487E2 and func_8C0492AA: copies of empty_delete (game/actor.c) */
HW_EXPORT(8c0487e2, RET(empty_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c0492aa, RET(empty_delete(ARG0, (int32_t)ARG1)))

/* was func_8C046CF8: the pieces' destructor body (pieces_destroy) */
gaddr pieces_destroy_self(gaddr obj) {
  pieces_destroy(obj);
  return obj;
}
HW_EXPORT(8c046cf8, RET(pieces_destroy_self(ARG0)))

/* was func_8C0479B0: the Contacts at 0x58 freed (actor_b_destroy) */
gaddr actor_b_destroy_self(gaddr obj) {
  actor_b_destroy(obj);
  return obj;
}
HW_EXPORT(8c0479b0, RET(actor_b_destroy_self(ARG0)))

/* was func_8C047F26: the buffer holder's blocks freed (buffer_pair_free) */
gaddr buffer_pair_destroy_self(gaddr obj) {
  buffer_pair_free(obj);
  return obj;
}
HW_EXPORT(8c047f26, RET(buffer_pair_destroy_self(ARG0)))

/* ---- type 0x00: the unseen sphere ---- */

/* was func_8C044A64: the base construction: the Actor, its Body at rest at the origin, its contacts owned by the
 * Body (radius 10, room for 8) */
gaddr actor_type00_base_construct(gaddr obj) {
  actor_construct(obj);
  gaddr body = member_addr(obj, ActorType00, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  contacts_construct(member_addr(obj, ActorType00, contacts), body, F32(10.0f), 8);
  return obj;
}
HW_EXPORT(8c044a64, RET(actor_type00_base_construct(ARG0)))

/* was func_8C044AEC: type 0's constructor: both vtables, then the base */
gaddr actor_type00_construct(gaddr obj) {
  ActorType00 *a = (ActorType00 *)hw_ptr(obj);
  a->base.vtable = 0x8C23DB8Cu;
  a->second_vtable = 0x8C23DBACu;
  actor_type00_base_construct(obj);
  return obj;
}
HW_EXPORT(8c044aec, RET(actor_type00_construct(ARG0)))

/* was func_8C044B08: the destructor body: the Contacts freed */
void actor_type00_destroy(gaddr obj) { contacts_free(member_addr(obj, ActorType00, contacts)); }
HW_EXPORT(8c044b08, actor_type00_destroy(ARG0))

/* was func_8C044B1C: the deleting destructor */
gaddr actor_type00_delete(gaddr obj, int32_t free_memory) {
  actor_type00_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c044b1c, RET(actor_type00_delete(ARG0, (int32_t)ARG1)))

/* was func_8C044B40 */
gaddr actor_type00_destroy_self(gaddr obj) {
  actor_type00_destroy(obj);
  return obj;
}
HW_EXPORT(8c044b40, RET(actor_type00_destroy_self(ARG0)))

/* ---- type 0x1B ---- */

/* was func_8C04928A: type 0x1B's constructor: both vtables, then the base (func_8C049276, a copy of
 * actor_base_construct) */
gaddr actor_type1b_construct(gaddr obj) {
  ActorType1B *a = (ActorType1B *)hw_ptr(obj);
  a->base.vtable = 0x8C23DE54u;
  a->second_vtable = 0x8C23DE74u;
  actor_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04928a, RET(actor_type1b_construct(ARG0)))

/* ---- type 0x2F ---- */

/* was func_8C048BF4: the base construction: the Actor, its Body at rest at the origin, its contacts owned by the
 * Body (radius 10, room for 2) */
gaddr actor_type2f_base_construct(gaddr obj) {
  actor_construct(obj);
  gaddr body = member_addr(obj, ActorType2F, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  contacts_construct(member_addr(obj, ActorType2F, contacts), body, F32(10.0f), 2);
  return obj;
}
HW_EXPORT(8c048bf4, RET(actor_type2f_base_construct(ARG0)))

/* was func_8C048C58: type 0x2F's constructor: both vtables, then the base */
gaddr actor_type2f_construct(gaddr obj) {
  ActorType2F *a = (ActorType2F *)hw_ptr(obj);
  a->base.vtable = 0x8C23DE14u;
  a->second_vtable = 0x8C23DE34u;
  actor_type2f_base_construct(obj);
  return obj;
}
HW_EXPORT(8c048c58, RET(actor_type2f_construct(ARG0)))

/* was func_8C048C88: the deleting destructor (its body func_8C048C74 is a copy of actor_a_destroy: the Contacts at
 * 0x74) */
HW_EXPORT(8c048c88, RET(actor_a_delete(ARG0, (int32_t)ARG1)))

/* ---- type 0x2B (ActorType2B, game/actor.h) ---- */

/* was func_8C049CCC: the destructor body: the Contacts freed */
void actor_type2b_destroy(gaddr obj) { contacts_free(member_addr(obj, ActorType2B, contacts)); }
HW_EXPORT(8c049ccc, actor_type2b_destroy(ARG0))

/* was func_8C049CE2: the deleting destructor */
gaddr actor_type2b_delete(gaddr obj, int32_t free_memory) {
  actor_type2b_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c049ce2, RET(actor_type2b_delete(ARG0, (int32_t)ARG1)))

/* was func_8C049D06 */
gaddr actor_type2b_destroy_self(gaddr obj) {
  actor_type2b_destroy(obj);
  return obj;
}
HW_EXPORT(8c049d06, RET(actor_type2b_destroy_self(ARG0)))
