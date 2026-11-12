/* Pen Pen port, S14 b11: the flock that flies off when a player comes (type 0x29) and the gate that turns in half
 * turns (type 0x17) (game/actor.h). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "race_scene.h"
#include "scenery.h"

/* ---- type 0x29: the flock ---- */

/* was func_8C055132: type 0x29's constructor: both vtables, then the Actor */
gaddr flock_construct(gaddr obj) {
  Flock *f = (Flock *)hw_ptr(obj);
  f->base.vtable = 0x8C23E31Cu;
  f->second_vtable = 0x8C23E33Cu;
  actor_base_construct(obj);
  return obj;
}
HW_EXPORT(8c055132, RET(flock_construct(ARG0)))

/* was func_8C0551CE: a flock body's constructor (the array's): at rest at the origin (body_reset's work) */
HW_EXPORT(8c0551ce, body_reset(ARG0))

/* was func_8C0553B8: step the flock: waiting, it starts flying once any player's cell reaches the trigger cell;
 * flying, every body less than 500 above the start height steps, and once none does the flock is gone */
void flock_update(gaddr obj) {
  Flock *f = (Flock *)hw_ptr(obj);
  switch (f->state) {
  case 0: {
    int32_t players = race_setup_at(g_race_setup)->player_count;
    int32_t reached = 0;
    for (int32_t i = 0; i < players; i++) {
      if ((int32_t)race_player_cell(g_panel, i) >= f->trigger_cell) {
        reached = 1;
        break;
      }
    }
    if (reached) f->state++;
    break;
  }
  case 1: {
    int32_t all_gone = 1;
    gaddr body = f->bodies;
    for (int32_t i = 0; i < f->count; i++, body += sizeof(Body)) {
      if (fgt(fadd(f->height, F32(500.0f)), body_at(body)->position.y)) {
        body_step(body);
        all_gone = 0;
      }
    }
    if (all_gone) f->state++;
    break;
  }
  default:
    break;
  }
}
HW_EXPORT(8c0553b8, flock_update(ARG0))

/* ---- type 0x17: the turning gate ---- */

#define GATE_RESPOND 0x8C055D78u      /* the gate's contact response (guest code) */
#define DEG_1   F32_BITS(0x43360B60u) /* one degree in Ninja units (182.04) */
#define DEG_30  F32_BITS(0x45AAAAAAu) /* 5461.33 */
#define DEG_180 F32(32768.0f)
#define DEG_210 F32_BITS(0x47155555u) /* 38229.33 */
#define DEG_360 F32(65536.0f)

/* was func_8C055AA8: the gate's base construction: the Actor, the turn at rest (0, 0, 0, 1) and the contacts (owned by
 * the position, radius 45, no room) */
gaddr turning_gate_base_construct(gaddr obj) {
  TurningGate *g = (TurningGate *)hw_ptr(obj);
  actor_construct(obj);
  g->turn.value = F32(0.0f);
  g->turn.velocity = F32(0.0f);
  g->turn.force = F32(0.0f);
  g->turn.damping = F32(1.0f);
  contacts_construct(member_addr(obj, TurningGate, contacts), member_addr(obj, TurningGate, position), F32(45.0f), 0);
  return obj;
}
HW_EXPORT(8c055aa8, RET(turning_gate_base_construct(ARG0)))

/* was func_8C055AE2: type 0x17's constructor: both vtables, then the base */
gaddr turning_gate_construct(gaddr obj) {
  TurningGate *g = (TurningGate *)hw_ptr(obj);
  g->base.vtable = 0x8C23E39Cu;
  g->second_vtable = 0x8C23E3BCu;
  turning_gate_base_construct(obj);
  return obj;
}
HW_EXPORT(8c055ae2, RET(turning_gate_construct(ARG0)))

/* was func_8C055B48: set the gate up from its placement: a gate with a param stays still (state 4); the turn starts
 * at 30 degrees, still; its contacts take part in others' (cell, the gate's response, the gate as their object,
 * kind 0xCD) */
void turning_gate_init(gaddr obj, gaddr placement) {
  TurningGate *g = (TurningGate *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  g->position = pl->position;
  g->state = pl->param == 0 ? 0 : 4;
  g->turn.value = F32(0.0f);
  g->turn.velocity = F32(0.0f);
  g->turn.force = F32(0.0f);
  g->turn.damping = F32(1.0f);
  g->turn.value = DEG_30;
  g->turn.velocity = F32(0.0f);
  g->turn.damping = F32(1.0f);
  g->timer = F32(0.0f);
  g->contacts.count = 0;
  g->contacts_word = 4;
  g->contacts.cell = g->base.cell;
  g->contacts.respond = GATE_RESPOND;
  g->contacts.unknown20 = (int32_t)obj;
  g->contacts.unknown1c = 0xCD;
}
HW_EXPORT(8c055b48, turning_gate_init(ARG0, ARG1))

/* was func_8C055C20: step the gate: resting, it counts 180 steps then turns at a degree a step; the first turn stops
 * at 210 degrees, the second once the angle (wrapped past a full turn) is between 30 and 180 degrees. The turn
 * steps and wraps at 65536; the contacts go into the race scene's slot 8 */
void turning_gate_update(gaddr obj) {
  TurningGate *g = (TurningGate *)hw_ptr(obj);
  switch (g->state) {
  case 0:
  case 2:
    g->timer = fadd(g->timer, F32(1.0f));
    if (fgt(g->timer, F32(180.0f))) {
      g->timer = F32(0.0f);
      g->state++;
      g->turn.velocity = DEG_1;
    }
    break;
  case 1:
    if (!fgt(DEG_210, g->turn.value)) {
      g->state++;
      g->turn.velocity = F32(0.0f);
    }
    break;
  case 3:
    if (!fgt(DEG_30, g->turn.value) && fgt(DEG_180, g->turn.value)) {
      g->state = 0;
      g->turn.velocity = F32(0.0f);
    }
    break;
  default:
    break;
  }
  body1_step(member_addr(obj, TurningGate, turn));
  if (fgt(g->turn.value, DEG_360)) g->turn.value = fadd(g->turn.value, F32(-65536.0f));
  race_scene_slot_insert(g_panel, member_addr(obj, TurningGate, contacts), 8);
}
HW_EXPORT(8c055c20, turning_gate_update(ARG0))
