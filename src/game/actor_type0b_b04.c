/* Pen Pen port, S18 b04: the wide contacts' response of placement type 0x0B (ActorType0B, game/actor_b07.h; its
 * other methods are game/actor_type0b.c): a body low enough (below the actor's height plus the radius plus 15.5) and
 * within 26.9 of its centre on the ground is flung away along the angle of the contact's other body (5/3 or, when
 * the param is not 0, 10/3 strong sideways and 10/3 up), and the views near the actor hear it; otherwise the default
 * response. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_scene_view_cell */
#include "actor_b07.h"    /* vec3_distance2 */

#define TYPE0B_ABOVE      F32_BITS(0x4177FFFFu)  /* 15.499999 */
#define TYPE0B_REACH2     F32_BITS(0x44345FE5u)  /* 721.5: the square of the reach on the ground */
#define TYPE0B_FLING      F32_BITS(0x3FD55555u)  /* 1.6666666 */
#define TYPE0B_FLING_BIG  F32_BITS(0x40555555u)  /* 3.3333333 */

/* core/contact.c (exported there, not in a header): the default response */
void contact_respond(gaddr self, int32_t i, gaddr body);
/* game/race_sound_ports.c (exported there, no header yet) */
void race_channel_sound_play(gaddr scene, int32_t i, int32_t channel, int32_t priority, int32_t sound);

/* was func_8C052862 (this unit's copy, still translated: written here): the body's velocity = (x, y, z) */
static void type0b_body_velocity_set(gaddr body, f32 x, f32 y, f32 z) {
  Body *b = body_at(body);
  b->velocity.x = x;
  b->velocity.y = y;
  b->velocity.z = z;
}

/* was func_8C052D20 (with its pieces func_8C052E0A, func_8C052E16, func_8C052E62): the wide contacts' response to
 * contact i with `body` (see the top of the file); `turn` is the contact's other object, whose first float is the
 * angle (0 when there is none); the response's fifth argument is only handed on to the default response */
void actor_type0b_respond(gaddr contacts, int32_t i, gaddr body, gaddr turn, uint32_t arg4) {
  gaddr owner = ((const gaddr *)hw_ptr(contacts_at(contacts)->unknown4c))[i];
  ActorType0B *a = (ActorType0B *)hw_ptr(owner);
  gaddr frame = hw_stack_alloc(0x34); /* the original's frame: the angle at 0, Vec3s at 8 and 0x14 */
  f32 y = body_at(body)->position.y;
  if (fgt(fadd(fadd(a->position.y, contacts_at(contacts)->radius), TYPE0B_ABOVE), y)) {
    vec3_copy(frame + 0x14, body);
    vec3_copy(frame + 0x08, member_addr(owner, ActorType0B, position));
    vec3_at(frame + 0x08)->y = F32(0.0f);
    vec3_at(frame + 0x14)->y = F32(0.0f);
    if (fgt(TYPE0B_REACH2, vec3_distance2(frame + 0x14, frame + 0x08))) {
      f32 strength = a->param == 0 ? TYPE0B_FLING : TYPE0B_FLING_BIG;
      f32 angle = turn != 0 ? F32_AT(turn) : F32(0.0f);
      F32_AT(frame) = angle;
      f32 z = fmul(njCos(f32_to_int(angle)), strength);
      f32 x = fmul(njSin(f32_to_int(angle)), strength);
      type0b_body_velocity_set(body, x, TYPE0B_FLING_BIG, z);
      for (int32_t v = 0; v < (int32_t)a->base.word0c; v++) {
        int32_t cell = a->base.cell;
        if (!fgt(f32_abs(f32_from_int(cell - race_scene_view_cell(panel_get(), v))), F32(1.0f)))
          race_channel_sound_play(panel_get(), v, 0, 8, 0x26);
      }
    } else {
      contact_respond(contacts, i, body); /* the original hands it all five arguments; it reads three */
    }
  }
  hw_stack_free(0x34);
}
HW_EXPORT(8c052d20, actor_type0b_respond(ARG0, (int32_t)ARG1, ARG2, ARG3, ARG4))
