/* Pen Pen port, S19 (u2): the init method of the type-0x2B actor (ActorType2B in game/actor.h; vtable 0x8C23DED4,
 * slot 1): a body that reaches out ahead of its place and back. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "actor.h"
#include "scenery.h"

#define g_type2b_reach_still GUEST_VAR(int32_t, 0x8C08819Cu) /* reach_max of a still one (active 0) */
#define g_type2b_scale_damping GUEST_VAR(f32, 0x8C088194u)   /* the three scale Body1s' damping */
#define g_type2b_radius GUEST_VAR(f32, 0x8C08818Cu)          /* 5.0: the contacts' radius */
#define TYPE2B_RISE F32(5.0f)                    /* the body starts 5 above the placement */
#define TYPE2B_BODY_DAMPING F32_BITS(0x3F666666u) /* 0.9 */
#define TYPE2B_TURN_DAMPING F32_BITS(0x3F733333u) /* 0.95: the x and z angles */
#define TYPE2B_YAW_DAMPING F32_BITS(0x3F4CCCCCu)  /* 0.8: the y angle */
#define TYPE2B_HALF_TURN F32(32768.0f)            /* Ninja units */
#define TYPE2B_TIMER_SPREAD F32(300.0f)
#define TYPE2B_TIMER_MIN 300
#define TYPE2B_MASS F32(0.5f)
#define TYPE2B_CONTACTS_1C 3
#define TYPE2B_RESPOND 0x8C04AA28u                /* contact_respond_slide (game/actor_misc_b04.c) */

/* the init's frame on the guest stack (the original's: 0x58 bytes under its saved PR and r8): Ninja works on the
 * matrix and the reach vector by address */
typedef struct Type2BInitFrame {
  gaddr models;      /* 0x00: the model set's data (a spill) */
  Matrix m;          /* 0x04 */
  Vec3 ahead;        /* 0x44: how far ahead it reaches, in its turned frame */
  gaddr placement;   /* 0x50 */
  gaddr self;        /* 0x54 */
  uint32_t saved[2]; /* 0x58: the original's PR and r8 slots */
} Type2BInitFrame;
HW_OFFSET(Type2BInitFrame, m, 0x04);
HW_OFFSET(Type2BInitFrame, ahead, 0x44);
HW_OFFSET(Type2BInitFrame, self, 0x54);
HW_SIZE(Type2BInitFrame, 0x60);

/* the data of a container entry the group handed the actor (its offset is an address once the file is loaded) */
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }

/* a Body1 at rest (quat_identity's code), then its damping and value */
static void value_reset(Body1 *b, f32 damping, f32 value, int value_first) {
  quat_identity(hw_addr(b));
  if (value_first) {
    b->value = value;
    b->damping = damping;
  } else {
    b->damping = damping;
    b->value = value;
  }
}

/* was func_8C049D2C: the init: kind (param bits 8-11), active (bits 0-3), the first motion; the body at rest 5 above
 * the placement (damping 0.9); the angles x, y, z as the turn values (damping 0.95, 0.8, 0.95), the y angle as the
 * aim of both sides (the second half a turn on); points_a: the body's place and the reach (0, 0, 100 / 130 / 145 by
 * active; none when still) turned z, y, x by the angles from it; reach_max by active (240 / 270 / 300); the scale at
 * 1 (the unit's damping); the model, its texlist and motions; the motion's last frame when the entry has one, the
 * frame a random whole frame below it; the cycle at its start; points_b: the step out (the reach / reach_max) and
 * back; the timer 300 to 600 at random; the contacts (radius 5, mass 0.5, kind 3, slide response) on the actor's
 * cell */
void actor_type2b_init(gaddr obj, gaddr placement) {
  gaddr fp = hw_stack_alloc(sizeof(Type2BInitFrame));
  Type2BInitFrame *f = (Type2BInitFrame *)hw_ptr(fp);
  ActorType2B *a = (ActorType2B *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  f->self = obj;
  f->placement = placement;
  a->kind = (int32_t)(p->param & 0xF00u) / 256;
  a->active = (int32_t)(p->param & 0xFu);
  a->motion_index = 0;
  gaddr body = member_addr(obj, ActorType2B, body);
  body_reset(body);
  vec3_set(p->position.x, fadd(p->position.y, TYPE2B_RISE), p->position.z, body);
  vec3_fill(member_addr(body, Body, damping), TYPE2B_BODY_DAMPING);

  /* the angles */
  value_reset(&a->values[3], TYPE2B_TURN_DAMPING, p->angle.x, 1);
  value_reset(&a->values[4], TYPE2B_YAW_DAMPING, p->angle.y, 1);
  value_reset(&a->values[5], TYPE2B_TURN_DAMPING, p->angle.z, 1);
  a->angle = a->values[4].value;
  a->angle_a = a->angle;
  a->angle_b = fadd(a->angle_a, TYPE2B_HALF_TURN);

  /* where it reaches to */
  a->points_a[0] = a->body.position;
  gaddr ahead = member_addr(fp, Type2BInitFrame, ahead);
  method_self(ahead); /* the vector's empty constructor */
  switch (a->active) {
  case 0:
    vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), ahead);
    a->reach_max = g_type2b_reach_still;
    break;
  case 1:
    vec3_set(F32(0.0f), F32(0.0f), F32(100.0f), ahead);
    a->reach_max = 240;
    break;
  case 2:
    vec3_set(F32(0.0f), F32(0.0f), F32(130.0f), ahead);
    a->reach_max = 270;
    break;
  case 3:
    vec3_set(F32(0.0f), F32(0.0f), F32(145.0f), ahead);
    a->reach_max = 300;
    break;
  default: break;
  }
  gaddr m = member_addr(fp, Type2BInitFrame, m);
  matrix_identity(m);
  njUnitMatrix(m);
  njTranslateV(m, method_self(body));
  njRotateZ(m, f32_to_int(a->values[5].value));
  njRotateY(m, f32_to_int(a->values[4].value));
  njRotateX(m, f32_to_int(a->values[3].value));
  njCalcPoint(m, ahead, member_addr(obj, ActorType2B, points_a[1]));

  /* the scale */
  value_reset(&a->values[0], g_type2b_scale_damping, F32(1.0f), 0);
  value_reset(&a->values[1], g_type2b_scale_damping, F32(1.0f), 0);
  value_reset(&a->values[2], g_type2b_scale_damping, F32(1.0f), 0);

  /* the model, its texlist and motions */
  f->models = entry_data(a->base.model_set);
  a->object = ((const gaddr *)hw_ptr(f->models))[a->kind];
  a->texlist = entry_data(a->base.texlist);
  a->motions = entry_data(a->base.word1c);
  if (((const Type2BMotion *)hw_ptr(a->motions))[a->motion_index].motion != 0) {
    const Type2BMotion *e = (const Type2BMotion *)hw_ptr(a->motions) + a->motion_index;
    a->frame_end = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
  }
  a->frame = f32_from_int(f32_to_int(random_timer_scaled(a->frame_end)));
  a->state = 0;
  a->word3c = 0;
  a->blending = 0;
  a->side = 0;
  a->reach = 0;

  /* the steps out and back */
  gaddr out = member_addr(obj, ActorType2B, points_b[0]);
  a->points_b[0] = a->points_a[1];
  vec3_sub(out, member_addr(obj, ActorType2B, points_a[0]));
  vec3_scale(f32_reciprocal(f32_from_int(a->reach_max)), out);
  a->points_b[1] = a->points_b[0];
  vec3_scale(fneg(F32(1.0f)), member_addr(obj, ActorType2B, points_b[1]));
  a->timer = f32_to_int(random_scaled(TYPE2B_TIMER_SPREAD)) + TYPE2B_TIMER_MIN;

  /* the contacts */
  Contacts *c = &a->contacts;
  field_reset_2c(member_addr(obj, ActorType2B, contacts));
  c->unknown18 = F32(1.0f);
  c->radius = g_type2b_radius;
  c->mass = TYPE2B_MASS;
  c->unknown1c = TYPE2B_CONTACTS_1C;
  c->cell = a->base.cell;
  c->respond = TYPE2B_RESPOND;
  hw_stack_free(sizeof(Type2BInitFrame));
}
HW_EXPORT(8c049d2c, actor_type2b_init(ARG0, ARG1))
