/* Pen Pen port, S18 b05: the draw of the geyser (type 0x18) (game/actor.h: Geyser; the rest is in
 * game/actor_geyser.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "actor.h"
#include "actor_b06.h" /* motes_draw */

/* SDK nj_ModelL3 (0x8C0D1996, port/symbols/sdk_calls.tsv) */
static inline void nj_ModelL3(gaddr model) { CALL1(0x8C0D1996u, model); }

/* was func_8C056488: the draw method: at its position turned about y and scaled by its scale Body, its model (by
 * cell_index) drawn with nj_ModelL3 (flag12) or nj_ModelL1; then its motes when they are on (a tail call) */
void geyser_draw(gaddr obj) {
  const Geyser *g = (const Geyser *)hw_ptr(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Geyser, position));
  njRotateY(0, f32_to_int(g->angle_y));
  njScale(0, g->scale.position.x, g->scale.position.y, g->scale.position.z);
  if (g->flag12 != 0)
    nj_ModelL3(actor_model(&g->base, g->cell_index));
  else
    nj_ModelL1(actor_model(&g->base, g->cell_index));
  njPopMatrix(1);
  if (g->motes_on != 0) motes_draw(member_addr(obj, Geyser, motes));
}
HW_EXPORT(8c056488, geyser_draw(ARG0))
