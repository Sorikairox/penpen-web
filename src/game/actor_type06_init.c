/* Pen Pen port, S19 (u): the init method of the type-0x06 actor (ActorType06, game/actor_b06.h; vtable 0x8C23DF54,
 * slot 1). */
#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"
#include "actor_b06.h"

#define TYPE06_DAMPING F32_BITS(0x3F4CCCCCu)     /* 0.79999995 */
#define TYPE06_BURST_RISE F32_BITS(0x40FFFFFFu)  /* 7.9999995: the burst starts this far above the body */
#define TYPE06_PART2_RISE F32_BITS(0x419D9999u)  /* 19.699999: the second part sits this far above it */
#define TYPE06_BURST_TEXTURE 0x8C0970E4u         /* the burst's particles' NJS_TEXANIM */
#define TYPE06_MASS F32_BITS(0x3E4CCCCCu)        /* 0.19999999 */
#define TYPE06_CONTACTS_1C 0xCD
#define CONTACTS_WORD_INIT 4

/* was func_8C04BCB4: the init: the burst's defaults; the body at rest at the placement (damping 0.8), the yaw from
 * the placement; the burst 8 above the body with its texture; the second part at rest 19.7 above the body; the
 * contacts (mass 0.2) on the actor's cell; touchable */
void actor_type06_init(gaddr obj, gaddr placement) {
  ActorType06 *a = (ActorType06 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  effect_defaults(a->burst);
  gaddr body = member_addr(obj, ActorType06, body);
  body_reset(body);
  f32 z = p->position.z, y = p->position.y, x = p->position.x;
  a->body.position.x = x;
  a->body.position.y = y;
  a->body.position.z = z;
  vec3_fill(member_addr(body, Body, damping), TYPE06_DAMPING);
  a->body.velocity.x = F32(0.0f);
  a->body.velocity.y = F32(0.0f);
  a->body.velocity.z = F32(0.0f);
  a->yaw = p->angle.y;
  a->state = 0;
  a->word60 = 0;

  /* the burst's origin and the second part's place: the body's position raised (through locals, as the original's
   * block copies) */
  Vec3 origin = a->body.position;
  origin.y = fadd(origin.y, TYPE06_BURST_RISE);
  Burst *b = (Burst *)hw_ptr(a->burst);
  b->origin = origin;
  b->texture = TYPE06_BURST_TEXTURE;
  Vec3 part2 = a->body.position;
  part2.y = fadd(part2.y, TYPE06_PART2_RISE);
  body_reset(member_addr(obj, ActorType06, body2));
  a->body2.position = part2;

  Contacts *c = &a->contacts;
  c->count = 0;
  a->contacts_word = CONTACTS_WORD_INIT;
  c->cell = a->base.cell;
  c->unknown18 = F32(1.0f);
  c->mass = TYPE06_MASS;
  c->cell = a->base.cell;
  c->unknown1c = TYPE06_CONTACTS_1C;
  a->touchable = 1;
}
HW_EXPORT(8c04bcb4, actor_type06_init(ARG0, ARG1))
