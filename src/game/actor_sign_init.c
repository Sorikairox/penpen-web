/* Pen Pen port, S19 (u2): the init method of the sign (type 0x22, Sign in game/actor.h; vtable 0x8C23E45C, slot 1). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

#define SIGN_TILT_20 F32_BITS(0x45638E38u)       /* 3640.889: mode 0's swing, 20 degrees (Ninja units) */
#define SIGN_RISE_0 F32_BITS(0x407FFFFFu)        /* 3.9999998: mode 0's contacts sit this much higher */
#define SIGN_SPIN_1 F32_BITS(0x43360B60u)        /* 182.04445: mode 1's swing speed, 1 degree per step */
#define SIGN_DAMPING_2 F32_BITS(0x3F333333u)     /* 0.7 (modes 2 and 5) */
#define SIGN_DAMPING_3 F32_BITS(0x3F199999u)     /* 0.59999996 */
#define SIGN_MOTES_SIDE F32(-40.0f)              /* mode 5: the motes start 40 to the side, turned by the angle */
#define SIGN_MOTES_TOP F32(-550.0f)
#define SIGN_BOARD_X F32(32.0f)                  /* mode 6: the board starts at (32, 16) */
#define SIGN_BOARD_Y F32(16.0f)
#define SIGN_BOARD_DAMPING F32_BITS(0x3F733333u) /* 0.95 */
#define SIGN_MOTES_MODEL 4                       /* mode 5: the model whose child loses evalflags bit 1 */
#define NJ_EVAL_UNIT_ANG 2u                      /* evalflags bit 1 (NJD_EVAL_UNIT_ANG) */

/* the init's frame on the guest stack (the original's: 0x6C bytes under its saved PR and r8): Ninja works on the
 * matrix and the two vectors by address (mode 5) */
typedef struct SignInitFrame {
  Vec3 point;        /* 0x00: mode 5: the motes' origin, out of njCalcPoint */
  Vec3 offset;       /* 0x0C: mode 5: (-40, 0, 0) */
  union {
    Matrix m;        /* 0x18 */
    struct {
      uint8_t pad_18[0x54 - 0x18];
      gaddr model;   /* 0x54: mode 5: a spill, after the matrix is done */
    };
  };
  f32 velocity;      /* 0x58: the swing's start: its velocity, damping and value */
  f32 damping;       /* 0x5C */
  f32 value;         /* 0x60 */
  gaddr placement;   /* 0x64 */
  gaddr self;        /* 0x68 */
  uint32_t saved[2]; /* 0x6C: the original's PR and r8 slots */
} SignInitFrame;
HW_OFFSET(SignInitFrame, offset, 0x0C);
HW_OFFSET(SignInitFrame, m, 0x18);
HW_OFFSET(SignInitFrame, model, 0x54);
HW_OFFSET(SignInitFrame, velocity, 0x58);
HW_OFFSET(SignInitFrame, value, 0x60);
HW_OFFSET(SignInitFrame, self, 0x68);
HW_SIZE(SignInitFrame, 0x74);

/* mode 5: the motes at 40 to the side of the body (turned by the angle about y), up to -550 (kind 0); the motes
 * model's child loses evalflags bit 1 */
static void sign_motes_init(gaddr obj, gaddr fp) {
  SignInitFrame *f = (SignInitFrame *)hw_ptr(fp);
  Sign *s = (Sign *)hw_ptr(obj);
  gaddr m = member_addr(fp, SignInitFrame, m);
  gaddr offset = member_addr(fp, SignInitFrame, offset);
  gaddr point = member_addr(fp, SignInitFrame, point);
  matrix_identity(m);
  njUnitMatrix(m);
  njRotateY(m, f32_to_int(s->angle.y));
  vec3_set(SIGN_MOTES_SIDE, F32(0.0f), F32(0.0f), offset);
  method_self(point); /* the vector's empty constructor */
  njCalcPoint(m, offset, point);
  vec3_add(point, method_self(member_addr(obj, Sign, body)));
  motes_setup(member_addr(obj, Sign, motes), point, 0, SIGN_MOTES_TOP);
  f->model = actor_model(&s->base, SIGN_MOTES_MODEL);
  NjObject *child = (NjObject *)hw_ptr(((const NjObject *)hw_ptr(f->model))->child);
  child->evalflags &= ~NJ_EVAL_UNIT_ANG;
}

/* was func_8C057810: the init: param, mode (bits 0-3), state (bits 4-7), timer 0; the body at rest at the placement
 * (damping 1), the angle, the contacts' owner at the body; by mode: 0 swings 20 degrees and sits 4 higher, 1 spins
 * (a degree per step), 2 and 3 damp the swing (0.7 / 0.6), 4 waits 20 steps per state, 5 waits 100 per state (damping
 * 0.7) and sets its motes up, 6 is the board (flags 1; at (32, 16), damping 0.95), hidden (state 2) unless the race
 * setup's mode or variant is set (state 3); then the swing and the contacts (mass 1) following the body's velocity
 * on the actor's cell */
void sign_init(gaddr obj, gaddr placement) {
  gaddr fp = hw_stack_alloc(sizeof(SignInitFrame));
  SignInitFrame *f = (SignInitFrame *)hw_ptr(fp);
  Sign *s = (Sign *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  f->self = obj;
  f->placement = placement;
  s->param = p->param;
  s->mode = (int32_t)(s->param & 0xFu);
  s->state = (int32_t)(s->param & 0xF0u) >> 4;
  s->timer = 0;
  gaddr body = member_addr(obj, Sign, body);
  body_reset(body);
  vec3_set(p->position.x, p->position.y, p->position.z, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_fill(member_addr(body, Body, damping), F32(1.0f));
  s->angle.x = p->angle.x;
  s->angle.y = p->angle.y;
  s->angle.z = p->angle.z;
  s->position = *(const Vec3 *)hw_ptr(method_self(body));

  f->value = F32(0.0f);
  f->damping = F32(1.0f);
  f->velocity = F32(0.0f);
  switch (s->mode) {
  case 0:
    f->value = SIGN_TILT_20;
    s->position.y = fadd(s->position.y, SIGN_RISE_0);
    break;
  case 1: f->velocity = SIGN_SPIN_1; break;
  case 2: f->damping = SIGN_DAMPING_2; break;
  case 3: f->damping = SIGN_DAMPING_3; break;
  case 4: s->timer = s->state * 20; break;
  case 5:
    s->timer = s->state * 100;
    f->damping = SIGN_DAMPING_2;
    sign_motes_init(obj, fp);
    break;
  case 6:
    s->base.flags = 1;
    vec3_set(SIGN_BOARD_X, SIGN_BOARD_Y, F32(0.0f), body);
    vec3_fill(member_addr(body, Body, damping), SIGN_BOARD_DAMPING);
    if (race_setup_at(race_setup_get())->mode != 0 || race_setup_at(race_setup_get())->variant != 0)
      s->state = 3;
    else
      s->state = 2;
    break;
  default: break;
  }

  /* the swing */
  gaddr swing = member_addr(obj, Sign, swing);
  quat_identity(swing);
  s->swing.value = f->value;
  s->swing.velocity = f->velocity;
  s->swing.damping = f->damping;

  /* the contacts */
  Contacts *c = &s->contacts;
  field_reset_2c(member_addr(obj, Sign, contacts));
  c->owner_velocity = member_addr(body, Body, velocity);
  c->mass = F32(1.0f);
  c->unknown18 = F32(1.0f);
  c->cell = s->base.cell;
  hw_stack_free(sizeof(SignInitFrame));
}
HW_EXPORT(8c057810, sign_init(ARG0, ARG1))
