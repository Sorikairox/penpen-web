/* Pen Pen port, S18 b04: the init and the update of placement type 0x0F (ActorType0F, game/actor_b07.h; its
 * construction and draw are game/actor_types_b07.c). Something that waits param * 240 updates, appears at the first
 * of a list of waypoints (pairs x, z of a static table, (0, 0) ending them), rises (values[2]: its height above the
 * body, -2 to 5), runs the waypoints on the ground, then runs to its goal, turns to its rest yaw, rests 180 updates
 * nose down, and starts over. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/transform.h"  /* matrix_align_up */
#include "game.h"               /* panel_get */
#include "actor.h"
#include "race_scene.h"         /* race_scene_slot_insert */
#include "actor_b06.h"          /* race_scene_course_ground */
#include "actor_b07.h"

/* a pair x, z of the unit's static tables */
typedef struct GroundXZ {
  f32 x, z;
} GroundXZ;
HW_SIZE(GroundXZ, 8);

#define g_type0f_waypoints  0x8C088454u                   /* GroundXZ[]: (0, 0) ends them */
#define g_type0f_goals      0x8C08849Cu                   /* GroundXZ[param] */
#define g_type0f_rest_yaws  0x8C0884BCu                   /* f32[param] */
#define TYPE0F_START_Y      F32_BITS(0x4285FFFFu)         /* 66.99999: the first waypoint's height */
#define TYPE0F_NEAR         F32_BITS(0x407FFFFFu)         /* 3.9999998: a waypoint nearer than this is reached */
#define TYPE0F_TURN         F32_BITS(0x44088888u)         /* 546.13 (3 degrees): its yaw turns by it every update */
#define TYPE0F_DAMPING      F32_BITS(0x3F333333u)         /* 0.7 */
#define TYPE0F_YAW_DAMPING  F32_BITS(0x3F4CCCCCu)         /* 0.8 */

static inline ActorType0F *type0f_at(gaddr a) { return (ActorType0F *)hw_ptr(a); }
static inline const GroundXZ *ground_xz(gaddr table, int32_t i) { return &((const GroundXZ *)hw_ptr(table))[i]; }

/* was func_8C053EC8: the init from its placement: home and the body there (damping 0.7), pitch and yaw from the
 * angles, state 0 with a wait of param * 240 + 1, hidden; its scale, goal and rest yaw by param; the contacts (radius
 * 4 times the scale, mass 1) in its cell */
void actor_type0f_init(gaddr obj, gaddr placement) {
  ActorType0F *a = type0f_at(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  gaddr local = hw_stack_alloc(0x18); /* the original's frame: a Vec3 at 0 and one at 12 */
  Vec3 *position = vec3_at(local + 12);
  f32 z = p->position.z, y = p->position.y, x = p->position.x;
  position->z = z;
  position->y = y;
  position->x = x;
  vec3_copy_bytes(member_addr(obj, ActorType0F, home), local + 12);
  gaddr body = member_addr(obj, ActorType0F, body);
  vec3_zero(member_addr(body, Body, position));
  vec3_zero(member_addr(body, Body, velocity));
  vec3_zero(member_addr(body, Body, force));
  vec3_fill(member_addr(body, Body, damping), F32(1.0f));
  vec3_copy_bytes(body, member_addr(obj, ActorType0F, home));
  a->body.damping.z = TYPE0F_DAMPING;
  a->body.damping.y = TYPE0F_DAMPING;
  a->body.damping.x = TYPE0F_DAMPING;
  a->values[0].value = p->angle.x;
  a->values[0].damping = TYPE0F_DAMPING;
  a->values[1].value = p->angle.y;
  a->values[1].damping = F32(1.0f);
  int32_t param = (int32_t)p->param;
  a->state = 0;
  a->timer = param * 240 + 1;
  a->shown = 0;
  a->scale = fmac(F32_BITS(0x3E4CCCCCu) /* 0.2 */, f32_from_int(param), F32(0.5f));
  const GroundXZ *g = ground_xz(g_type0f_goals, param);
  Vec3 *goal = vec3_at(local);
  goal->z = g->z;
  goal->y = F32(0.0f);
  goal->x = g->x;
  vec3_copy_bytes(member_addr(obj, ActorType0F, goal), local);
  a->rest_yaw = ((const f32 *)hw_ptr(g_type0f_rest_yaws))[param];
  a->contacts.count = 0;
  a->contacts_word = 4;
  a->contacts.radius = fmul(a->scale, F32_BITS(0x407FFFFFu) /* 3.9999998 */);
  a->contacts.mass = F32(1.0f);
  a->contacts.unknown18 = F32(3.0f);
  a->contacts.cell = a->base.cell;
  a->contacts.unknown1c = 0;
  hw_stack_free(0x18);
}
HW_EXPORT(8c053ec8, actor_type0f_init(ARG0, ARG1))

/* the original's frame of the update (0x3C bytes) */
enum {
  F0F_UP = 0x00,        /* Vec3: (0, 1, 0) when it is not on the ground */
  F0F_POSITION = 0x0C,  /* Vec3: where it is drawn */
  F0F_PUSH = 0x10,      /* Vec3: state 3's push */
  F0F_GROUND = 0x18,    /* Vec3: the ground query's point; state 4's push */
  F0F_NORMAL = 0x24,    /* Vec3: the ground's normal */
  F0F_TARGET3 = 0x20,   /* Vec3: state 3's waypoint */
  F0F_TARGET4 = 0x28,   /* Vec3: state 4's goal */
  F0F_DISTANCE = 0x30,  /* f32: the ground's distance below */
  F0F_FRAME = 0x3C
};

/* push the body toward `target` by `strength` (a unit vector scaled), through the frame's Vec3 at `push` */
static void type0f_push_toward(gaddr obj, gaddr push, gaddr target, f32 strength) {
  gaddr body = member_addr(obj, ActorType0F, body);
  vec3_copy(push, target);
  vec3_sub(push, body);
  njUnitVector(push);
  vec3_scale(strength, push);
  vec3_add(member_addr(body, Body, force), push);
}

/* was func_8C054034: the update (see the top of the file); while shown the body steps, stands on the ground of its
 * cell (when found, and when it moved this update), and its matrix is built (at the body, values[2] higher, aligned
 * to the ground's normal, turned by yaw then pitch, scaled); its contacts are offered to the scene (slot -1) and its
 * cell follows the body */
void actor_type0f_update(gaddr obj) {
  ActorType0F *a = type0f_at(obj);
  gaddr frame = hw_stack_alloc(F0F_FRAME);
  gaddr body = member_addr(obj, ActorType0F, body);
  int32_t moved = 0;
  switch (a->state) {
  case 0: /* waiting, hidden */
    a->timer--;
    if (a->timer == 0) a->state++;
    a->shown = 0;
    break;
  case 1: { /* placed at the first waypoint, at rest */
    a->state++;
    a->shown = 0;
    a->waypoint = 1;
    const GroundXZ *w = ground_xz(g_type0f_waypoints, 0);
    vec3_set(w->x, TYPE0F_START_Y, w->z, body);
    a->values[2].value = F32_BITS(0xBFFFFFFFu) /* -1.9999999 */;
    a->values[2].damping = F32_BITS(0x3F666666u) /* 0.9 */;
    a->values[1].value = F32(0.0f);
    a->values[0].value = F32(0.0f);
    a->timer = 0;
    break;
  }
  case 2: /* rising for 60 updates */
    body1_pull(F32(5.0f), F32_BITS(0x3DF5C28Fu) /* 0.12 */, member_addr(obj, ActorType0F, values[2]));
    body1_step(member_addr(obj, ActorType0F, values[2]));
    a->timer++;
    if (a->timer > 60) {
      a->state++;
      a->values[2].value = F32_BITS(0x40866666u) /* 4.2 */;
      a->values[2].damping = F32(1.0f);
    }
    a->shown = 1;
    moved = 1;
    break;
  case 3: { /* running the waypoints, turning all the while */
    const GroundXZ *w = ground_xz(g_type0f_waypoints, a->waypoint);
    f32 tx = w->x;
    f32 tz = ground_xz(g_type0f_waypoints, a->waypoint)->z;
    if (feq(tx, F32(0.0f)) && feq(tz, F32(0.0f))) {
      a->state++;
      a->timer = 0;
      a->values[1].damping = TYPE0F_YAW_DAMPING;
    } else {
      vec3_set(tx, a->body.position.y, tz, frame + F0F_TARGET3);
      f32 d = vec3_distance(body, frame + F0F_TARGET3);
      if (fgt(TYPE0F_NEAR, d))
        a->waypoint++;
      else
        type0f_push_toward(obj, frame + F0F_PUSH, frame + F0F_TARGET3, F32_BITS(0x3E4CCCCCu) /* 0.2 */);
      f32_add_to(TYPE0F_TURN, member_addr(obj, ActorType0F, values[1].value));
      body1_pull(F32(5.0f), F32_BITS(0x3CF5C28Fu) /* 0.03 */, member_addr(obj, ActorType0F, values[2]));
      body1_step(member_addr(obj, ActorType0F, values[2]));
    }
    a->shown = 1;
    moved = 1;
    break;
  }
  case 4: { /* running to its goal; then turning to its rest yaw (slowly at first, 25 degrees past it) */
    gaddr goal = frame + F0F_TARGET4;
    vec3_copy(goal, member_addr(obj, ActorType0F, goal));
    vec3_at(goal)->y = a->body.position.y;
    f32 d = vec3_distance(body, goal);
    if (fgt(TYPE0F_NEAR, d)) {
      a->state++;
      a->timer = 0;
      a->values[1].damping = TYPE0F_YAW_DAMPING;
    } else {
      type0f_push_toward(obj, frame + F0F_GROUND, goal, F32_BITS(0x3F199999u) /* 0.6 */);
    }
    if (a->timer > 15) {
      body1_pull_angle(a->rest_yaw, F32_BITS(0x3E999999u) /* 0.3 */, member_addr(obj, ActorType0F, values[1]));
      a->values[1].damping = F32(1.0f);
    } else {
      body1_pull_angle(fadd(a->rest_yaw, F32_BITS(0x458E38E3u) /* 4551.1: 25 degrees */), F32_BITS(0x3E4CCCCCu),
                       member_addr(obj, ActorType0F, values[1]));
      a->values[1].damping = TYPE0F_DAMPING;
      a->timer++;
    }
    body1_step(member_addr(obj, ActorType0F, values[1]));
    a->shown = 1;
    moved = 1;
    break;
  }
  case 5: /* resting nose down for 180 updates, sinking */
    a->timer++;
    if (a->timer > 180) {
      a->timer = 0;
      a->state++;
    } else {
      body1_pull_angle(F32(-16384.0f), F32_BITS(0x3D23D70Au) /* 0.04 */, member_addr(obj, ActorType0F, values[0]));
      body1_step(member_addr(obj, ActorType0F, values[0]));
      body1_pull_angle(a->rest_yaw, F32_BITS(0x3E999999u), member_addr(obj, ActorType0F, values[1]));
      a->values[1].damping = F32(1.0f);
      body1_step(member_addr(obj, ActorType0F, values[1]));
      f32_add_to(F32_BITS(0xBD638E38u) /* -0.0556 */, member_addr(body, Body, force.y));
    }
    a->shown = 1;
    break;
  case 6:
    a->state = 1;
    break;
  default:
    break;
  }

  if (a->shown != 0) {
    body_step(body);
    gaddr ground = frame + F0F_GROUND;
    gaddr normal = frame + F0F_NORMAL;
    vec3_copy(ground, body);
    vec3_at(ground)->y = fadd(vec3_at(ground)->y, F32(10.0f));
    int32_t found = race_scene_course_ground(a->base.scene, ground, a->base.cell, frame + F0F_DISTANCE,
                                             (int32_t)normal);
    if (found != 0 && moved != 0) {
      a->body.position.y = fsub(vec3_at(ground)->y, F32_AT(frame + F0F_DISTANCE));
    } else {
      vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), frame + F0F_UP);
      vec3_copy_bytes(normal, frame + F0F_UP);
    }
    gaddr m = member_addr(obj, ActorType0F, matrix);
    njUnitMatrix(m);
    gaddr position = frame + F0F_POSITION;
    vec3_copy(position, body);
    vec3_at(position)->y = fadd(vec3_at(position)->y, a->values[2].value);
    njTranslateV(m, position);
    matrix_align_up(m, normal);
    njRotateY(m, f32_to_int(a->values[1].value));
    njRotateX(m, f32_to_int(a->values[0].value));
    matrix_scale_on(m, a->scale, a->scale, a->scale);
    race_scene_slot_insert(panel_get(), member_addr(obj, ActorType0F, contacts), -1);
    actor_set_cell(obj, body, a->base.cell);
    a->contacts.cell = a->base.cell;
  }
  hw_stack_free(F0F_FRAME);
}
HW_EXPORT(8c054034, actor_type0f_update(ARG0))
