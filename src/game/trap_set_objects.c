/* Pen Pen port, S18 b02: the TrapSet's objects (game/trap_set.h): building them from the layout's placements (the
 * ActorGroup's factory, game/actor_factory.c, whose copy this unit has), handing each its data, attaching them
 * again, and drawing them. These mirror the ActorGroup's methods (game/actor_group.c), with the TrapSet's container
 * for every object's models. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "../core/file.h"
#include "actor.h"
#include "trap_set.h"

/* was func_8C01F6AC (an Actor inline method): its flags word */
uint32_t actor_flags(gaddr actor) { return actor_at(actor)->flags; }
HW_EXPORT(8c01f6ac, RET(actor_flags(ARG0)))

/* was func_8C01F74C (a TrapSet inline method): whether the objects are updated and drawn */
void trap_set_set_shown(gaddr self, int32_t shown) { trap_set_at(self)->shown = shown; }
HW_EXPORT(8c01f74c, trap_set_set_shown(ARG0, (int32_t)ARG1))

/* was func_8C01F780: this unit's copy of the actor factory (func_8C043D4C: the same 0x34 classes, sizes and
 * constructors, the same default): a new actor of placement type `type` */
HW_EXPORT(8c01f780, RET(actor_create(ARG0, ARG1)))

/* was func_8C0217EC: hand a new object what it needs: the TrapSet's parent (the scene) as its scene; the container
 * as its texture list; its model and motion lists from the container (kind k = param bits 12-15: its entries 2k+1
 * and 2k+2); its index and the TrapSet's word434; then its init method with the placement. The original also
 * copies the placement's position into a local Vec3 that nothing reads (kept: the original's frame, 0x28 bytes,
 * the vector at its bottom, live across the init call). */
void trap_set_attach(gaddr self, gaddr actor, gaddr placement, int32_t index) {
  TrapSet *t = trap_set_at(self);
  Actor *a = actor_at(actor);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->scene = t->task.parent;
  /* the mask leaves no sign bit: the original's rounding of a negative value before the shift never applies */
  int32_t kind = (int32_t)(p->param & 0xF000u) >> 12;
  a->texlist = t->file;
  gaddr entry = t->file + (gaddr)((2 * kind + 1) * (int32_t)sizeof(ContainerEntry));
  a->model_set = entry;
  a->word1c = entry + (gaddr)sizeof(ContainerEntry);
  a->index = index;
  a->word0c = (uint32_t)t->word434;
  gaddr frame = hw_stack_alloc(0x28);
  vec3_set(p->position.x, p->position.y, p->position.z, frame);
  const VMethod *init = &actor_vtbl(actor)->init;
  hw_call(init->fn, 2, actor + (uint32_t)init->this_adjust, placement, 0, 0);
  hw_stack_free(0x28);
}
HW_EXPORT(8c0217ec, trap_set_attach(ARG0, ARG1, ARG2, (int32_t)ARG3))

/* was func_8C021718: build the layout's objects: none yet, not shown; one object per placement (up to the -1 type)
 * from the factory, kept and attached (the original has no bound on the 256 slots) */
void trap_set_populate(gaddr self) {
  TrapSet *t = trap_set_at(self);
  t->object_count = 0;
  t->shown = 0;
  gaddr placement = g_trap_set_placements[t->layout];
  while (((const ActorPlacement *)hw_ptr(placement))->type != -1) {
    gaddr actor = actor_create(self, (uint32_t)((const ActorPlacement *)hw_ptr(placement))->type);
    t->objects[t->object_count] = actor;
    trap_set_attach(self, actor, placement, t->object_count);
    placement += sizeof(ActorPlacement);
    t->object_count++;
  }
}
HW_EXPORT(8c021718, trap_set_populate(ARG0))

/* was func_8C02176E: not shown; every object gets its placement again (layout 1's list, by index: a constant of
 * the original, not TrapSet.layout): its flags bit 1 set, then its data handed again */
void trap_set_reattach(gaddr self) {
  TrapSet *t = trap_set_at(self);
  int32_t layout = 1;
  t->shown = 0;
  gaddr placement = g_trap_set_placements[layout];
  for (int32_t i = 0; ((const ActorPlacement *)hw_ptr(placement))->type != -1; i++, placement += sizeof(ActorPlacement)) {
    gaddr actor = t->objects[i];
    actor_at(actor)->flags = actor_flags(actor) | 2u;
    trap_set_attach(self, actor, placement, i);
  }
}
HW_EXPORT(8c02176e, trap_set_reattach(ARG0))

/* was func_8C02168E: when shown, each object's draw method (ActorVtbl.draw, on the object moved by the method's
 * adjustment); the count is read again for each */
void trap_set_draw(gaddr self) {
  TrapSet *t = trap_set_at(self);
  if (t->shown == 0)
    return;
  for (int32_t i = 0; i < t->object_count; i++) {
    gaddr actor = t->objects[i];
    const VMethod *m = &actor_vtbl(actor)->draw;
    hw_call(m->fn, 1, actor + (uint32_t)m->this_adjust, 0, 0, 0);
  }
}
HW_EXPORT(8c02168e, trap_set_draw(ARG0))

/* was func_8C02189A: when shown, each object's per-view draw (ActorVtbl.draw_view) after the TrapSet's texture list */
void trap_set_draw_view(gaddr self) {
  TrapSet *t = trap_set_at(self);
  if (t->shown == 0)
    return;
  for (int32_t i = 0; i < t->object_count; i++) {
    njSetTexture(t->texlist);
    gaddr actor = t->objects[i];
    const VMethod *m = &actor_vtbl(actor)->draw_view;
    hw_call(m->fn, 1, actor + (uint32_t)m->this_adjust, 0, 0, 0);
  }
}
HW_EXPORT(8c02189a, trap_set_draw_view(ARG0))
