/* Pen Pen port, S18 b05: the scenery actor of placement type 0x31 (game/actor_b08.h: ActorType31; its
 * construction, draw, item reset and two phase starts are in game/actor_types_b08.c): a creature with a body of 26
 * items whose vertices it rebuilds every step. Kind 0 wanders on a circle about its home, turning; other kinds wait
 * for a player within 40, then hold that player (its stick damped) on the course cells 206-212, until the player is
 * touched by another (handed on) or gets away (released). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/file.h" /* ContainerEntry, NjObject */
#include "actor.h"
#include "actor_b08.h"
#include "race_scene.h" /* race_setup_get, RaceScene */
#include "game.h"       /* panel_get */
#include "scenery.h"    /* random_scaled, random_timer_scaled, race_player_body */

/* func_8C0628E8 (C runtime, float): atan2f(y, x) (as guest code) */
static f32 rt_atan2f(f32 y, f32 x) {
  FARGS2(y, x);
  CALL0(0x8C0628E8u);
  return hw_fret();
}
/* game/race_scene_players.c (not in a header yet) */
static int32_t player_stick_damped(gaddr scene, int32_t i) { return (int32_t)CALL2(0x8C02C888u, scene, i); }
static void player_set_stick_damped(gaddr scene, int32_t i, int32_t v) { CALL3(0x8C02C8B8u, scene, i, v); }
static int32_t player_had_contacts(gaddr scene, int32_t i) { return (int32_t)CALL2(0x8C02C90Au, scene, i); }

/* statics of the binary the unit reads */
#define g_type31_vlist_words GUEST_VAR(int32_t, 0x8C08A1BCu) /* 107: the vertex chunk's words */
#define g_type31_plist_halfs GUEST_VAR(int32_t, 0x8C08A1C0u) /* 105: the polygon chunk's halfwords */
#define g_type31_item_rate   GUEST_VAR(f32, 0x8C08A1C8u)     /* 0.2: an item's pull toward its place */
#define g_type31_item_turn   GUEST_VAR(f32, 0x8C08A1CCu)     /* 0.2: its angles' pull */
#define g_type31_item_offset ((const Vec3 *)hw_ptr(0x8C08A1D8u)) /* Vec3[26]: each item's place */

#define TYPE31_DAMPING       F32_BITS(0x3F4CCCCCu) /* 0.8 */
#define TYPE31_KIND_SHIFT    256                   /* the kind: param bits 8-11 */
#define TYPE31_WANDER_RATE   F32_BITS(0x435A740Cu) /* 218.45 (1.2 degrees): kind 0's turn about y, either way */
#define TYPE31_WANDER_SPIN   F32_BITS(0x4211A2B3u) /* 36.41 (0.2 degrees): kind 0's random spin about x */
#define TYPE31_WANDER_SPIN0  F32_BITS(0x42B60B60u) /* 91.02 (0.5 degrees) */
#define TYPE31_WAIT_RATE     F32_BITS(0x43B60B60u) /* 364.09 (2 degrees): the others' turn */
#define TYPE31_WAIT_SPIN     F32_BITS(0x4191A2B3u) /* 18.2 (0.1 degree) */
#define TYPE31_TURN_RATE     F32_BITS(0x3D4CCCCCu) /* 0.05 */
#define TYPE31_RADIUS_WANDER F32(15.0f)
#define TYPE31_RADIUS_WAIT   F32(20.0f)
#define TYPE31_FULL_TURN     F32(65536.0f)
#define TYPE31_CENTER_X      F32_BITS(0x3CBE9856u) /* 0.023266: the model's bounding sphere */
#define TYPE31_CENTER_Y      F32_BITS(0x3D9C6759u) /* 0.076369 */
#define TYPE31_CENTER_Z      F32_BITS(0xBB22EC28u) /* -0.002486 */
#define TYPE31_MODEL_RADIUS  F32_BITS(0x411D7403u) /* 9.840823 */
#define TYPE31_WANDER_WAIT   F32(180.0f)
#define TYPE31_PULL          F32_BITS(0x3D4CCCCCu) /* 0.05: the Body's pull toward its point */
#define TYPE31_WANDER_DAMP   F32(0.5f)
#define TYPE31_FACE_RATE     F32_BITS(0x3CF5C28Fu) /* 0.03 */
#define TYPE31_ANGLE_UNITS   F32(65536.0f)
#define TYPE31_TWO_PI        F32_BITS(0x40C90FD7u) /* 6.2831836 */
#define TYPE31_HOLD_CELL_LO  206                   /* the course cells it holds a player on */
#define TYPE31_HOLD_CELL_HI  212
#define TYPE31_HOLD_TIME     480
#define TYPE31_CHUNK_END     0xFFu                 /* NJD_CE: the vertex chunk's end */

static ActorType31 *type31_at(gaddr obj) { return (ActorType31 *)hw_ptr(obj); }
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }
static gaddr item_body(const ActorType31 *a, int32_t k) { return a->bodies + (uint32_t)k * sizeof(Body); }
static gaddr item_vec(const ActorType31 *a, int32_t k) { return a->vecs + (uint32_t)k * sizeof(Vec3); }
static gaddr item_turn(gaddr block, int32_t k) { return block + (uint32_t)k * sizeof(Body1); }

/* the Body's damping set on all three axes (the unit's inline setter at 0x8C05EC5A: z, y, x) */
static void body_damping_fill(gaddr body, f32 d) {
  Body *b = body_at(body);
  b->damping.z = d;
  b->damping.y = d;
  b->damping.x = d;
}

/* a Body1 at rest (the inline Quat constructor), then its value and damping */
static void type31_body1_set(Body1 *b, f32 value, f32 damping) {
  quat_identity(hw_addr(b));
  b->value = value;
  b->damping = damping;
}

/* was func_8C05F0C8 (the init method: this, the placement): its kind and id from the param; its object (by kind) and
 * texlist; its Body at rest at the placement's position (damping 0.8), that position also v68 and v164; its angles
 * the placement's (damping 0.8); facing nobody, its scene and player count; kind 0 wanders (radius 15, turning 1.2
 * degrees a step one way or the other at random), the others wait (radius 20, 2 degrees); random angles; the
 * object's model's chunks copied into its own model; the items reset (a tail call) */
void actor_type31_init(gaddr obj, gaddr placement) {
  ActorType31 *a = type31_at(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->kind = (int32_t)(p->param & 0xF00) / TYPE31_KIND_SHIFT;
  a->id = (int32_t)(p->param & 0xF);
  a->object = ((const gaddr *)hw_ptr(entry_data(a->base.model_set)))[a->kind];
  a->texlist = entry_data(a->base.texlist);
  gaddr body = member_addr(obj, ActorType31, body);
  body_reset(body);
  vec3_set(p->position.x, p->position.y, p->position.z, body);
  body_damping_fill(body, TYPE31_DAMPING);
  memmove(&a->v68, hw_ptr(method_self(body)), sizeof(Vec3));
  type31_body1_set(&a->b74, p->angle.x, TYPE31_DAMPING);
  type31_body1_set(&a->b84, p->angle.y, TYPE31_DAMPING);
  type31_body1_set(&a->b94, p->angle.z, TYPE31_DAMPING);
  a->yaw = F32(0.0f);
  a->player = 0;
  a->distance = F32(0.0f);
  a->timer = 0;
  a->players = 0;
  vec3_set(F32(0.0f), F32(0.0f), F32(1.0f), member_addr(obj, ActorType31, to_player));
  a->scene = panel_get();
  a->players = race_setup_at(race_setup_get())->player_count;
  int32_t clockwise = f32_to_int(random_timer_scaled(F32(1.0f)));
  f32 (*pick)(f32); /* the angles' random function: kind 0's is random_timer_scaled */
  f32 radius;
  if (a->kind == 0) {
    a->state = 0;
    a->rate_b = clockwise != 0 ? TYPE31_WANDER_RATE : fneg(TYPE31_WANDER_RATE);
    a->rate_a = fadd(random_timer_scaled(TYPE31_WANDER_SPIN), TYPE31_WANDER_SPIN0);
    radius = TYPE31_RADIUS_WANDER;
    pick = random_timer_scaled;
  } else {
    a->state = 10;
    a->rate_b = clockwise != 0 ? TYPE31_WAIT_RATE : fneg(TYPE31_WAIT_RATE);
    a->rate_a = fadd(random_scaled(TYPE31_WAIT_SPIN), TYPE31_WANDER_SPIN);
    radius = TYPE31_RADIUS_WAIT;
    pick = random_scaled;
  }
  a->turn_rate = TYPE31_TURN_RATE;
  a->turn_damping = TYPE31_DAMPING;
  quat_identity(member_addr(obj, ActorType31, q17c));
  quat_identity(member_addr(obj, ActorType31, q18c));
  memmove(&a->v164, &a->v68, sizeof(Vec3));
  a->f150 = radius;
  a->angle_a = pick(TYPE31_FULL_TURN);
  a->angle_b = pick(TYPE31_FULL_TURN);
  const NjObject *object = (const NjObject *)hw_ptr(a->object);
  const gaddr *model = (const gaddr *)hw_ptr(object->model); /* NJS_CNK_MODEL: vlist, plist */
  const uint32_t *vsrc = (const uint32_t *)hw_ptr(model[0]);
  uint32_t *vdst = (uint32_t *)hw_ptr(a->block_dc);
  for (int32_t i = 0; i < g_type31_vlist_words; i++) *vdst++ = *vsrc++;
  const int16_t *psrc = (const int16_t *)hw_ptr(model[1]);
  int16_t *pdst = (int16_t *)hw_ptr(a->block_e0);
  for (int32_t i = 0; i < g_type31_plist_halfs; i++) *pdst++ = *psrc++;
  a->model_center.x = TYPE31_CENTER_X;
  a->model_center.y = TYPE31_CENTER_Y;
  a->model_center.z = TYPE31_CENTER_Z;
  a->model_radius = TYPE31_MODEL_RADIUS;
  actor_type31_reset_items(obj);
}
HW_EXPORT(8c05f0c8, actor_type31_init(ARG0, ARG1))

/* was func_8C05F642: turn toward its player: when not holding one (held 0) the nearest player is taken (its
 * direction and distance kept), else the held player's offset from the Body; its heading (whole Ninja units) pulls
 * b84 (0.03), which steps */
void actor_type31_face_player(gaddr obj, int32_t held) {
  ActorType31 *a = type31_at(obj);
  gaddr to_player = member_addr(obj, ActorType31, to_player);
  if (held == 0) {
    a->player = race_scene_nearest_player(a->scene, method_self(member_addr(obj, ActorType31, body)), to_player,
                                          member_addr(obj, ActorType31, distance));
  } else {
    memmove(hw_ptr(to_player), hw_ptr(race_player_body(a->scene, a->player)), sizeof(Vec3));
    vec3_sub(to_player, method_self(member_addr(obj, ActorType31, body)));
  }
  f32 angle = rt_atan2f(a->to_player.x, a->to_player.z);
  a->yaw = f32_from_int(f32_to_int(fdiv(fmul(angle, TYPE31_ANGLE_UNITS), TYPE31_TWO_PI)));
  body1_pull_angle(a->yaw, TYPE31_FACE_RATE, member_addr(obj, ActorType31, b84));
  body1_step(member_addr(obj, ActorType31, b84));
}
/* S19: no racer is within 10000 of CSD's type 0x31 actors for most of the race, so race_scene_nearest_player leaves
 * to_player as its never-written "best" (a fresh stack frame: zero, sh4_ops.h) and atan2f(0, 0) sets errno to EDOM;
 * S18's differences here came from that stack word, which held other garbage on each build */
HW_EXPORT(8c05f642, actor_type31_face_player(ARG0, (int32_t)ARG1))

/* the original's frame of actor_type31_wander (0x4C bytes under r14, fr15 and PR): its matrix and vector go to
 * Ninja */
typedef struct WanderFrame {
  Vec3 offset;       /* 0x00: (0, 0, radius) */
  Matrix matrix;     /* 0x0C */
  uint32_t saved[3]; /* 0x4C: PR, fr15, r14 */
} WanderFrame;
HW_OFFSET(WanderFrame, matrix, 0x0C);
HW_SIZE(WanderFrame, 0x58);

/* was func_8C05F550: one step on its circle: angle_a and angle_b advanced by their rates and followed by q17c and
 * q18c; the point v170 at the radius from v164 turned by them (y, then x); the Body (damping 0.5) pulled toward it
 * and stepped; its course cell found */
void actor_type31_wander(gaddr obj) {
  ActorType31 *a = type31_at(obj);
  gaddr frame = hw_stack_alloc(sizeof(WanderFrame));
  gaddr matrix = member_addr(frame, WanderFrame, matrix);
  gaddr offset = member_addr(frame, WanderFrame, offset);
  a->angle_a = fadd(a->angle_a, a->rate_a);
  a->angle_b = fadd(a->angle_b, a->rate_b);
  a->q17c.damping = a->turn_damping;
  body1_pull_angle(a->angle_a, a->turn_rate, member_addr(obj, ActorType31, q17c));
  body1_step(member_addr(obj, ActorType31, q17c));
  a->q18c.damping = a->turn_damping;
  body1_pull_angle(a->angle_b, a->turn_rate, member_addr(obj, ActorType31, q18c));
  body1_step(member_addr(obj, ActorType31, q18c));
  matrix_identity(matrix); /* the matrix's constructor */
  njUnitMatrix(matrix);
  method_self(offset); /* the Vec3's (empty) constructor */
  vec3_set(F32(0.0f), F32(0.0f), a->f150, offset);
  njTranslateV(matrix, member_addr(obj, ActorType31, v164));
  njRotateY(matrix, f32_to_int(a->q18c.value));
  njRotateX(matrix, f32_to_int(a->q17c.value));
  njCalcPoint(matrix, offset, member_addr(obj, ActorType31, v170));
  gaddr body = member_addr(obj, ActorType31, body);
  body_damping_fill(body, TYPE31_WANDER_DAMP);
  body_pull(TYPE31_PULL, body, member_addr(obj, ActorType31, v170));
  body_step(body);
  actor_set_cell(obj, method_self(body), (int32_t)a->base.cell);
  hw_stack_free(sizeof(WanderFrame));
}
HW_EXPORT(8c05f550, actor_type31_wander(ARG0))

/* the original's frame of actor_type31_hold (0x1C bytes under PR): the player's position passes through it */
typedef struct HoldFrame {
  uint32_t words[3]; /* 0x00 */
  Vec3 at;           /* 0x0C */
  gaddr self;        /* 0x18 */
  uint32_t saved_pr; /* 0x1C */
} HoldFrame;
HW_OFFSET(HoldFrame, at, 0x0C);
HW_SIZE(HoldFrame, 0x20);

/* was func_8C05F7B0: hold its player for a step (returns 1), or (the player touched another: 0) pass to that one
 * when its stick is free (phase 11 again for 480 steps, the first let go), else let go. Holding: its wander centre
 * (v164) put at the player's position, its cell found from its Body, and the player's stick damped with id + 1 */
int32_t actor_type31_hold(gaddr obj) {
  ActorType31 *a = type31_at(obj);
  if (player_had_contacts(a->scene, a->player) != 0) {
    const RaceScene *rs = race_scene_at(a->scene);
    int32_t other = (&rs->player_pairs[0][0])[a->player * 2]; /* func_8C05ED74: the player it touched */
    if (player_stick_damped(a->scene, other) == 0) {
      a->state = 11;
      a->timer = TYPE31_HOLD_TIME;
      player_set_stick_damped(a->scene, a->player, 0);
      a->player = other;
    } else {
      actor_type31_start_release(obj);
    }
    return 0;
  }
  gaddr frame = hw_stack_alloc(sizeof(HoldFrame));
  gaddr at = member_addr(frame, HoldFrame, at);
  vec3_copy(at, race_player_body(a->scene, a->player)); /* func_8C05EA1C */
  /* S21: the player's position goes to v164 (the point its wander circles: the original's block copy targets
     this + 0x164), not to the Body (S18 wrote 0xA4: a synthetic sample with a held player caught it) */
  memmove(&a->v164, hw_ptr(at), sizeof(Vec3));
  int32_t cell = (int32_t)a->base.cell;
  actor_set_cell(obj, method_self(member_addr(obj, ActorType31, body)), cell);
  player_set_stick_damped(a->scene, a->player, a->id + 1);
  hw_stack_free(sizeof(HoldFrame));
  return 1;
}
HW_EXPORT(8c05f7b0, RET(actor_type31_hold(ARG0)))

/* was func_8C05F380: the update method: by its state (0 wander: a random wait and random angles; 1 face the nearest
 * and wander until the wait ends; 10 face, wander and wait for a player within 40; 11 hold the player while it is
 * on cells 206-212 and its stick is free or its own, for the hold time; 12 face, wander, back to 10 after the
 * release time); then its angles step and its items are rebuilt */
void actor_type31_update(gaddr obj) {
  ActorType31 *a = type31_at(obj);
  switch (a->state) {
  case 0:
    actor_type31_face_player(obj, 0);
    a->angle_a = random_timer_scaled(TYPE31_FULL_TURN);
    a->angle_b = random_timer_scaled(TYPE31_FULL_TURN);
    a->timer = f32_to_int(random_timer_scaled(TYPE31_WANDER_WAIT));
    a->state += 1;
    break;
  case 1:
    actor_type31_face_player(obj, 0);
    a->timer -= 1;
    if (a->timer < 0) {
      a->timer = 0;
      a->state = 0;
    }
    actor_type31_wander(obj);
    break;
  case 10:
    actor_type31_face_player(obj, 0);
    actor_type31_wander(obj);
    actor_type31_start_near(obj);
    break;
  case 11:
    actor_type31_face_player(obj, 1);
    actor_type31_wander(obj);
    if (a->base.cell >= TYPE31_HOLD_CELL_LO && !(a->base.cell > TYPE31_HOLD_CELL_HI) &&
        (player_stick_damped(a->scene, a->player) == 0 || player_stick_damped(a->scene, a->player) == a->id + 1)) {
      a->timer -= 1;
      if (a->timer > 0) {
        actor_type31_hold(obj);
        break;
      }
    }
    actor_type31_start_release(obj);
    break;
  case 12:
    actor_type31_face_player(obj, 0);
    actor_type31_wander(obj);
    a->timer -= 1;
    if (a->timer < 0) a->state = 10;
    break;
  default: /* 2, 3 and others */
    break;
  }
  body1_step(member_addr(obj, ActorType31, b74));
  body1_step(member_addr(obj, ActorType31, b84));
  body1_step(member_addr(obj, ActorType31, b94));
  actor_type31_step_items(obj);
}
HW_EXPORT(8c05f380, actor_type31_update(ARG0))

/* the original's frame of actor_type31_place_item (0x54 bytes under seven saved registers and PR): its matrix and
 * vector go to Ninja */
typedef struct ItemFrame {
  uint32_t body_offset; /* 0x00 */
  gaddr offset_z;       /* 0x04 */
  Vec3 offset;          /* 0x08: the item's place */
  Matrix matrix;        /* 0x14: the item's own turn (items 10 and on) */
  uint32_t saved[8];    /* 0x54: PR, r8-r14 */
} ItemFrame;
HW_OFFSET(ItemFrame, offset, 0x08);
HW_OFFSET(ItemFrame, matrix, 0x14);
HW_SIZE(ItemFrame, 0x74);

/* was func_8C05FA30: place item k's vertex (vecs[k]) at its place g_type31_item_offset[k]: items 0-9 in the actor's
 * matrix (items 7 and 8 also pull the Body and the angles of item k + 3 to it and to the actor's angles); items 10
 * and on in their own Body's matrix turned by their angles, pulling item k + 2 after them */
void actor_type31_place_item(gaddr obj, int32_t k) {
  ActorType31 *a = type31_at(obj);
  gaddr frame = hw_stack_alloc(sizeof(ItemFrame));
  ItemFrame *f = (ItemFrame *)hw_ptr(frame);
  gaddr matrix = member_addr(frame, ItemFrame, matrix);
  gaddr offset = member_addr(frame, ItemFrame, offset);
  const Vec3 *place = &g_type31_item_offset[k];
  matrix_identity(matrix); /* the matrix's constructor */
  njUnitMatrix(matrix);
  f->body_offset = (uint32_t)k * sizeof(Body);
  f->offset_z = hw_addr(&place->z);
  if (k == 7 || k == 8) {
    vec3_set(place->x, place->y, place->z, offset);
    njCalcPoint(member_addr(obj, ActorType31, matrix), offset, item_vec(a, k));
    body_pull(g_type31_item_rate, item_body(a, k + 3), item_vec(a, k));
    body1_pull_angle(a->b74.value, g_type31_item_turn, item_turn(a->turns_a, k + 3));
    body1_pull_angle(a->b84.value, g_type31_item_turn, item_turn(a->turns_b, k + 3));
    body1_pull_angle(a->b94.value, g_type31_item_turn, item_turn(a->turns_c, k + 3));
  } else if (k < 10) {
    vec3_set(place->x, place->y, place->z, offset);
    njCalcPoint(member_addr(obj, ActorType31, matrix), offset, item_vec(a, k));
  } else {
    njTranslateV(matrix, method_self(item_body(a, k)));
    njRotateX(matrix, f32_to_int(body1_at(item_turn(a->turns_a, k))->value));
    njRotateY(matrix, f32_to_int(body1_at(item_turn(a->turns_b, k))->value));
    njRotateZ(matrix, f32_to_int(body1_at(item_turn(a->turns_c, k))->value));
    vec3_set(place->x, place->y, place->z, offset);
    njCalcPoint(matrix, offset, item_vec(a, k));
    body_pull(g_type31_item_rate, item_body(a, k + 2), item_vec(a, k));
    body1_pull_angle(body1_at(item_turn(a->turns_a, k))->value, g_type31_item_turn, item_turn(a->turns_a, k + 2));
    body1_pull_angle(body1_at(item_turn(a->turns_b, k))->value, g_type31_item_turn, item_turn(a->turns_b, k + 2));
    body1_pull_angle(body1_at(item_turn(a->turns_c, k))->value, g_type31_item_turn, item_turn(a->turns_c, k + 2));
  }
  hw_stack_free(sizeof(ItemFrame));
}
HW_EXPORT(8c05fa30, actor_type31_place_item(ARG0, (int32_t)ARG1))

/* was func_8C05F91C: rebuild its model's vertex chunk: the object's chunk header copied (its first word, then its
 * second word at the first vertex's place); every item's Body and angles stepped; the matrix at the Body turned by
 * b74, b84, b94; each item placed and its vertex copied in (16 bytes a vertex); the chunk ended */
void actor_type31_step_items(gaddr obj) {
  ActorType31 *a = type31_at(obj);
  const NjObject *object = (const NjObject *)hw_ptr(a->object);
  const uint32_t *vlist = (const uint32_t *)hw_ptr(*(const gaddr *)hw_ptr(object->model));
  uint32_t *dst = (uint32_t *)hw_ptr(a->block_dc);
  dst[0] = vlist[0];
  dst[2] = vlist[1]; /* (the original's store lands at +8: the first vertex overwrites it) */
  gaddr vertex = a->block_dc + 8;
  int32_t n = (int16_t)(vlist[1] >> 16); /* the chunk's vertex count */
  for (int32_t k = 0; k < n; k++) {
    body_step(item_body(a, k));
    body1_step(item_turn(a->turns_a, k));
    body1_step(item_turn(a->turns_b, k));
    body1_step(item_turn(a->turns_c, k));
  }
  gaddr matrix = member_addr(obj, ActorType31, matrix);
  njUnitMatrix(matrix);
  njTranslateV(matrix, method_self(member_addr(obj, ActorType31, body)));
  njRotateX(matrix, f32_to_int(a->b74.value));
  njRotateY(matrix, f32_to_int(a->b84.value));
  njRotateZ(matrix, f32_to_int(a->b94.value));
  for (int32_t k = 0; k < n; k++, vertex += 0x10) {
    actor_type31_place_item(obj, k);
    memmove(hw_ptr(vertex), hw_ptr(item_vec(a, k)), sizeof(Vec3)); /* the SHC copy helper 0x8C0625EC */
  }
  *(uint32_t *)hw_ptr(vertex) = TYPE31_CHUNK_END;
}
HW_EXPORT(8c05f91c, actor_type31_step_items(ARG0))
