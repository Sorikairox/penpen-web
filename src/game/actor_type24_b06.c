/* Pen Pen port, S17 b06: the destructors and the draw of the scenery actor of placement type 0x24 (game/actor.h:
 * ActorType24). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "actor.h"
#include "scenery.h"
#include "actor_b06.h"

/* was func_8C051FFC: the destructor body: its contacts freed */
void actor_type24_destroy(gaddr obj) { contacts_free(member_addr(obj, ActorType24, contacts)); }
HW_EXPORT(8c051ffc, actor_type24_destroy(ARG0))

/* was func_8C052012: the deleting destructor */
gaddr actor_type24_delete(gaddr obj, int32_t free_memory) {
  actor_type24_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c052012, RET(actor_type24_delete(ARG0, (int32_t)ARG1)))

/* was func_8C052036: the destructor body, returning the object */
gaddr actor_type24_destroy_self(gaddr obj) {
  actor_type24_destroy(obj);
  return obj;
}
HW_EXPORT(8c052036, RET(actor_type24_destroy_self(ARG0)))

/* was func_8C052668: draw it: its texture (word184), its model at the body's position turned about z, y, then x by
 * its angles and scaled by the first three values, with the shape motion of its motion entry at its frame */
void actor_type24_draw(gaddr obj) {
  const ActorType24 *a = (const ActorType24 *)hw_ptr(obj);
  njSetTexture(a->word184);
  matrix_push();
  matrix_translate_v(member_addr(obj, ActorType24, body.position));
  matrix_rotate_z(a->angle.z);
  matrix_rotate_y(a->angle.y);
  matrix_rotate_x(a->angle.x);
  matrix_scale(a->values[0].value, a->values[1].value, a->values[2].value);
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  shape_motion_draw_l1(a->model, e->motion, e->shape, member_addr(obj, ActorType24, frame), 1);
  matrix_pop();
}
HW_EXPORT(8c052668, actor_type24_draw(ARG0))
