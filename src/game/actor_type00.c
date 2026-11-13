/* Pen Pen port, S17 b05: the methods of placement type 0x00 (ActorType00, game/actor_b05.h): an unseen sphere of
 * radius 10 that starts 3 to the side of its home and is pulled back to it (0.05 of the way as a force each update);
 * its contacts go to the race scene's slot list, so other bodies bump it. Its draw shows the sphere (a debug
 * draw). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "race_scene.h"
#include "actor.h"
#include "actor_b05.h"

#define TYPE00_OFFSET_X F32_BITS(0x403FFFFFu) /* 2.9999998: the body starts this far in +x of home */
#define TYPE00_PULL     F32_BITS(0x3D4CCCCCu) /* 0.05: of the way home, as a force */
#define TYPE00_RADIUS   F32(10.0f)

/* func_8C011A94: draw a debug sphere (a unit model moved to the centre and scaled by twice the radius) */
static void sphere_draw_debug(gaddr sphere) { CALL1(0x8C011A94u, sphere); }

/* was func_8C044B52: the init method: home = the placement's position, the body there plus 3 in x with no damping;
 * its contacts follow the body's velocity with mass 1 */
void actor_type00_init(gaddr obj, gaddr placement) {
  ActorType00 *a = (ActorType00 *)hw_ptr(obj);
  Vec3 position = ((const ActorPlacement *)hw_ptr(placement))->position;
  memmove(&a->home, &position, sizeof(Vec3));
  memmove(&a->body.position, &a->home, sizeof(Vec3));
  a->body.position.x = fadd(a->body.position.x, TYPE00_OFFSET_X);
  a->body.damping.z = F32(1.0f);
  a->body.damping.y = F32(1.0f);
  a->body.damping.x = F32(1.0f);
  a->contacts.owner_velocity = member_addr(obj, ActorType00, body.velocity);
  a->contacts.mass = F32(1.0f);
  a->contacts.unknown18 = F32(1.0f);
}
HW_EXPORT(8c044b52, actor_type00_init(ARG0, ARG1))

/* the update's frame: the pull at 4 */
typedef struct Type00Frame {
  uint8_t pad_00[4];
  Vec3 pull;                /* 0x04 */
} Type00Frame;
HW_OFFSET(Type00Frame, pull, 0x04);
HW_SIZE(Type00Frame, 0x10);

/* was func_8C044BD6: the update: force += 0.05 * (home - position), a step, the contacts into a free slot of the
 * race scene's list */
void actor_type00_update(gaddr obj) {
  gaddr frame = hw_stack_alloc(sizeof(Type00Frame));
  gaddr pull = member_addr(frame, Type00Frame, pull);
  gaddr body = member_addr(obj, ActorType00, body);
  vec3_copy(pull, member_addr(obj, ActorType00, home));
  vec3_sub(pull, body);
  vec3_scale(TYPE00_PULL, pull);
  vec3_add(member_addr(body, Body, force), pull);
  body_step(body);
  race_scene_slot_insert(g_panel, member_addr(obj, ActorType00, contacts), -1);
  hw_stack_free(sizeof(Type00Frame));
}
HW_EXPORT(8c044bd6, actor_type00_update(ARG0))

/* the draw's frame: the sphere at 4 */
typedef struct Type00DrawFrame {
  uint8_t pad_00[4];
  Sphere sphere;            /* 0x04 */
} Type00DrawFrame;
HW_OFFSET(Type00DrawFrame, sphere, 0x04);
HW_SIZE(Type00DrawFrame, 0x14);

/* was func_8C044C60: the draw: the debug sphere at the body's position, radius 10 */
void actor_type00_draw(gaddr obj) {
  njPushMatrix(0);
  gaddr frame = hw_stack_alloc(sizeof(Type00DrawFrame));
  gaddr sphere = member_addr(frame, Type00DrawFrame, sphere);
  vec3_copy(member_addr(sphere, Sphere, center), member_addr(obj, ActorType00, body.position));
  ((Sphere *)hw_ptr(sphere))->radius = TYPE00_RADIUS;
  sphere_draw_debug(sphere);
  hw_stack_free(sizeof(Type00DrawFrame));
  njPopMatrix(1);
}
HW_EXPORT(8c044c60, actor_type00_draw(ARG0))
