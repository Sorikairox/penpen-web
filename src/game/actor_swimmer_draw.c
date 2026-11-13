/* Pen Pen port, S18 b05: the draw of the swimmer (type 0x21) (game/actor.h: Swimmer; its update and setup are in
 * game/ files of S14/S15). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"

/* SDK njColorBlendingMode (0x8C0A23A8) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

#define SWIMMER_STILL_ROLL   F32(8192.0f)          /* 45 degrees about z when it stays where it is (still == 1) */
#define SWIMMER_TURN_AROUND  F32(32768.0f)         /* its model faces back: heading + 180 degrees */
#define SWIMMER_BIG_BODY     37                    /* kind 0's models: the body, and a fin on each side */
#define SWIMMER_BIG_FIN      38
#define SWIMMER_FIN_X        F32_BITS(0x418D47AEu) /* 17.66: the fins' offsets (x: -, then +) */
#define SWIMMER_FIN_Y        F32_BITS(0xC15DC28Fu) /* -13.86 */
#define SWIMMER_FIN_Z        F32_BITS(0x4235C28Fu) /* 45.44 */
#define SWIMMER_SMALL_A      34                    /* kind 1's models: two still, a tail that wags */
#define SWIMMER_SMALL_B      35
#define SWIMMER_SMALL_TAIL   36
#define SWIMMER_TAIL_Y       F32_BITS(0xC0228F5Cu) /* -2.54 */
#define SWIMMER_TAIL_Z       F32_BITS(0x40DC7AE1u) /* 6.89 */
#define BUBBLE_TEXLIST       0x8C09703Cu           /* the bubbles' NJS_TEXLIST (a static of the binary) */
#define BUBBLE_TEXANIM       0x8C097044u           /* its NJS_TEXANIM */
#define BUBBLE_CELL          3
#define BUBBLE_ATTR          0x30u                 /* njDrawSprite3D's attribute */

/* the original's frame (0x40 bytes under PR): the fins' offsets and the bubbles' sprite go to Ninja */
typedef struct SwimmerDrawFrame {
  Vec3 fin_right;    /* 0x00 */
  Vec3 fin_left;     /* 0x0C */
  int32_t i;         /* 0x18 */
  NjSprite bubble;   /* 0x1C */
  gaddr self;        /* 0x3C */
  uint32_t saved_pr; /* 0x40 */
} SwimmerDrawFrame;
HW_OFFSET(SwimmerDrawFrame, fin_left, 0x0C);
HW_OFFSET(SwimmerDrawFrame, bubble, 0x1C);
HW_OFFSET(SwimmerDrawFrame, self, 0x3C);
HW_SIZE(SwimmerDrawFrame, 0x44);

/* a fin of the big one: at its offset, turned about z by the wag */
static void swimmer_draw_fin(const Swimmer *s, gaddr offset, f32 x) {
  vec3_set(x, SWIMMER_FIN_Y, SWIMMER_FIN_Z, offset);
  matrix_translate_v(offset);
  matrix_rotate_z(s->wag);
  nj_ModelL1(actor_model(&s->base, SWIMMER_BIG_FIN));
}

/* was func_8C057160: the draw method: at its Body's position (rolled 45 degrees when still is 1), turned about y by
 * its heading + 180 degrees: kind 0 its body and two fins wagging, kind 1 two models and a wagging tail (other kinds
 * nothing); then its 8 bubbles as sprites (blending modes 8 / 6) */
void swimmer_draw(gaddr obj) {
  const Swimmer *s = (const Swimmer *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(SwimmerDrawFrame));
  SwimmerDrawFrame *f = (SwimmerDrawFrame *)hw_ptr(frame);
  f->self = obj;
  matrix_push();
  matrix_translate_v(method_self(member_addr(obj, Swimmer, body)));
  if (s->still == 1) matrix_rotate_z(SWIMMER_STILL_ROLL);
  matrix_rotate_y(fadd(s->heading.value, SWIMMER_TURN_AROUND)); /* the unit's inline getter (func_8C05667A) */
  switch (s->kind) {
  case 0:
    nj_ModelL1(actor_model(&s->base, SWIMMER_BIG_BODY));
    matrix_push();
    swimmer_draw_fin(s, member_addr(frame, SwimmerDrawFrame, fin_left), fneg(SWIMMER_FIN_X));
    matrix_pop();
    swimmer_draw_fin(s, member_addr(frame, SwimmerDrawFrame, fin_right), SWIMMER_FIN_X);
    break;
  case 1:
    nj_ModelL1(actor_model(&s->base, SWIMMER_SMALL_A));
    nj_ModelL1(actor_model(&s->base, SWIMMER_SMALL_B));
    matrix_translate(F32(0.0f), SWIMMER_TAIL_Y, SWIMMER_TAIL_Z);
    matrix_rotate_z(s->wag);
    nj_ModelL1(actor_model(&s->base, SWIMMER_SMALL_TAIL));
    break;
  default:
    break;
  }
  matrix_pop();
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  NjSprite *sp = &f->bubble;
  sp->tlist = BUBBLE_TEXLIST;
  sp->tanim = BUBBLE_TEXANIM;
  sp->ang = 0;
  sp->sx = F32(1.0f);
  sp->sy = F32(1.0f);
  for (f->i = 0; f->i < SWIMMER_BUBBLES; f->i++) {
    const Body *b = &s->bubbles[f->i];
    sp->p.x = b->position.x; /* the unit's inline getters (func_8C0566AA, ...) */
    sp->p.y = b->position.y;
    sp->p.z = b->position.z;
    njDrawSprite3D(member_addr(frame, SwimmerDrawFrame, bubble), BUBBLE_CELL, BUBBLE_ATTR);
  }
  hw_stack_free(sizeof(SwimmerDrawFrame));
}
HW_EXPORT(8c057160, swimmer_draw(ARG0))
