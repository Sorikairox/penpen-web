/* Pen Pen port, S13 (batch b): the stage's objects (0x8C039484-0x8C03DB40): the course's object sets (their
 * per-object arrays freed together), the stage's lights, a slot list, and the stage's tables of per-player values.
 * Field names are from what these functions do with them. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"

/* free the blocks at these offsets of an object (each when not 0), in this order */
static void free_blocks(gaddr obj, const uint16_t *offsets, int n) {
  for (int i = 0; i < n; i++) {
    gaddr p = U32_AT(obj + offsets[i]);
    if (p != 0) syFree(p);
  }
}

/* was func_8C039EE6: free an object set's arrays */
void object_set_a_free(gaddr set) {
  static const uint16_t offsets[] = {0x30, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x34, 0x38};
  free_blocks(set, offsets, 8);
}
HW_EXPORT(8c039ee6, object_set_a_free(ARG0))

/* was func_8C03B10A: free another object set's arrays */
void object_set_b_free(gaddr set) {
  static const uint16_t offsets[] = {0x3C, 0x34, 0x38, 0x40, 0x44, 0x48, 0x4C, 0x50};
  free_blocks(set, offsets, 8);
}
HW_EXPORT(8c03b10a, object_set_b_free(ARG0))

/* was func_8C03BF70: free a third set's arrays */
void object_set_c_free(gaddr set) {
  static const uint16_t offsets[] = {0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40};
  free_blocks(set, offsets, 7);
}
HW_EXPORT(8c03bf70, object_set_c_free(ARG0))

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

/* was func_8C03C654: the word at 0x1AC */
HW_EXPORT(8c03c654, RET(field_get(ARG0, 0x1AC)))
/* was func_8C03C690: the stage's (0x38) word at 0x572C */
HW_EXPORT(8c03c690, RET(field_get(U32_AT(ARG0 + 0x38), 0x572C)))

/* ---- a slot list: {count, next free, slots} ---- */

typedef struct SlotList {
  int32_t size;   /* 0x00 */
  int32_t used;   /* 0x04: slots handed out by slot_list_insert(-1) */
  gaddr slots;    /* 0x08: `size` words, each the first item of a chain */
} SlotList;
HW_SIZE(SlotList, 12);

/* was func_8C03D5E0: allocate `size` empty slots (the first 9 are taken); returns the list */
gaddr slot_list_init(gaddr list, int32_t size) {
  SlotList *l = (SlotList *)hw_ptr(list);
  l->size = size;
  gaddr slots = syMalloc((uint32_t)size * 4);
  l->slots = slots;
  l->used = 9;
  for (int32_t i = 0; i < size; i++) U32_AT(l->slots + 4u * (uint32_t)i) = 0;
  return list;
}
HW_EXPORT(8c03d5e0, RET(slot_list_init(ARG0, (int32_t)ARG1)))

/* was func_8C03D622: free the slots */
void slot_list_free(gaddr list) { syFree(((SlotList *)hw_ptr(list))->slots); }
HW_EXPORT(8c03d622, slot_list_free(ARG0))

/* was func_8C03D630: chain an item into slot `slot` (a new one when < 0) after resetting it (its state word 0x2C
 * = 0, 0x54 = 4); returns the slot */
int32_t slot_list_insert(gaddr list, gaddr item, int32_t slot) {
  SlotList *l = (SlotList *)hw_ptr(list);
  field_reset_2c(item);
  if (slot < 0) slot = l->used++;
  U32_AT(item) = U32_AT(l->slots + 4u * (uint32_t)slot);
  U32_AT(l->slots + 4u * (uint32_t)slot) = item;
  return slot;
}
HW_EXPORT(8c03d630, RET(slot_list_insert(ARG0, ARG1, (int32_t)ARG2)))

/* ---- the stage's members and tables ---- */

/* was func_8C03DA94 / func_8C03DA9A / func_8C03DAA0: the addresses of the members at 0x5674 / 0x5704 / 0x56F4 */
HW_EXPORT(8c03da94, RET(field_addr(ARG0, 0x5674)))
HW_EXPORT(8c03da9a, RET(field_addr(ARG0, 0x5704)))
HW_EXPORT(8c03daa0, RET(field_addr(ARG0, 0x56F4)))

/* was func_8C03DB0E: the float [i][j] of the 8-per-row table at 0x45C */
f32 stage_value(gaddr stage, int32_t i, int32_t j) {
  return F32_AT(stage + 0x45C + 32u * (uint32_t)i + 4u * (uint32_t)j);
}
HW_EXPORT(8c03db0e, RETF(stage_value(ARG0, (int32_t)ARG1, (int32_t)ARG2)))

/* was func_8C03DB20: the address of the Vec3 [i][j] of the 8-per-row table at 0x55C */
gaddr stage_point(gaddr stage, int32_t i, int32_t j) {
  return stage + 0x55C + 96u * (uint32_t)i + 12u * (uint32_t)j;
}
HW_EXPORT(8c03db20, RET(stage_point(ARG0, (int32_t)ARG1, (int32_t)ARG2)))

/* ---- the third object set's update ---- */

#define g_object_set_c_count GUEST_VAR(int32_t, 0x8C08310Cu) /* 32 */
#define g_object_set_c_limit GUEST_VAR(f32, 0x8C083120u)     /* 10.0 */

/* was func_8C03BFE4: step every object of the set by its state: 1 = wait out the counter (0x28), then go on; 2 =
 * count the float (0x38) up by 1 until the limit, then go on; 3 = back to 0 (off); its Body moves in states 1 and 2 */
void object_set_c_update(gaddr set) {
  gaddr counter = U32_AT(set + 0x28), state = U32_AT(set + 0x2C), value = U32_AT(set + 0x38);
  gaddr body = U32_AT(set + 0x40);
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
    body += 0x30;
  }
}
HW_EXPORT(8c03bfe4, object_set_c_update(ARG0))
