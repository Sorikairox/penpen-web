/* Pen Pen port, S20 (W3): the init method of the type-0x1B actor (ActorType1B, game/actor_b05.h; vtables 0x8C23DE54
 * and 0x8C23DE74, slot 1 of each: ActorVtbl.init). The same as type 0x1C's (game/actor_type1c_init.c): a model of the
 * group file's model set with one of its motions. Reached only through the vtables. */
#include "hw.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "actor.h"
#include "actor_b05.h"

/* the data of a container entry the group handed the actor (its offset is an address once the file is loaded) */
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }

/* was func_8C049308: the init (this: the Actor, as the first vtable's slot passes it; r5: its ActorPlacement): kind
 * (param bits 8-11) and motion entry (bits 0-3), the position (through a local, as the original's block copy
 * func_8C0625EC of 12 bytes), the kind's model of the model set, the texlist and the motion table from the group
 * file's entries; the motion's last frame when the entry has a motion; the frame 0, the angle */
void actor_type1b_init(gaddr obj, gaddr placement) {
  ActorType1B *a = (ActorType1B *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->kind = (int32_t)(p->param & 0xF00u) / 256;
  a->motion = (int32_t)(p->param & 0xFu);
  Vec3 position = p->position;
  a->position = position;
  a->object = ((const gaddr *)hw_ptr(entry_data(a->base.model_set)))[a->kind];
  a->texlist = entry_data(a->base.texlist);
  a->motions = entry_data(a->base.word1c);
  const MotionEntry *e = (const MotionEntry *)hw_ptr(a->motions) + a->motion;
  if (e->motion != 0)
    a->last_frame = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
  a->frame = F32(0.0f);
  a->angle.x = p->angle.x;
  a->angle.y = p->angle.y;
  a->angle.z = p->angle.z;
}
HW_EXPORT(8c049308, actor_type1b_init(ARG0, ARG1))
