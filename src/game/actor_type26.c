/* Pen Pen port, S18 b05: the update and draw of the scenery actor of placement type 0x26 (game/actor_b07.h:
 * ActorType26; its construction and reset are in game/actor_types_b07.c): a model playing its motion that sends
 * up a particle each time the motion starts over. Each of its 8 particles (a stage, a size and a timer in its
 * blocks, and a Body) starts at a random point near the actor (in its turned frame), rises, grows and fades for
 * g_type26_particle_life steps. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"
#include "actor_b07.h"
#include "race_scene.h" /* race_scene_slot_insert */
#include "game.h"       /* g_panel */
#include "scenery.h"    /* random_timer_scaled, shape_motion_draw_l1 */

/* statics of the binary the unit reads */
#define g_type26_particles      GUEST_VAR(int32_t, 0x8C0885FCu)  /* 8: the particles */
#define g_type26_reset_word     GUEST_VAR(uint32_t, 0x8C096D10u) /* the particles' texlist with word30 not 0 */
#define g_type26_sprite_attr    GUEST_VAR(uint32_t, 0x8C088600u) /* 0x33: njDrawSprite3D's attribute */
#define g_type26_size_a         GUEST_VAR(f32, 0x8C088604u)      /* 0.2857: a new particle's size (word30 0) */
#define g_type26_size_b         GUEST_VAR(f32, 0x8C088608u)      /* 0.4167: (word30 not 0) */
#define g_type26_particle_life  GUEST_VAR(f32, 0x8C08860Cu)      /* 80: a particle's timer runs up to it */
#define g_type26_lift           GUEST_VAR(f32, 0x8C088610u)      /* 0.1: a new particle's push up, times cos x */
#define g_type26_growth_a       GUEST_VAR(f32, 0x8C088614u)      /* 0.0035714: a particle's growth (word30 0) */
#define g_type26_growth_b       GUEST_VAR(f32, 0x8C088618u)      /* 0.0052083: (word30 not 0) */

#define TYPE26_TIMER_A       F32(20.0f)            /* a new particle's timer (word30 0; else 0) */
#define TYPE26_BODY_DAMPING  F32_BITS(0x3F4CCCCCu) /* 0.8 */
/* a new particle's offset in the actor's frame: random ranges and bases */
#define TYPE26_OFFSET_Z_RANGE F32_BITS(0xC0897B4Au) /* -4.2963 */
#define TYPE26_OFFSET_Z_BASE  F32_BITS(0x4150E76Cu) /* 13.0565 */
#define TYPE26_OFFSET_Y_RANGE F32_BITS(0xBE489A02u) /* -0.1959 */
#define TYPE26_OFFSET_Y_BASE  F32_BITS(0x3D944673u) /* 0.0724 */
#define TYPE26_OFFSET_X_RANGE F32_BITS(0x3F87EF9Du) /* 1.062 */
#define TYPE26_OFFSET_X_BASE  F32_BITS(0xBEA6DC5Du) /* -0.3259 */

static int32_t *particle_word(gaddr block, int32_t i) { return &((int32_t *)hw_ptr(block))[i]; }
static f32 *particle_float(gaddr block, int32_t i) { return &((f32 *)hw_ptr(block))[i]; }

/* was func_8C0590F4: one step of every particle: a rising one (stage 1) counts its timer up to
 * g_type26_particle_life (then stage 2), steps its Body and grows; a done one (stage 2) is freed */
void actor_type26_step_particles(gaddr obj) {
  const ActorType26 *a = (const ActorType26 *)hw_ptr(obj);
  int32_t *stage = particle_word(a->blocks[TYPE26_STAGES], 0);
  f32 *size = particle_float(a->blocks[TYPE26_SIZES], 0);
  f32 *timer = particle_float(a->blocks[TYPE26_TIMERS], 0);
  gaddr body = a->bodies;
  for (int32_t n = g_type26_particles; n > 0; n--, timer++, stage++, size++, body += sizeof(Body)) {
    switch (*stage) {
    case 1:
      if (fgt(g_type26_particle_life, *timer))
        *timer = fadd(*timer, F32(1.0f));
      else
        *stage += 1;
      body_step(body);
      *size = fadd(*size, a->word30 == 0 ? g_type26_growth_a : g_type26_growth_b);
      break;
    case 2:
      *stage = 0;
      break;
    default: /* 0: free */
      break;
    }
  }
}
HW_EXPORT(8c0590f4, actor_type26_step_particles(ARG0))

/* was func_8C059192: draw the particles: the njControl3D flags pushed and set to 0x10, the particles' texlist (the
 * actor's with word30 0, else the global one); every particle not free drawn as the sprite at its Body, its size
 * and fading out with its timer; then the flags popped */
void actor_type26_draw_particles(gaddr obj) {
  ActorType26 *a = (ActorType26 *)hw_ptr(obj);
  control3d_push();
  control3d_set(0x10);
  const int32_t *stage = particle_word(a->blocks[TYPE26_STAGES], 0);
  const f32 *size = particle_float(a->blocks[TYPE26_SIZES], 0);
  const f32 *timer = particle_float(a->blocks[TYPE26_TIMERS], 0);
  gaddr body = a->bodies;
  if (a->word30 == 0)
    njSetTexture(a->word88);
  else
    njSetTexture(g_type26_reset_word);
  for (int32_t n = g_type26_particles; n > 0; n--, stage++, size++, timer++, body += sizeof(Body)) {
    if (*stage == 0) continue;
    f32 alpha = fsub(F32(1.0f), fdiv(*timer, g_type26_particle_life));
    material_set_89b8(F32(1.0f), F32(1.0f), F32(1.0f), alpha);
    a->sprite.sx = *size;
    a->sprite.sy = *size;
    const Body *b = body_at(body);
    a->sprite.p.x = b->position.x; /* the unit's inline getters (func_8C0589F8, ...) */
    a->sprite.p.y = b->position.y;
    a->sprite.p.z = b->position.z;
    njDrawSprite3D(member_addr(obj, ActorType26, sprite), 0, g_type26_sprite_attr); /* thunk func_8C0589E2 */
  }
  control3d_pop();
}
HW_EXPORT(8c059192, actor_type26_draw_particles(ARG0))

/* the original's frame of actor_type26_spawn_particle (0x74 bytes under PR): its matrix and vectors go to Ninja */
typedef struct SpawnFrame {
  f32 lift;          /* 0x00 */
  Vec3 point;        /* 0x04: where the particle starts */
  Vec3 offset;       /* 0x10: its offset in the actor's frame */
  Matrix matrix;     /* 0x1C */
  uint32_t locals[5]; /* 0x5C: the loop's count and pointers */
  gaddr self;        /* 0x70 */
  uint32_t saved_pr; /* 0x74 */
} SpawnFrame;
HW_OFFSET(SpawnFrame, point, 0x04);
HW_OFFSET(SpawnFrame, offset, 0x10);
HW_OFFSET(SpawnFrame, matrix, 0x1C);
HW_OFFSET(SpawnFrame, self, 0x70);
HW_SIZE(SpawnFrame, 0x78);

/* was func_8C05929C: the first free particle (if any) set off: rising, its size and timer by word30; its Body at
 * rest (damping 0.8) at a random point near the actor (the offset turned by its angles about y and x, from its
 * Body), pushed up by g_type26_lift times the cosine of its angle about x */
void actor_type26_spawn_particle(gaddr obj) {
  const ActorType26 *a = (const ActorType26 *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(SpawnFrame));
  SpawnFrame *f = (SpawnFrame *)hw_ptr(frame);
  gaddr matrix = member_addr(frame, SpawnFrame, matrix);
  gaddr point = member_addr(frame, SpawnFrame, point);
  gaddr offset = member_addr(frame, SpawnFrame, offset);
  f->self = obj;
  int32_t *stage = particle_word(a->blocks[TYPE26_STAGES], 0);
  f32 *size = particle_float(a->blocks[TYPE26_SIZES], 0);
  f32 *timer = particle_float(a->blocks[TYPE26_TIMERS], 0);
  gaddr body = a->bodies;
  for (int32_t n = g_type26_particles; n > 0; n--, stage++, size++, timer++, body += sizeof(Body)) {
    if (*stage != 0) continue;
    *stage = 1;
    if (a->word30 == 0) {
      *size = g_type26_size_a;
      *timer = TYPE26_TIMER_A;
    } else {
      *size = g_type26_size_b;
      *timer = F32(0.0f);
    }
    matrix_identity(matrix); /* func_8C0588DC: the matrix's constructor */
    njUnitMatrix(matrix);
    njTranslateV(matrix, method_self(member_addr(obj, ActorType26, body)));
    njRotateY(matrix, f32_to_int(a->angle.y));
    njRotateX(matrix, f32_to_int(a->angle.x));
    method_self(offset); /* func_8C05886C: the Vec3's (empty) constructor */
    method_self(point);
    f32 z = fadd(random_timer_scaled(TYPE26_OFFSET_Z_RANGE), TYPE26_OFFSET_Z_BASE);
    f32 y = fadd(random_timer_scaled(TYPE26_OFFSET_Y_RANGE), TYPE26_OFFSET_Y_BASE);
    f32 x = fadd(random_timer_scaled(TYPE26_OFFSET_X_RANGE), TYPE26_OFFSET_X_BASE);
    vec3_set(x, y, z, offset);
    njCalcPoint(matrix, offset, point);
    body_reset(body);
    vec3_copy_bytes(body, point);
    vec3_fill(member_addr(body, Body, damping), TYPE26_BODY_DAMPING);
    f->lift = F32(0.0f);
    f->lift = fmul(njCos(f32_to_int(a->angle.x)), g_type26_lift);
    f32_add_to(f->lift, member_addr(body, Body, force.y)); /* func_8C058A6E */
    break;
  }
  hw_stack_free(sizeof(SpawnFrame));
}
HW_EXPORT(8c05929c, actor_type26_spawn_particle(ARG0))

/* was func_8C058F18: the update method: a particle set off when the frame is 0; the frame advanced (by 1, or 0.5
 * with word30); the contacts put in the race scene's slot 8; the particles stepped (a tail call) */
void actor_type26_update(gaddr obj) {
  ActorType26 *a = (ActorType26 *)hw_ptr(obj);
  if (feq(a->frame, F32(0.0f))) actor_type26_spawn_particle(obj);
  f32 step = a->word30 == 0 ? F32(1.0f) : F32(0.5f);
  a->frame = fadd(a->frame, step);
  race_scene_slot_insert(g_panel, member_addr(obj, ActorType26, contacts), 8); /* inlined: the scene's slot list */
  actor_type26_step_particles(obj);
}
HW_EXPORT(8c058f18, actor_type26_update(ARG0))

/* was func_8C058F84: the draw method: its textures; at its Body's position, turned about z, y and x, its object
 * drawn with the shape motion of its motion entry at its frame (looping); then the particles (a tail call) */
void actor_type26_draw(gaddr obj) {
  const ActorType26 *a = (const ActorType26 *)hw_ptr(obj);
  njSetTexture(a->word88);
  njPushMatrix(0);
  njTranslateV(0, member_addr(obj, ActorType26, body));
  njRotateZ(0, f32_to_int(a->angle.z));
  njRotateY(0, f32_to_int(a->angle.y));
  njRotateX(0, f32_to_int(a->angle.x));
  const MotionEntry *e = &((const MotionEntry *)hw_ptr(a->motions))[a->motion];
  shape_motion_draw_l1(a->object, e->motion, e->shape, member_addr(obj, ActorType26, frame), 1);
  njPopMatrix(1);
  actor_type26_draw_particles(obj);
}
HW_EXPORT(8c058f84, actor_type26_draw(ARG0))
