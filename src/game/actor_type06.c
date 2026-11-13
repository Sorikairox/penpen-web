/* Pen Pen port, S17 b06: the scenery actor of placement type 0x06 (game/actor_b06.h): its construction and
 * destruction, and this unit's copy of race_scene_view_cell. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"
#include "race_scene.h"
#include "actor_b06.h"

#define TYPE06_VTABLE        0x8C23DF54u
#define TYPE06_SECOND_VTABLE 0x8C23DF74u
#define TYPE06_BURST_PARTICLES 20

/* was func_8C04BAFC: this unit's copy of race_scene_view_cell (game/camera.c) */
HW_EXPORT(8c04bafc, RET(race_scene_view_cell(ARG0, (int32_t)ARG1)))

/* a Body at rest at the origin, through the setters (each a vec3_set) */
static void body_rest_set(gaddr body) {
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
}

/* was func_8C04BB32: the base construction: the Actor, both Bodies at rest, the contacts owned by the first (radius
 * 10, room for 2) and a Burst of 20 particles (0 when out of memory) */
gaddr actor_type06_base_construct(gaddr obj) {
  actor_construct(obj);
  body_rest_set(member_addr(obj, ActorType06, body));
  body_rest_set(member_addr(obj, ActorType06, body2));
  contacts_construct(member_addr(obj, ActorType06, contacts), member_addr(obj, ActorType06, body), F32(10.0f), 2);
  gaddr block = syMalloc(sizeof(Burst));
  gaddr burst = 0;
  if (block != 0) burst = burst_construct(block, TYPE06_BURST_PARTICLES);
  ((ActorType06 *)hw_ptr(obj))->burst = burst;
  return obj;
}
HW_EXPORT(8c04bb32, RET(actor_type06_base_construct(ARG0)))

/* was func_8C04BBEE: type 6's constructor: both vtables, then the base */
gaddr actor_type06_construct(gaddr obj) {
  ActorType06 *a = (ActorType06 *)hw_ptr(obj);
  a->base.vtable = TYPE06_VTABLE;
  a->second_vtable = TYPE06_SECOND_VTABLE;
  actor_type06_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04bbee, RET(actor_type06_construct(ARG0)))

/* the Burst deleted through its virtual destructor ("free" = 1) */
static void burst_delete(gaddr burst) {
  const VMethod *destroy = (const VMethod *)hw_ptr(((const Burst *)hw_ptr(burst))->vtable);
  hw_call(destroy->fn, 2, burst + (uint32_t)destroy->this_adjust, 1, 0, 0);
}

/* was func_8C04BC38: the destructor body: the Burst deleted (when there is one), the contacts freed */
void actor_type06_destroy(gaddr obj) {
  gaddr burst = ((ActorType06 *)hw_ptr(obj))->burst;
  if (burst != 0) burst_delete(burst);
  contacts_free(member_addr(obj, ActorType06, contacts));
}
HW_EXPORT(8c04bc38, actor_type06_destroy(ARG0))

/* was func_8C04BC72: the deleting destructor */
gaddr actor_type06_delete(gaddr obj, int32_t free_memory) {
  actor_type06_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04bc72, RET(actor_type06_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04BC96: the destructor body, returning the object */
gaddr actor_type06_destroy_self(gaddr obj) {
  actor_type06_destroy(obj);
  return obj;
}
HW_EXPORT(8c04bc96, RET(actor_type06_destroy_self(ARG0)))

/* func_8C0419DA: the course's ground below a position (still translated) */
static int32_t course_ground_below(gaddr course, gaddr pos, int32_t cell, gaddr distance, int32_t flags) {
  return (int32_t)CALL5(0x8C0419DAu, course, pos, cell, distance, flags);
}

/* was func_8C04BAD8: the ground query of the race scene's course (a thunk: the five arguments passed on, the scene
 * swapped for its course): non-zero when found, the distance in *distance */
int32_t race_scene_course_ground(gaddr scene, gaddr pos, int32_t cell, gaddr distance, int32_t flags) {
  return course_ground_below(race_scene_at(scene)->course, pos, cell, distance, flags);
}
HW_EXPORT(8c04bad8, RET(race_scene_course_ground(ARG0, ARG1, (int32_t)ARG2, ARG3, (int32_t)ARG4)))
