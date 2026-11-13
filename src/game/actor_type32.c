/* Pen Pen port, S17 b09: the scenery actor of placement type 0x32 (game/actor_b08.h: ActorType32): its
 * construction and destruction, and its 32 ripples: each ripple is a Body of the bodies array with a word or float
 * in each of five blocks (its sprite cell, timer, stage, size and alpha). A ripple is spawned at the actor's Body,
 * waits out its timer, then grows; the splash ones (cells 1-3) move with their Body and cycle their cell, the ring
 * (cell 0) stays on the actor's Body and swells and shrinks while it waits. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "../core/view.h"
#include "actor.h"
#include "actor_b08.h"
#include "scenery.h" /* random_timer_scaled */
#include "../core/motion_frame.h"
#include "../core/file.h" /* ContainerEntry, NjMotion */

/* SDK njColorBlendingMode (0x8C0A23A8) */
static void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

#define TYPE32_VTABLE        0x8C23E83Cu
#define TYPE32_SECOND_VTABLE 0x8C23E85Cu
#define TYPE32_TEXANIM       0x8C0996E4u /* the sprite's NJS_TEXANIM (static) */
#define RIPPLE_DAMPING       F32_BITS(0x3F733333u) /* 0.95: a new ripple's Body damping */
#define SPLASH_CELL_LAST     3           /* the splash cells 1..3 cycle */
#define SPLASH_GROW_STEP     F32(1.0f)
#define RING_GROW_LIMIT      F32(120.0f) /* the ring's stage 2 grows to this */
#define RING_WAVE_PERIOD     20          /* the ring's size while it waits, by its timer modulo this */

/* statics of the binary the unit reads (not folded into the code) */
#define g_type32_ripples      GUEST_VAR(int32_t, 0x8C08A328u) /* 32: the ripples in use */
#define g_type32_splash_limit GUEST_VAR(f32, 0x8C08A348u)     /* 20: a splash's stage 2 grows to this */
#define g_type32_push_bounce  GUEST_VAR(f32, 0x8C08A350u)     /* 0.92592591 */
#define g_type32_sprite_attr  GUEST_VAR(uint32_t, 0x8C08A33Cu) /* 0x32: njDrawSprite3D's attribute */
#define g_type32_splash_scale GUEST_VAR(f32, 0x8C08A340u)     /* 0.078125: a splash's sprite scale */
#define g_type32_splash_wait  GUEST_VAR(int32_t, 0x8C08A334u) /* 20: a new splash's timer */
#define g_type32_splash_push  GUEST_VAR(f32, 0x8C08A34Cu)     /* 0.4: a new splash's push, times its offset */
#define g_type32_radius       GUEST_VAR(f32, 0x8C08A310u)     /* 5: the contacts' radius */
#define g_type32_object_a     GUEST_VAR(int32_t, 0x8C08A318u) /* 0: the first pair's object (and the next) */
#define g_type32_object_b     GUEST_VAR(int32_t, 0x8C08A31Cu) /* 2: the alternate pair's */
#define g_type32_motion_a     GUEST_VAR(int32_t, 0x8C08A320u) /* 0: the first pair's motion entry (and the next) */
#define g_type32_motion_b     GUEST_VAR(int32_t, 0x8C08A324u) /* 2: the alternate pair's */
#define g_type32_ring_scale   GUEST_VAR(f32, 0x8C08A344u)     /* 1.5625: the ring's sprite scale */

/* the ripple's slot of a block */
static int32_t *ripple_word(gaddr block, int32_t i) { return &((int32_t *)hw_ptr(block))[i]; }
static f32 *ripple_float(gaddr block, int32_t i) { return &((f32 *)hw_ptr(block))[i]; }
static gaddr ripple_body(const ActorType32 *a, int32_t i) { return a->bodies + (uint32_t)i * sizeof(Body); }

/* ---- construction ---- */

/* was func_8C0603B6: type 0x32's constructor: both vtables, then the base */
gaddr actor_type32_construct(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  a->base.vtable = TYPE32_VTABLE;
  a->second_vtable = TYPE32_SECOND_VTABLE;
  actor_type32_base_construct(obj);
  return obj;
}
HW_EXPORT(8c0603b6, RET(actor_type32_construct(ARG0)))

/* was func_8C060EA8: the ripple blocks freed (each that is not 0): cells, timers, stages, the Bodies, sizes, alphas */
void actor_type32_free_ripples(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  if (a->cells != 0) syFree(a->cells);
  if (a->timers != 0) syFree(a->timers);
  if (a->stages != 0) syFree(a->stages);
  if (a->bodies != 0) syFree(a->bodies);
  if (a->sizes != 0) syFree(a->sizes);
  if (a->alphas != 0) syFree(a->alphas);
}
HW_EXPORT(8c060ea8, actor_type32_free_ripples(ARG0))

/* was func_8C060414: the destructor body: the ripples freed, then the contacts */
void actor_type32_destroy(gaddr obj) {
  actor_type32_free_ripples(obj);
  contacts_free(member_addr(obj, ActorType32, contacts));
}
HW_EXPORT(8c060414, actor_type32_destroy(ARG0))

/* was func_8C06042E (both vtables' first entry) */
gaddr actor_type32_delete(gaddr obj, int32_t free_memory) {
  actor_type32_destroy(obj);
  if (free_memory) syFree(obj);
  return obj;
}
HW_EXPORT(8c06042e, RET(actor_type32_delete(ARG0, (int32_t)ARG1)))

/* was func_8C060452 */
gaddr actor_type32_destroy_self(gaddr obj) {
  actor_type32_destroy(obj);
  return obj;
}
HW_EXPORT(8c060452, RET(actor_type32_destroy_self(ARG0)))

/* ---- the ripples ---- */

/* was func_8C060E7A: the sprite unturned, with the actor's textures and the static texanim; every ripple's stage
 * free */
void actor_type32_reset_ripples(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  a->sprite.ang = 0;
  a->sprite.tlist = a->textures;
  a->sprite.tanim = TYPE32_TEXANIM;
  int32_t *stage = ripple_word(a->stages, 0);
  for (int32_t n = g_type32_ripples; n > 0; n--) *stage++ = 0;
}
HW_EXPORT(8c060e7a, actor_type32_reset_ripples(ARG0))

/* was func_8C06155C: the first free ripple (if any) spawned: waiting `wait` steps, size 0, alpha 1, its Body at rest
 * with a damping of 0.95, then at the actor's Body; the sprite unturned; the ripple's cell 0 (the ring) */
void actor_type32_spawn_ripple(gaddr obj, int32_t wait) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  for (int32_t i = 0; i < g_type32_ripples; i++) {
    if (*ripple_word(a->stages, i) != 0) continue;
    gaddr body = ripple_body(a, i);
    *ripple_word(a->timers, i) = wait;
    *ripple_float(a->sizes, i) = F32(0.0f);
    *ripple_float(a->alphas, i) = F32(1.0f);
    *ripple_word(a->stages, i) = 1;
    body_reset(body);
    vec3_fill(member_addr(body, Body, damping), RIPPLE_DAMPING); /* func_8C0600C6: the inline damping setter */
    a->sprite.ang = 0;
    *ripple_word(a->cells, i) = 0;
    vec3_copy_bytes(body, method_self(member_addr(obj, ActorType32, body))); /* the Body's position */
    return;
  }
}
HW_EXPORT(8c06155c, actor_type32_spawn_ripple(ARG0, (int32_t)ARG1))

/* the original's frame of the contact handler (0x18 bytes under the saved PR and r14): the step vector at 0x0C */
typedef struct PushFrame {
  uint32_t args[3]; /* 0x00: the body, the index, the contacts (spilled) */
  Vec3 step;        /* 0x0C */
  uint32_t saved[2]; /* 0x18: PR, r14 */
} PushFrame;
HW_OFFSET(PushFrame, step, 0x0C);
HW_SIZE(PushFrame, 0x20);

/* was func_8C0615F8: the contact handler (Contacts.handler, installed by the init): the body pushed out of contact
 * i along the contact's normal by its depth, and its velocity set to the normal times g_type32_push_bounce */
void actor_type32_contact_push(gaddr contacts, int32_t i, gaddr body) {
  const Contacts *c = contacts_at(contacts);
  gaddr normal = c->normal + (uint32_t)i * sizeof(Vec3);
  gaddr frame = hw_stack_alloc(sizeof(PushFrame));
  PushFrame *f = (PushFrame *)hw_ptr(frame);
  f->args[0] = body;
  f->args[1] = (uint32_t)i;
  f->args[2] = contacts;
  gaddr step = member_addr(frame, PushFrame, step);
  method_self(step); /* func_8C05FD2C: the Vec3's (empty) constructor */
  vec3_scaled(((const f32 *)hw_ptr(c->depth))[i], step, normal);
  vec3_add(body, step);
  vec3_scaled(g_type32_push_bounce, step, normal);
  vec3_copy_bytes(member_addr(body, Body, velocity), step);
  hw_stack_free(sizeof(PushFrame));
}
HW_EXPORT(8c0615f8, actor_type32_contact_push(ARG0, (int32_t)ARG1, ARG2))

/* the ring's size while it waits, by its timer modulo RING_WAVE_PERIOD: up by 6 to 60, then down (the original's
 * switch) */
static const f32 g_ring_wave[RING_WAVE_PERIOD] = {
    {0.0f},  {6.0f},  {12.0f}, {18.0f}, {24.0f}, {30.0f}, {36.0f}, {42.0f}, {48.0f}, {54.0f},
    {60.0f}, {54.0f}, {48.0f}, {42.0f}, {36.0f}, {30.0f}, {24.0f}, {18.0f}, {12.0f}, {6.0f},
};

/* a stage 2 that grows by one a step to `limit`, then passes to stage 3 */
static void ripple_grow(int32_t *stage, f32 *size, f32 limit) {
  if (fgt(limit, *size))
    *size = fadd(*size, SPLASH_GROW_STEP);
  else
    *stage += 1;
}

/* the splash's next cell (1, 2, 3, 1, ...) */
static void splash_next_cell(int32_t *cell) {
  *cell += 1;
  if (*cell > SPLASH_CELL_LAST) *cell = 1;
}

/* was func_8C060F48: one step of every ripple. A splash (cell 1-3) in stage 1 or 2 counts its timer down (to stage
 * 2 when it was already below 0) or grows to g_type32_splash_limit (then stage 3), cycles its cell and steps its
 * Body; in stage 3 it is freed. The ring (cell 0) is put on the actor's Body; in stage 1 it counts down (to stage 2
 * with size 0 when it was already below 0), its size the wave of its timer; in stage 2 it grows to 120; in stage 3
 * it is freed. */
void actor_type32_update_ripples(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  int32_t *cell = ripple_word(a->cells, 0);
  int32_t *timer = ripple_word(a->timers, 0);
  int32_t *stage = ripple_word(a->stages, 0);
  f32 *size = ripple_float(a->sizes, 0);
  gaddr body = a->bodies;
  for (int32_t n = g_type32_ripples; n > 0; n--, size++, stage++, cell++, timer++, body += sizeof(Body)) {
    int32_t c = *cell;
    if (c == 1 || c == 2 || c == 3) {
      switch (*stage) {
      case 1: {
        int32_t t = (*timer)--;
        if (t < 0) *stage += 1;
        splash_next_cell(cell);
        body_step(body);
        break;
      }
      case 2:
        ripple_grow(stage, size, g_type32_splash_limit);
        splash_next_cell(cell);
        body_step(body);
        break;
      case 3:
        *stage = 0;
        break;
      default: /* 0: free */
        break;
      }
    } else if (c == 0) {
      vec3_copy_bytes(body, method_self(member_addr(obj, ActorType32, body))); /* the Body's position */
      switch (*stage) {
      case 1: {
        int32_t t = (*timer)--;
        if (t < 0) {
          *stage += 1;
          *size = F32(0.0f);
        }
        int32_t k = *timer % RING_WAVE_PERIOD; /* signed: a negative timer gives no size */
        if ((uint32_t)k < RING_WAVE_PERIOD) *size = g_ring_wave[k];
        break;
      }
      case 2:
        ripple_grow(stage, size, RING_GROW_LIMIT);
        break;
      case 3:
        *stage = 0;
        break;
      default: /* 0: free */
        break;
      }
    }
  }
}
HW_EXPORT(8c060f48, actor_type32_update_ripples(ARG0))

/* the sprite put on a ripple's Body, scaled by `scale`, with the alpha 1 - size / limit (also kept as the ripple's
 * alpha) as the constant material's last component */
static void ripple_sprite_place(ActorType32 *a, f32 *alpha, f32 size, f32 limit, gaddr body, f32 scale) {
  *alpha = fsub(F32(1.0f), fdiv(size, limit));
  material_set_ff24(F32(1.0f), F32(1.0f), F32(1.0f), *alpha);
  const Body *b = body_at(body);
  a->sprite.p.x = b->position.x; /* the unit's inline getters (func_8C06003E, ...) */
  a->sprite.p.y = b->position.y;
  a->sprite.p.z = b->position.z;
  a->sprite.sx = scale;
  a->sprite.sy = scale;
}

/* was func_8C06111E: draw the ripples: fog off (and the unit's empty call before), the njControl3D flags pushed and
 * set to 0x10, blending modes 8 / 10; every ripple not free drawn as the sprite at its Body, at its cell: a splash
 * (cells 1-3) small, fading as it grows to g_type32_splash_limit, the ring (cell 0) large, fading as it grows to
 * 120 (other cells not drawn); then blending modes 8 / 6, the flags popped, the fog as the current view wants it */
void actor_type32_draw_ripples(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  method_empty(); /* func_8C05FF5A */
  fog_enable(0);
  control3d_push();
  control3d_set(0x10);
  int32_t *cell = ripple_word(a->cells, 0);
  int32_t *stage = ripple_word(a->stages, 0);
  f32 *size = ripple_float(a->sizes, 0);
  f32 *alpha = ripple_float(a->alphas, 0);
  gaddr body = a->bodies;
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 10);
  for (int32_t i = 0; i < g_type32_ripples; i++, cell++, stage++, size++, alpha++, body += sizeof(Body)) {
    if (*stage == 0) continue;
    int32_t c = *cell;
    uint32_t attr;
    if (c == 1 || c == 2 || c == 3) {
      ripple_sprite_place(a, alpha, *size, g_type32_splash_limit, body, g_type32_splash_scale);
      attr = g_type32_sprite_attr;
    } else if (c == 0) {
      ripple_sprite_place(a, alpha, *size, RING_GROW_LIMIT, body, g_type32_ring_scale);
      attr = g_type32_sprite_attr;
    } else {
      continue;
    }
    njDrawSprite3D(member_addr(obj, ActorType32, sprite), *cell, attr); /* through the unit's thunk func_8C05FF4E */
  }
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  control3d_pop();
  view_apply_fog();
}
HW_EXPORT(8c06111e, actor_type32_draw_ripples(ARG0))

/* the original's frame of actor_type32_spawn_splash (0x64 bytes under eleven saved registers and PR): its matrix
 * goes to the Ninja matrix functions */
typedef struct SplashFrame {
  Vec3 push;         /* 0x00: the splash's offset from the actor's Body, scaled into its force */
  Matrix matrix;     /* 0x0C */
  Vec3 point;        /* 0x4C: where the splash starts */
  Vec3 offset;       /* 0x58: (0, 0, 5) before the turn */
  uint32_t saved[12]; /* 0x64: PR, fr12-fr15, r8-r14 */
} SplashFrame;
HW_OFFSET(SplashFrame, matrix, 0x0C);
HW_OFFSET(SplashFrame, point, 0x4C);
HW_OFFSET(SplashFrame, offset, 0x58);
HW_SIZE(SplashFrame, 0x94);

#define SPLASH_CELLS      F32(3.0f)               /* a random cell 1..3 */
#define SPLASH_DISTANCE   F32(5.0f)
#define SPLASH_PITCH      F32_BITS(0x46AAAAAAu)   /* 21845.33: the random pitch's range (120 degrees) */
#define SPLASH_PITCH_LOW  F32_BITS(0xC62AAAAAu)   /* -10922.67: its low end (-60 degrees) */
#define FULL_TURN         F32(65536.0f)           /* Ninja angle units */

/* was func_8C061344: the first free ripple (if any) spawned as a splash: waiting g_type32_splash_wait steps, size
 * 0, alpha 1, its Body at rest with a damping of 0.95; a random cell 1-3; the sprite turned at random; the Body put
 * 5 away from the actor's Body in a random direction (a random heading, a pitch of -60 to 60 degrees), and pushed
 * by g_type32_splash_push times that offset */
void actor_type32_spawn_splash(gaddr obj) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  for (int32_t i = 0; i < g_type32_ripples; i++) {
    if (*ripple_word(a->stages, i) != 0) continue;
    gaddr body = ripple_body(a, i);
    gaddr frame = hw_stack_alloc(sizeof(SplashFrame));
    gaddr push = member_addr(frame, SplashFrame, push);
    gaddr matrix = member_addr(frame, SplashFrame, matrix);
    gaddr point = member_addr(frame, SplashFrame, point);
    gaddr offset = member_addr(frame, SplashFrame, offset);
    *ripple_word(a->timers, i) = g_type32_splash_wait;
    *ripple_float(a->sizes, i) = F32(0.0f);
    *ripple_float(a->alphas, i) = F32(1.0f);
    *ripple_word(a->stages, i) = 1;
    body_reset(body);
    vec3_fill(member_addr(body, Body, damping), RIPPLE_DAMPING); /* func_8C0600C6 */
    *ripple_word(a->cells, i) = f32_to_int(random_timer_scaled(SPLASH_CELLS)) + 1;
    a->sprite.ang = f32_to_int(random_timer_scaled(FULL_TURN));
    method_self(offset); /* func_8C05FD2C: the Vec3's (empty) constructor */
    method_self(point);
    vec3_set(F32(0.0f), F32(0.0f), SPLASH_DISTANCE, offset);
    f32 pitch = fadd(random_timer_scaled(SPLASH_PITCH), SPLASH_PITCH_LOW);
    f32 heading = random_timer_scaled(FULL_TURN);
    matrix_identity(matrix); /* func_8C05FDF4: the matrix's constructor */
    njUnitMatrix(matrix);
    njTranslateV(matrix, method_self(member_addr(obj, ActorType32, body)));
    njRotateY(matrix, f32_to_int(heading));
    njRotateX(matrix, f32_to_int(pitch));
    njCalcPoint(matrix, offset, point);
    vec3_copy_bytes(body, point);
    vec3_copy(push, point);
    vec3_sub(push, method_self(member_addr(obj, ActorType32, body)));
    vec3_scale(g_type32_splash_push, push);
    vec3_add(member_addr(body, Body, force), push);
    hw_stack_free(sizeof(SplashFrame));
    return;
  }
}
HW_EXPORT(8c061344, actor_type32_spawn_splash(ARG0))

/* the objects and motion entries of the actor's lists */
static gaddr type32_object(const ActorType32 *a, int32_t i) { return ((const gaddr *)hw_ptr(a->objects))[i]; }
static const MotionEntry *type32_motion(const ActorType32 *a, int32_t i) {
  return &((const MotionEntry *)hw_ptr(a->motions))[i];
}

/* was func_8C060D1A: draw it: its textures set; at its Body's position, turned about z, x and y by three of its
 * Body1s and scaled, object k drawn with motion entry m at its frame and object k + 1 with the shape motion of
 * entry m + 1 (k, m: the first pair, or with `alternate` the second); then the ripples */
void actor_type32_draw(gaddr obj) {
  const ActorType32 *a = (const ActorType32 *)hw_ptr(obj);
  njSetTexture(a->textures); /* through the unit's thunk func_8C05FF54 */
  matrix_push();
  matrix_translate_v(method_self(member_addr(obj, ActorType32, body))); /* the Body's position */
  matrix_rotate_z(a->turns[5].value); /* the unit's inline getters (func_8C060016) */
  matrix_rotate_x(a->turns[3].value);
  matrix_rotate_y(a->turns[4].value);
  matrix_scale_all(a->scale);
  gaddr frame = member_addr(obj, ActorType32, frame);
  if (a->alternate != 0) {
    motion_draw_l1(type32_object(a, g_type32_object_b), type32_motion(a, g_type32_motion_b)->motion, frame, 1);
    const MotionEntry *next = type32_motion(a, g_type32_motion_b + 1);
    shape_motion_draw_l1(type32_object(a, g_type32_object_b + 1), next->motion, next->shape, frame, 1);
  } else {
    motion_draw_l1(type32_object(a, g_type32_object_a), type32_motion(a, g_type32_motion_a)->motion, frame, 1);
    const MotionEntry *next = type32_motion(a, g_type32_motion_a + 1);
    shape_motion_draw_l1(type32_object(a, g_type32_object_a + 1), next->motion, next->shape, frame, 1);
  }
  matrix_pop();
  actor_type32_draw_ripples(obj);
}
HW_EXPORT(8c060d1a, actor_type32_draw(ARG0))

#define TYPE32_TURN_DAMPING  F32_BITS(0x3F4CCCCCu) /* 0.8: the Body's and the angle Body1s' damping */
#define TYPE32_SCALE_DAMPING F32_BITS(0x3F733333u) /* 0.95: the scale Body1s' damping */
#define TYPE32_WAIT_A        F32(240.0f)  /* the first wait: random 0..240 + random 0..300 - 120 */
#define TYPE32_WAIT_B        F32(300.0f)
#define TYPE32_WAIT_BASE     F32(-120.0f)
#define TYPE32_CONTACT_MASS  F32(0.5f)
#define TYPE32_CONTACT_WORD  4            /* the Contacts' word after it (field_reset_2c) */
#define TYPE32_CONTACT_1C    2
#define TYPE32_CONTACT_PUSH  0x8C0615F8u  /* actor_type32_contact_push, as the guest calls it */

/* the data word (offset, an address once loaded) of a ContainerEntry */
static gaddr entry_data(gaddr entry) { return (gaddr)((const ContainerEntry *)hw_ptr(entry))->offset; }
/* a Body1 at rest, its value and damping given (the unit's inline Quat constructor and setters) */
static void type32_turn_set(gaddr obj, int32_t k, f32 value, f32 damping) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  quat_identity(member_addr(obj, ActorType32, turns[k]));
  a->turns[k].value = value;
  a->turns[k].damping = damping;
}

/* was func_8C060470 (the init method: this, the placement): its kind; its object, texture and motion lists from
 * the group's entries; a random frame of motion 0; the Body at rest at the placement's position (damping 0.8), and
 * that position as its home; the angle Body1s at the placement's angles, the scale Body1s at 1; a random first
 * wait; the contacts set up (radius g_type32_radius, mass 0.5, its cell, the push handler); the ripples reset */
void actor_type32_init(gaddr obj, gaddr placement) {
  ActorType32 *a = (ActorType32 *)hw_ptr(obj);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->kind = (int32_t)(p->param & 0xF);
  a->word28 = 0;
  a->motion = 0;
  a->objects = entry_data(a->base.model_set);
  a->textures = entry_data(a->base.texlist);
  a->motions = entry_data(a->base.word1c);
  const NjMotion *m = (const NjMotion *)hw_ptr(type32_motion(a, a->motion)->motion);
  a->frames = f32_from_uint((uint32_t)(m->nb_frames - 1));
  a->frame = f32_from_int(f32_to_int(random_scaled(a->frames)));
  gaddr body = member_addr(obj, ActorType32, body);
  body_reset(body);
  vec3_set(p->position.x, p->position.y, p->position.z, body);
  vec3_fill(member_addr(body, Body, damping), TYPE32_TURN_DAMPING); /* func_8C0600C6 */
  vec3_copy_bytes(member_addr(obj, ActorType32, home), method_self(body));
  a->v64.x = F32(0.0f);
  a->v64.y = F32(0.0f);
  a->v64.z = F32(0.0f);
  type32_turn_set(obj, 3, p->angle.x, TYPE32_TURN_DAMPING);
  type32_turn_set(obj, 4, p->angle.y, TYPE32_TURN_DAMPING);
  type32_turn_set(obj, 5, p->angle.z, TYPE32_TURN_DAMPING);
  a->scale = F32(1.0f);
  for (int32_t k = 0; k < 3; k++) type32_turn_set(obj, k, a->scale, TYPE32_SCALE_DAMPING);
  a->alternate = 0;
  f32 wait = f32_from_int(f32_to_int(random_scaled(TYPE32_WAIT_A)));
  wait = fadd(wait, f32_from_int(f32_to_int(random_scaled(TYPE32_WAIT_B))));
  wait = fadd(wait, TYPE32_WAIT_BASE);
  a->wait = f32_to_int(wait);
  a->state = 0;
  a->word40 = 0;
  a->word44 = 0;
  a->radius = g_type32_radius;
  Contacts *c = &a->contacts;
  c->count = 0; /* field_reset_2c's copy: the count and the word after the Contacts */
  a->contacts_word = TYPE32_CONTACT_WORD;
  c->unknown18 = F32(1.0f);
  c->radius = a->radius;
  c->mass = TYPE32_CONTACT_MASS;
  c->unknown1c = TYPE32_CONTACT_1C;
  c->cell = a->base.cell;
  c->respond = TYPE32_CONTACT_PUSH;
  actor_type32_reset_ripples(obj);
}
HW_EXPORT(8c060470, actor_type32_init(ARG0, ARG1))
