/* Pen Pen port, S17 b07: the scenery actor of placement type 0x0B (game/actor_b07.h): a base turned about y, an arm
 * that rocks up and down and a part that spins, with a wide contact set (its own response) and a small one at the
 * arm's tip. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"
#include "actor_b07.h"

#define TYPE0B_VTABLE        0x8C23E1DCu
#define TYPE0B_SECOND_VTABLE 0x8C23E1FCu
#define TYPE0B_RESPOND       0x8C052D20u /* the wide contacts' response (still translated) */
#define TYPE0B_TIP_RADIUS    F32_BITS(0x40EBF7CEu) /* 7.374 */
#define TYPE0B_RADIUS        F32_BITS(0x41F7FFFFu) /* 30.999998 */
#define TYPE0B_CONTACT_KIND  0xCD
#define TYPE0B_SLOT          8           /* both contact sets' slot in the race scene's list */
#define TYPE0B_MODEL_BASE    5           /* in the actor's model list; 6 when the param is not 0 */
#define TYPE0B_MODEL_BASE_ALT 6
#define TYPE0B_MODEL_ARM     7
#define TYPE0B_MODEL_SPINNER 8

#define DEG_7   F32_BITS(0x449F49F4u) /* 1274.311: 7 degrees in Ninja units */
#define DEG_10  F32_BITS(0x44E38E38u) /* 1820.4443 */
#define DEG_M30 F32_BITS(0xC5AAAAAAu) /* -5461.333 */

/* was func_8C05293C: the base construction: the Actor, its Body at rest, the wide contacts owned by the position
 * (no radius, no room yet) and the tip's (radius 7.37, room for 3) */
gaddr actor_type0b_base_construct(gaddr obj) {
  actor_construct(obj);
  gaddr body = member_addr(obj, ActorType0B, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  contacts_construct(member_addr(obj, ActorType0B, contacts), member_addr(obj, ActorType0B, position), F32(0.0f), 0);
  contacts_construct(member_addr(obj, ActorType0B, tip_contacts), member_addr(obj, ActorType0B, tip),
                     TYPE0B_TIP_RADIUS, 3);
  return obj;
}
HW_EXPORT(8c05293c, RET(actor_type0b_base_construct(ARG0)))

/* was func_8C0529B2: type 0x0B's constructor: both vtables, then the base */
gaddr actor_type0b_construct(gaddr obj) {
  ActorType0B *a = (ActorType0B *)hw_ptr(obj);
  a->base.vtable = TYPE0B_VTABLE;
  a->second_vtable = TYPE0B_SECOND_VTABLE;
  actor_type0b_base_construct(obj);
  return obj;
}
HW_EXPORT(8c0529b2, RET(actor_type0b_construct(ARG0)))

/* was func_8C0529CE: the destructor body: the tip's contacts freed, then the wide ones */
void actor_type0b_destroy(gaddr obj) {
  contacts_free(member_addr(obj, ActorType0B, tip_contacts));
  contacts_free(member_addr(obj, ActorType0B, contacts));
}
HW_EXPORT(8c0529ce, actor_type0b_destroy(ARG0))

/* was func_8C0529EC */
gaddr actor_type0b_delete(gaddr obj, int32_t free_memory) {
  actor_type0b_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c0529ec, RET(actor_type0b_delete(ARG0, (int32_t)ARG1)))

/* was func_8C052A10 */
gaddr actor_type0b_destroy_self(gaddr obj) {
  actor_type0b_destroy(obj);
  return obj;
}
HW_EXPORT(8c052a10, RET(actor_type0b_destroy_self(ARG0)))

/* the init's frame on the guest stack (0x5C bytes, as the original's) */
typedef struct Type0BInitFrame {
  uint32_t slot;            /* 0x00 */
  Vec3 offset;              /* 0x04: the tip's offset from the position, before the turn */
  Matrix turn;              /* 0x10 */
  Vec3 position;            /* 0x50: the placement's position, copied through it */
} Type0BInitFrame;
HW_OFFSET(Type0BInitFrame, offset, 0x04);
HW_OFFSET(Type0BInitFrame, turn, 0x10);
HW_OFFSET(Type0BInitFrame, position, 0x50);
HW_SIZE(Type0BInitFrame, 0x5C);

/* was func_8C052A50: set it up from its placement: at the placement's position and angle about y, its Body at rest
 * there, the arm level and the spinner still; the wide contacts (radius 31, kind 0xCD, its own response with the
 * actor as its argument) and the tip's on the actor's cell; the tip at (0, 8.4, -34.12) turned by the angle, from
 * the position */
void actor_type0b_init(gaddr obj, gaddr placement) {
  ActorType0B *a = (ActorType0B *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  gaddr frame = hw_stack_alloc(sizeof(Type0BInitFrame));
  Type0BInitFrame *f = (Type0BInitFrame *)hw_ptr(frame);

  f->position = pl->position;
  a->position = f->position;
  a->angle_y = pl->angle.y;
  gaddr body = member_addr(obj, ActorType0B, body);
  vec3_zero(member_addr(body, Body, position));
  vec3_zero(member_addr(body, Body, velocity));
  vec3_zero(member_addr(body, Body, force));
  body_damping_none(body);
  a->body.position = a->position;
  a->body.damping.z = F32(1.0f);
  a->body.damping.y = F32(1.0f);
  a->body.damping.x = F32(1.0f);

  a->param = pl->param;
  a->rock = F32(0.0f);
  a->rising = 0;
  a->spin = F32(0.0f);
  a->word78 = 0;
  a->contacts.count = 0;
  a->contacts_word = 4;
  a->contacts.cell = a->base.cell;
  a->contacts.radius = TYPE0B_RADIUS;
  a->contacts.unknown1c = TYPE0B_CONTACT_KIND;
  a->contacts.respond = TYPE0B_RESPOND;
  a->contacts.unknown20 = (int32_t)obj;
  a->tip_contacts.count = 0;
  a->tip_contacts_word = 4;
  a->tip_contacts.cell = a->base.cell;

  /* the tip: the offset turned about y (a local matrix: first the inline identity, then njUnitMatrix) */
  gaddr turn = member_addr(frame, Type0BInitFrame, turn);
  matrix_identity(turn);
  njUnitMatrix(turn);
  njRotateY(turn, f32_to_int(a->angle_y));
  f->offset.x = F32(0.0f);
  f->offset.y = F32_BITS(0x41066666u); /* 8.4 */
  f->offset.z = F32_BITS(0xC2087AE1u); /* -34.12 */
  njCalcPoint(turn, member_addr(frame, Type0BInitFrame, offset), member_addr(obj, ActorType0B, tip));
  vec3_add(member_addr(obj, ActorType0B, tip), member_addr(obj, ActorType0B, position));
  hw_stack_free(sizeof(Type0BInitFrame));
}
HW_EXPORT(8c052a50, actor_type0b_init(ARG0, ARG1))

/* was func_8C052BF4: step it: the spinner turns 10 degrees; the arm rocks 7 degrees a step, down to -30 degrees
 * and back up to 0; both contact sets go into the race scene's slot 8 */
void actor_type0b_update(gaddr obj) {
  ActorType0B *a = (ActorType0B *)hw_ptr(obj);
  a->spin = fadd(a->spin, DEG_10);
  if (a->rising != 0) {
    a->rock = fadd(a->rock, DEG_7);
    if (fgt(a->rock, F32(0.0f))) a->rising = 0;
  } else {
    a->rock = fsub(a->rock, DEG_7);
    if (fgt(DEG_M30, a->rock)) a->rising = 1;
  }
  race_scene_slot_insert(g_panel, member_addr(obj, ActorType0B, contacts), TYPE0B_SLOT);
  race_scene_slot_insert(g_panel, member_addr(obj, ActorType0B, tip_contacts), TYPE0B_SLOT);
}
HW_EXPORT(8c052bf4, actor_type0b_update(ARG0))

/* was func_8C052C70: draw it: the base (model 5, or 6 when the param is set) at the position turned about y; on it
 * the arm (model 7) at (0, 25.3, -37.2) rocked about x, and the spinner (model 8) at (0, 41.9, -32.3) turned about
 * y by its spin */
void actor_type0b_draw(gaddr obj) {
  ActorType0B *a = (ActorType0B *)hw_ptr(obj);
  matrix_push();
  matrix_translate_v(member_addr(obj, ActorType0B, position));
  matrix_rotate_y(a->angle_y);
  int32_t base_model = TYPE0B_MODEL_BASE;
  if (a->param != 0) base_model = TYPE0B_MODEL_BASE_ALT;
  nj_ModelL1(actor_model(&a->base, base_model));
  matrix_push();
  matrix_translate(F32(0.0f), F32_BITS(0x41CA6666u) /* 25.3 */, F32_BITS(0xC214CCCCu) /* -37.2 */);
  matrix_rotate_x(a->rock);
  nj_ModelL1(actor_model(&a->base, TYPE0B_MODEL_ARM));
  matrix_pop();
  matrix_translate(F32(0.0f), F32_BITS(0x42279998u) /* 41.9 */, F32_BITS(0xC2013332u) /* -32.3 */);
  matrix_rotate_y(a->spin);
  nj_ModelL1(actor_model(&a->base, TYPE0B_MODEL_SPINNER));
  matrix_pop();
}
HW_EXPORT(8c052c70, actor_type0b_draw(ARG0))
