/* Pen Pen port, S16 (b06): the set-up and the update of the actor of placement type 1 (ActorType01, game/actor.h;
 * func_8C045086, func_8C04530A).
 *
 * The set-up places it from its placement (position, heading) and turns its four direction points and its wind
 * direction (`point`) by the heading with a Ninja matrix on the stack: dir[0] / dir[2] 30 to either side of it,
 * dir[1] / dir[3] its forward turned by 60 degrees either way; its contacts (kind word 0xCD, its own response).
 *
 * The actor is a wind maker: it waits until a player reaches course cell 0x39, then steps through a table of wind
 * levels (one per kind_a, a step every 121 updates; a level 0 stops it, -1 starts the table over). Its spin is
 * pulled toward an angle that grows with the level and turns it (word38); with kind_b 0 it blows: its push is its
 * point scaled by the level's strength, and its contacts reach 40 + 25 per level. State 3 (kind_b set) only turns. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"   /* method_self */
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_setup_get, race_scene_slot_insert */
#include "scenery.h"      /* race_player_cell */

#define g_wind_level_tables ((const gaddr *)hw_ptr(0x8C097370u)) /* by kind_a: int32_t levels, 0 / -1 ended */

#define WIND_START_CELL   0x39       /* a player on this cell or beyond starts the wind */
#define WIND_TIMER_START  10000      /* the first step ends at once */
#define WIND_STEP_FRAMES  120        /* a step ends once the timer is above this */
#define WIND_STRENGTH     F32_BITS(0x3C9F49F4u) /* 0.019444445: per level */
#define WIND_SPIN_TARGET  F32_BITS(0x441F49F4u) /* 637.1555: the spin's target per level (Ninja angle units) */
#define WIND_SPIN_RATE    F32_BITS(0x3B83126Eu) /* 0.004 */
#define WIND_TURN         F32_BITS(0x42B60B60u) /* 91.02222: half a degree per update in state 3 */
#define WIND_REACH        F32(40.0f)
#define WIND_REACH_LEVEL  F32(25.0f)
#define WIND_SLOT         8          /* the race scene's slot list the contacts go to */

enum { WIND_WAIT = 0, WIND_BLOW = 1, WIND_STOP = 2, WIND_SPIN = 3 };

#define TYPE01_RESPOND      0x8C0455B0u /* the class's contact response (func_8C0455B0) */
#define TYPE01_CONTACT_KIND 0xCD        /* Contacts.unknown1c: the kind other bodies see (no grip, game/racer.c) */
#define TYPE01_SPIN_DAMPING F32_BITS(0x3F666666u) /* 0.9 */
#define TYPE01_SIDE         F32(30.0f)  /* dir[0] / dir[2]: this far to either side */
#define TURN_60             F32_BITS(0x462AAAABu) /* 10922.667: 60 degrees (Ninja angle units) */
#define TURN_MINUS_120      F32_BITS(0xC6AAAAABu) /* -21845.334: -120 degrees */

/* the set-up's frame (0x80 bytes under the six saved registers): the Ninja matrix and the vectors it turns */
typedef struct Type01SetupFrame {
  uint8_t pad_00[0x04];  /* 0x00 */
  Vec3 turned;           /* 0x04: the forward turned by +-60 degrees */
  Vec3 side_at;          /* 0x10: a side point, turned, then moved to the position */
  Vec3 side;             /* 0x1C: (+-30, 0, 0) */
  Vec3 forward;          /* 0x28: (0, 0, 1) */
  Matrix m;              /* 0x34 */
  Vec3 position;         /* 0x74: the placement's position */
  uint8_t saved[0x98 - 0x80]; /* 0x80: r14, r13, fr15-fr13, PR */
} Type01SetupFrame;
HW_OFFSET(Type01SetupFrame, side_at, 0x10);
HW_OFFSET(Type01SetupFrame, forward, 0x28);
HW_OFFSET(Type01SetupFrame, m, 0x34);
HW_OFFSET(Type01SetupFrame, position, 0x74);
HW_SIZE(Type01SetupFrame, 0x98);

/* was func_8C045086: the set-up from the placement (see the top of the file) */
void actor_type01_setup(gaddr obj, gaddr placement) {
  ActorType01 *a = (ActorType01 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  gaddr frame = hw_stack_alloc(sizeof(Type01SetupFrame));
  Type01SetupFrame *f = (Type01SetupFrame *)hw_ptr(frame);
  gaddr m = member_addr(frame, Type01SetupFrame, m);
  gaddr forward = member_addr(frame, Type01SetupFrame, forward);
  gaddr side = member_addr(frame, Type01SetupFrame, side);
  gaddr side_at = member_addr(frame, Type01SetupFrame, side_at);
  gaddr turned = member_addr(frame, Type01SetupFrame, turned);

  a->kind_a = (int32_t)(p->param & 0xF);
  a->kind_b = (int32_t)(p->param & 0xF0) >> 4;
  f->position.z = p->position.z;
  f->position.y = p->position.y;
  f->position.x = p->position.x;
  memmove(&a->position, &f->position, sizeof(Vec3));
  a->heading = p->angle.y;
  a->word38 = F32(0.0f);
  a->spin.value = F32(0.0f);    /* func_8C044DD2 .. func_8C044DE8: the Body1 at rest */
  a->spin.velocity = F32(0.0f);
  a->spin.force = F32(0.0f);
  a->spin.damping = F32(1.0f);
  a->spin.value = F32(0.0f);
  a->spin.damping = TYPE01_SPIN_DAMPING;
  a->word5c = 0;
  if (a->kind_b == 0)
    a->word64 = WIND_WAIT;
  else
    a->word64 = WIND_SPIN;
  a->word4c = F32(0.0f);
  a->word6c = 0;
  a->push.z = F32(0.0f);
  a->push.y = F32(0.0f);
  a->push.x = F32(0.0f);
  a->reach = WIND_REACH;
  a->contacts.count = 0;
  a->contacts_word = 4;
  a->contacts.cell = a->base.cell;
  a->contacts.owner = member_addr(obj, ActorType01, position);
  a->contacts.respond = TYPE01_RESPOND;
  a->contacts.unknown20 = (int32_t)obj;
  a->contacts.unknown1c = TYPE01_CONTACT_KIND;

  /* the matrix: identity, then turned by the heading */
  memset(&f->m, 0, sizeof(Matrix)); /* the original stores the identity's 16 words one by one */
  f->m.m[0] = F32(1.0f);
  f->m.m[5] = F32(1.0f);
  f->m.m[10] = F32(1.0f);
  f->m.m[15] = F32(1.0f);
  njUnitMatrix(m);
  njRotateY(m, f32_to_int(a->heading));
  f->forward.z = F32(1.0f);
  f->forward.y = F32(0.0f);
  f->forward.x = F32(0.0f);
  njCalcPoint(m, forward, member_addr(obj, ActorType01, point));
  vec3_set(fneg(TYPE01_SIDE), F32(0.0f), F32(0.0f), side);
  njCalcPoint(m, side, side_at);
  vec3_add(side_at, member_addr(obj, ActorType01, position));
  memmove(&a->dir[0], &f->side_at, sizeof(Vec3));
  vec3_set(TYPE01_SIDE, F32(0.0f), F32(0.0f), side);
  njCalcPoint(m, side, side_at);
  vec3_add(side_at, member_addr(obj, ActorType01, position));
  memmove(&a->dir[2], &f->side_at, sizeof(Vec3));
  method_self(turned); /* the local's (empty) constructor */
  njRotateY(m, f32_to_int(TURN_60));
  njCalcPoint(m, forward, turned);
  memmove(&a->dir[1], &f->turned, sizeof(Vec3));
  njRotateY(m, f32_to_int(TURN_MINUS_120));
  njCalcPoint(m, forward, turned);
  memmove(&a->dir[3], &f->turned, sizeof(Vec3));
  a->base.flags = 1;
  hw_stack_free(sizeof(Type01SetupFrame));
}
HW_EXPORT(8c045086, actor_type01_setup(ARG0, ARG1))

/* the spin pulled toward `target` and stepped (states 1 and 2) */
static void actor_type01_spin(gaddr obj, f32 target) {
  body1_pull_angle(target, WIND_SPIN_RATE, member_addr(obj, ActorType01, spin));
  body1_step(member_addr(obj, ActorType01, spin));
}

/* was func_8C04530A: the update (see the top of the file) */
void actor_type01_update(gaddr obj) {
  ActorType01 *a = (ActorType01 *)hw_ptr(obj);
  switch (a->word64) {
  case WIND_WAIT: {
    int32_t started = 0;
    int32_t players = race_setup_at(race_setup_get())->player_count;
    for (int32_t i = 0; i < players; i++) {
      if ((int32_t)race_player_cell(panel_get(), i) >= WIND_START_CELL) {
        started = 1;
        break;
      }
    }
    if (started) {
      a->timer = WIND_TIMER_START;
      a->word5c = 0;
      a->word64++;
    }
    break;
  }
  case WIND_BLOW:
    a->timer++;
    if (a->timer > WIND_STEP_FRAMES) {
      a->timer = 0;
      a->word5c++;
      const int32_t *levels = (const int32_t *)hw_ptr(g_wind_level_tables[a->kind_a]);
      a->word6c = levels[a->word5c];
      if (a->word6c == 0)
        a->word64++;
      if (a->word6c == -1) {
        a->word5c = 0;
        a->word6c = levels[0];
      }
    }
    a->word4c = fmul(f32_from_int(a->word6c), WIND_STRENGTH);
    actor_type01_spin(obj, fmul(f32_from_int(a->word6c), WIND_SPIN_TARGET));
    break;
  case WIND_STOP:
    a->word4c = F32(0.0f);
    actor_type01_spin(obj, F32(0.0f));
    break;
  case WIND_SPIN:
    a->word38 = fadd(a->word38, WIND_TURN);
    break;
  default:
    break;
  }
  if (a->kind_b == 0) {
    gaddr push = hw_stack_alloc(sizeof(Vec3));
    vec3_copy(push, member_addr(obj, ActorType01, point));
    vec3_scale(a->word4c, push);
    memmove(&a->push, hw_ptr(push), sizeof(Vec3));
    hw_stack_free(sizeof(Vec3));
    a->reach = fmac(WIND_REACH_LEVEL, f32_from_int(a->word6c), WIND_REACH);
    a->contacts.radius = a->reach;
    a->word38 = fadd(a->word38, a->spin.value);
    race_scene_slot_insert(panel_get(), member_addr(obj, ActorType01, contacts), WIND_SLOT);
  }
}
HW_EXPORT(8c04530a, actor_type01_update(ARG0))
