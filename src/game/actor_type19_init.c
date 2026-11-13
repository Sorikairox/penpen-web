/* Pen Pen port, S19 (u2): the init method of the type-0x19 actor (ActorType19 in game/actor.h; vtable 0x8C23E15C,
 * slot 1): its state (the placement's param bits 0-3) picks how it starts. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "actor.h"
#include "scenery.h"

#define g_type19_trigger0     GUEST_VAR(f32, 0x8C08836Cu) /* state 0's trigger cell (a float) */
#define g_type19_damping      GUEST_VAR(f32, 0x8C088330u) /* 0.6: the body's damping (state 0) */
#define g_type19_center_x     GUEST_VAR(f32, 0x8C088344u) /* state 1's circle centre */
#define g_type19_center_y     GUEST_VAR(f32, 0x8C088348u)
#define g_type19_center_z     GUEST_VAR(f32, 0x8C08834Cu)
#define g_type19_circle_speed GUEST_VAR(f32, 0x8C088368u) /* 82.3: also the spread of state 1's radius */
#define g_type19_trigger2     GUEST_VAR(f32, 0x8C088370u) /* state 2's trigger cell (a float) */
#define g_type19_start2_x     GUEST_VAR(f32, 0x8C088354u) /* state 2's start point */
#define g_type19_start2_y     GUEST_VAR(f32, 0x8C088358u)
#define g_type19_start2_z     GUEST_VAR(f32, 0x8C08835Cu)
#define g_type19_turn_damping GUEST_VAR(f32, 0x8C088328u) /* the three turns' damping */
#define g_type19_radius       GUEST_VAR(f32, 0x8C088318u) /* 1.9999999: the contacts' radius */
#define TYPE19_RISE_SPREAD    F32(40.0f)                  /* state 1: the centre 30 to 70 higher */
#define TYPE19_RISE_MIN       F32(30.0f)
#define TYPE19_RADIUS_MIN     F32(20.0f)
#define TYPE19_TURN           F32(65536.0f)               /* a whole turn, Ninja units */
#define TYPE19_HOP_SPREAD     F32(5.0f)
#define TYPE19_DELAY_SPREAD   F32(180.0f)
#define TYPE19_DAMPING_1      F32_BITS(0x3F666666u)       /* 0.9 */
#define TYPE19_DAMPING_23     F32_BITS(0x3F199999u)       /* 0.59999996 */
#define TYPE19_CELL_2         0x7F                        /* state 2's cell hint */
#define TYPE19_MASS           F32_BITS(0x3E4CCCCCu)       /* 0.2 */
#define TYPE19_CONTACTS_1C    3

/* the init's frame on the guest stack (the original's: 0x64 bytes under its saved PR and r8): Ninja works on the
 * matrix and the two vectors by address (state 1) */
typedef struct Type19InitFrame {
  union {
    Vec3 point;      /* 0x00: state 1: the body's place on the circle, out of njCalcPoint */
    gaddr models;    /* 0x00: the model set's data (a spill) */
  };
  Vec3 offset;       /* 0x0C: state 1: (0, 0, the radius) */
  union {
    Matrix m;        /* 0x18 */
    struct {
      uint8_t pad_18[0x54 - 0x18];
      f32 rise;      /* 0x54: state 1: a spill, before the matrix is set up */
    };
  };
  uint8_t pad_58[0x5C - 0x58];
  gaddr placement;   /* 0x5C */
  gaddr self;        /* 0x60 */
  uint32_t saved[2]; /* 0x64: the original's PR and r8 slots */
} Type19InitFrame;
HW_OFFSET(Type19InitFrame, offset, 0x0C);
HW_OFFSET(Type19InitFrame, m, 0x18);
HW_OFFSET(Type19InitFrame, rise, 0x54);
HW_OFFSET(Type19InitFrame, placement, 0x5C);
HW_OFFSET(Type19InitFrame, self, 0x60);
HW_SIZE(Type19InitFrame, 0x6C);

/* the data of a container entry the group handed the actor (its offset is an address once the file is loaded) */
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }

/* a turn at rest (quat_identity's code), with the turns' damping and its value */
static void turn_reset(Quat *t, f32 value) {
  Body1 *b = (Body1 *)t;
  quat_identity(hw_addr(t));
  b->damping = g_type19_turn_damping;
  b->value = value;
}

/* state 0: at the placement, its trigger cell from the unit's float */
static void type19_start0(gaddr obj, const ActorPlacement *p) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType19, body);
  gaddr home = member_addr(obj, ActorType19, points[0]);
  a->slow = 1;
  a->trigger_cell = f32_to_int(g_type19_trigger0);
  vec3_set(p->position.x, p->position.y, p->position.z, home);
  body_reset(body);
  vec3_copy_bytes(body, home);
  vec3_fill(member_addr(body, Body, damping), g_type19_damping);
  a->points[1] = a->points[0];
  a->roll_target = p->angle.z;
}

/* state 1: circling the unit's centre 30 to 70 higher, at a radius of 20 to 102, from a random angle, 0 to 5 up; its
 * cell from the body's place; a random roll; a delay of 0 to 179 */
static void type19_start1(gaddr obj, gaddr fp) {
  Type19InitFrame *f = (Type19InitFrame *)hw_ptr(fp);
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType19, body);
  a->slow = 0;
  f32 rise = fadd(random_scaled(TYPE19_RISE_SPREAD), TYPE19_RISE_MIN);
  f->rise = rise;
  vec3_set(g_type19_center_x, fadd(g_type19_center_y, rise), g_type19_center_z,
           member_addr(obj, ActorType19, points[0]));
  body_reset(body);
  vec3_fill(member_addr(body, Body, damping), TYPE19_DAMPING_1);
  a->radius = fadd(random_scaled(g_type19_circle_speed), TYPE19_RADIUS_MIN);
  a->circle_angle = random_scaled(TYPE19_TURN);

  gaddr m = member_addr(fp, Type19InitFrame, m);
  gaddr offset = member_addr(fp, Type19InitFrame, offset);
  gaddr point = member_addr(fp, Type19InitFrame, point);
  matrix_identity(m);
  njUnitMatrix(m);
  method_self(offset); /* the two vectors' empty constructors */
  method_self(point);
  vec3_set(F32(0.0f), F32(0.0f), a->radius, offset);
  njTranslateV(m, member_addr(obj, ActorType19, points[0]));
  njRotateY(m, f32_to_int(a->circle_angle));
  njCalcPoint(m, offset, point);
  vec3_copy_bytes(body, point);
  f32_add_to(random_scaled(TYPE19_HOP_SPREAD), member_addr(body, Body, position.y));
  int32_t hint = a->base.cell;
  actor_set_cell(obj, method_self(body), hint);
  ((Body1 *)&a->turns[1])->value = random_scaled(TYPE19_TURN); /* turns[1]'s value (set again below) */
  a->delay = f32_to_int(random_scaled(TYPE19_DELAY_SPREAD));
}

/* state 2: at the unit's start point, its trigger cell from the unit's float, its cell searched from 127; no
 * direction yet */
static void type19_start2(gaddr obj) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType19, body);
  gaddr home = member_addr(obj, ActorType19, points[0]);
  a->slow = 0;
  a->trigger_cell = f32_to_int(g_type19_trigger2);
  vec3_set(g_type19_start2_x, g_type19_start2_y, g_type19_start2_z, home);
  body_reset(body);
  vec3_copy_bytes(body, home);
  vec3_fill(member_addr(body, Body, damping), TYPE19_DAMPING_23);
  a->base.cell = TYPE19_CELL_2;
  int32_t hint = a->base.cell;
  actor_set_cell(obj, method_self(body), hint);
  vec3_zero(member_addr(obj, ActorType19, points[1]));
}

/* state 3: at the placement */
static void type19_start3(gaddr obj, const ActorPlacement *p) {
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  gaddr body = member_addr(obj, ActorType19, body);
  gaddr home = member_addr(obj, ActorType19, points[0]);
  a->slow = 1;
  vec3_set(p->position.x, p->position.y, p->position.z, home);
  body_reset(body);
  vec3_copy_bytes(body, home);
  vec3_fill(member_addr(body, Body, damping), TYPE19_DAMPING_23);
}

/* was func_8C0505A4: the init: the state from the param and its start; then model 0, the texlist and the motion
 * table from the group file's entries; the motion's last frame, the clock a random time below it; the phase and the
 * watch reset; the three turns at the placement's angles (the unit's damping); the contacts (radius 2, mass 0.2,
 * kind 3) following the body's velocity on the actor's cell */
void actor_type19_init(gaddr obj, gaddr placement) {
  gaddr fp = hw_stack_alloc(sizeof(Type19InitFrame));
  Type19InitFrame *f = (Type19InitFrame *)hw_ptr(fp);
  ActorType19 *a = (ActorType19 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  f->self = obj;
  f->placement = placement;
  a->state = (int32_t)(p->param & 0xFu);
  switch (a->state) {
  case 0: type19_start0(obj, p); break;
  case 1: type19_start1(obj, fp); break;
  case 2: type19_start2(obj); break;
  case 3: type19_start3(obj, p); break;
  default: break;
  }
  a->kind = 0;
  a->first_player = 0;

  /* the model, its texlist and motions (the entry `slow` always has a motion) */
  f->models = entry_data(a->base.model_set);
  a->object = ((const gaddr *)hw_ptr(f->models))[a->kind];
  a->texlist = entry_data(a->base.texlist);
  a->motions = entry_data(a->base.word1c);
  const MotionEntry *e = (const MotionEntry *)hw_ptr(a->motions) + a->slow;
  a->last_frame = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(e->motion))->nb_frames - 1));
  a->clock = random_timer_scaled(a->last_frame);
  a->phase = 0;
  a->waiting = 0;

  turn_reset(&a->turns[0], p->angle.x);
  turn_reset(&a->turns[1], p->angle.y);
  turn_reset(&a->turns[2], p->angle.z);

  /* the contacts */
  Contacts *c = &a->contacts;
  gaddr body = member_addr(obj, ActorType19, body);
  field_reset_2c(member_addr(obj, ActorType19, contacts));
  c->owner_velocity = member_addr(body, Body, velocity);
  c->unknown18 = F32(1.0f);
  c->radius = g_type19_radius;
  c->mass = TYPE19_MASS;
  c->unknown1c = TYPE19_CONTACTS_1C;
  c->cell = a->base.cell;
  hw_stack_free(sizeof(Type19InitFrame));
}
HW_EXPORT(8c0505a4, actor_type19_init(ARG0, ARG1))
