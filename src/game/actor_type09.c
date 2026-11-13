/* Pen Pen port, S17 b06: the scenery actor of placement type 0x09 (game/actor_b06.h): a model that bursts into
 * particles the first time something touches it. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"
#include "actor.h"
#include "actor_b05.h"
#include "race_scene.h"
#include "scenery.h"
#include "actor_b06.h"

#define TYPE09_VTABLE          0x8C23E01Cu
#define TYPE09_SECOND_VTABLE   0x8C23E03Cu
#define TYPE09_BURST_PARTICLES 20
#define TYPE09_BURST_TEXTURE   0x8C099054u /* the particles' texture (a static of the binary) */
#define TYPE09_MODEL           2           /* in the actor's model list */
#define TYPE09_SLOT            8           /* its contacts' slot in the race scene's list */
#define TYPE09_SOUND           0x27        /* the burst, of bank 8 */

enum { TYPE09_WHOLE = 0, TYPE09_BURST = 1 };

/* func_8C0482A0: draw a Burst's particles (still translated) */
static void burst_draw(gaddr burst) { CALL1(0x8C0482A0u, burst); }

/* was func_8C04CD5A: the base construction: the Actor, the contacts owned by the position (radius 7, room for 2),
 * a Burst of 20 particles (0 when out of memory) */
gaddr actor_type09_base_construct(gaddr obj) {
  actor_construct(obj);
  contacts_construct(member_addr(obj, ActorType09, contacts), member_addr(obj, ActorType09, position),
                     F32_BITS(0x40DFFFFFu) /* 6.9999995 */, 2);
  gaddr block = syMalloc(sizeof(Burst));
  gaddr burst = 0;
  if (block != 0) burst = burst_construct(block, TYPE09_BURST_PARTICLES);
  ((ActorType09 *)hw_ptr(obj))->burst = burst;
  return obj;
}
HW_EXPORT(8c04cd5a, RET(actor_type09_base_construct(ARG0)))

/* was func_8C04CDA0: type 9's constructor: both vtables, then the base */
gaddr actor_type09_construct(gaddr obj) {
  ActorType09 *a = (ActorType09 *)hw_ptr(obj);
  a->base.vtable = TYPE09_VTABLE;
  a->second_vtable = TYPE09_SECOND_VTABLE;
  actor_type09_base_construct(obj);
  return obj;
}
HW_EXPORT(8c04cda0, RET(actor_type09_construct(ARG0)))

/* was func_8C04CDF0: the destructor body: the Burst deleted through its virtual destructor ("free" = 1) when there
 * is one, the contacts freed */
void actor_type09_destroy(gaddr obj) {
  gaddr burst = ((ActorType09 *)hw_ptr(obj))->burst;
  if (burst != 0) {
    const VMethod *destroy = (const VMethod *)hw_ptr(((const Burst *)hw_ptr(burst))->vtable);
    hw_call(destroy->fn, 2, burst + (uint32_t)destroy->this_adjust, 1, 0, 0);
  }
  contacts_free(member_addr(obj, ActorType09, contacts));
}
HW_EXPORT(8c04cdf0, actor_type09_destroy(ARG0))

/* was func_8C04CE28: the deleting destructor */
gaddr actor_type09_delete(gaddr obj, int32_t free_memory) {
  actor_type09_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04ce28, RET(actor_type09_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04CE4C: the destructor body, returning the object */
gaddr actor_type09_destroy_self(gaddr obj) {
  actor_type09_destroy(obj);
  return obj;
}
HW_EXPORT(8c04ce4c, RET(actor_type09_destroy_self(ARG0)))

/* was func_8C04CE5E: set it up from its placement: the Burst's defaults, whole, at the placement's position and
 * angle about y, model 2; the Burst starts at the position with the actor's texture list and the particle texture;
 * the contacts (mass 0.4, kind 2) on the actor's cell */
void actor_type09_init(gaddr obj, gaddr placement) {
  ActorType09 *a = (ActorType09 *)hw_ptr(obj);
  const ActorPlacement *pl = (const ActorPlacement *)hw_ptr(placement);
  effect_defaults(a->burst);
  a->state = TYPE09_WHOLE;
  a->position = pl->position;
  a->model = actor_model(&a->base, TYPE09_MODEL);
  a->angle_y = pl->angle.y;
  Burst *b = (Burst *)hw_ptr(a->burst);
  b->origin = a->position;
  b->texlist = a->base.texlist;
  b->texture = TYPE09_BURST_TEXTURE;
  a->contacts.count = 0;
  a->contacts_word = 4;
  a->contacts.unknown18 = F32(1.0f);
  a->contacts.mass = F32_BITS(0x3ECCCCCCu); /* 0.4 */
  a->contacts.cell = a->base.cell;
  a->contacts.unknown1c = 2;
}
HW_EXPORT(8c04ce5e, actor_type09_init(ARG0, ARG1))

/* was func_8C04CF08: step it: while whole, a touch bursts it (every view whose camera is within a cell of it plays
 * the burst sound); untouched, its contacts go into the race scene's slot 8. The Burst steps either way. */
void actor_type09_update(gaddr obj) {
  ActorType09 *a = (ActorType09 *)hw_ptr(obj);
  switch (a->state) {
  case TYPE09_WHOLE:
    if (a->contacts.count != 0) {
      burst_trigger(a->burst);
      a->state++;
      for (int32_t view = 0; view < (int32_t)a->base.word0c; view++) {
        int32_t d = a->base.cell - race_scene_view_cell(g_panel, view);
        if (!fgt(fabs_(f32_from_int(d)), F32(1.0f))) race_scene_sound(g_panel, view, 0, 8, TYPE09_SOUND);
      }
    } else {
      race_scene_slot_insert(g_panel, member_addr(obj, ActorType09, contacts), TYPE09_SLOT);
    }
    break;
  case TYPE09_BURST:
  default:
    break;
  }
  burst_update(a->burst);
}
HW_EXPORT(8c04cf08, actor_type09_update(ARG0))

/* was func_8C04CFDC: draw it: the model at its position while whole, then the Burst */
void actor_type09_draw(gaddr obj) {
  ActorType09 *a = (ActorType09 *)hw_ptr(obj);
  if (a->state == TYPE09_WHOLE) {
    njPushMatrix(0);
    njTranslateV(0, member_addr(obj, ActorType09, position));
    nj_ModelL1(a->model);
    njPopMatrix(1);
  }
  burst_draw(a->burst);
}
HW_EXPORT(8c04cfdc, actor_type09_draw(ARG0))
