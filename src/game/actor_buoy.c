/* Pen Pen port, S14 b11: the buoy (type 0x2A) (game/actor.h): a body pulled back home, bumped by contacts. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

/* was func_8C0584D4: the buoy's base construction: the Actor, its body at rest and the contacts (owned by the
 * position, radius 15.58, room for 2) */
gaddr buoy_base_construct(gaddr obj) {
  actor_construct(obj);
  body_reset(member_addr(obj, Buoy, body));
  contacts_construct(member_addr(obj, Buoy, contacts), member_addr(obj, Buoy, position),
                     F32_BITS(0x417947ADu) /* 15.58 */, 2);
  return obj;
}
HW_EXPORT(8c0584d4, RET(buoy_base_construct(ARG0)))

/* was func_8C058538: type 0x2A's constructor: both vtables, then the base */
gaddr buoy_construct(gaddr obj) {
  Buoy *b = (Buoy *)hw_ptr(obj);
  b->base.vtable = 0x8C23E49Cu;
  b->second_vtable = 0x8C23E4BCu;
  buoy_base_construct(obj);
  return obj;
}
HW_EXPORT(8c058538, RET(buoy_construct(ARG0)))

/* was func_8C0585C0: set the buoy up from its placement: home and body at the position (damping 0.98 vertically,
 * none across), the angles, the kinds; its contacts follow the body (velocity, cell). Kind 1: radius 3, half a unit
 * higher, no damping */
void buoy_init(gaddr obj, gaddr placement) {
  Buoy *b = (Buoy *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  b->home = pl->position;
  body_reset(member_addr(obj, Buoy, body));
  b->body.position = b->home;
  b->body.damping.x = F32(0.0f);
  b->body.damping.y = F32_BITS(0x3F7AE147u); /* 0.98 */
  b->body.damping.z = F32(0.0f);
  b->body.velocity.x = F32(0.0f);
  b->body.velocity.y = F32(0.0f);
  b->body.velocity.z = F32(0.0f);
  b->kind = (int32_t)(pl->param & 0xF);
  b->subkind = (int32_t)(pl->param & 0xF0) >> 4;
  b->angle_x = pl->angle.x;
  b->angle_y = pl->angle.y;
  b->contacts.count = 0;
  b->contacts_word = 4;
  b->contacts.owner_velocity = member_addr(obj, Buoy, body.velocity);
  b->contacts.mass = F32(1.0f);
  b->contacts.unknown18 = F32(1.0f);
  b->contacts.cell = b->base.cell;
  if (b->kind != 0) {
    b->contacts.radius = F32_BITS(0x403FFFFFu); /* 3 (2.9999998) */
    b->body.position.y = fadd(b->body.position.y, F32_BITS(0x3EFFFFFFu) /* 0.5 (0.49999997) */);
    b->body.damping.z = F32(1.0f);
    b->body.damping.y = F32(1.0f);
    b->body.damping.x = F32(1.0f);
  }
}
HW_EXPORT(8c0585c0, buoy_init(ARG0, ARG1))

/* pull the body toward home by `rate` of the distance, and step it */
static void buoy_pull_home(gaddr obj, f32 rate) {
  gaddr pull = hw_stack_alloc(sizeof(Vec3));
  gaddr body = member_addr(obj, Buoy, body);
  vec3_copy(pull, member_addr(obj, Buoy, home));
  vec3_sub(pull, body);
  vec3_scale(rate, pull);
  vec3_add(member_addr(obj, Buoy, body.force), pull);
  hw_stack_free(sizeof(Vec3));
  body_step(body);
}

/* was func_8C0586DE: step the buoy: kind 0 answers its contacts, is pulled home (0.05), moves its contacts to the
 * body and puts them into the race scene's slot 8; kind 1 is only pulled home (0.02) */
void buoy_update(gaddr obj) {
  Buoy *b = (Buoy *)hw_ptr(obj);
  switch (b->kind) {
  case 0:
    contacts_call_handlers(member_addr(obj, Buoy, contacts), member_addr(obj, Buoy, body), 0, 0);
    buoy_pull_home(obj, F32_BITS(0x3D4CCCCCu) /* 0.05 */);
    b->position = b->body.position;
    race_scene_slot_insert(g_panel, member_addr(obj, Buoy, contacts), 8);
    break;
  case 1:
    buoy_pull_home(obj, F32_BITS(0x3CA3D70Au) /* 0.02 */);
    break;
  default:
    break;
  }
}
HW_EXPORT(8c0586de, buoy_update(ARG0))
