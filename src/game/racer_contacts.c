/* Pen Pen port, S16 (b04): the racer sorts the contacts its collider found this update (func_8C031D8C).
 *
 * After the contact handlers ran, each contact is either with another racer (the other body's id, Contacts.other,
 * is below 8) or with something else. The first contact of each sort gives the racer a push out of it (the
 * contact's normal times how deep it went, less the racer's radius) and the velocity of the meeting (the other
 * body's velocity plus the racer's own); later contacts of the same sort only count while the first one was not of
 * kind 1. A contact of kind 4 with an object ends the sorting: it may start action 4. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"   /* racer_set_action */
#include "../core/contact.h"
#include "scenery.h"          /* contacts_call_handlers */
#include "racer.h"

#define RACER_ID_LIMIT  8                        /* other bodies with an id below this are racers */
#define CONTACT_KIND_SOLID 1                     /* unknown48: a contact that hides the later ones of its sort */
#define CONTACT_KIND_SPECIAL 4                   /* unknown48: an object contact that may start action 4 */
#define CONTACT_KIND_SLIPPERY 0xCD               /* unknown48: an object contact without grip */
#define CONTACT_DEEP F32_BITS(0x3F7FFFFFu)       /* 0.99999994 */

/* was func_8C031D8C: run the contact handlers, then note the racer and object contacts of this update (racer_contact /
 * object_contact, their pushes and velocities, the variant they show) */
void racer_sort_contacts(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr contacts = member_addr(racer, Racer, contacts);
  Contacts *c = contacts_at(contacts);
  contacts_call_handlers(contacts, member_addr(racer, Racer, body), member_addr(racer, Racer, heading),
                         member_addr(racer, Racer, value33c));
  r->on_kind4 = 0;
  r->racer_contact_prev = r->racer_contact;
  r->object_contact_prev = r->object_contact;
  r->racer_contact = -1;
  r->object_contact = -1;
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, vec0bc));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, object_push));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, racer_velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(racer, Racer, object_velocity));
  int32_t racer_done = 0, object_done = 0;
  for (int32_t i = 0; i < c->count; i++) {
    int32_t other = ((int32_t *)hw_ptr(c->other))[i];
    int32_t kind = ((int32_t *)hw_ptr(c->unknown48))[i];
    f32 depth = ((f32 *)hw_ptr(c->depth))[i];
    if (other < RACER_ID_LIMIT) {
      r->mark_d = 1;
      if (!racer_done) {
        r->racer_contact = i;
        f32 radius = ((CharacterParams *)hw_ptr(r->params))->radius;
        vec3_scaled(fsub(depth, radius), member_addr(racer, Racer, vec0bc), c->normal + (uint32_t)i * sizeof(Vec3));
        vec3_sum(member_addr(racer, Racer, racer_velocity), c->velocity + (uint32_t)i * sizeof(Vec3),
                 c->owner_velocity);
        if (kind == CONTACT_KIND_SOLID)
          racer_done = 1;
        racer_set_variant(racer, 3);
      } else if (fgt(depth, CONTACT_DEEP)) {
        racer_set_variant(racer, 1);
      }
    } else if (kind == CONTACT_KIND_SPECIAL) {
      r->on_kind4 = 1;
      r->on_kind4_new = (r->on_kind4 != 0 && r->on_kind4_prev == 0) ? 1 : 0;
      if (r->on_kind4_new) {
        racer_set_action(racer, 4);
        racer_set_variant(racer, 4);
      }
      break;
    } else if (!object_done) {
      r->object_contact = i;
      f32 radius = ((CharacterParams *)hw_ptr(r->params))->radius;
      vec3_scaled(fsub(depth, radius), member_addr(racer, Racer, object_push), c->normal + (uint32_t)i * sizeof(Vec3));
      vec3_sum(member_addr(racer, Racer, object_velocity), c->velocity + (uint32_t)i * sizeof(Vec3),
               c->owner_velocity);
      if (kind == CONTACT_KIND_SOLID)
        object_done = 1;
      if (kind == CONTACT_KIND_SLIPPERY) {
        r->object_grip = F32(0.0f);
      } else {
        r->object_grip = F32(1.0f);
        racer_set_variant(racer, 2);
      }
    }
  }
  r->racer_speed = njScalor(member_addr(racer, Racer, racer_velocity));
  r->speed = njScalor(member_addr(racer, Racer, object_velocity));
}
HW_EXPORT(8c031d8c, racer_sort_contacts(ARG0))
