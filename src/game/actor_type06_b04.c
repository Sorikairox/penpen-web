/* Pen Pen port, S18 b04: the update and the draw of placement type 0x06 (ActorType06, game/actor_b06.h; its
 * construction is game/actor_type06.c): a thing in two parts; the first player to touch it knocks the second part
 * off: it flies the way the player faces, spinning, and lands on the ground; a Burst starts with the knock. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "actor_b05.h"    /* burst_trigger */
#include "race_scene.h"
#include "actor_b06.h"

#define TYPE06_KNOCK      F32_BITS(0x403FFFFFu)  /* 2.9999998: the knocked part's speed on the ground */
#define TYPE06_KNOCK_UP   F32_BITS(0x3F999999u)  /* 1.2: and up */
#define TYPE06_DAMPING    F32_BITS(0x3F7AE147u)  /* 0.98 */
#define TYPE06_SPIN       F32_BITS(0x44987653u)  /* 1219.7 (6.7 degrees) a step */
#define TYPE06_GRAVITY    F32_BITS(0xBD5F0122u)  /* -0.0545 */
#define TYPE06_REST       F32_BITS(0x40733333u)  /* 3.8: it lands this high above the ground */

/* game/race_scene.c (exported there, no header yet): the heading of player i */
f32 race_player_heading(gaddr scene, int32_t i);
/* game/race_sound_ports.c (exported there, no header yet) */
void race_channel_sound_play(gaddr scene, int32_t i, int32_t channel, int32_t priority, int32_t sound);

static inline ActorType06 *type06_at(gaddr a) { return (ActorType06 *)hw_ptr(a); }

/* the original's frame of the update (0x20 bytes) */
enum {
  T06_F_KNOCK = 0x04,     /* Vec3: the knocked part's velocity */
  T06_F_DISTANCE = 0x0C,  /* f32: the ground's distance below (state 1) */
  T06_F_HEADING = 0x10,   /* f32: the knocking player's heading (state 0) */
  T06_F_GROUND = 0x10,    /* Vec3: the ground query's point (state 1) */
  T06_F_FRAME = 0x20
};

/* was func_8C04BE38: the update. State 0: the last player (below 8) among the contacts' others knocks it: the Burst
 * starts, the second part flies off the way that player faces (3 sideways, 1.2 up, damping 0.98), unspun; the
 * contacts are withdrawn, and the views near it hear it. State 1: the part spins and falls; once it is less than 3.8
 * above the ground it is lifted to 3.8 and lands (state 2). The contacts are offered while touchable; the Burst
 * steps. */
void actor_type06_update(gaddr obj) {
  ActorType06 *a = type06_at(obj);
  gaddr frame = hw_stack_alloc(T06_F_FRAME);
  gaddr part = member_addr(obj, ActorType06, body2);
  switch (a->state) {
  case 0: {
    int32_t player = -1;
    for (int32_t i = 0; i < a->contacts.count; i++) {
      int32_t other = ((const int32_t *)hw_ptr(a->contacts.other))[i];
      if (other <= 7) player = other;
    }
    if (player == -1) break;
    a->state++;
    burst_trigger(a->burst);
    f32 heading = race_player_heading(a->base.scene, player);
    F32_AT(frame + T06_F_HEADING) = heading;
    Vec3 *v = vec3_at(frame + T06_F_KNOCK);
    v->x = fmul(njSin(f32_to_int(heading)), TYPE06_KNOCK);
    v->z = fmul(njCos(f32_to_int(heading)), TYPE06_KNOCK);
    v->y = TYPE06_KNOCK_UP;
    vec3_copy_bytes(member_addr(part, Body, velocity), frame + T06_F_KNOCK);
    vec3_fill(member_addr(part, Body, damping), TYPE06_DAMPING);
    a->spin = F32(0.0f);
    a->touchable = 0;
    for (int32_t i = 0; i < (int32_t)a->base.word0c; i++) {
      int32_t cell = a->base.cell;
      if (!fgt(f32_abs(f32_from_int(cell - race_scene_view_cell(panel_get(), i))), F32(1.0f)))
        race_channel_sound_play(panel_get(), i, 0, 8, 0x25);
    }
    break;
  }
  case 1: {
    a->spin = fadd(a->spin, TYPE06_SPIN);
    f32_add_to(TYPE06_GRAVITY, member_addr(part, Body, force.y));
    body_step(part);
    vec3_copy(frame + T06_F_GROUND, part);
    if (race_scene_course_ground(a->base.scene, frame + T06_F_GROUND, a->base.cell, frame + T06_F_DISTANCE, 0) != 0 &&
        fgt(TYPE06_REST, F32_AT(frame + T06_F_DISTANCE))) {
      f32_add_to(fsub(TYPE06_REST, F32_AT(frame + T06_F_DISTANCE)), member_addr(part, Body, position.y));
      a->state++;
    }
    break;
  }
  default:
    break;
  }
  if (a->touchable != 0) race_scene_slot_insert(panel_get(), member_addr(obj, ActorType06, contacts), 8);
  burst_update(a->burst);
  hw_stack_free(T06_F_FRAME);
}
HW_EXPORT(8c04be38, actor_type06_update(ARG0))

/* was func_8C04C02A: the draw: in state 0 the first part (model 0) at the body turned about y and the second part
 * (model 1) at its body; while it flies or lies, the second part spun about x; then the Burst */
void actor_type06_draw(gaddr obj) {
  const ActorType06 *a = type06_at(obj);
  switch (a->state) {
  case 0:
    matrix_push();
    matrix_translate_v(member_addr(obj, ActorType06, body));
    matrix_rotate_y(a->yaw);
    nj_ModelL1(actor_model(&a->base, 0));
    matrix_pop();
    matrix_push();
    matrix_translate_v(member_addr(obj, ActorType06, body2));
    nj_ModelL1(actor_model(&a->base, 1));
    matrix_pop();
    break;
  case 1:
  case 2:
    matrix_push();
    matrix_translate_v(member_addr(obj, ActorType06, body2));
    matrix_rotate_x(a->spin);
    nj_ModelL1(actor_model(&a->base, 1));
    matrix_pop();
    break;
  default:
    break;
  }
  burst_draw(a->burst);
}
HW_EXPORT(8c04c02a, actor_type06_draw(ARG0))
