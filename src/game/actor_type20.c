/* Pen Pen port, S15 b09: the update of the actor class of placement type 0x20 (ActorType20, game/actor.h; its
 * constructors and set-up are in game/actor_types.c). The actor sways: two phases advance at their rates, two Body1s
 * (a tilt and a turn) are pulled toward them, and the body is pulled toward its start plus an offset of `radius`
 * turned by them. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "game.h"
#include "actor.h"

#define TYPE20_BODY_DAMPING F32(0.5f)
#define TYPE20_BODY_PULL    F32_BITS(0x3D4CCCCCu) /* 0.049999997 */
#define TYPE20_FRAMES       16                    /* the animation frame wraps at 16 */

/* the original's stack frame (0x50 bytes under the saved PR): the offset vector and the matrix are handed to
 * Ninja, so they live on the guest stack where the original keeps them */
typedef struct SwayFrame {
  Vec3 offset;     /* 0x00 */
  Matrix matrix;   /* 0x0C */
  gaddr self;      /* 0x4C: (the original's saved this) */
  uint32_t pr;     /* 0x50: (the saved PR) */
} SwayFrame;
HW_OFFSET(SwayFrame, matrix, 0x0C);
HW_SIZE(SwayFrame, 0x54);

/* was func_8C061C68: the sway: both phases advance by their rates; the tilt and the turn (Body1s, damped by
 * damping94) are pulled toward them (as angles) and stepped; the target is the start plus (0, 0, radius) turned by
 * the turn (Y) and then the tilt (X); the body, damped by half, is pulled toward it and stepped */
void actor_type20_sway(gaddr obj) {
  ActorType20 *a = (ActorType20 *)hw_ptr(obj);
  gaddr tilt = member_addr(obj, ActorType20, c4);
  gaddr turn = member_addr(obj, ActorType20, d4);
  gaddr body = member_addr(obj, ActorType20, body);

  a->phase_a = fadd(a->phase_a, a->rate_a);
  a->phase_b = fadd(a->phase_b, a->rate_b);

  a->c4.damping = a->damping94;
  body1_pull_angle(a->phase_a, a->pull, tilt);
  body1_step(tilt);
  a->d4.damping = a->damping94;
  body1_pull_angle(a->phase_b, a->pull, turn);
  body1_step(turn);

  gaddr frame = hw_stack_alloc(sizeof(SwayFrame));
  gaddr matrix = member_addr(frame, SwayFrame, matrix);
  gaddr offset = member_addr(frame, SwayFrame, offset);
  matrix_identity(matrix); /* (the Matrix constructor) */
  njUnitMatrix(matrix);
  /* (the Vec3 constructor of `offset` does nothing) */
  vec3_set(F32(0.0f), F32(0.0f), a->radius, offset);
  njTranslateV(matrix, member_addr(obj, ActorType20, start));
  njRotateY(matrix, f32_to_int(a->d4.value));
  njRotateX(matrix, f32_to_int(a->c4.value));
  njCalcPoint(matrix, offset, member_addr(obj, ActorType20, target));
  hw_stack_free(sizeof(SwayFrame));

  vec3_fill(member_addr(body, Body, damping), TYPE20_BODY_DAMPING);
  body_pull(TYPE20_BODY_PULL, body, member_addr(obj, ActorType20, target));
  body_step(body);
}
HW_EXPORT(8c061c68, actor_type20_sway(ARG0))

/* was func_8C061B90: the update: the next animation frame (wrapping at 16), the sway, then home = the body's
 * position */
void actor_type20_update(gaddr obj) {
  ActorType20 *a = (ActorType20 *)hw_ptr(obj);
  a->frame += 1;
  if (a->frame >= TYPE20_FRAMES) a->frame -= TYPE20_FRAMES;
  actor_type20_sway(obj);
  a->home.x = a->body.position.x;
  a->home.y = a->body.position.y;
  a->home.z = a->body.position.z;
}
HW_EXPORT(8c061b90, actor_type20_update(ARG0))
