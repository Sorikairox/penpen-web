/* Pen Pen port, S19 (u): the init method of the type-0x26 actor (ActorType26, game/actor_b07.h; vtable 0x8C23E4DC,
 * slot 1). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "../core/contact.h"
#include "actor.h"
#include "actor_b07.h"
#include "scenery.h"

#define g_type26_radius GUEST_VAR(f32, 0x8C0885F8u) /* 5: type 0x26's contacts' radius */
#define TYPE26_POINT_RISE F32(5.0f)                  /* the contacts' point: 5 up in the actor's turned frame */
#define TYPE26_CONTACTS_1C 3
#define CONTACTS_WORD_INIT 4

/* the init's frame on the guest stack (the original's: 0x64 bytes under its saved PR and r8): Ninja works on the
 * matrix and the two vectors by address */
typedef struct Type26InitFrame {
  gaddr models;      /* 0x00: the model set's data (a spill) */
  Matrix m;          /* 0x04 */
  Vec3 point;        /* 0x44: the contacts' point, out of njCalcPoint */
  Vec3 offset;       /* 0x50: (0, 5, 0) */
  gaddr placement;   /* 0x5C */
  gaddr self;        /* 0x60 */
  uint32_t saved[2]; /* 0x64: the original's PR and r8 slots */
} Type26InitFrame;
HW_OFFSET(Type26InitFrame, m, 0x04);
HW_OFFSET(Type26InitFrame, point, 0x44);
HW_OFFSET(Type26InitFrame, offset, 0x50);
HW_OFFSET(Type26InitFrame, self, 0x60);
HW_SIZE(Type26InitFrame, 0x6C);

/* the data of a container entry the group handed the actor (its offset is an address once the file is loaded) */
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }

/* was func_8C058CDC: the init: kind (param bits 8-11), motion entry 0, word30 (bits 0-3); the body at rest at the
 * placement, the angle; the contacts' point 5 above the body in the frame turned z, y, x by the angle; the kind's
 * model, the texlist and the motion table from the group file's entries; the motion's last frame when the entry has
 * one, the frame a random whole frame below it; the contacts (radius 5, mass 1) following the body's velocity on the
 * actor's cell; then the reset */
void actor_type26_init(gaddr obj, gaddr placement) {
  gaddr fp = hw_stack_alloc(sizeof(Type26InitFrame));
  Type26InitFrame *f = (Type26InitFrame *)hw_ptr(fp);
  ActorType26 *a = (ActorType26 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  f->self = obj;
  f->placement = placement;
  a->kind = (int32_t)(p->param & 0xF00u) / 256;
  a->motion = 0;
  a->word30 = (int32_t)(p->param & 0xFu);
  gaddr body = member_addr(obj, ActorType26, body);
  body_reset(body);
  vec3_set(p->position.x, p->position.y, p->position.z, body);
  a->angle.x = p->angle.x;
  a->angle.y = p->angle.y;
  a->angle.z = p->angle.z;

  /* the contacts' point */
  gaddr m = member_addr(fp, Type26InitFrame, m);
  vec3_set(F32(0.0f), TYPE26_POINT_RISE, F32(0.0f), member_addr(fp, Type26InitFrame, offset));
  matrix_identity(m);
  njUnitMatrix(m);
  njTranslateV(m, body);
  njRotateZ(m, f32_to_int(a->angle.z));
  njRotateY(m, f32_to_int(a->angle.y));
  njRotateX(m, f32_to_int(a->angle.x));
  njCalcPoint(m, member_addr(fp, Type26InitFrame, offset), member_addr(fp, Type26InitFrame, point));
  a->point = f->point;

  /* the model, its texlist and motions */
  f->models = entry_data(a->base.model_set);
  a->object = ((const gaddr *)hw_ptr(f->models))[a->kind];
  a->word88 = entry_data(a->base.texlist);
  a->motions = entry_data(a->base.word1c);
  const MotionEntry *e = (const MotionEntry *)hw_ptr(a->motions) + a->motion;
  if (e->motion != 0)
    a->last_frame = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
  a->frame = f32_from_int(f32_to_int(random_timer_scaled(a->last_frame)));

  /* the contacts */
  Contacts *c = &a->contacts;
  c->count = 0;
  a->contacts_word = CONTACTS_WORD_INIT;
  c->owner_velocity = member_addr(body, Body, velocity);
  c->unknown18 = F32(0.0f);
  c->radius = g_type26_radius;
  c->mass = F32(1.0f);
  c->unknown1c = TYPE26_CONTACTS_1C;
  c->cell = a->base.cell;
  actor_type26_reset(obj);
  hw_stack_free(sizeof(Type26InitFrame));
}
HW_EXPORT(8c058cdc, actor_type26_init(ARG0, ARG1))
