/* Pen Pen port, S19 (u): the init method of the type-0x25 actor (ActorType25, game/actor.h; vtable 0x8C23E51C,
 * slot 1): a model of the group file's model set with one of its motions, started at a random frame. */
#include "hw.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "actor.h"
#include "scenery.h"

#define TYPE25_DAMPING F32_BITS(0x3F4CCCCCu) /* 0.79999995 */
#define TYPE25_SCALE   F32(1.5f)

/* the data of a container entry the group handed the actor (its offset is an address once the file is loaded) */
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }

/* was func_8C05964A: the init: kind (param bits 8-11) and motion entry (bits 0-3), the body at the placement
 * (damping 0.8, at rest), the kind's model, the texlist and the motion table from the group file's entries; the
 * motion's last frame when the entry has one; the clock at a random whole frame below it; the angle; scale 1.5 */
void actor_type25_init(gaddr obj, gaddr placement) {
  ActorType25 *a = (ActorType25 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->kind = (int32_t)(p->param & 0xF00u) / 256;
  int32_t motion = (int32_t)(p->param & 0xFu);
  a->first_motion = motion;
  a->motion = motion;
  a->body.position.x = p->position.x;
  a->body.position.y = p->position.y;
  a->body.position.z = p->position.z;
  vec3_fill(member_addr(obj, ActorType25, body.damping), TYPE25_DAMPING);
  a->body.velocity.x = F32(0.0f);
  a->body.velocity.y = F32(0.0f);
  a->body.velocity.z = F32(0.0f);
  a->object = ((const gaddr *)hw_ptr(entry_data(a->base.model_set)))[a->kind];
  a->textures = entry_data(a->base.texlist);
  a->motions = entry_data(a->base.word1c);
  const MotionEntry *e = (const MotionEntry *)hw_ptr(a->motions) + a->motion;
  if (e->motion != 0)
    a->last_frame = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
  a->clock = f32_from_int(f32_to_int(random_timer_scaled(a->last_frame)));
  a->angle.x = p->angle.x;
  a->angle.y = p->angle.y;
  a->angle.z = p->angle.z;
  a->scale = TYPE25_SCALE;
}
HW_EXPORT(8c05964a, actor_type25_init(ARG0, ARG1))
