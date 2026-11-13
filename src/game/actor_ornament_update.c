/* Pen Pen port, S19 (u2): the per-frame method of the ornament (type 0x14, Ornament in game/actor.h; vtable
 * 0x8C23E67C, slot 0x10): by its style the two parts swing, the body bobs round its place, or it cruises between
 * three fixed points. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

#define ORN_SWING_A_TARGET F32_BITS(0x45638E38u) /* 3640.889: 20 degrees */
#define ORN_SWING_A_RATE F32_BITS(0x3DA3D70Au)   /* 0.08 */
#define ORN_SWING_B_TARGET F32_BITS(0xC5638E38u) /* -20 degrees */
#define ORN_SWING_B_RATE F32_BITS(0x3DCCCCCCu)   /* 0.1 */
#define ORN_TURN_2 F32_BITS(0x43B60B60u)         /* 364.08887: 2 degrees */
#define ORN_TURN_1 F32_BITS(0x43360B60u)         /* 182.04443: 1 degree */
#define ORN_RADIUS_1 F32(20.0f)                  /* style 1's circle */
#define ORN_RADIUS_2 F32(25.0f)                  /* style 2's */
#define ORN_BOB_SPEED F32(4.0f)                  /* the bob runs 4 times the circle */
#define ORN_BOB_HEIGHT F32_BITS(0x407FFFFFu)     /* 3.9999998 */
#define ORN_LEG_STEPS 0x96                       /* style 3: steps toward each point */
#define ORN_LEG_LAST 2                           /* style 3: points 0 to 2 */
#define ORN_CRUISE_RATE F32_BITS(0x3BE56041u)    /* 0.007 */
#define ORN_SLOT 8                               /* the race scene's slot for the contacts */

/* the update's frame on the guest stack (the original's: 0x64 bytes under its saved PR and r8): Ninja works on the
 * matrix and the two vectors by address; style 3's target is a local handed to body_pull by address */
typedef struct OrnamentUpdateFrame {
  Vec3 point;          /* 0x00: styles 1, 2: the body's place on its circle, out of njCalcPoint */
  Vec3 offset;         /* 0x0C: styles 1, 2: (0, the bob, the radius) */
  f32 bob;             /* 0x18: a spill */
  union {
    struct {
      Matrix m;        /* 0x1C */
      union {
        f32 radius;    /* 0x5C: styles 1, 2 */
        gaddr part;    /* 0x5C: style 0: a spill of the model's parts */
      };
    };
    struct {
      uint8_t pad_1c[0x54 - 0x1C];
      Vec3 target;     /* 0x54: style 3: the point it cruises to (left as it was for a fourth point) */
    };
  };
  gaddr self;          /* 0x60 */
  uint32_t saved[2];   /* 0x64: the original's PR and r8 slots */
} OrnamentUpdateFrame;
HW_OFFSET(OrnamentUpdateFrame, offset, 0x0C);
HW_OFFSET(OrnamentUpdateFrame, bob, 0x18);
HW_OFFSET(OrnamentUpdateFrame, m, 0x1C);
HW_OFFSET(OrnamentUpdateFrame, target, 0x54);
HW_OFFSET(OrnamentUpdateFrame, radius, 0x5C);
HW_OFFSET(OrnamentUpdateFrame, self, 0x60);
HW_SIZE(OrnamentUpdateFrame, 0x6C);

/* func_8C013DCE (core/contact.c, not in a header): each contact's handler (self, index, a, b, c) */
static void contacts_handlers(gaddr contacts, uint32_t a, uint32_t b, uint32_t c3) {
  CALL4(0x8C013DCEu, contacts, a, b, c3);
}

/* style 0 (kinds 1, 2): both swings pulled toward +-20 degrees and stepped; the model's first part turns about y by
 * the first, the next by the second */
static void ornament_swing(gaddr obj, gaddr fp) {
  OrnamentUpdateFrame *f = (OrnamentUpdateFrame *)hw_ptr(fp);
  Ornament *o = (Ornament *)hw_ptr(obj);
  gaddr swing_a = member_addr(obj, Ornament, swing_a);
  gaddr swing_b = member_addr(obj, Ornament, swing_b);
  body1_pull_angle(ORN_SWING_A_TARGET, ORN_SWING_A_RATE, swing_a);
  body1_step(swing_a);
  body1_pull_angle(ORN_SWING_B_TARGET, ORN_SWING_B_RATE, swing_b);
  body1_step(swing_b);
  f->part = actor_model(&o->base, o->model);
  f->part = ((const NjObjectPose *)hw_ptr(f->part))->child;
  ((NjObjectPose *)hw_ptr(f->part))->ang[1] = f32_to_int(o->swing_a.value);
  f->part = ((const NjObjectPose *)hw_ptr(f->part))->sibling;
  ((NjObjectPose *)hw_ptr(f->part))->ang[1] = f32_to_int(o->swing_b.value);
}

/* styles 1, 2: the angle turns 2 degrees a step; the body circles the place (radius 20, 2 degrees a step; radius
 * 25, 1 degree) bobbing 4 up and down */
static void ornament_circle(gaddr obj, gaddr fp) {
  OrnamentUpdateFrame *f = (OrnamentUpdateFrame *)hw_ptr(fp);
  Ornament *o = (Ornament *)hw_ptr(obj);
  if (o->style == 1) {
    o->angle.y = fadd(o->angle.y, ORN_TURN_2);
    o->f88 = fadd(o->f88, ORN_TURN_2);
    f->radius = ORN_RADIUS_1;
  } else {
    o->angle.y = fadd(o->angle.y, ORN_TURN_2);
    o->f88 = fadd(o->f88, ORN_TURN_1);
    f->radius = ORN_RADIUS_2;
  }
  gaddr m = member_addr(fp, OrnamentUpdateFrame, m);
  gaddr offset = member_addr(fp, OrnamentUpdateFrame, offset);
  gaddr point = member_addr(fp, OrnamentUpdateFrame, point);
  matrix_identity(m);
  njUnitMatrix(m);
  njRotateY(m, f32_to_int(o->f88));
  f->bob = fmul(njSin(f32_to_int(fmul(o->f88, ORN_BOB_SPEED))), ORN_BOB_HEIGHT);
  vec3_set(F32(0.0f), f->bob, f->radius, offset);
  method_self(point); /* the vector's empty constructor */
  njCalcPoint(m, offset, point);
  vec3_add(point, member_addr(obj, Ornament, position));
  vec3_copy_bytes(member_addr(obj, Ornament, body), point);
}

/* style 3: pulled toward point word3c (of three) for 150 steps each, round and round; the body steps, its cell and
 * the contacts' cell follow it */
static void ornament_cruise(gaddr obj, gaddr fp) {
  Ornament *o = (Ornament *)hw_ptr(obj);
  gaddr body = member_addr(obj, Ornament, body);
  o->word38++;
  if (o->word38 < ORN_LEG_STEPS) {
    gaddr target = member_addr(fp, OrnamentUpdateFrame, target);
    method_self(target); /* the vector's empty constructor */
    switch (o->word3c) {
    case 0: vec3_set(F32_BITS(0x4534BFFFu), F32(-310.0f), F32_BITS(0xC62223FFu), target); break; /* 2892, -10377 */
    case 1: vec3_set(F32_BITS(0x452F1FFFu), F32(-325.0f), F32_BITS(0xC62133FFu), target); break; /* 2802, -10317 */
    case 2: vec3_set(F32_BITS(0x45306FFFu), F32(-305.0f), F32_BITS(0xC621FFFFu), target); break; /* 2823, -10368 */
    default: break;
    }
    body_pull(ORN_CRUISE_RATE, body, target);
  } else {
    o->word38 = 0;
    o->word3c++;
    if (o->word3c > ORN_LEG_LAST) o->word3c = 0;
  }
  body_step(body);
  int32_t hint = o->base.cell;
  actor_set_cell(obj, method_self(body), hint);
  o->contacts.cell = o->base.cell;
}

/* was func_8C05B4B4: the update: with a subkind its contacts' handlers run (with the body); then by style: 0 swings
 * the parts and, as 4 does, pulls the body home (by f8c) and steps it; 1, 2 circle; 3 cruises; with a subkind its
 * contacts go into the race scene's slot 8 */
void ornament_update(gaddr obj) {
  gaddr fp = hw_stack_alloc(sizeof(OrnamentUpdateFrame));
  OrnamentUpdateFrame *f = (OrnamentUpdateFrame *)hw_ptr(fp);
  Ornament *o = (Ornament *)hw_ptr(obj);
  f->self = obj;
  if (o->subkind != 0)
    contacts_handlers(member_addr(obj, Ornament, contacts), member_addr(obj, Ornament, body), 0, 0);
  switch (o->style) {
  case 0:
    ornament_swing(obj, fp);
    /* fall through: and home */
  case 4:
    body_pull(o->f8c, member_addr(obj, Ornament, body), member_addr(obj, Ornament, position));
    body_step(member_addr(obj, Ornament, body));
    break;
  case 1:
  case 2: ornament_circle(obj, fp); break;
  case 3: ornament_cruise(obj, fp); break;
  default: break;
  }
  if (o->subkind != 0)
    race_scene_slot_insert(panel_get(), member_addr(obj, Ornament, contacts), ORN_SLOT);
  hw_stack_free(sizeof(OrnamentUpdateFrame));
}
HW_EXPORT(8c05b4b4, ornament_update(ARG0))
