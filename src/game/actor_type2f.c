/* Pen Pen port, S17 b05: the init method of placement type 0x2F (ActorType2F, game/actor_b05.h): a body placed 2
 * above its home, its contacts answered by func_8C048B56, drawn in a translucent green. (Its update,
 * func_8C048DE6, is still translated.) */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"
#include "actor_b05.h"

#define TYPE2F_RESPOND   0x8C048B56u              /* the contacts' response (guest code: its address is data) */
#define TYPE2F_LIFT      F32_BITS(0x3FFFFFFFu)    /* 1.9999999: the body starts this far above home */
#define TYPE2F_ALPHA     F32_BITS(0x3F199999u)    /* 0.6 */
#define TYPE2F_CONTACT_KIND 0xCD                  /* Contacts.unknown1c */

/* was func_8C048CE8: the init method: home = the placement's position; the body reset, placed at home with no
 * damping and at rest; the state, the counters and the contacts cleared; the contacts follow the body's velocity
 * with mass 1, in the actor's cell; the body lifted by 2; the colour (0.6, 0, 1, 0) */
void actor_type2f_init(gaddr obj, gaddr placement) {
  ActorType2F *a = (ActorType2F *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  f32 x = p->position.x, y = p->position.y, z = p->position.z;
  a->home.z = z;
  a->home.y = y;
  a->home.x = x;
  gaddr body = member_addr(obj, ActorType2F, body);
  vec3_zero(member_addr(body, Body, position));
  vec3_zero(member_addr(body, Body, velocity));
  vec3_zero(member_addr(body, Body, force));
  vec3_fill(member_addr(body, Body, damping), F32(1.0f));
  memmove(&a->body.position, &a->home, sizeof(Vec3));
  a->body.damping.z = F32(1.0f);
  a->body.damping.y = F32(1.0f);
  a->body.damping.x = F32(1.0f);
  a->body.velocity.x = F32(0.0f);
  a->body.velocity.y = F32(0.0f);
  a->body.velocity.z = F32(0.0f);
  a->param = p->param;
  a->word30 = 0;
  a->state = 0;
  a->worddc = -1;
  a->contacts.count = 0;
  a->contacts_word = 4;
  a->contacts.owner_velocity = member_addr(obj, ActorType2F, body.velocity);
  a->contacts.unknown18 = F32(1.0f);
  a->contacts.mass = F32(1.0f);
  a->contacts.cell = a->base.cell;
  a->contacts.unknown1c = TYPE2F_CONTACT_KIND;
  a->contacts.respond = TYPE2F_RESPOND;
  a->body.position.y = fadd(a->body.position.y, TYPE2F_LIFT);
  a->color.a = TYPE2F_ALPHA;
  a->color.r = F32(0.0f);
  a->color.g = F32(1.0f);
  a->color.b = F32(0.0f);
}
HW_EXPORT(8c048ce8, actor_type2f_init(ARG0, ARG1))

/* func_8C011A94: draw a debug sphere (a unit model moved to the centre and scaled by twice the radius; the colour
 * the caller also passes in r5 is not read) */
static void sphere_draw_debug(gaddr sphere) { CALL1(0x8C011A94u, sphere); }

/* the draw's frame: the sphere at 4 */
typedef struct Type2FDrawFrame {
  uint8_t pad_00[4];
  Sphere sphere;            /* 0x04 */
} Type2FDrawFrame;
HW_OFFSET(Type2FDrawFrame, sphere, 0x04);
HW_SIZE(Type2FDrawFrame, 0x14);

/* was func_8C04913A: the draw (states below 5): the debug sphere at the body's position, radius 10 */
void actor_type2f_draw(gaddr obj) {
  if (((ActorType2F *)hw_ptr(obj))->state >= 5) return;
  gaddr frame = hw_stack_alloc(sizeof(Type2FDrawFrame));
  gaddr sphere = member_addr(frame, Type2FDrawFrame, sphere);
  vec3_copy(member_addr(sphere, Sphere, center), member_addr(obj, ActorType2F, body.position));
  ((Sphere *)hw_ptr(sphere))->radius = F32(10.0f);
  sphere_draw_debug(sphere);
  hw_stack_free(sizeof(Type2FDrawFrame));
}
HW_EXPORT(8c04913a, actor_type2f_draw(ARG0))
