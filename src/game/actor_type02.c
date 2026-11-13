/* Pen Pen port, S16 (b06): the update of the actor of placement type 2 (ActorType02, game/actor.h; func_8C045B36).
 *
 * A boulder that waits (state 0) until a player reaches course cell 0x42 (unless param bit 6 is set), counts its
 * delay down (state 1), then is thrown toward its target (velocity = 0.02 of the way, damping 0.99) and rolls
 * (state 2): gravity and a small push in x, a step, then it rests on the ground below it (the ground query of the
 * course: within 5, it is lifted onto it and a fall faster than 0.1 is heard), it spins with its speed, and its
 * contacts follow it. After 300 updates, or when it hits something, it bursts (state 3): the Burst starts at its
 * position. The Burst updates every frame; a landing or the burst plays sound 0x25 in every view whose camera is
 * within a cell of the boulder. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "game.h"         /* panel_get */
#include "actor.h"
#include "race_scene.h"   /* race_setup_get, race_scene_slot_insert */
#include "scenery.h"      /* race_player_cell, race_scene_sound */

#define BOULDER_START_CELL 0x42
#define BOULDER_DAMPING    F32_BITS(0x3F7D70A3u) /* 0.99 */
#define BOULDER_THROW      F32_BITS(0x3CA3D70Au) /* 0.02: of the way to the target, as velocity */
#define BOULDER_GRAVITY    F32_BITS(0xBD5F0122u) /* -0.05444444 */
#define BOULDER_DRIFT      F32_BITS(0x3BB60B60u) /* 0.0055555: pushed in x every update */
#define BOULDER_REST       F32(5.0f)             /* rests this far above the ground */
#define BOULDER_THUD       F32_BITS(0x3D4CCCCBu) /* 0.049999993: half the fall speed above it is heard */
#define BOULDER_TILT       F32_BITS(0x46638E38u) /* 14563.556: 80 degrees (Ninja angle units) */
#define BOULDER_SPIN       F32(2086.0f)          /* spin per squared speed */
#define BOULDER_LIFETIME   300
#define BOULDER_SLOT       8                     /* the race scene's slot list the contacts go to */
#define SOUND_LAND         0x25
#define SOUND_BURST        0x26

enum { BOULDER_WAIT = 0, BOULDER_DELAY = 1, BOULDER_ROLL = 2, BOULDER_BURST = 3 };

/* func_8C04581C: the course's ground below pos (a thunk to func_8C0419DA on the race scene's course): non-zero when
 * found, the distance to it in *distance */
static int32_t race_scene_ground_below(gaddr scene, gaddr pos, int32_t cell, gaddr distance, int32_t flags) {
  return (int32_t)CALL5(0x8C04581Cu, scene, pos, cell, distance, flags);
}
/* func_8C045840: the course cell under view i's camera (-1: the current view) */
static int32_t race_scene_view_cell_at(gaddr scene, int32_t view) {
  return (int32_t)CALL2(0x8C045840u, scene, view);
}

/* the original's frame (0x2C bytes under r8 and PR): the out-parameter the ground query writes, the vectors */
typedef struct BoulderFrame {
  uint8_t pad_00[0x08];  /* 0x00: half the fall speed, the spin */
  Vec3 flat;             /* 0x08: the velocity without its y */
  f32 ground;            /* 0x14: the ground query's distance (and the player's cell in state 0) */
  Vec3 v;                /* 0x18: the way to the target, the drift */
  uint8_t rest[0x34 - 0x24]; /* 0x24: the sound, the counter, the actor, r8, PR */
} BoulderFrame;
HW_OFFSET(BoulderFrame, flat, 0x08);
HW_OFFSET(BoulderFrame, ground, 0x14);
HW_OFFSET(BoulderFrame, v, 0x18);
HW_SIZE(BoulderFrame, 0x34);

/* was func_8C045B36: the update (see the top of the file) */
void actor_type02_update(gaddr obj) {
  ActorType02 *a = (ActorType02 *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(BoulderFrame));
  BoulderFrame *f = (BoulderFrame *)hw_ptr(frame);
  gaddr body = member_addr(obj, ActorType02, body);
  gaddr v = member_addr(frame, BoulderFrame, v);
  int32_t sound = 0;
  switch (a->state) {
  case BOULDER_WAIT: {
    if (a->flag40 != 0) break;
    int32_t found = 0;
    int32_t players = race_setup_at(race_setup_get())->player_count;
    for (int32_t i = 0; i < players; i++) {
      if ((int32_t)race_player_cell(panel_get(), i) >= BOULDER_START_CELL) {
        found = 1;
        break;
      }
    }
    if (found)
      a->state++;
    break;
  }
  case BOULDER_DELAY:
    a->delay--;
    if (a->delay > 0) break;
    a->body.damping.z = BOULDER_DAMPING; /* func_8C04573A */
    a->body.damping.y = BOULDER_DAMPING;
    a->body.damping.x = BOULDER_DAMPING;
    vec3_copy(v, member_addr(obj, ActorType02, target));
    vec3_sub(v, body);
    vec3_scale(BOULDER_THROW, v);
    vec3_copy_bytes(member_addr(body, Body, velocity), v);
    a->state++;
    break;
  case BOULDER_ROLL: {
    f32_add_to(BOULDER_GRAVITY, member_addr(body, Body, force.y));
    vec3_set(BOULDER_DRIFT, F32(0.0f), F32(0.0f), v); /* written word by word in the original */
    vec3_add(member_addr(body, Body, force), v);
    body_step(body);
    if (race_scene_ground_below(a->base.scene, body, a->base.cell, member_addr(frame, BoulderFrame, ground), 0) != 0 &&
        fgt(BOULDER_REST, f->ground)) {
      f32_add_to(fsub(BOULDER_REST, f->ground), member_addr(body, Body, position.y));
      f32 half_fall = fmul(a->body.velocity.y, F32(0.5f));
      f32_add_to(fneg(half_fall), member_addr(body, Body, force.y));
      a->body.velocity.y = F32(0.0f);
      if (fgt(half_fall, BOULDER_THUD))
        sound = SOUND_LAND;
    }
    a->angle_y = BOULDER_TILT;
    gaddr flat = member_addr(frame, BoulderFrame, flat);
    vec3_copy(flat, member_addr(body, Body, velocity));
    f->flat.y = F32(0.0f);
    a->angle_x = fadd(a->angle_x, fmul(njScalor2(flat), BOULDER_SPIN));
    actor_set_cell(obj, body, a->base.cell);
    a->contacts.cell = a->base.cell;
    a->wordd4++;
    if (a->wordd4 > BOULDER_LIFETIME || a->contacts.count != 0) {
      a->state++;
      Burst *b = (Burst *)hw_ptr(a->burst);
      b->origin = a->body.position; /* func_8C045788: a block copy */
      b->trigger = 1;               /* func_8C04829A */
      sound = SOUND_BURST;
    } else {
      race_scene_slot_insert(panel_get(), member_addr(obj, ActorType02, contacts), BOULDER_SLOT);
    }
    break;
  }
  case BOULDER_BURST:
  default:
    break;
  }
  burst_update(a->burst);
  if (sound != 0) {
    /* the original plays SOUND_LAND whichever sound was set */
    for (int32_t view = 0; view < (int32_t)a->base.word0c; view++) {
      int32_t cell = a->base.cell;
      int32_t d = cell - race_scene_view_cell_at(panel_get(), view);
      if (!fgt(f32_abs(f32_from_int(d)), F32(1.0f)))
        race_scene_sound(panel_get(), view, 0, 8, SOUND_LAND);
    }
  }
  hw_stack_free(sizeof(BoulderFrame));
}
HW_EXPORT(8c045b36, actor_type02_update(ARG0))
