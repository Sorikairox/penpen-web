/* Pen Pen port, S18 b04: the particle effect (type 0x1A, Effect, game/actor.h; its construction, init and step are
 * game/actor_effect.c): its destruction, its draw and its particles' draw, and the burst a hit starts. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h"
#include "../core/motion_frame.h"
#include "actor.h"
#include "scenery.h"      /* random_timer_scaled */

#define g_effect_slots       GUEST_VAR(int32_t, 0x8C0881B0u) /* 32: the particles */
#define g_effect_countdown   GUEST_VAR(int32_t, 0x8C0881B4u) /* 20: a new particle's countdown */
#define g_effect_sprite_attr GUEST_VAR(uint32_t, 0x8C0881B8u) /* 0x33: njDrawSprite3D's attributes */
#define g_effect_sprite_turn GUEST_VAR(f32, 0x8C0881BCu)     /* 2730.67 (15 degrees): the sprite's turn per particle */
#define g_effect_value_limit GUEST_VAR(f32, 0x8C0881CCu)     /* 20: a particle is gone at this value */
#define g_effect_radius      GUEST_VAR(f32, 0x8C0881A4u)     /* 5.9999995: a burst particle starts this far out */
#define g_effect_burst_push  GUEST_VAR(f32, 0x8C0881D0u)     /* 0.1 */
#define EFFECT_BURST_MAX     8                               /* a burst starts up to 9 particles */

static inline Effect *effect_at(gaddr a) { return (Effect *)hw_ptr(a); }

/* was func_8C04B53C: free the per-particle blocks that were allocated (frames, countdowns, states, Bodies, values,
 * extra, in that order) */
void effect_free_blocks(gaddr obj) {
  const Effect *e = effect_at(obj);
  gaddr p;
  if ((p = e->frames) != 0) syFree(p);
  if ((p = e->countdowns) != 0) syFree(p);
  if ((p = e->states) != 0) syFree(p);
  if ((p = e->bodies) != 0) syFree(p);
  if ((p = e->values) != 0) syFree(p);
  if ((p = e->extra) != 0) syFree(p);
}
HW_EXPORT(8c04b53c, effect_free_blocks(ARG0))

/* was func_8C04B0C8: the destructor body: the blocks, then the contacts */
void effect_destroy(gaddr obj) {
  effect_free_blocks(obj);
  contacts_free(member_addr(obj, Effect, contacts));
}
HW_EXPORT(8c04b0c8, effect_destroy(ARG0))

/* was func_8C04B0E2: the deleting destructor */
gaddr effect_delete(gaddr obj, int32_t free_memory) {
  effect_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c04b0e2, RET(effect_delete(ARG0, (int32_t)ARG1)))

/* was func_8C04B106: the destructor body, returning the object */
gaddr effect_destroy_self(gaddr obj) {
  effect_destroy(obj);
  return obj;
}
HW_EXPORT(8c04b106, RET(effect_destroy_self(ARG0)))

/* was func_8C04B6BC: draw the live particles: each one faded by its value (its alpha kept), as sprite `frame` at its
 * Body, the sprite turned 15 degrees further for each */
void effect_draw_particles(gaddr obj) {
  Effect *e = effect_at(obj);
  control3d_push();
  control3d_set(0x20);
  const int32_t *frames = (const int32_t *)hw_ptr(e->frames);
  const int32_t *states = (const int32_t *)hw_ptr(e->states);
  const f32 *values = (const f32 *)hw_ptr(e->values);
  f32 *alphas = (f32 *)hw_ptr(e->extra);
  gaddr body = e->bodies;
  for (int32_t i = 0; i < g_effect_slots; i++, body += sizeof(Body)) {
    if (states[i] == 0) continue;
    f32 alpha = fsub(F32(1.0f), fdiv(values[i], g_effect_value_limit));
    alphas[i] = alpha;
    material_set_ac6a(F32(1.0f), F32(1.0f), F32(1.0f), alpha);
    e->sprite.ang = f32_to_int(fadd(f32_from_int(e->sprite.ang), g_effect_sprite_turn));
    const Body *b = body_at(body);
    e->sprite.p.x = b->position.x;
    e->sprite.p.y = b->position.y;
    e->sprite.p.z = b->position.z;
    njDrawSprite3D(member_addr(obj, Effect, sprite), frames[i], g_effect_sprite_attr);
  }
  control3d_pop();
}
HW_EXPORT(8c04b6bc, effect_draw_particles(ARG0))

/* was func_8C04B45C: draw it: its model at the body, turned about z, y, x by its angles, scaled by its pulses, with
 * the motion of its motion entry at its frame; then its particles (a tail call) */
void effect_draw(gaddr obj) {
  const Effect *e = effect_at(obj);
  njSetTexture(e->word_ec);
  matrix_push();
  matrix_translate_v(member_addr(obj, Effect, body));
  matrix_rotate_z(e->angle[2].value);
  matrix_rotate_y(e->angle[1].value);
  matrix_rotate_x(e->angle[0].value);
  f32 sz = e->pulse[2].value;
  f32 sy = e->pulse[1].value;
  matrix_scale(e->pulse[0].value, sy, sz);
  const MotionEntry *m = &((const MotionEntry *)hw_ptr(e->motions))[e->motion];
  motion_draw_l1(e->model, m->motion, member_addr(obj, Effect, frame), 1);
  matrix_pop();
  effect_draw_particles(obj);
}
HW_EXPORT(8c04b45c, effect_draw(ARG0))

/* the original's frame of the burst (0x78 bytes) */
enum {
  BURST_POS = 0x00,     /* gaddr: pos */
  BURST_ANGLE = 0x08,   /* f32: angle */
  BURST_YAW = 0x10,     /* f32: the particle's yaw */
  BURST_PUSH = 0x14,    /* Vec3 */
  BURST_MATRIX = 0x20,  /* Matrix */
  BURST_POINT = 0x60,   /* Vec3: where the particle starts */
  BURST_OUT = 0x6C,     /* Vec3: (0, 0, 6) */
  BURST_FRAME = 0x78
};

/* was func_8C04B78C: start a burst at pos around `angle`: up to 9 free particles start counting down, each with a
 * random sprite (0..6), 6 out from pos at a random yaw (angle - 90 + 0..180 degrees) and pitch (-60..60 degrees),
 * pushed away from pos */
void effect_burst(gaddr obj, gaddr pos, f32 angle) {
  const Effect *e = effect_at(obj);
  gaddr frame = hw_stack_alloc(BURST_FRAME);
  U32_AT(frame + BURST_POS) = pos;
  F32_AT(frame + BURST_ANGLE) = angle;
  int32_t *frames = (int32_t *)hw_ptr(e->frames);
  int32_t *countdowns = (int32_t *)hw_ptr(e->countdowns);
  int32_t *states = (int32_t *)hw_ptr(e->states);
  f32 *values = (f32 *)hw_ptr(e->values);
  f32 *alphas = (f32 *)hw_ptr(e->extra);
  gaddr body = e->bodies;
  int32_t started = 0;
  gaddr m = frame + BURST_MATRIX;
  for (int32_t i = 0; i < g_effect_slots; i++, body += sizeof(Body)) {
    if (states[i] != 0) continue;
    frames[i] = f32_to_int(random_timer_scaled(F32(7.0f)));
    countdowns[i] = g_effect_countdown;
    values[i] = F32(0.0f);
    alphas[i] = F32(1.0f);
    states[i] = 1;
    body_reset(body);
    vec3_fill(member_addr(body, Body, damping), F32_BITS(0x3F733333u) /* 0.95 */);
    vec3_set(F32(0.0f), F32(0.0f), g_effect_radius, frame + BURST_OUT);
    f32 pitch = fadd(random_timer_scaled(F32_BITS(0x46AAAAAAu) /* 21845.33 */), F32_BITS(0xC62AAAAAu) /* -10922.67 */);
    f32 r = random_timer_scaled(F32(32768.0f));
    f32 yaw = fadd(fadd(F32_AT(frame + BURST_ANGLE), r), F32(-16384.0f));
    F32_AT(frame + BURST_YAW) = yaw;
    matrix_identity(m);
    njUnitMatrix(m);
    njTranslateV(m, pos);
    njRotateY(m, f32_to_int(yaw));
    njRotateX(m, f32_to_int(pitch));
    njCalcPoint(m, frame + BURST_OUT, frame + BURST_POINT);
    vec3_copy_bytes(body, frame + BURST_POINT);
    vec3_copy(frame + BURST_PUSH, frame + BURST_POINT);
    vec3_sub(frame + BURST_PUSH, pos);
    vec3_scale(g_effect_burst_push, frame + BURST_PUSH);
    vec3_add(member_addr(body, Body, force), frame + BURST_PUSH);
    if (++started > EFFECT_BURST_MAX) break;
  }
  hw_stack_free(BURST_FRAME);
}
HW_EXPORT(8c04b78c, effect_burst(ARG0, ARG1, FARG0))
