/* Pen Pen port, S17 b09: the destruction and the drawing of the scenery actor of placement type 0x20
 * (game/actor.h: ActorType20, constructed by game/actor_types.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "actor.h"

#define TYPE20_SPRITE_ATTR 0x30 /* njDrawSprite3D's attribute */

/* SDK njColorBlendingMode (0x8C0A23A8) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* was func_8C061984 (both vtables' first entry): the destructor body is empty (func_8C061980, a copy of
 * method_empty) */
gaddr actor_type20_delete(gaddr obj, int32_t free_memory) {
  method_empty();
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c061984, RET(actor_type20_delete(ARG0, (int32_t)ARG1)))

/* was func_8C0619A8 */
gaddr actor_type20_destroy_self(gaddr obj) {
  method_empty();
  return obj;
}
HW_EXPORT(8c0619a8, RET(actor_type20_destroy_self(ARG0)))

/* was func_8C061BD6: draw it: without fog and with blending modes 10 / 10, its texture list set, its sprite (an
 * NJS_SPRITE at `home`: home, word4c / word50 the scales, word54 the angle, word58 the texlist, table the texanim)
 * at cell `frame`; then the usual blending modes 8 / 6 and the fog as the current view wants it */
void actor_type20_draw(gaddr obj) {
  const ActorType20 *a = (const ActorType20 *)hw_ptr(obj);
  njFogDisable();
  njColorBlendingMode(0, 10);
  njColorBlendingMode(1, 10);
  njSetTexture(a->tex_word);
  njDrawSprite3D(member_addr(obj, ActorType20, home), a->frame, TYPE20_SPRITE_ATTR);
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  view_apply_fog();
}
HW_EXPORT(8c061bd6, actor_type20_draw(ARG0))
