/* Pen Pen port, S17 b05: the draw of the turning actor (Spinner, game/actor.h; by its address, placement type
 * 0x0E): its model at its position, tilted about x and turned about y, drawn without fog. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "actor.h"

/* was func_8C04764E: the draw: fog off, the model at the position turned by tilt (x) and angle (y), then the fog as
 * the current view wants it */
void spinner_draw(gaddr obj) {
  const Spinner *s = (const Spinner *)hw_ptr(obj);
  njFogDisable();
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Spinner, position));
  njRotateX(0, f32_to_int(s->tilt));
  njRotateY(0, f32_to_int(s->angle));
  nj_ModelL1(s->model);
  njPopMatrix(1);
  if (view_at(g_cur_view)->fog != 0)
    njFogEnable();
  else
    njFogDisable();
}
HW_EXPORT(8c04764e, spinner_draw(ARG0))
