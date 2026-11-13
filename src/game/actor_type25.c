/* Pen Pen port, S18 b05: the draw of the scenery actor of placement type 0x25 (game/actor.h: ActorType25; its
 * construction and clock are in game/actor_flyer.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "actor.h"
#include "scenery.h" /* shape_motion_draw_l1 */

/* was func_8C059780: the draw method: its textures; at its Body's position, turned about z, x and y and scaled by
 * its scale, its object drawn with the shape motion of its motion entry at its clock (looping) */
void actor_type25_draw(gaddr obj) {
  const ActorType25 *a = (const ActorType25 *)hw_ptr(obj);
  njSetTexture(a->textures);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, ActorType25, body));
  njRotateZ(0, f32_to_int(a->angle.z));
  njRotateX(0, f32_to_int(a->angle.x));
  njRotateY(0, f32_to_int(a->angle.y));
  njScale(0, a->scale, a->scale, a->scale);
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  shape_motion_draw_l1(a->object, e->motion, e->shape, member_addr(obj, ActorType25, clock), 1);
  njPopMatrix(1);
}
HW_EXPORT(8c059780, actor_type25_draw(ARG0))
