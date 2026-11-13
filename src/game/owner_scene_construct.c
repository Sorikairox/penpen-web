/* Pen Pen port, S18 (b03): the owner scene's constructor (game/owner_scene.h): the RaceSetup's third scene. */
#include "hw.h"
#include "owner_scene.h"
#include "../core/task.h"

#define OWNER_SCENE_VTABLE 0x8C23DA8Cu


/* was func_8C02FC12: the OwnerScene (argument: the RaceSetup, handed on in r5): its vtable, then its construction;
 * returns the scene */
gaddr owner_scene_construct(gaddr scene, gaddr setup) {
  HW_RESUMABLE(0x8C02FC12u, scene, setup);
  HW_RESUME_AT(0);
  task_at(scene)->vtable = OWNER_SCENE_VTABLE;
  HW_YIELDING_RET(0, owner_scene_init(scene, setup), 0);
  return scene;
}
HW_EXPORT_RESUMABLE(8c02fc12, RET(owner_scene_construct(ARG0, ARG1)))
