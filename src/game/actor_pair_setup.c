/* Pen Pen port, S16 (b06): the set-up of the actor of placement type 3 (ActorPair, game/actor.h; func_8C0460A6).
 *
 * The placement's param picks one of three fixed pairs of the course: where its two parts stand, the angle both
 * rest at, the angles they turn to when a player comes near (actor_pair_update, game/actor_misc.c), their models
 * (two of the actor's model list) and how much their turns are damped. The centre is half way between the parts.
 * Another param keeps both damping 1 and leaves the rest as constructed. */
#include <string.h>
#include "hw.h"
#include "../core/vec.h"
#include "actor.h"

#define PAIR_Y F32_BITS(0xC3C47FFFu) /* -392.99997: the height of every part */

/* both parts' positions (x, z) */
static void actor_pair_place(gaddr obj, f32 ax, f32 az, f32 bx, f32 bz) {
  ActorPair *a = (ActorPair *)hw_ptr(obj);
  gaddr v = hw_stack_alloc(sizeof(Vec3));
  vec3_set(ax, PAIR_Y, az, v);
  memmove(&a->position_a, hw_ptr(v), sizeof(Vec3));
  vec3_set(bx, PAIR_Y, bz, v);
  memmove(&a->position_b, hw_ptr(v), sizeof(Vec3));
  hw_stack_free(sizeof(Vec3));
}

/* the angles (Ninja units) and the models (indexes in the actor's model list) */
static void actor_pair_angles(gaddr obj, f32 rest, f32 target_a, f32 target_b, int32_t model_a) {
  ActorPair *a = (ActorPair *)hw_ptr(obj);
  a->rest_angle = rest;
  a->target_a = target_a;
  a->target_b = target_b;
  a->model_a = actor_model(&a->base, model_a);
  a->model_b = actor_model(&a->base, model_a + 1);
}

/* was func_8C0460A6: the set-up from the placement (see the top of the file) */
void actor_pair_setup(gaddr obj, gaddr placement) {
  ActorPair *a = (ActorPair *)hw_ptr(obj);
  f32 damping = F32(1.0f);
  a->kind = (int32_t)((const ActorPlacement *)hw_ptr(placement))->param;
  switch (a->kind) {
  case 0:
    actor_pair_place(obj, F32(-2260.0f), F32(-7180.0f),
                     F32_BITS(0xC50B3FFFu) /* -2227.9998 */, F32_BITS(0xC5E10FFFu) /* -7201.9995 */);
    actor_pair_angles(obj, F32_BITS(0x46B1C71Cu) /* 22755.555: 125 degrees */,
                      F32_BITS(0x4718E38Eu) /* 39139.555: 215 degrees */,
                      F32_BITS(0x45C71C71u) /* 6371.555: 35 degrees */, 2);
    damping = F32_BITS(0x3F6E147Au); /* 0.93 */
    break;
  case 1:
    actor_pair_place(obj, F32_BITS(0xC51D0FFFu) /* -2512.9998 */, F32_BITS(0xC5DF17FFu) /* -7138.9995 */,
                     F32_BITS(0xC51EDFFFu) /* -2541.9998 */, F32(-7180.0f));
    actor_pair_angles(obj, F32_BITS(0x46B1C71Cu) /* 22755.555: 125 degrees */,
                      F32_BITS(0x46F8E38Eu) /* 31857.777: 175 degrees */,
                      F32_BITS(0x458E38E3u) /* 4551.111: 25 degrees */, 4);
    damping = F32_BITS(0x3F68F5C2u); /* 0.91 */
    break;
  case 2:
    actor_pair_place(obj, F32(-2805.0f), F32_BITS(0xC5E077FFu) /* -7182.9995 */,
                     F32(-2780.0f), F32_BITS(0xC5E1D7FFu) /* -7226.9995 */);
    actor_pair_angles(obj, F32_BITS(0x46D7530Eu) /* 27561.527 */,
                      F32_BITS(0x46555555u) /* 13653.333: 75 degrees */,
                      F32_BITS(0x47271C71u) /* 42780.44: 235 degrees */, 6);
    damping = F32_BITS(0x3F599999u); /* 0.85 */
    break;
  default:
    break;
  }
  gaddr center = hw_stack_alloc(sizeof(Vec3));
  vec3_copy(center, member_addr(obj, ActorPair, position_b));
  vec3_sub(center, member_addr(obj, ActorPair, position_a));
  vec3_scale(F32(0.5f), center);
  vec3_add(center, member_addr(obj, ActorPair, position_a));
  memmove(&a->center, hw_ptr(center), sizeof(Vec3));
  hw_stack_free(sizeof(Vec3));
  a->turn_a.value = a->rest_angle;
  a->turn_a.damping = damping;
  a->angle_b.value = a->rest_angle;
  a->angle_b.damping = damping;
  a->sounded = 0;
}
HW_EXPORT(8c0460a6, actor_pair_setup(ARG0, ARG1))
