/* Pen Pen port, S19 (u): the init method of the turning actor (Spinner, game/actor.h; placement type 0x0E, vtable
 * 0x8C23DD0C, slot 1). */
#include "hw.h"
#include "../core/vec.h"
#include "actor.h"

#define SPINNER_MODEL_FIRST 12 /* the param picks model 12 + param of the actor's list */

/* was func_8C0475F4: the init: the placement's position (copied through a local, as the original's block copy),
 * tilt and angle from its angle's x and y, the model by its param */
void spinner_init(gaddr obj, gaddr placement) {
  Spinner *s = (Spinner *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  Vec3 position = p->position;
  s->position = position;
  s->tilt = p->angle.x;
  s->angle = p->angle.y;
  s->model = actor_model(&s->base, (int32_t)p->param + SPINNER_MODEL_FIRST);
}
HW_EXPORT(8c0475f4, spinner_init(ARG0, ARG1))
