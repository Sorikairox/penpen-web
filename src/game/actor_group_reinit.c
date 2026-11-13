/* Pen Pen port, S17 b05: the actor group's re-initialisation (ActorGroup, game/actor.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "race_scene.h"
#include "actor.h"

/* was func_8C044766: every actor of the group gets its placement again (the course's list, by index): its flags
 * bit 1 set, then the group hands it its data again (actor_group_attach) */
void actor_group_reinit(gaddr group) {
  ActorGroup *g = (ActorGroup *)hw_ptr(group);
  gaddr scene = g->task.parent;
  gaddr p = course_actor_placements(race_scene_at(scene)->course);
  for (int32_t i = 0; ((const ActorPlacement *)hw_ptr(p))->type != -1; i++, p += sizeof(ActorPlacement)) {
    gaddr actor = g->actors[i];
    ((Actor *)hw_ptr(actor))->flags |= 2;
    actor_group_attach(group, actor, p, i);
  }
}
HW_EXPORT(8c044766, actor_group_reinit(ARG0))
