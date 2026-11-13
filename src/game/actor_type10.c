/* Pen Pen port, S18 b05: the scenery actor of placement type 0x10 (game/actor_b07.h: ActorType10; its
 * construction is in game/actor_types_b07.c): a box with two doors and an arm with a hand. It waits 120 steps, opens
 * its doors, reaches its arm out (and plays a sound to the first player standing in its cell), turns the hand back,
 * draws the arm in and closes; its contacts follow the arm's end. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"
#include "actor_b07.h"
#include "race_scene.h" /* race_scene_view_cell, race_scene_slot_insert */
#include "game.h"       /* panel_get */
#include "scenery.h"    /* race_scene_sound */

/* func_8C02EE6A (game/race_sound_ports.c, not in a header): stop the one-shot on player i's channel */
static void race_channel_sound_stop(gaddr scene, int32_t player, int32_t channel) { CALL3(0x8C02EE6Au, scene, player, channel); }
/* func_8C062848: the C runtime's acosf (guest code: the float in FR4, the result in FR0) */
static f32 rt_acosf(f32 x) { FARGS1(x); CALL0(0x8C062848u); return hw_fret(); }

#define TYPE10_EARLY_TIMER   (-180)                /* a placement with a param waits 180 steps more */
#define TYPE10_ARM_REST      F32_BITS(0x3F7FFFFFu) /* 0.99999994: the arm's length at rest */
#define TYPE10_ARM_DAMPING   F32_BITS(0x3F599999u) /* 0.85 */
#define TYPE10_DOOR_DAMPING  F32_BITS(0x3F333333u) /* 0.7 */
#define TYPE10_MODEL_DOOR_A  13                    /* the doors with a param (13, 14), else 15, 16 */
#define TYPE10_MODEL_DOOR_B  15
#define TYPE10_CONTACT_WORD  4                     /* the Contacts' word after it (field_reset_2c) */
#define TYPE10_CONTACT_1C    3
/* the states' lengths (the timer must pass them) */
#define TYPE10_WAIT          120
#define TYPE10_OPEN_STEPS    25
#define TYPE10_REACH_STEPS   55
#define TYPE10_TURN_STEPS    90
#define TYPE10_DRAW_STEPS    60
#define TYPE10_CLOSE_STEPS   30
#define TYPE10_DOORS_OPEN    F32_BITS(0x46AAAAAAu) /* 21845.33 (120 degrees) */
#define TYPE10_DOORS_RATE    F32_BITS(0x3D75C28Fu) /* 0.06 */
#define TYPE10_REACH         F32(40.0f)
#define TYPE10_REACH_PITCH   F32_BITS(0xC5638E38u) /* -3640.89 (-20 degrees) */
#define TYPE10_REACH_RATE    F32_BITS(0x3CA3D70Au) /* 0.02 */
#define TYPE10_TIP_TURN      F32_BITS(0x44638E38u) /* 910.22 (5 degrees) */
#define TYPE10_TIP_RATE      F32(0.5f)
#define TYPE10_DRAW_RATE     F32_BITS(0x3D23D70Au) /* 0.04 */
#define TYPE10_SOUND_BANK    8
#define TYPE10_SOUND_ID      0x2F
/* the draw: the doors, the arm's 16 links and its hand */
#define TYPE10_DOOR_X        F32(7.5f)
#define TYPE10_DOOR_Z        F32_BITS(0xC13B3332u) /* -11.7 */
#define TYPE10_ARM_SPAN      F32_BITS(0xC2899999u) /* -68.8: the arm's length at full reach */
#define TYPE10_ANGLE_UNITS   F32(65536.0f)
#define TYPE10_TWO_PI        F32_BITS(0x40C90FD7u) /* 6.2831836 */
#define TYPE10_HALF_TURN     F32(32768.0f)
#define TYPE10_LINKS         16
#define TYPE10_MODEL_LINK    10
#define TYPE10_MODEL_HAND    9
#define TYPE10_LINK_IN       F32_BITS(0x3F333332u) /* 0.7 */
#define TYPE10_LINK_OUT      F32_BITS(0xC0899999u) /* -4.3 */
#define TYPE10_HAND_OUT      F32_BITS(0xC03FFFFFu) /* -3.0 */

/* a Body1 at rest: value 0, no velocity or force, damping 1 (the unit's inline setters) */
static void body1_at_rest(Body1 *b) {
  b->value = F32(0.0f);
  b->velocity = F32(0.0f);
  b->force = F32(0.0f);
  b->damping = F32(1.0f);
}

/* was func_8C054976 (the init method: this, the placement): state 0, an early start with a param; home (and the
 * contacts' point) at the placement's position; the tip at 0, the arm at rest (damping 0.85), the doors shut
 * (damping 0.7), its heading the placement's; its two door models by the param; the contacts (mass 1, kind 3); the
 * matrix at home turned by the heading; nobody heard it */
void actor_type10_init(gaddr obj, gaddr placement) {
  ActorType10 *a = (ActorType10 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->state = 0;
  a->timer = p->param != 0 ? TYPE10_EARLY_TIMER : 0;
  a->home = p->position; /* through a local and the SHC copy helper 0x8C0625EC */
  a->point = a->home;
  body1_at_rest(&a->value58);
  a->value58.value = F32(0.0f);
  a->value58.damping = F32(1.0f);
  a->heading = p->angle.y;
  body1_at_rest(&a->value6c);
  a->value6c.value = TYPE10_ARM_REST;
  a->value6c.damping = TYPE10_ARM_DAMPING;
  body1_at_rest(&a->value30);
  a->value30.value = F32(0.0f);
  a->value30.damping = TYPE10_DOOR_DAMPING;
  a->yaw = F32(0.0f);
  a->pitch = F32(0.0f);
  if (p->param != 0) {
    a->door_left = actor_model(&a->base, TYPE10_MODEL_DOOR_A);
    a->door_right = actor_model(&a->base, TYPE10_MODEL_DOOR_A + 1);
  } else {
    a->door_left = actor_model(&a->base, TYPE10_MODEL_DOOR_B);
    a->door_right = actor_model(&a->base, TYPE10_MODEL_DOOR_B + 1);
  }
  Contacts *c = &a->contacts;
  c->count = 0; /* field_reset_2c's copy: the count and the word after the Contacts */
  a->contacts_word = TYPE10_CONTACT_WORD;
  c->mass = F32(1.0f);
  c->unknown18 = F32(1.0f);
  c->unknown1c = TYPE10_CONTACT_1C;
  gaddr matrix = member_addr(obj, ActorType10, matrix);
  njUnitMatrix(matrix);
  njTranslateV(matrix, member_addr(obj, ActorType10, home));
  njRotateY(matrix, f32_to_int(a->heading));
  a->heard_by = 0;
}
HW_EXPORT(8c054976, actor_type10_init(ARG0, ARG1))

/* the original's frame of the update (0x54 bytes under r8 and PR): its matrix and vector go to Ninja */
typedef struct Type10Frame {
  Vec3 arm;          /* 0x00: (0, 0, -length) */
  Matrix matrix;     /* 0x0C (the loop of type10_call_player keeps its player in its slot 0x48) */
  int32_t moved;     /* 0x4C: the arm moved: the contacts follow */
  gaddr self;        /* 0x50 */
  uint32_t saved[2]; /* 0x54: PR, r8 */
} Type10Frame;
HW_OFFSET(Type10Frame, matrix, 0x0C);
HW_OFFSET(Type10Frame, moved, 0x4C);
HW_SIZE(Type10Frame, 0x5C);

/* the timer counted; past `steps` it restarts and the next state comes (returns 1 then) */
static int32_t type10_count(ActorType10 *a, int32_t steps) {
  a->timer += 1;
  if (a->timer <= steps) return 0;
  a->timer = 0;
  return 1;
}

/* the arm out: the first player standing in the actor's cell hears it (and is remembered) */
static void type10_call_player(ActorType10 *a) {
  for (int32_t j = 0; j < (int32_t)a->base.word0c; j++) {
    int32_t cell = a->base.cell;
    int32_t near = race_scene_view_cell(panel_get(), j);
    if (fgt(f32_abs(f32_from_int(cell - near)), F32(0.0f))) continue;
    race_scene_sound(panel_get(), j, 0, TYPE10_SOUND_BANK, TYPE10_SOUND_ID);
    a->heard_by = j + 1;
    return;
  }
}

/* was func_8C054B0A: the update method: one step of the state; while the arm moves (states 2 and 3) the contacts'
 * point is put at its end (the matrix turned by yaw and pitch, the arm's length along -z) and the contacts go into a
 * new slot of the race scene */
void actor_type10_update(gaddr obj) {
  ActorType10 *a = (ActorType10 *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(Type10Frame));
  Type10Frame *f = (Type10Frame *)hw_ptr(frame);
  f->self = obj;
  f->moved = 0;
  switch (a->state) {
  case 0: /* waiting */
    if (type10_count(a, TYPE10_WAIT)) a->state += 1;
    break;
  case 1: /* the doors open */
    body1_pull_angle(TYPE10_DOORS_OPEN, TYPE10_DOORS_RATE, member_addr(obj, ActorType10, value30));
    body1_step(member_addr(obj, ActorType10, value30));
    if (type10_count(a, TYPE10_OPEN_STEPS)) {
      a->state += 1;
      a->reach = TYPE10_REACH;
      a->yaw = F32(0.0f);
      a->pitch = TYPE10_REACH_PITCH;
    }
    break;
  case 2: /* the arm reaches out */
    body1_pull(a->reach, TYPE10_REACH_RATE, member_addr(obj, ActorType10, value6c));
    body1_step(member_addr(obj, ActorType10, value6c));
    if (type10_count(a, TYPE10_REACH_STEPS)) {
      a->state += 1;
      a->value58.value = TYPE10_TIP_TURN;
      type10_call_player(a);
    }
    f->moved = 1;
    break;
  case 3: /* the tip turns back */
    body1_pull_angle(F32(0.0f), TYPE10_TIP_RATE, member_addr(obj, ActorType10, value58));
    body1_step(member_addr(obj, ActorType10, value58));
    if (type10_count(a, TYPE10_TURN_STEPS)) {
      a->value58.value = F32(0.0f);
      a->state += 1;
    }
    f->moved = 1;
    break;
  case 4: /* the arm is drawn in */
    body1_pull(TYPE10_ARM_REST, TYPE10_DRAW_RATE, member_addr(obj, ActorType10, value6c));
    body1_step(member_addr(obj, ActorType10, value6c));
    if (type10_count(a, TYPE10_DRAW_STEPS)) {
      a->state += 1;
      a->value6c.value = TYPE10_ARM_REST;
      if (a->heard_by != 0) race_channel_sound_stop(panel_get(), a->heard_by - 1, 0);
    }
    break;
  case 5: /* the doors close */
    body1_pull_angle(F32(0.0f), TYPE10_DOORS_RATE, member_addr(obj, ActorType10, value30));
    body1_step(member_addr(obj, ActorType10, value30));
    if (type10_count(a, TYPE10_CLOSE_STEPS)) {
      a->state += 1;
      a->value30.value = F32(0.0f);
    }
    break;
  case 6:
    a->state = 0;
    break;
  default:
    break;
  }
  if (f->moved != 0) {
    gaddr matrix = member_addr(frame, Type10Frame, matrix);
    gaddr arm = member_addr(frame, Type10Frame, arm);
    matrix_assign(matrix, member_addr(obj, ActorType10, matrix));
    njRotateY(matrix, f32_to_int(a->yaw));
    njRotateX(matrix, f32_to_int(a->pitch));
    vec3_set(F32(0.0f), F32(0.0f), fneg(a->value6c.value), arm); /* the unit's inline getter (func_8C054716) */
    njCalcPoint(matrix, arm, member_addr(obj, ActorType10, point));
    gaddr contacts = member_addr(obj, ActorType10, contacts);
    race_scene_slot_insert(panel_get(), contacts, -1);
  }
  hw_stack_free(sizeof(Type10Frame));
}
HW_EXPORT(8c054b0a, actor_type10_update(ARG0))

/* a door: at (x, 0, -11.7), turned about y */
static void type10_draw_door(f32 x, f32 swing, gaddr model) {
  matrix_push();
  matrix_translate(x, F32(0.0f), TYPE10_DOOR_Z);
  matrix_rotate_y(swing);
  nj_ModelL1(model);
  matrix_pop();
}

/* was func_8C054EA0: the draw method: at home turned by the heading: the doors swung out by value30 (the left one
 * one way, the right the other); the arm turned by yaw and pitch, folded by the angle whose cosine is its length
 * over -68.8: 16 links zig-zagging (each turned by twice the fold, alternately back), then the hand turned by the
 * tip's turn */
void actor_type10_draw(gaddr obj) {
  const ActorType10 *a = (const ActorType10 *)hw_ptr(obj);
  matrix_push();
  matrix_translate_v(member_addr(obj, ActorType10, home));
  matrix_rotate_y(a->heading);
  type10_draw_door(fneg(TYPE10_DOOR_X), a->value30.value, a->door_left); /* the unit's getters (func_8C05474C) */
  type10_draw_door(TYPE10_DOOR_X, fneg(a->value30.value), a->door_right);
  f32 fold = rt_acosf(fdiv(a->value6c.value, TYPE10_ARM_SPAN));
  fold = f32_from_int(f32_to_int(fdiv(fmul(fold, TYPE10_ANGLE_UNITS), TYPE10_TWO_PI)));
  fold = fsub(TYPE10_HALF_TURN, fold);
  matrix_rotate_y(a->yaw);
  matrix_rotate_x(a->pitch);
  for (int32_t i = 0; i < TYPE10_LINKS; i++) {
    f32 turn = fold;
    if (i & 1) turn = fneg(turn);
    if (i != 0) turn = fmul(turn, fadd(F32(1.0f), F32(1.0f)));
    matrix_rotate_x(turn);
    matrix_translate(F32(0.0f), F32(0.0f), TYPE10_LINK_IN);
    nj_ModelL1(actor_model(&a->base, TYPE10_MODEL_LINK));
    matrix_translate(F32(0.0f), F32(0.0f), TYPE10_LINK_OUT);
  }
  matrix_rotate_x(fold);
  matrix_translate(F32(0.0f), F32(0.0f), TYPE10_HAND_OUT);
  matrix_rotate_x(a->value58.value);
  nj_ModelL1(actor_model(&a->base, TYPE10_MODEL_HAND));
  matrix_pop();
}
HW_EXPORT(8c054ea0, actor_type10_draw(ARG0))
