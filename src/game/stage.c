/* Pen Pen port, S13 (batch b): the racers' effect sets (their per-particle arrays freed together), the stage's
 * lights, the race scene's slot list and its tables of per-player values (0x8C039484-0x8C03DB40). S14: the classes
 * are in game/effects.h, game/race_scene.h, game/course.h, game/racer.h, core/contact.h. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/contact.h"
#include "effects.h"
#include "race_scene.h"
#include "course.h"
#include "racer.h"

/* free the blocks at these offsets of an object (each when not 0), in this order */
static void free_blocks(gaddr obj, const uint16_t *offsets, int n) {
  for (int i = 0; i < n; i++) {
    gaddr p = U32_AT(obj + offsets[i]);
    if (p != 0) syFree(p);
  }
}

#define A(i) offsetof(EffectSetA, arrays[i])
#define B(i) offsetof(EffectSetB, arrays[i])

/* was func_8C039EE6: free effect set A's arrays */
void effect_set_a_free(gaddr set) {
  static const uint16_t offsets[] = {A(0), A(3), A(4), A(5), A(6), A(7), A(1), A(2)};
  free_blocks(set, offsets, 8);
}
HW_EXPORT(8c039ee6, effect_set_a_free(ARG0))

/* was func_8C03B10A: free effect set B's arrays and bodies */
void effect_set_b_free(gaddr set) {
  static const uint16_t offsets[] = {B(2), B(0), B(1), B(3), B(4), B(5), B(6), offsetof(EffectSetB, bodies)};
  free_blocks(set, offsets, 8);
}
HW_EXPORT(8c03b10a, effect_set_b_free(ARG0))

/* was func_8C03BF70: free effect set C's arrays and bodies */
void effect_set_c_free(gaddr set) {
  static const uint16_t offsets[] = {
      offsetof(EffectSetC, counters), offsetof(EffectSetC, states), offsetof(EffectSetC, array30),
      offsetof(EffectSetC, array34),  offsetof(EffectSetC, values), offsetof(EffectSetC, array3c),
      offsetof(EffectSetC, bodies)};
  free_blocks(set, offsets, 7);
}
HW_EXPORT(8c03bf70, effect_set_c_free(ARG0))
#undef A
#undef B

/* ---- the stage's three light directions ---- */

#define STAGE_LIGHT_DIRS 0x8C1BAC2Cu /* three Vec3 */

/* was func_8C03C2BC: the lights from the upper right, the upper left and straight above (1.6667 up and across) */
gaddr stage_lights_init(void) {
  const f32 a = F32_BITS(0x3FD55555u); /* 1.6666666 */
  vec3_set(a, a, F32(0.0f), STAGE_LIGHT_DIRS);
  vec3_set(F32_BITS(0xBFD55555u), a, F32(0.0f), STAGE_LIGHT_DIRS + 12);
  return vec3_set(F32(0.0f), a, F32(0.0f), STAGE_LIGHT_DIRS + 24);
}
HW_EXPORT(8c03c2bc, RET(stage_lights_init()))

/* was func_8C03C4F6: a Body at rest */
HW_EXPORT(8c03c4f6, body_reset(ARG0))

/* was func_8C03C54C: pull a Body toward a point with a rate per axis: force += (target - position) * rate */
void body_pull_axes(gaddr b, gaddr target, gaddr rate) {
  Body *p = (Body *)hw_ptr(b);
  Vec3 d = *vec3_at(target);
  d.x = fsub(d.x, p->position.x);
  d.y = fsub(d.y, p->position.y);
  d.z = fsub(d.z, p->position.z);
  const Vec3 r = *vec3_at(rate);
  d.x = fmul(d.x, r.x);
  d.y = fmul(d.y, r.y);
  d.z = fmul(d.z, r.z);
  p->force.z = fadd(d.z, p->force.z);
  p->force.y = fadd(d.y, p->force.y);
  p->force.x = fadd(d.x, p->force.x);
}
HW_EXPORT(8c03c54c, body_pull_axes(ARG0, ARG1, ARG2))

/* was func_8C03C654: the racer's word at 0x1AC */
uint32_t racer_word1ac(gaddr racer) { return racer_at(racer)->word1ac; }
HW_EXPORT(8c03c654, RET(racer_word1ac(ARG0)))
/* was func_8C03C690: the start segment of the race scene's course */
int32_t race_scene_start_segment(gaddr scene) { return course_at(race_scene_at(scene)->course)->start_segment; }
HW_EXPORT(8c03c690, RET(race_scene_start_segment(ARG0)))

/* ---- the race scene's slot list (SlotList, game/race_scene.h): Contacts chained per slot ---- */

/* was func_8C03D5E0: allocate `size` empty slots (the first 9 are taken); returns the list */
gaddr slot_list_init(gaddr list, int32_t size) {
  SlotList *l = (SlotList *)hw_ptr(list);
  l->size = size;
  gaddr slots = syMalloc((uint32_t)size * 4);
  l->slots = slots;
  l->used = 9;
  for (int32_t i = 0; i < size; i++) ((gaddr *)hw_ptr(l->slots))[i] = 0;
  return list;
}
HW_EXPORT(8c03d5e0, RET(slot_list_init(ARG0, (int32_t)ARG1)))

/* was func_8C03D622: free the slots */
void slot_list_free(gaddr list) { syFree(((SlotList *)hw_ptr(list))->slots); }
HW_EXPORT(8c03d622, slot_list_free(ARG0))

/* was func_8C03D630: chain a Contacts into slot `slot` (a new one when < 0) after resetting it (no contacts in
 * use, the word after it = 4); returns the slot */
int32_t slot_list_insert(gaddr list, gaddr contacts, int32_t slot) {
  SlotList *l = (SlotList *)hw_ptr(list);
  field_reset_2c(contacts);
  if (slot < 0) slot = l->used++;
  contacts_at(contacts)->next = ((const gaddr *)hw_ptr(l->slots))[slot];
  ((gaddr *)hw_ptr(l->slots))[slot] = contacts;
  return slot;
}
HW_EXPORT(8c03d630, RET(slot_list_insert(ARG0, ARG1, (int32_t)ARG2)))

/* ---- the stage's members and tables ---- */

/* was func_8C03DA94 / func_8C03DA9A / func_8C03DAA0: the addresses of the course's model sets, its static words and
 * its per-env words */
gaddr course_models(gaddr course) { return member_addr(course, Course, models); }
gaddr course_words(gaddr course) { return member_addr(course, Course, course_words); }
gaddr course_env_words(gaddr course) { return member_addr(course, Course, env_word); }
HW_EXPORT(8c03da94, RET(course_models(ARG0)))
HW_EXPORT(8c03da9a, RET(course_words(ARG0)))
HW_EXPORT(8c03daa0, RET(course_env_words(ARG0)))

/* was func_8C03DB0E: the race scene's value [i][j] (i: a player) */
f32 race_scene_value(gaddr scene, int32_t i, int32_t j) {
  return F32_AT(member_addr(scene, RaceScene, values) + 32u * (uint32_t)i + 4u * (uint32_t)j);
}
HW_EXPORT(8c03db0e, RETF(race_scene_value(ARG0, (int32_t)ARG1, (int32_t)ARG2)))

/* was func_8C03DB20: the address of the race scene's point [i][j] (i: a player) */
gaddr race_scene_point(gaddr scene, int32_t i, int32_t j) {
  return member_addr(scene, RaceScene, points) + 96u * (uint32_t)i + 12u * (uint32_t)j;
}
HW_EXPORT(8c03db20, RET(race_scene_point(ARG0, (int32_t)ARG1, (int32_t)ARG2)))

/* ---- the third object set's update ---- */

#define g_object_set_c_count GUEST_VAR(int32_t, 0x8C08310Cu) /* 32 */
#define g_object_set_c_limit GUEST_VAR(f32, 0x8C083120u)     /* 10.0 */

/* was func_8C03BFE4: step every particle of set C by its state: 1 = wait out the counter, then go on; 2 = count the
 * value up by 1 until the limit, then go on; 3 = back to 0 (off); its Body moves in states 1 and 2 */
void effect_set_c_update(gaddr set) {
  const EffectSetC *c = (const EffectSetC *)hw_ptr(set);
  gaddr counter = c->counters, state = c->states, value = c->values;
  gaddr body = c->bodies;
  for (int32_t n = g_object_set_c_count; n > 0; n--) {
    int32_t *s = (int32_t *)hw_ptr(state);
    switch (*s) {
    case 1: {
      int32_t *c = (int32_t *)hw_ptr(counter);
      int32_t left = (*c)--;
      if (left < 0) (*s)++;
      body_step(body);
      break;
    }
    case 2:
      if (fgt(g_object_set_c_limit, F32_AT(value)))
        F32_AT(value) = fadd(F32_AT(value), F32(1.0f));
      else
        (*s)++;
      body_step(body);
      break;
    case 3:
      *s = 0;
      break;
    default:
      break;
    }
    counter += 4;
    state += 4;
    value += 4;
    body += sizeof(Body);
  }
}
HW_EXPORT(8c03bfe4, effect_set_c_update(ARG0))
