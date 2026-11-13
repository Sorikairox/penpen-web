/* Pen Pen port, S17 b08: the construction and destruction of the scenery actors of placement types 0x30, 0x31 and
 * 0x32 (game/actor_b08.h), and type 0x30's puffs (reset, spawn). */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "../core/file.h"
#include "game.h"
#include "actor.h"
#include "actor_b05.h"
#include "scenery.h"
#include "actor_b08.h"

#define TYPE30_VTABLE           0x8C23E7BCu
#define TYPE30_SECOND_VTABLE    0x8C23E7DCu
#define TYPE30_MATRIX_CTOR      0x8C05D038u /* matrix_set_identity's copy (core/vec.c) */
#define TYPE30_VEC3_CTOR        0x8C05CCE2u /* an empty Vec3 constructor (method_empty's copy) */
#define TYPE30_TABLE            0x8C099680u /* the table the reset installs (static) */
#define TYPE30_PUFF_SCALE0      F32(1.25f)
#define TYPE31_VTABLE           0x8C23E7FCu
#define TYPE31_SECOND_VTABLE    0x8C23E81Cu
#define TYPE31_QUAT_CTOR        0x8C05ED88u /* quat_identity's copy */
#define TYPE31_VEC3_CTOR        0x8C05EB56u /* an empty Vec3 constructor (method_empty's copy) */
#define TYPE31_BODY_CTOR        0x8C05ED9Cu /* body_reset_set3's copy (game/scenery_b08.c) */
#define TYPE32_BODY_CTOR        0x8C060230u /* body_reset_set3's copy (game/scenery_b08.c) */

/* statics of the binary the unit reads (not folded into the code) */
#define g_type30_radius_a     GUEST_VAR(f32, 0x8C08A178u)     /* 1.9999999 */
#define g_type30_radius_b     GUEST_VAR(f32, 0x8C08A17Cu)     /* 0.99999994 */
#define g_type30_puffs        GUEST_VAR(int32_t, 0x8C08A19Cu) /* 4: the puffs in use */
#define g_type30_puff_time    GUEST_VAR(int32_t, 0x8C08A1A0u) /* 20: a new puff's timer */
#define g_type30_puff_scale1  GUEST_VAR(f32, 0x8C08A1A8u)     /* 0.12499999: a kind-1 puff's first scale */
#define g_type30_reset_word   GUEST_VAR(uint32_t, 0x8C096D10u)
#define g_type31_items        GUEST_VAR(int32_t, 0x8C08A1C4u) /* 26 */
#define g_type31_body_damping GUEST_VAR(f32, 0x8C08A1D0u)     /* 0.6 */
#define g_type31_turn_damping GUEST_VAR(f32, 0x8C08A1D4u)     /* 0.6 */
#define g_type32_radius       GUEST_VAR(f32, 0x8C08A310u)     /* 5 */

/* a Body1 at rest: (0, 0, 0, 1) (the inline constructor) */
static void body1_rest(Body1 *v) {
  v->value = F32(0.0f);
  v->velocity = F32(0.0f);
  v->force = F32(0.0f);
  v->damping = F32(1.0f);
}

/* ---- type 0x30 ---- */

/* was func_8C05D074: the base construction: the Actor; the two points (empty constructors); six Quats (0, 0, 0, 1);
 * the Body at rest; five matrices the identity (in place) and a sixth; the two contact sets; the six puff blocks and
 * the puffs' Vec3s (operator new[]: 0 when out of memory) */
gaddr actor_type30_base_construct(gaddr obj) {
  ActorType30 *a = (ActorType30 *)hw_ptr(obj);
  actor_construct(obj);
  method_self(member_addr(obj, ActorType30, point_a));
  method_self(member_addr(obj, ActorType30, point_b));
  for (int32_t i = 0; i < 3; i++) quat_identity(member_addr(obj, ActorType30, quats_a[i]));
  for (int32_t i = 0; i < 3; i++) quat_identity(member_addr(obj, ActorType30, quats_b[i]));
  body_init(member_addr(obj, ActorType30, body));
  array_construct(TYPE30_MATRICES, sizeof(Matrix), member_addr(obj, ActorType30, matrices), TYPE30_MATRIX_CTOR);
  matrix_identity(member_addr(obj, ActorType30, matrix));
  contacts_construct(member_addr(obj, ActorType30, contacts_a), member_addr(obj, ActorType30, point_a),
                     g_type30_radius_a, 2);
  contacts_construct(member_addr(obj, ActorType30, contacts_b), member_addr(obj, ActorType30, point_b),
                     g_type30_radius_b, 2);
  a->puff_kind = syMalloc(TYPE30_PUFF_BLOCK);
  a->puff_timer = syMalloc(TYPE30_PUFF_BLOCK);
  a->puff_state = syMalloc(TYPE30_PUFF_BLOCK);
  a->puff_fade = syMalloc(TYPE30_PUFF_BLOCK);
  a->puff_scale = syMalloc(TYPE30_PUFF_BLOCK);
  a->puff_frame = syMalloc(TYPE30_PUFF_BLOCK);
  gaddr block = syMalloc(4 * sizeof(Vec3));
  gaddr pos = 0;
  if (block != 0) pos = array_construct_new(4, sizeof(Vec3), block, TYPE30_VEC3_CTOR);
  a->puff_pos = pos;
  return obj;
}
HW_EXPORT(8c05d074, RET(actor_type30_base_construct(ARG0)))

/* was func_8C05D190: type 0x30's constructor: both vtables, then the base */
gaddr actor_type30_construct(gaddr obj) {
  ActorType30 *a = (ActorType30 *)hw_ptr(obj);
  a->base.vtable = TYPE30_VTABLE;
  a->second_vtable = TYPE30_SECOND_VTABLE;
  actor_type30_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05d190, RET(actor_type30_construct(ARG0)))

/* was func_8C05E3C0: the puff blocks freed (each that is not 0), the Vec3s last */
void actor_type30_free_puffs(gaddr obj) {
  ActorType30 *a = (ActorType30 *)hw_ptr(obj);
  if (a->puff_kind != 0) syFree(a->puff_kind);
  if (a->puff_timer != 0) syFree(a->puff_timer);
  if (a->puff_state != 0) syFree(a->puff_state);
  if (a->puff_fade != 0) syFree(a->puff_fade);
  if (a->puff_scale != 0) syFree(a->puff_scale);
  if (a->puff_frame != 0) syFree(a->puff_frame);
  if (a->puff_pos != 0) syFree(a->puff_pos);
}
HW_EXPORT(8c05e3c0, actor_type30_free_puffs(ARG0))

/* was func_8C05D200: the destructor body: the puffs freed, then the second contacts, then the first */
void actor_type30_destroy(gaddr obj) {
  actor_type30_free_puffs(obj);
  contacts_free(member_addr(obj, ActorType30, contacts_b));
  contacts_free(member_addr(obj, ActorType30, contacts_a));
}
HW_EXPORT(8c05d200, actor_type30_destroy(ARG0))

/* was func_8C05D220 */
gaddr actor_type30_delete(gaddr obj, int32_t free_memory) {
  actor_type30_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05d220, RET(actor_type30_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05D244 */
gaddr actor_type30_destroy_self(gaddr obj) {
  actor_type30_destroy(obj);
  return obj;
}
HW_EXPORT(8c05d244, RET(actor_type30_destroy_self(ARG0)))

/* was func_8C05E36C: reset it: the three floats at 0x36C cleared, word384 the global word, the static table, and
 * every puff's state off */
void actor_type30_reset(gaddr obj) {
  ActorType30 *a = (ActorType30 *)hw_ptr(obj);
  a->f36c = F32(0.0f);
  a->f370 = F32(0.0f);
  a->f374 = F32(0.0f);
  a->word384 = g_type30_reset_word;
  a->table = TYPE30_TABLE;
  int32_t *state = (int32_t *)hw_ptr(a->puff_state);
  for (int32_t n = g_type30_puffs; n > 0; n--) *state++ = 0;
}
HW_EXPORT(8c05e36c, actor_type30_reset(ARG0))

/* was func_8C05E7BC: start a kind-0 puff in the first free slot (none when all are in use): at the body's position,
 * frame 0, the timer full, fade 0, scale 1.25, state 1 */
void actor_type30_spawn_puff(gaddr obj) {
  ActorType30 *a = (ActorType30 *)hw_ptr(obj);
  int32_t *kind = (int32_t *)hw_ptr(a->puff_kind);
  int32_t *timer = (int32_t *)hw_ptr(a->puff_timer);
  int32_t *state = (int32_t *)hw_ptr(a->puff_state);
  f32 *fade = (f32 *)hw_ptr(a->puff_fade);
  f32 *scale = (f32 *)hw_ptr(a->puff_scale);
  f32 *frame = (f32 *)hw_ptr(a->puff_frame);
  gaddr pos = a->puff_pos;
  for (int32_t i = 0; i < g_type30_puffs; i++, pos += sizeof(Vec3)) {
    if (state[i] != 0) continue;
    memmove(hw_ptr(pos), &a->body.position, sizeof(Vec3));
    frame[i] = F32(0.0f);
    timer[i] = g_type30_puff_time;
    fade[i] = F32(0.0f);
    kind[i] = 0;
    scale[i] = TYPE30_PUFF_SCALE0;
    state[i] = 1;
    return;
  }
}
HW_EXPORT(8c05e7bc, actor_type30_spawn_puff(ARG0))

/* was func_8C05E842: the same for a kind-1 puff (scale g_type30_puff_scale1); the position through the Body's
 * (empty) accessor */
void actor_type30_spawn_puff_b(gaddr obj) {
  ActorType30 *a = (ActorType30 *)hw_ptr(obj);
  int32_t *kind = (int32_t *)hw_ptr(a->puff_kind);
  int32_t *timer = (int32_t *)hw_ptr(a->puff_timer);
  int32_t *state = (int32_t *)hw_ptr(a->puff_state);
  f32 *fade = (f32 *)hw_ptr(a->puff_fade);
  f32 *scale = (f32 *)hw_ptr(a->puff_scale);
  f32 *frame = (f32 *)hw_ptr(a->puff_frame);
  gaddr pos = a->puff_pos;
  for (int32_t i = 0; i < g_type30_puffs; i++, pos += sizeof(Vec3)) {
    if (state[i] != 0) continue;
    gaddr from = method_self(member_addr(obj, ActorType30, body)); /* func_8C05CE94: the Body's position */
    memmove(hw_ptr(pos), hw_ptr(from), sizeof(Vec3));
    frame[i] = F32(0.0f);
    timer[i] = g_type30_puff_time;
    fade[i] = F32(0.0f);
    kind[i] = 1;
    scale[i] = g_type30_puff_scale1;
    state[i] = 1;
    return;
  }
}
HW_EXPORT(8c05e842, actor_type30_spawn_puff_b(ARG0))

/* ---- type 0x31 ---- */

/* was func_8C05EDF0: the base construction: the Actor; three Body1s at rest; the Body at rest through the Vec3
 * setters; the matrix the identity (from its last element); the Vec3 (empty) and two Quats (0, 0, 0, 1); contacts
 * with no owner (radius 0, room for 4); then the blocks (the arrays from operator new[]: 0 when out of memory) */
gaddr actor_type31_base_construct(gaddr obj) {
  ActorType31 *a = (ActorType31 *)hw_ptr(obj);
  actor_construct(obj);
  body1_rest(&a->b74);
  body1_rest(&a->b84);
  body1_rest(&a->b94);
  body_reset_set3(member_addr(obj, ActorType31, body));
  matrix_identity(member_addr(obj, ActorType31, matrix));
  method_self(member_addr(obj, ActorType31, v170));
  quat_identity(member_addr(obj, ActorType31, q17c));
  quat_identity(member_addr(obj, ActorType31, q18c));
  contacts_construct(member_addr(obj, ActorType31, contacts), 0, F32(0.0f), 4);
  a->block_dc = syMalloc(0x1AC);
  a->block_e0 = syMalloc(0xD2);
  gaddr block = syMalloc(TYPE31_ITEMS * sizeof(Body1));
  a->turns_a = block != 0 ? array_construct_new(TYPE31_ITEMS, sizeof(Body1), block, TYPE31_QUAT_CTOR) : 0;
  block = syMalloc(TYPE31_ITEMS * sizeof(Body1));
  a->turns_b = block != 0 ? array_construct_new(TYPE31_ITEMS, sizeof(Body1), block, TYPE31_QUAT_CTOR) : 0;
  block = syMalloc(TYPE31_ITEMS * sizeof(Body1));
  a->turns_c = block != 0 ? array_construct_new(TYPE31_ITEMS, sizeof(Body1), block, TYPE31_QUAT_CTOR) : 0;
  block = syMalloc(TYPE31_ITEMS * sizeof(Vec3));
  a->vecs = block != 0 ? array_construct_new(TYPE31_ITEMS, sizeof(Vec3), block, TYPE31_VEC3_CTOR) : 0;
  block = syMalloc(TYPE31_ITEMS * sizeof(Body));
  a->bodies = block != 0 ? array_construct_new(TYPE31_ITEMS, sizeof(Body), block, TYPE31_BODY_CTOR) : 0;
  return obj;
}
HW_EXPORT(8c05edf0, RET(actor_type31_base_construct(ARG0)))

/* was func_8C05EFB4: type 0x31's constructor: both vtables, then the base */
gaddr actor_type31_construct(gaddr obj) {
  ActorType31 *a = (ActorType31 *)hw_ptr(obj);
  a->base.vtable = TYPE31_VTABLE;
  a->second_vtable = TYPE31_SECOND_VTABLE;
  actor_type31_base_construct(obj);
  return obj;
}
HW_EXPORT(8c05efb4, RET(actor_type31_construct(ARG0)))

/* was func_8C05EFD0: the destructor body: the blocks freed (each that is not 0), then the contacts */
void actor_type31_destroy(gaddr obj) {
  ActorType31 *a = (ActorType31 *)hw_ptr(obj);
  if (a->block_dc != 0) syFree(a->block_dc);
  if (a->block_e0 != 0) syFree(a->block_e0);
  if (a->turns_a != 0) syFree(a->turns_a);
  if (a->turns_b != 0) syFree(a->turns_b);
  if (a->turns_c != 0) syFree(a->turns_c);
  if (a->vecs != 0) syFree(a->vecs);
  if (a->bodies != 0) syFree(a->bodies);
  contacts_free(member_addr(obj, ActorType31, contacts));
}
HW_EXPORT(8c05efd0, actor_type31_destroy(ARG0))

/* was func_8C05F08C */
gaddr actor_type31_delete(gaddr obj, int32_t free_memory) {
  actor_type31_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c05f08c, RET(actor_type31_delete(ARG0, (int32_t)ARG1)))

/* was func_8C05F0B0 */
gaddr actor_type31_destroy_self(gaddr obj) {
  actor_type31_destroy(obj);
  return obj;
}
HW_EXPORT(8c05f0b0, RET(actor_type31_destroy_self(ARG0)))

/* ---- type 0x32 ---- */

/* was func_8C060284: the base construction: the Actor; six Body1s at rest; the Body at rest through the Vec3
 * setters; the contacts owned by the Body (radius g_type32_radius, room for 2); three 0x80-byte blocks, 32 Bodies
 * (operator new[]: 0 when out of memory), two more blocks */
gaddr actor_type32_base_construct(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  actor_construct(obj);
  for (int32_t i = 0; i < 6; i++) body1_rest(&a->turns[i]);
  body_reset_set3(member_addr(obj, ActorType32, body));
  contacts_construct(member_addr(obj, ActorType32, contacts), member_addr(obj, ActorType32, body), g_type32_radius,
                     2);
  for (int32_t i = 0; i < 3; i++) a->blocks_a[i] = syMalloc(0x80);
  gaddr block = syMalloc(TYPE32_BODIES * sizeof(Body));
  a->bodies = block != 0 ? array_construct_new(TYPE32_BODIES, sizeof(Body), block, TYPE32_BODY_CTOR) : 0;
  for (int32_t i = 0; i < 2; i++) a->blocks_b[i] = syMalloc(0x80);
  return obj;
}
HW_EXPORT(8c060284, RET(actor_type32_base_construct(ARG0)))

/* func_8C0D02FE (nj_ObjectL1 in port/symbols/sdk_calls.tsv), through this unit's thunk */
static inline void nj_ObjectL1(gaddr p) { CALL1(0x8C0D02FEu, p); }

/* was func_8C05F4EC: draw it: its texture list set, nj_ObjectL1 given the two blocks at 0xDC; then at the Body's
 * position, turned about x, y and z by the three Body1s, the child of its object (the unit's thunks and matrix
 * helpers) */
void actor_type31_draw(gaddr obj) {
  ActorType31 *a = (ActorType31 *)hw_ptr(obj);
  njSetTexture(a->texlist);
  nj_ObjectL1(member_addr(obj, ActorType31, block_dc));
  matrix_push();
  matrix_translate_v(method_self(member_addr(obj, ActorType31, body))); /* func_8C05EC10: the Body's position */
  matrix_rotate_x(a->b74.value);
  matrix_rotate_y(a->b84.value);
  matrix_rotate_z(a->b94.value);
  nj_ModelL1(((const NjObject *)hw_ptr(a->object))->child);
  matrix_pop();
}
HW_EXPORT(8c05f4ec, actor_type31_draw(ARG0))

/* func_8C05EC5A (guest code inside func_8C05EC4E's block, not hand-written): the Body's damping = (d, d, d) */
static void body_set_damping_all(f32 d, gaddr body) {
  FARGS1(d);
  CALL1(0x8C05EC5Au, body);
}

/* was func_8C05FC78: reset the 26 items: each Body at rest with damping g_type31_body_damping, each of the three
 * Body1s at rest with damping g_type31_turn_damping, each Vec3 zero */
void actor_type31_reset_items(gaddr obj) {
  ActorType31 *a = (ActorType31 *)hw_ptr(obj);
  gaddr body = a->bodies, vec = a->vecs, ta = a->turns_a, tb = a->turns_b, tc = a->turns_c;
  for (int32_t n = g_type31_items; n > 0; n--) {
    body_reset(body);
    body_set_damping_all(g_type31_body_damping, body);
    quat_identity(ta);
    body1_at(ta)->damping = g_type31_turn_damping;
    quat_identity(tb);
    body1_at(tb)->damping = g_type31_turn_damping;
    quat_identity(tc);
    body1_at(tc)->damping = g_type31_turn_damping;
    vec3_zero(vec);
    tc += sizeof(Body1);
    tb += sizeof(Body1);
    body += sizeof(Body);
    ta += sizeof(Body1);
    vec += sizeof(Vec3);
  }
}
HW_EXPORT(8c05fc78, actor_type31_reset_items(ARG0))

/* func_8C02C888 (game/race_scene_players.c, no header yet): whether player i's stick is damped */
static int32_t player_stick_damped(gaddr scene, int32_t i) { return (int32_t)CALL2(0x8C02C888u, scene, i); }
/* func_8C02C8B8 (game/race_scene_players.c, no header yet): set player i's stick_damped */
static void player_set_stick_damped(gaddr scene, int32_t i, int32_t v) { CALL3(0x8C02C8B8u, scene, i, v); }

/* was func_8C05F708: when its player is nearer than 40 and that player's stick is not damped: phase 11 for 480
 * steps, f150 15, angle_a -2730.67, rate_a 0, rate_b 910.22 keeping rate_b's sign */
void actor_type31_start_near(gaddr obj) {
  ActorType31 *a = (ActorType31 *)hw_ptr(obj);
  if (!fgt(F32(40.0f), a->distance)) return;
  if (player_stick_damped(a->scene, a->player) != 0) return;
  a->state = 11;
  a->timer = 0x1E0;
  a->f150 = F32(15.0f);
  a->angle_a = F32_BITS(0xC52AAAAAu); /* -2730.6665 (-15 degrees) */
  if (fgt(F32(0.0f), a->rate_b)) {
    a->rate_a = F32(0.0f);
    a->rate_b = F32_BITS(0xC4638E38u); /* -910.2222 (-5 degrees) */
  } else {
    a->rate_a = F32(0.0f);
    a->rate_b = F32_BITS(0x44638E38u); /* 910.2222 */
  }
}
HW_EXPORT(8c05f708, actor_type31_start_near(ARG0))

/* was func_8C05F87C: let its player go (stick_damped 0): phase 12 for 60 steps, v164 = v68, f150 20, rate_b 364.09
 * keeping its sign, rate_a 36.41 + random 0..18.2 */
void actor_type31_start_release(gaddr obj) {
  ActorType31 *a = (ActorType31 *)hw_ptr(obj);
  player_set_stick_damped(a->scene, a->player, 0);
  a->state = 12;
  a->timer = 0x3C;
  memmove(&a->v164, &a->v68, sizeof(Vec3));
  a->f150 = F32(20.0f);
  a->rate_b = fgt(F32(0.0f), a->rate_b) ? F32_BITS(0xC3B60B60u) /* -364.0889 (-2 degrees) */
                                        : F32_BITS(0x43B60B60u); /* 364.0889 */
  a->rate_a = fadd(random_scaled(F32_BITS(0x4191A2B3u) /* 18.204445 (0.1 degree) */),
                   F32_BITS(0x4211A2B3u) /* 36.40889 */);
}
HW_EXPORT(8c05f87c, actor_type31_start_release(ARG0))
