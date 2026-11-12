/* Pen Pen port, S14 b11: the ornament of six kinds (type 0x14) (game/actor.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

#define NJD_EVAL_UNIT_ANG 2u /* NJS_OBJECT evalflags: no rotation */

/* was func_8C05B108: the ornament's base construction: the Actor, its body and two swings at rest and the contacts
 * (owned by the body, radius 5, room for 2) */
gaddr ornament_base_construct(gaddr obj) {
  Ornament *o = (Ornament *)hw_ptr(obj);
  actor_construct(obj);
  body_reset(member_addr(obj, Ornament, body));
  o->swing_a.value = F32(0.0f);
  o->swing_a.velocity = F32(0.0f);
  o->swing_a.force = F32(0.0f);
  o->swing_a.damping = F32(1.0f);
  o->swing_b.value = F32(0.0f);
  o->swing_b.velocity = F32(0.0f);
  o->swing_b.force = F32(0.0f);
  o->swing_b.damping = F32(1.0f);
  contacts_construct(member_addr(obj, Ornament, contacts), member_addr(obj, Ornament, body), F32(5.0f), 2);
  return obj;
}
HW_EXPORT(8c05b108, RET(ornament_base_construct(ARG0)))

/* was func_8C05B194: type 0x14's constructor: both vtables, then the base */
gaddr ornament_construct(gaddr obj) {
  Ornament *o = (Ornament *)hw_ptr(obj);
  o->base.vtable = 0x8C23E67Cu;
  o->second_vtable = 0x8C23E69Cu;
  ornament_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05b194, RET(ornament_construct(ARG0)))

/* a swing at rest at `value` (damping 1) */
static void swing_reset(Body1 *s) {
  s->value = F32(0.0f);
  s->velocity = F32(0.0f);
  s->force = F32(0.0f);
  s->damping = F32(1.0f);
}

/* was func_8C05B230: set the ornament up from its placement: position, body there (no damping), angles, param.
 * By kind: 0 model 1, the body moved by (0.5, 1, 0.8); 1 and 2 model 2, the body moved by (0.8, 0.1 or 0.3, 0.3),
 * two swings from 25 / -35 degrees (kind 1) or 40 / -25 degrees, the model's first two parts rotating; 3 model 11;
 * 4 model 13; 5 model 11 with damping 0.92. Its contacts follow the body (velocity, cell, kind 3) */
void ornament_init(gaddr obj, gaddr placement) {
  Ornament *o = (Ornament *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  o->position.z = pl->position.z;
  o->position.y = pl->position.y;
  o->position.x = pl->position.x;
  body_reset(member_addr(obj, Ornament, body));
  o->body.position = o->position;
  o->body.damping.z = F32(1.0f);
  o->body.damping.y = F32(1.0f);
  o->body.damping.x = F32(1.0f);
  o->angle.x = pl->angle.x;
  o->angle.y = pl->angle.y;
  o->angle.z = pl->angle.z;
  o->f88 = F32(0.0f);
  o->param = pl->param;
  o->word38 = 0;
  o->word3c = 0;
  switch (o->param & 0xF) {
  case 0:
    o->model = 1;
    o->style = 4;
    o->body.position.x = fadd(o->body.position.x, F32_BITS(0x3EFFFFFFu) /* 0.5 */);
    o->body.position.y = fadd(o->body.position.y, F32_BITS(0x3F7FFFFFu) /* 1 */);
    o->body.position.z = fadd(o->body.position.z, F32_BITS(0x3F4CCCCCu) /* 0.8 */);
    o->f8c = F32_BITS(0x3EB33333u); /* 0.35 */
    break;
  case 1:
  case 2: {
    o->model = 2;
    o->style = 0;
    o->body.position.x = fadd(o->body.position.x, F32_BITS(0x3F4CCCCCu) /* 0.8 */);
    o->body.position.z = fadd(o->body.position.z, F32_BITS(0x3E999999u) /* 0.3 */);
    swing_reset(&o->swing_a);
    swing_reset(&o->swing_b);
    f32 b;
    if (o->param == 1) {
      o->f8c = F32_BITS(0x3ECCCCCCu); /* 0.4 */
      o->body.position.y = fadd(o->body.position.y, F32_BITS(0x3DCCCCCCu) /* 0.1 */);
      o->swing_a.value = F32_BITS(0x458E38E3u); /* 25 degrees */
      b = F32_BITS(0xC5C71C71u);                /* -35 degrees */
    } else {
      o->f8c = F32_BITS(0x3E999999u); /* 0.3 */
      o->body.position.y = fadd(o->body.position.y, F32_BITS(0x3E999999u) /* 0.3 */);
      o->swing_a.value = F32_BITS(0x45E38E38u); /* 40 degrees */
      b = F32_BITS(0xC58E38E3u);                /* -25 degrees */
    }
    o->swing_b.value = b;
    o->swing_a.damping = F32(1.0f);
    o->swing_b.damping = F32(1.0f);
    NjObjectPose *root = (NjObjectPose *)hw_ptr(actor_model(&o->base, o->model));
    NjObjectPose *child = (NjObjectPose *)hw_ptr(root->child);
    child->evalflags &= ~NJD_EVAL_UNIT_ANG;
    NjObjectPose *next = (NjObjectPose *)hw_ptr(child->sibling);
    next->evalflags &= ~NJD_EVAL_UNIT_ANG;
    break;
  }
  case 3:
    o->model = 11;
    o->style = 1;
    break;
  case 4:
    o->model = 13;
    o->style = 2;
    break;
  case 5: {
    o->model = 11;
    o->style = 3;
    const f32 d = F32_BITS(0x3F6B851Eu); /* 0.92 */
    o->body.damping.z = d;
    o->body.damping.y = d;
    o->body.damping.x = d;
    break;
  }
  default:
    break;
  }
  o->subkind = (int32_t)(o->param & 0xF0) >> 4;
  o->contacts.count = 0;
  o->contacts_word = 4;
  o->contacts.owner_velocity = member_addr(obj, Ornament, body.velocity);
  o->contacts.unknown18 = F32(1.0f);
  o->contacts.mass = F32(1.0f);
  o->contacts.cell = o->base.cell;
  o->contacts.unknown1c = 3;
}
HW_EXPORT(8c05b230, ornament_init(ARG0, ARG1))
