/* Pen Pen port, S14 (batch b08): the race scene's contact pass over its slot list (func_8C03D65A).
 *
 * Each frame the bodies that can touch put their Contacts into the scene's SlotList (race_scene_slot_insert, one
 * chain per slot); this pass tests every pair of bodies of two different slots and records each overlap in both
 * bodies' contact arrays (core/contact.h), then empties the list for the next frame. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "race_scene.h"

/* the slot list's `used` after a pass: the first 9 slots stay reserved */
#define SLOT_LIST_RESERVED 9

/* record in `self` (if it keeps contacts) a contact with `other` (of slot `other_slot`): the normal (unit, pointing
 * away from the other body), how deep they overlap, the other's mass, restitution, two words and its response,
 * and its velocity (0 when it has none); the count stops at the last entry */
static void contact_record(Contacts *self, const Contacts *other, int32_t other_slot, gaddr normal, f32 depth) {
  if (self->capacity == 0) {
    self->count++;
    return;
  }
  uint32_t n = (uint32_t)self->count;
  memmove(hw_ptr(self->normal + n * sizeof(Vec3)), hw_ptr(normal), sizeof(Vec3));
  ((f32 *)hw_ptr(self->depth))[n] = depth;
  ((int32_t *)hw_ptr(self->other))[n] = other_slot;
  ((f32 *)hw_ptr(self->other_mass))[n] = other->mass;
  ((f32 *)hw_ptr(self->restitution))[n] = other->unknown18;
  ((int32_t *)hw_ptr(self->unknown48))[n] = other->unknown1c;
  ((int32_t *)hw_ptr(self->unknown4c))[n] = other->unknown20;
  ((gaddr *)hw_ptr(self->handler))[n] = other->respond;
  if (other->owner_velocity != 0) {
    memmove(hw_ptr(self->velocity + n * sizeof(Vec3)), hw_ptr(other->owner_velocity), sizeof(Vec3));
  } else {
    Vec3 *v = vec3_at(self->velocity + n * sizeof(Vec3));
    v->z = F32(0.0f);
    v->y = F32(0.0f);
    v->x = F32(0.0f);
  }
  self->count = (int32_t)n + 1;
  if (self->count >= self->capacity) self->count = self->capacity - 1;
}

/* two bodies may touch unless both know their course cell and the cells are more than one apart */
static int cells_near(const Contacts *a, const Contacts *b) {
  if (a->cell == -1 || b->cell == -1) return 1;
  return a->cell >= b->cell - 1 && a->cell <= b->cell + 1;
}

/* was func_8C03D65A: test every pair of bodies of two different slots (cells near, spheres of their radii
 * overlapping) and record the contact in both (b's normal points from a to b, a's the other way); then the list
 * is emptied (all slots 0, `used` back to the reserved slots) */
void slot_list_collide(gaddr list) {
  SlotList *l = (SlotList *)hw_ptr(list);
  gaddr d = hw_stack_alloc(sizeof(Vec3)); /* from a's position to b's, then its unit (the Ninja calls take it) */
  Vec3 *dv = vec3_at(d);
  for (int32_t i = 0; i < l->used - 1; i++) {
    for (int32_t j = i + 1; j < l->used; j++) {
      for (gaddr pa = ((const gaddr *)hw_ptr(l->slots))[i]; pa != 0; pa = contacts_at(pa)->next) {
        for (gaddr pb = ((const gaddr *)hw_ptr(l->slots))[j]; pb != 0; pb = contacts_at(pb)->next) {
          Contacts *a = contacts_at(pa), *b = contacts_at(pb);
          if (!cells_near(a, b)) continue;
          const Vec3 *posa = vec3_at(a->owner), *posb = vec3_at(b->owner);
          f32 x = fsub(posb->x, posa->x), y = fsub(posb->y, posa->y), z = fsub(posb->z, posa->z);
          dv->z = z;
          dv->y = y;
          dv->x = x;
          f32 gap = fsub(fsub(njScalor(d), a->radius), b->radius);
          if (!fgt(F32(0.0f), gap)) continue;
          njUnitVector(d);
          f32 depth = fneg(gap);
          contact_record(b, a, i, d, depth);
          if (a->capacity != 0) {
            f32 nx = fneg(dv->x), ny = fneg(dv->y), nz = fneg(dv->z);
            dv->z = nz;
            dv->y = ny;
            dv->x = nx;
          }
          contact_record(a, b, j, d, depth);
        }
      }
    }
  }
  l->used = SLOT_LIST_RESERVED;
  for (int32_t k = 0; k < l->size; k++) ((gaddr *)hw_ptr(l->slots))[k] = 0;
  hw_stack_free(sizeof(Vec3));
}
HW_EXPORT(8c03d65a, slot_list_collide(ARG0))
