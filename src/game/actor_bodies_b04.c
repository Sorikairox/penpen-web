/* Pen Pen port, S18 b04: the contact responses of the post and of the balls (Post, Balls, game/actor.h; their inits
 * are game/actor_bodies.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "actor.h"

#define POST_BODY_RADIUS  F32_BITS(0x40FFFFFFu)  /* 7.9999995: the touching body's sphere */
#define POST_STEEP        F32_BITS(0x3F34FDF3u)  /* 0.7070: cos 45 degrees */
#define POST_MODEL        5                      /* in the actor's model list */

/* func_8C0148C4 (still translated): test the contacts against a model drawn by the current matrix */
static void contacts_test_model(gaddr contacts, gaddr model) { CALL2(0x8C0148C4u, contacts, model); }

/* the original's frame (0x90 bytes) */
enum {
  POST_F_PUSH = 0x04,      /* Vec3 */
  POST_F_CONTACTS = 0x24,  /* Contacts (0x54 bytes): the touching body's own, for the test against the model */
  POST_F_FRAME = 0x90
};

/* was func_8C04D468: the post's contact response (Post.contacts.respond) to contact i with `body`: the body's own
 * sphere (radius 8, room for 8) is tested against the post's model (model 5, at the post's position turned by its
 * angles z, y, x); for each contact found the body is pushed out along the normal by the depth and, when it moves
 * into it, loses that part of its velocity; when it touched and every normal was steeper than 45 degrees (the
 * lowest normal y above 0.707), it is pushed by the post's tip */
void post_respond(gaddr contacts, int32_t i, gaddr body) {
  gaddr post = ((const gaddr *)hw_ptr(contacts_at(contacts)->unknown4c))[i];
  const Post *p = (const Post *)hw_ptr(post);
  gaddr frame = hw_stack_alloc(POST_F_FRAME);
  gaddr own = frame + POST_F_CONTACTS;
  gaddr push = frame + POST_F_PUSH;
  contacts_construct(own, body, POST_BODY_RADIUS, 8);
  matrix_push();
  matrix_reset();
  matrix_translate_v(member_addr(post, Post, position));
  matrix_rotate_z(p->angle.z);
  matrix_rotate_y(p->angle.y);
  matrix_rotate_x(p->angle.x);
  contacts_test_model(own, actor_model(&p->base, POST_MODEL));
  matrix_pop();
  const Contacts *c = contacts_at(own);
  gaddr depth = c->depth, normal = c->normal;
  const f32 bounce = F32(1.0f); /* the original's local: the part of the velocity into the normal taken away */
  f32 lowest = F32(1.0f);
  for (int32_t k = 0; k < c->count; k++, depth += 4, normal += sizeof(Vec3)) {
    vec3_scaled(F32_AT(depth), push, normal);
    vec3_add(body, push);
    f32 d = fmul(njInnerProduct(normal, member_addr(body, Body, velocity)), bounce);
    if (fgt(F32(0.0f), d)) {
      vec3_scaled(fneg(d), push, normal);
      vec3_add(member_addr(body, Body, velocity), push);
      if (fgt(lowest, vec3_at(normal)->y)) lowest = vec3_at(normal)->y;
    }
  }
  if (c->count != 0 && fgt(lowest, POST_STEEP))
    vec3_add(member_addr(body, Body, velocity), member_addr(post, Post, tip));
  contacts_free(own);
  hw_stack_free(POST_F_FRAME);
}
HW_EXPORT(8c04d468, post_respond(ARG0, (int32_t)ARG1, ARG2))

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}

/* the original's frame of the balls' response (0x2C bytes) */
enum {
  BALLS_F_SPEED = 0x00,   /* f32 */
  BALLS_F_ANGLE = 0x04,   /* f32 */
  BALLS_F_DIST = 0x08,    /* f32 */
  BALLS_F_OFFSET = 0x0C,  /* Vec3 */
  BALLS_F_FRAME = 0x2C
};

/* was func_8C04DD94: the balls' contact response (Balls.contacts.respond) to contact i with `body` (`carried`: a
 * float the swing is added to, or 0): a body not above the top (the centre's height plus the radius) is put on top,
 * the contact's normal straight up (its word 0xFF), its fall stopped, and it is carried round with the swing: moved
 * along the circle about the centre by the swing's angle (distance x swing x 2 pi / 65536, a quarter turn ahead of
 * its direction from the centre) */
void balls_respond(gaddr contacts, int32_t i, gaddr body, gaddr carried) {
  const Contacts *c = contacts_at(contacts);
  gaddr balls = ((const gaddr *)hw_ptr(c->unknown4c))[i];
  const Balls *b = (const Balls *)hw_ptr(balls);
  gaddr frame = hw_stack_alloc(BALLS_F_FRAME);
  Body *bd = body_at(body);
  if (fgt(bd->position.y, fadd(b->position.y, c->radius))) {
    hw_stack_free(BALLS_F_FRAME);
    return;
  }
  bd->position.y = fadd(b->position.y, c->radius);
  ((int32_t *)hw_ptr(c->unknown48))[i] = 0xFF;
  vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), c->normal + (gaddr)(i * (int32_t)sizeof(Vec3)));
  if (fgt(F32(0.0f), bd->velocity.y)) bd->velocity.y = F32(0.0f);
  gaddr offset = frame + BALLS_F_OFFSET;
  vec3_copy(offset, body);
  vec3_sub(offset, member_addr(balls, Balls, position));
  vec3_at(offset)->y = F32(0.0f);
  f32 d = njScalor(offset);
  F32_AT(frame + BALLS_F_DIST) = d;
  f32 radians = rt_atan2f(vec3_at(offset)->x, vec3_at(offset)->z);
  f32 angle = f32_from_int(f32_to_int(fdiv(fmul(radians, F32(65536.0f)), F32_BITS(0x40C90FD7u) /* 2 pi */)));
  f32 speed = fdiv(fmul(fmul(d, b->swing.value), F32_BITS(0x40C90FDAu) /* 2 pi */), F32(65536.0f));
  F32_AT(frame + BALLS_F_SPEED) = speed;
  angle = fadd(angle, F32(16384.0f));
  F32_AT(frame + BALLS_F_ANGLE) = angle;
  f32 z = fmul(speed, njCos(f32_to_int(angle)));
  f32 x = fmul(speed, njSin(f32_to_int(angle)));
  vec3_add_xyz(x, F32(0.0f), z, body);
  if (carried != 0) f32_add_to(b->swing.value, carried);
  hw_stack_free(BALLS_F_FRAME);
}
HW_EXPORT(8c04dd94, balls_respond(ARG0, (int32_t)ARG1, ARG2, ARG3))
