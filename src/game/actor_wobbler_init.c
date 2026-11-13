/* Pen Pen port, S19 (u2): the init method of the wobbler (type 0x33, Wobbler in game/actor.h; vtable 0x8C23E11C,
 * slot 1). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "actor.h"
#include "scenery.h"

#define g_wobble_damping GUEST_VAR(f32, 0x8C088314u) /* the three wobbles' damping */
#define g_wobble_radius  GUEST_VAR(f32, 0x8C08830Cu) /* 5.9999995: the contacts' radius (a small one: half) */
#define WOBBLER_BODY_DAMPING F32_BITS(0x3F4CCCCCu)   /* 0.8 */
#define WOBBLER_REST_BIG F32_BITS(0x3FA66666u)       /* 1.3 */
#define WOBBLER_REST_SMALL F32_BITS(0x3F266666u)     /* 0.65 */
#define WOBBLER_HALF F32(0.5f)
#define WOBBLER_CONTACTS_1C 3

/* the init's frame on the guest stack (the original's: 0x64 bytes under its saved PR and r8): Ninja works on the
 * matrix and the two vectors by address */
typedef struct WobblerInitFrame {
  Matrix m;          /* 0x00 */
  Vec3 point;        /* 0x40: the contacts' owner's place, out of njCalcPoint */
  Vec3 offset;       /* 0x4C: (0, the radius, 0) */
  gaddr models;      /* 0x58: the model set's data (a spill) */
  gaddr placement;   /* 0x5C */
  gaddr self;        /* 0x60 */
  uint32_t saved[2]; /* 0x64: the original's PR and r8 slots */
} WobblerInitFrame;
HW_OFFSET(WobblerInitFrame, point, 0x40);
HW_OFFSET(WobblerInitFrame, offset, 0x4C);
HW_OFFSET(WobblerInitFrame, models, 0x58);
HW_OFFSET(WobblerInitFrame, self, 0x60);
HW_SIZE(WobblerInitFrame, 0x6C);

/* the data of a container entry the group handed the actor (its offset is an address once the file is loaded) */
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }

/* a wobble at rest at `rest`, with the wobbles' damping */
static void wobble_reset(gaddr wobble, f32 rest) {
  Body1 *b = (Body1 *)hw_ptr(wobble);
  quat_identity(wobble);
  b->damping = g_wobble_damping;
  b->value = rest;
}

/* was func_8C04FB4E: the init: kind (param bits 8-11), small (bits 4-7), motion entry (bits 0-3); the body at rest
 * at the placement (damping 0.8); the kind's model, the texlist and the motion table from the group file's entries;
 * the motion's last frame when the entry has one, the clock a random whole frame below it; the angle; the rest value
 * (1.3, a small one 0.65) and the three wobbles at it; the contacts' owner (the position) at the radius (half for a
 * small one) above the body in the frame turned z, y, x by the angle; the contacts (mass 1, kind 3) on the actor's
 * cell */
void wobbler_init(gaddr obj, gaddr placement) {
  gaddr fp = hw_stack_alloc(sizeof(WobblerInitFrame));
  WobblerInitFrame *f = (WobblerInitFrame *)hw_ptr(fp);
  Wobbler *w = (Wobbler *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  f->self = obj;
  f->placement = placement;
  w->kind = (int32_t)(p->param & 0xF00u) / 256;
  w->small = (int32_t)(p->param & 0xF0u) / 16;
  w->motion = (int32_t)(p->param & 0xFu);
  gaddr body = member_addr(obj, Wobbler, body);
  body_reset(body);
  vec3_set(p->position.x, p->position.y, p->position.z, body);
  vec3_fill(member_addr(body, Body, damping), WOBBLER_BODY_DAMPING);

  /* the model, its texlist and motions */
  f->models = entry_data(w->base.model_set);
  w->object = ((const gaddr *)hw_ptr(f->models))[w->kind];
  w->texlist = entry_data(w->base.texlist);
  w->motions = entry_data(w->base.word1c);
  if (((const MotionEntry *)hw_ptr(w->motions))[w->motion].motion != 0) {
    const MotionEntry *e = (const MotionEntry *)hw_ptr(w->motions) + w->motion;
    w->last_frame = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
  }
  w->clock = f32_from_int(f32_to_int(random_timer_scaled(w->last_frame)));
  w->angle.x = p->angle.x;
  w->angle.y = p->angle.y;
  w->angle.z = p->angle.z;

  /* the wobbles */
  w->rest = w->small == 0 ? WOBBLER_REST_BIG : WOBBLER_REST_SMALL;
  wobble_reset(member_addr(obj, Wobbler, wobble[0]), w->rest);
  wobble_reset(member_addr(obj, Wobbler, wobble[1]), w->rest);
  wobble_reset(member_addr(obj, Wobbler, wobble[2]), w->rest);

  /* the contacts' owner */
  gaddr offset = member_addr(fp, WobblerInitFrame, offset);
  gaddr point = member_addr(fp, WobblerInitFrame, point);
  gaddr m = member_addr(fp, WobblerInitFrame, m);
  method_self(offset); /* the two vectors' empty constructors */
  method_self(point);
  if (w->small != 0)
    vec3_set(F32(0.0f), fmul(g_wobble_radius, WOBBLER_HALF), F32(0.0f), offset);
  else
    vec3_set(F32(0.0f), g_wobble_radius, F32(0.0f), offset);
  matrix_identity(m);
  njUnitMatrix(m);
  njTranslateV(m, method_self(body));
  njRotateZ(m, f32_to_int(w->angle.z));
  njRotateY(m, f32_to_int(w->angle.y));
  njRotateX(m, f32_to_int(w->angle.x));
  njCalcPoint(m, offset, point);
  w->position = f->point;

  /* the contacts */
  Contacts *c = &w->contacts;
  field_reset_2c(member_addr(obj, Wobbler, contacts));
  c->unknown18 = F32(0.0f);
  if (w->small == 0)
    c->radius = g_wobble_radius;
  else
    c->radius = fmul(g_wobble_radius, WOBBLER_HALF);
  c->mass = F32(1.0f);
  c->unknown1c = WOBBLER_CONTACTS_1C;
  c->cell = w->base.cell;
  hw_stack_free(sizeof(WobblerInitFrame));
}
HW_EXPORT(8c04fb4e, wobbler_init(ARG0, ARG1))
