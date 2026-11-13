/* Pen Pen port, S17 b06: the destructors and the drawing of the leaning post (type 0x2E) and of the big body with
 * eight small spheres (type 0x1E) (game/actor_bodies.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"
#include "scenery.h"
#include "actor_b06.h"

/* ---- type 0x2E: the post ---- */

#define POST_MODEL 5 /* in the actor's model list */

/* was func_8C04D23C: the post's destructor body: its contacts freed */
void post_destroy(gaddr obj) { contacts_free(member_addr(obj, Post, contacts)); }
HW_EXPORT(8c04d23c, post_destroy(ARG0))

/* was func_8C04D250: the deleting destructor */
gaddr post_delete(gaddr obj, int32_t free_memory) {
  post_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04d250, RET(post_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04D274: the destructor body, returning the object */
gaddr post_destroy_self(gaddr obj) {
  post_destroy(obj);
  return obj;
}
HW_EXPORT(8c04d274, RET(post_destroy_self(ARG0)))

/* was func_8C04D3D2: draw the post: model 5 at its position, turned about z, y, then x by its angles */
void post_draw(gaddr obj) {
  const Post *p = (const Post *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Post, position));
  njRotateZ(0, f32_to_int(p->angle.z));
  njRotateY(0, f32_to_int(p->angle.y));
  njRotateX(0, f32_to_int(p->angle.x));
  nj_ModelL1(actor_model(&p->base, POST_MODEL));
  njPopMatrix(1);
}
HW_EXPORT(8c04d3d2, post_draw(ARG0))

/* ---- type 0x1E: the big body with eight small spheres ---- */

#define BALLS_MODEL 3 /* in the actor's model list */
#define CONTACTS_FREE 0x8C013D38u /* contacts_free (core/contact.c): the spheres' destructor, by address */

/* was func_8C04D940: the destructor body: the eight spheres' contacts destroyed, then the big contacts freed */
void balls_destroy(gaddr obj) {
  array_destroy(BALLS_POINTS, sizeof(ContactsSlot), member_addr(obj, Balls, spheres), CONTACTS_FREE);
  contacts_free(member_addr(obj, Balls, contacts));
}
HW_EXPORT(8c04d940, balls_destroy(ARG0))

/* was func_8C04D966: the deleting destructor */
gaddr balls_delete(gaddr obj, int32_t free_memory) {
  balls_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04d966, RET(balls_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04D98A: the destructor body, returning the object */
gaddr balls_destroy_self(gaddr obj) {
  balls_destroy(obj);
  return obj;
}
HW_EXPORT(8c04d98a, RET(balls_destroy_self(ARG0)))

/* was func_8C04DD36: draw the big body: model 3 at its position, turned about y by f34 */
void balls_draw(gaddr obj) {
  const Balls *b = (const Balls *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Balls, position));
  njRotateY(0, f32_to_int(b->f34));
  nj_ModelL1(actor_model(&b->base, BALLS_MODEL));
  njPopMatrix(1);
}
HW_EXPORT(8c04dd36, balls_draw(ARG0))
