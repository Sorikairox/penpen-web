/* Pen Pen port, S14 b11: the flyer (type 0x11) and type 0x25's construction (game/actor.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

/* ---- type 0x25 ---- */

/* was func_8C059580: type 0x25's base construction: the Actor and its body at rest */
gaddr actor_type25_base_construct(gaddr obj) {
  actor_construct(obj);
  body_reset(member_addr(obj, ActorType25, body));
  return obj;
}
HW_EXPORT(8c059580, RET(actor_type25_base_construct(ARG0)))

/* was func_8C0595D0: type 0x25's constructor: both vtables, then the base */
gaddr actor_type25_construct(gaddr obj) {
  ActorType25 *a = (ActorType25 *)hw_ptr(obj);
  a->base.vtable = 0x8C23E51Cu;
  a->second_vtable = 0x8C23E53Cu;
  actor_type25_base_construct(obj);
  return obj;
}
HW_EXPORT(8c0595d0, RET(actor_type25_construct(ARG0)))

/* was func_8C059756: the clock ticks */
void actor_type25_tick(gaddr obj) {
  ActorType25 *a = (ActorType25 *)hw_ptr(obj);
  a->clock = fadd(a->clock, F32(1.0f));
}
HW_EXPORT(8c059756, actor_type25_tick(ARG0))

/* ---- type 0x11: the flyer ---- */

/* the flyer's poses by kind (static tables of pointers): per posed node its angles (3 floats; a first angle of one
 * degree hides the node) and its position (3 floats; kinds other than 3 and 4 start 6 positions in) */
#define g_flyer_angles    ((const gaddr *)hw_ptr(0x8C0994C8u))
#define g_flyer_positions ((const gaddr *)hw_ptr(0x8C0994A0u))
#define FLYER_FIRST_POSED 6

/* was func_8C059BE0: the flyer's base construction: the Actor and its body at rest */
gaddr flyer_base_construct(gaddr obj) {
  actor_construct(obj);
  body_reset(member_addr(obj, Flyer, body));
  return obj;
}
HW_EXPORT(8c059be0, RET(flyer_base_construct(ARG0)))

/* was func_8C059C30: type 0x11's constructor: both vtables, then the base */
gaddr flyer_construct(gaddr obj) {
  Flyer *f = (Flyer *)hw_ptr(obj);
  f->base.vtable = 0x8C23E55Cu;
  f->second_vtable = 0x8C23E57Cu;
  flyer_base_construct(obj);
  return obj;
}
HW_EXPORT(8c059c30, RET(flyer_construct(ARG0)))

/* was func_8C059D46: step the flyer: waiting, once any player's cell reaches 193 it starts counting (damping
 * 0.982); after 120 steps it flies: pulled toward (2633, -307.4, -10275.9) by 0.016 of the distance, and moves */
void flyer_update(gaddr obj) {
  Flyer *f = (Flyer *)hw_ptr(obj);
  switch (f->state) {
  case 1: {
    int32_t players = race_setup_at(g_race_setup)->player_count;
    int32_t reached = 0;
    for (int32_t i = 0; i < players; i++) {
      if ((int32_t)race_player_cell(g_panel, i) >= 193) {
        reached = 1;
        break;
      }
    }
    if (reached) {
      f->state++;
      const f32 damping = F32_BITS(0x3F7B645Au); /* 0.982 */
      f->body.damping.z = damping;
      f->body.damping.y = damping;
      f->body.damping.x = damping;
      f->timer = 0;
    }
    break;
  }
  case 2:
    if (++f->timer > 120) {
      f->timer = 0;
      f->state++;
    }
    break;
  case 3: {
    gaddr target = hw_stack_alloc(2 * sizeof(Vec3));
    gaddr pull = target + sizeof(Vec3);
    Vec3 *t = vec3_at(target);
    t->z = F32_BITS(0xC6208F99u); /* -10275.9 */
    t->y = F32_BITS(0xC399B333u); /* -307.4 */
    t->x = F32_BITS(0x45248FFFu); /* 2633.0 */
    vec3_copy(pull, target);
    vec3_sub(pull, member_addr(obj, Flyer, body));
    vec3_scale(F32_BITS(0x3C83126Eu) /* 0.016 */, pull);
    vec3_add(member_addr(obj, Flyer, body.force), pull);
    hw_stack_free(2 * sizeof(Vec3));
    body_step(member_addr(obj, Flyer, body));
    break;
  }
  default:
    break;
  }
}
HW_EXPORT(8c059d46, flyer_update(ARG0))

/* was func_8C059E54: pose the flyer's model by its kind: walk the node tree depth first (a node's child, else its
 * sibling; at a leaf back up to the nearest sibling, the walk ends at the root's level) and from the seventh node on
 * give each its position and angles from the tables, hidden when its first angle is one degree */
void flyer_pose(gaddr obj) {
  const Flyer *f = (const Flyer *)hw_ptr(obj);
  const f32 *angles = (const f32 *)hw_ptr(g_flyer_angles[f->kind]);
  const f32 *positions;
  if (f->kind == 3 || f->kind == 4)
    positions = (const f32 *)hw_ptr(g_flyer_positions[f->kind]);
  else
    positions = (const f32 *)hw_ptr(g_flyer_positions[f->kind] + 6 * sizeof(Vec3));
  gaddr node = actor_model(&f->base, 0);
  gaddr parents[64];
  int32_t depth = 0;
  for (int32_t i = 0;; i++) {
    NjObjectPose *n = (NjObjectPose *)hw_ptr(node);
    if (i >= FLYER_FIRST_POSED) {
      if (feq(angles[0], F32_BITS(0x43360B60u) /* 1 degree */))
        n->evalflags |= 8;
      else
        n->evalflags &= ~8u;
      n->pos.x = positions[0];
      n->pos.y = positions[1];
      n->pos.z = positions[2];
      positions += 3;
      n->ang[0] = f32_to_int(angles[0]);
      n->ang[1] = f32_to_int(angles[1]);
      n->ang[2] = f32_to_int(angles[2]);
      angles += 3;
    }
    if (n->child != 0) {
      parents[depth++] = node;
      node = n->child;
    } else if (n->sibling != 0) {
      node = n->sibling;
    } else {
      if (depth == 0) return;
      while (n->sibling == 0 && depth > 0) {
        node = parents[--depth];
        n = (NjObjectPose *)hw_ptr(node);
      }
      if (depth == 0) return;
      node = n->sibling;
    }
  }
}
HW_EXPORT(8c059e54, flyer_pose(ARG0))

/* was func_8C059F26: draw the flyer: pose it, then its model at the body turned z, y, x */
void flyer_draw(gaddr obj) {
  const Flyer *f = (const Flyer *)hw_ptr(obj);
  flyer_pose(obj);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, Flyer, body));
  njRotateZ(0, f32_to_int(f->angle.z));
  njRotateY(0, f32_to_int(f->angle.y));
  njRotateX(0, f32_to_int(f->angle.x));
  nj_ModelL1(actor_model(&f->base, 0));
  njPopMatrix(1);
}
HW_EXPORT(8c059f26, flyer_draw(ARG0))
