/* Pen Pen port, S17 b07: more scenery actor classes (placement types 0x0A, 0x0B, 0x0F, 0x10, 0x26 and 0x27, built by
 * the factory func_8C043D4C, game/actor_factory.c), and declarations of the functions this batch exports. To be
 * merged into game/actor.h. */
#ifndef PENPEN_ACTOR_B07_H
#define PENPEN_ACTOR_B07_H

#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/ninja.h" /* S18 b05: NjSprite */
#include "actor.h"

/* placement type 0x0A (constructor func_8C05568A, 0x60 bytes; vtable 0x8C23E35C, second 0x8C23E37C; its destructor
 * body is empty): a chunk model whose vertices are moved up and down in groups of five by two ripples (Body1s) */

/* a chunk vertex list's header (8 bytes) and a vertex with its normal (24 bytes) */
typedef struct CnkVertexHeader {
  uint32_t head;            /* 0x00: the chunk type and flags */
  int32_t sizes;            /* 0x04: bits 16-31: the vertex count */
} CnkVertexHeader;
HW_SIZE(CnkVertexHeader, 8);
typedef struct CnkVertexN {
  Vec3 pos;                 /* 0x00 */
  Vec3 normal;              /* 0x0C */
} CnkVertexN;
HW_SIZE(CnkVertexN, 0x18);

typedef struct ActorType0A {
  Actor base;               /* 0x00 */
  int32_t word28;           /* 0x28: 0 at init */
  Body1 a;                  /* 0x2C: a ripple, pulled back to 0 (starts at 1) */
  Body1 b;                  /* 0x3C: a second ripple (starts at 0.6) */
  gaddr model;              /* 0x4C: NJS_CNK_OBJECT: model 11 of the list */
  gaddr heights;            /* 0x50: the address of its vertex list's first vertex's y (CnkVertexN.pos.y) */
  f32 rest_y;               /* 0x54: that y at init */
  int32_t groups;           /* 0x58: the vertex count / 5: each group of five is moved by the ripples */
  gaddr second_vtable;      /* 0x5C */
} ActorType0A;
HW_OFFSET(ActorType0A, a, 0x2C);
HW_OFFSET(ActorType0A, b, 0x3C);
HW_OFFSET(ActorType0A, model, 0x4C);
HW_OFFSET(ActorType0A, heights, 0x50);
HW_OFFSET(ActorType0A, rest_y, 0x54);
HW_OFFSET(ActorType0A, groups, 0x58);
HW_OFFSET(ActorType0A, second_vtable, 0x5C);
HW_SIZE(ActorType0A, 0x60);

/* placement type 0x0B (constructor func_8C0529B2, 0x13C bytes; vtable 0x8C23E1DC, second 0x8C23E1FC): three models:
 * a base turned about y, an arm that rocks about x between -30 degrees and 0 (7 degrees a step), and a part that
 * spins about y (10 degrees a step); two contact sets: a wide one (radius 31, its own response func_8C052D20) at the
 * position, a small one (radius 7.37, room for 3) at the arm's tip */
typedef struct ActorType0B {
  Actor base;               /* 0x000 */
  uint32_t param;           /* 0x028: the placement's param: not 0 = the base is model 6 of the list, else 5 */
  Vec3 position;            /* 0x02C: the placement's; the wide contacts' owner */
  f32 angle_y;              /* 0x038: the placement's angle about y */
  Body body;                /* 0x03C: at rest at the position */
  f32 rock;                 /* 0x06C: the arm's angle about x (Ninja units), -30 to 0 degrees */
  int32_t rising;           /* 0x070: 1 while the arm rocks up (toward 0) */
  f32 spin;                 /* 0x074: the spinning part's angle about y (Ninja units) */
  int32_t word78;           /* 0x078: 0 at init */
  Contacts contacts;        /* 0x07C: owned by the position */
  int32_t contacts_word;    /* 0x0D0: 4 at init */
  Contacts tip_contacts;    /* 0x0D4: owned by the tip, radius 7.37, room for 3 */
  int32_t tip_contacts_word; /* 0x128: 4 at init */
  Vec3 tip;                 /* 0x12C: (0, 8.4, -34.12) turned by angle_y, from the position */
  gaddr second_vtable;      /* 0x138 */
} ActorType0B;
HW_OFFSET(ActorType0B, param, 0x028);
HW_OFFSET(ActorType0B, position, 0x02C);
HW_OFFSET(ActorType0B, angle_y, 0x038);
HW_OFFSET(ActorType0B, body, 0x03C);
HW_OFFSET(ActorType0B, rock, 0x06C);
HW_OFFSET(ActorType0B, rising, 0x070);
HW_OFFSET(ActorType0B, spin, 0x074);
HW_OFFSET(ActorType0B, word78, 0x078);
HW_OFFSET(ActorType0B, contacts, 0x07C);
HW_OFFSET(ActorType0B, contacts_word, 0x0D0);
HW_OFFSET(ActorType0B, tip_contacts, 0x0D4);
HW_OFFSET(ActorType0B, tip_contacts_word, 0x128);
HW_OFFSET(ActorType0B, tip, 0x12C);
HW_OFFSET(ActorType0B, second_vtable, 0x138);
HW_SIZE(ActorType0B, 0x13C);

/* placement type 0x0F (constructor func_8C053E26, 0x154 bytes; vtable 0x8C23E29C, second 0x8C23E2BC): a Body with
 * contacts (radius 4, room for 3), three Body1s and a matrix */
typedef struct ActorType0F {
  Actor base;               /* 0x000 */
  Vec3 home;                /* 0x028: S18 b04: the placement's position */
  Body body;                /* 0x034: the contacts' owner */
  Body1 values[3];          /* 0x064: (0, 0, 0, 1) at construction; S18 b04: [0] its pitch, [1] its yaw (Ninja
                             * units), [2] its height above the body (drawn that much higher) */
  f32 scale;                /* 0x094: S18 b04: 0.5 + 0.2 param (the model's scale; the contacts' radius 4 times it) */
  Matrix matrix;            /* 0x098: the identity at construction */
  f32 rest_yaw;             /* 0x0D8: S18 b04: by param (a table) */
  Vec3 goal;                /* 0x0DC: S18 b04: by param (a table of x, z; y 0): where it ends its run */
  int32_t state;            /* 0x0E8: S18 b04: 0 waits, 1 placed at the first waypoint, 2 speeds up, 3 runs the
                             * waypoints, 4 runs to its goal, 5 rests there, 6 starts over (state 1) */
  int32_t waypoint;         /* 0x0EC: S18 b04: the waypoint it runs to (state 3) */
  int32_t timer;            /* 0x0F0: S18 b04: updates (state 0 counts param * 240 + 1 down) */
  int32_t shown;            /* 0x0F4: not 0 = drawn (model 9 of the list, by the matrix) */
  Contacts contacts;        /* 0x0F8 */
  int32_t contacts_word;    /* 0x14C */
  gaddr second_vtable;      /* 0x150 */
} ActorType0F;
HW_OFFSET(ActorType0F, home, 0x028);
HW_OFFSET(ActorType0F, body, 0x034);
HW_OFFSET(ActorType0F, scale, 0x094);
HW_OFFSET(ActorType0F, rest_yaw, 0x0D8);
HW_OFFSET(ActorType0F, goal, 0x0DC);
HW_OFFSET(ActorType0F, state, 0x0E8);
HW_OFFSET(ActorType0F, waypoint, 0x0EC);
HW_OFFSET(ActorType0F, timer, 0x0F0);
HW_OFFSET(ActorType0F, values, 0x064);
HW_OFFSET(ActorType0F, matrix, 0x098);
HW_OFFSET(ActorType0F, shown, 0x0F4);
HW_OFFSET(ActorType0F, contacts, 0x0F8);
HW_OFFSET(ActorType0F, contacts_word, 0x14C);
HW_OFFSET(ActorType0F, second_vtable, 0x150);
HW_SIZE(ActorType0F, 0x154);

/* placement type 0x10 (constructor func_8C0548E0, 0x13C bytes; vtable 0x8C23E2DC, second 0x8C23E2FC): three Body1s,
 * a matrix and contacts (radius 2.5, room for 3) owned by a point */
typedef struct ActorType10 {
  Actor base;               /* 0x000 */
  int32_t state;            /* 0x028: S18 b05: 0 waiting, 1 opening, 2 reaching out, 3 its tip turning back, 4
                             * drawing in, 5 closing, 6 back to 0 */
  int32_t timer;            /* 0x02C: S18 b05: steps of the state (-180 at the init with a param, else 0) */
  Body1 value30;            /* 0x030: (0, 0, 0, 1) at construction; S18 b05: the doors' swing about y (Ninja units),
                             * damping 0.7 */
  Vec3 home;                /* 0x040: S18 b05: the placement's position */
  Vec3 point;               /* 0x04C: the contacts' owner; S18 b05: the arm's end, moved by the update */
  Body1 value58;            /* 0x058: S18 b05: the tip's turn about x (Ninja units) */
  f32 heading;              /* 0x068: S18 b05: the placement's angle about y */
  Body1 value6c;            /* 0x06C: S18 b05: the arm's length (1 at rest), damping 0.85 */
  uint8_t pad_07c[0x088 - 0x07C];
  f32 reach;                /* 0x088: S18 b05: the length the arm reaches out to (40) */
  f32 pitch;                /* 0x08C: S18 b05: the arm's angle about x (-20 degrees out) */
  f32 yaw;                  /* 0x090: S18 b05: the arm's angle about y (0) */
  gaddr door_left;          /* 0x094: S18 b05: models 13 and 14 with a param, else 15 and 16 */
  gaddr door_right;         /* 0x098 */
  int32_t heard_by;         /* 0x09C: S18 b05: 1 + the player whose sound it played (0: none) */
  Matrix matrix;            /* 0x0A0: the identity at construction; S18 b05: at home, turned by the heading */
  Contacts contacts;        /* 0x0E0 */
  int32_t contacts_word;    /* 0x134 */
  gaddr second_vtable;      /* 0x138 */
} ActorType10;
HW_OFFSET(ActorType10, value30, 0x030);
HW_OFFSET(ActorType10, timer, 0x02C);
HW_OFFSET(ActorType10, home, 0x040);
HW_OFFSET(ActorType10, heading, 0x068);
HW_OFFSET(ActorType10, reach, 0x088);
HW_OFFSET(ActorType10, door_left, 0x094);
HW_OFFSET(ActorType10, heard_by, 0x09C);
HW_OFFSET(ActorType10, point, 0x04C);
HW_OFFSET(ActorType10, value58, 0x058);
HW_OFFSET(ActorType10, value6c, 0x06C);
HW_OFFSET(ActorType10, matrix, 0x0A0);
HW_OFFSET(ActorType10, contacts, 0x0E0);
HW_OFFSET(ActorType10, contacts_word, 0x134);
HW_OFFSET(ActorType10, second_vtable, 0x138);
HW_SIZE(ActorType10, 0x13C);

/* placement type 0x26 (constructor func_8C058C2A, 0x11C bytes; vtable 0x8C23E4DC, second 0x8C23E4FC): a Body,
 * contacts owned by a point (room for 2), three 0x20-byte blocks and 8 Bodies (operator new[]) */
#define TYPE26_BODIES 8
#define TYPE26_STAGES 0 /* S18 b05: the blocks */
#define TYPE26_SIZES  1
#define TYPE26_TIMERS 2
typedef struct ActorType26 {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x02C - 0x028];
  int32_t motion;           /* 0x02C: S18 b05: the entry of `motions` it is drawn with */
  int32_t word30;           /* 0x030: picks the reset's table (0: 0x8C099428 and word100 = word88); S18 b05: also
                             * the frame's speed (0: 1, else 0.5) and its particles' growth and timer */
  f32 frame;                /* 0x034: S18 b05: the motion's frame (handed by address); a particle starts at 0 */
  uint8_t pad_038[0x03C - 0x038];
  union {
    f32 f3c;                /* 0x03C: the reset's counter (as a float) */
    Vec3 angle;             /* 0x03C: S18 b05: about x, y, z (drawn turned z, y, x; x is the sprite's angle) */
  };
  Vec3 point;               /* 0x048: the contacts' owner */
  Body body;                /* 0x054: at rest at construction */
  gaddr object;             /* 0x084: S18 b05: the NJS_OBJECT drawn */
  uint32_t word88;          /* 0x088: S18 b05: its NJS_TEXLIST (the particles' too with word30 0) */
  gaddr motions;            /* 0x08C: S18 b05: MotionEntry[] */
  Contacts contacts;        /* 0x090: radius g_type26_radius */
  int32_t contacts_word;    /* 0x0E4 */
  union {
    NjSprite sprite;        /* 0x0E8: S18 b05: the sprite every particle is drawn with */
    struct {
      uint8_t pad_0e8[0x0FC - 0x0E8];
      int32_t counter;      /* 0x0FC: 0, then f3c at reset (the sprite's angle) */
      uint32_t word100;     /* 0x100: word88, or a global word, at reset (the sprite's texlist) */
      gaddr table;          /* 0x104: a static table at reset (the sprite's texanim) */
    };
  };
  /* blocks[TYPE26_STAGES]: int32_t: 0 free, 1 rising, 2 done (back to 0); blocks[TYPE26_SIZES]: f32: the sprite's
   * size, growing; blocks[TYPE26_TIMERS]: f32: counts up to g_type26_particle_life (S18 b05) */
  gaddr blocks[3];          /* 0x108: 0x20 bytes each (syMalloc) */
  gaddr bodies;             /* 0x114: Body[8] or 0 */
  gaddr second_vtable;      /* 0x118 */
} ActorType26;
HW_OFFSET(ActorType26, word30, 0x030);
HW_OFFSET(ActorType26, motion, 0x02C);
HW_OFFSET(ActorType26, frame, 0x034);
HW_OFFSET(ActorType26, angle, 0x03C);
HW_OFFSET(ActorType26, object, 0x084);
HW_OFFSET(ActorType26, motions, 0x08C);
HW_OFFSET(ActorType26, sprite, 0x0E8);
HW_OFFSET(ActorType26, f3c, 0x03C);
HW_OFFSET(ActorType26, word88, 0x088);
HW_OFFSET(ActorType26, counter, 0x0FC);
HW_OFFSET(ActorType26, word100, 0x100);
HW_OFFSET(ActorType26, table, 0x104);
HW_OFFSET(ActorType26, point, 0x048);
HW_OFFSET(ActorType26, body, 0x054);
HW_OFFSET(ActorType26, contacts, 0x090);
HW_OFFSET(ActorType26, contacts_word, 0x0E4);
HW_OFFSET(ActorType26, blocks, 0x108);
HW_OFFSET(ActorType26, bodies, 0x114);
HW_OFFSET(ActorType26, second_vtable, 0x118);
HW_SIZE(ActorType26, 0x11C);

/* placement type 0x27 (constructor func_8C05A0A8, 0x69C bytes; vtables 0x8C23E59C, 0x8C23E5BC (at the Flyer's
 * second-vtable slot) and 0x8C23E5DC): a Flyer with 21 Bodies, 21 Quats, two matrices and contacts owned by the
 * Flyer's Body (radius 10, room for 2) */
#define TYPE27_PARTS 21
typedef struct ActorType27 {
  Flyer flyer;              /* 0x000 */
  Body parts[TYPE27_PARTS]; /* 0x074: at rest at construction */
  Body1 turns[TYPE27_PARTS]; /* 0x464: (0, 0, 0, 1) at construction (the Quat constructor); S18 b05: each part's
                             * angle about x (its value; damping 0.98 at the init) */
  Matrix matrix_a;          /* 0x5B4: the identity at construction; S18 b05: the view matrix the parts are drawn
                             * in (njGetMatrix) */
  Matrix matrix_b;          /* 0x5F4: the identity at construction; S18 b05: the parts' tree matrix (unit at the
                             * draw's start, each node's own transform multiplied on for its children) */
  Contacts contacts;        /* 0x634 */
  int32_t contacts_word;    /* 0x688 */
  int32_t phase;            /* 0x68C: S18 b05: 0 the Flyer (until it is touched), 1 its parts fly apart, 2 gone */
  int32_t timer;            /* 0x690: S18 b05: steps of phase 1 (phase 2 after 180) */
  int32_t part_next;        /* 0x694: 0 at init; S18 b05: the next part the draw's node walk takes */
  gaddr third_vtable;       /* 0x698 */
} ActorType27;
HW_OFFSET(ActorType27, parts, 0x074);
HW_OFFSET(ActorType27, turns, 0x464);
HW_OFFSET(ActorType27, matrix_a, 0x5B4);
HW_OFFSET(ActorType27, matrix_b, 0x5F4);
HW_OFFSET(ActorType27, contacts, 0x634);
HW_OFFSET(ActorType27, contacts_word, 0x688);
HW_OFFSET(ActorType27, phase, 0x68C);
HW_OFFSET(ActorType27, timer, 0x690);
HW_OFFSET(ActorType27, part_next, 0x694);
HW_OFFSET(ActorType27, third_vtable, 0x698);
HW_SIZE(ActorType27, 0x69C);

/* ---- S17 b07: exported functions ---- */
/* core/ninja_b07.c */
void matrix_load(gaddr m);
gaddr matrix_assign(gaddr dst, gaddr src);
void matrix_scale_on(gaddr m, f32 x, f32 y, f32 z);
void matrix_multiply_on(gaddr m, gaddr by);
f32 vec3_distance2(gaddr from, gaddr to);
/* game/actor_type0b.c */
gaddr actor_type0b_base_construct(gaddr obj);
gaddr actor_type0b_construct(gaddr obj);
void actor_type0b_destroy(gaddr obj);
gaddr actor_type0b_delete(gaddr obj, int32_t free_memory);
gaddr actor_type0b_destroy_self(gaddr obj);
void actor_type0b_init(gaddr obj, gaddr placement);
void actor_type0b_update(gaddr obj);
void actor_type0b_draw(gaddr obj);
/* game/actor_types_b07.c */
gaddr actor_type0a_base_construct(gaddr obj);
gaddr actor_type0a_construct(gaddr obj);
void actor_type0a_init(gaddr obj);
void actor_type0a_update(gaddr obj);
void actor_type0a_draw(gaddr obj);
void actor_type0f_draw(gaddr obj);
gaddr actor_type0f_base_construct(gaddr obj);
gaddr actor_type0f_construct(gaddr obj);
void actor_type0f_destroy(gaddr obj);
gaddr actor_type0f_delete(gaddr obj, int32_t free_memory);
gaddr actor_type0f_destroy_self(gaddr obj);
gaddr actor_type10_base_construct(gaddr obj);
gaddr actor_type10_construct(gaddr obj);
void actor_type10_destroy(gaddr obj);
gaddr actor_type10_delete(gaddr obj, int32_t free_memory);
gaddr actor_type10_destroy_self(gaddr obj);
gaddr actor_type26_base_construct(gaddr obj);
gaddr actor_type26_construct(gaddr obj);
void actor_type26_destroy(gaddr obj);
gaddr actor_type26_delete(gaddr obj, int32_t free_memory);
gaddr actor_type26_destroy_self(gaddr obj);
void actor_type26_reset(gaddr obj);
gaddr actor_type27_base_construct(gaddr obj);
gaddr actor_type27_construct(gaddr obj);
void actor_type27_destroy(gaddr obj);
gaddr actor_type27_delete(gaddr obj, int32_t free_memory);
gaddr actor_type27_destroy_self(gaddr obj);
/* S18 b05: hand-written elsewhere, not in a header yet */
void flyer_update(gaddr obj);                         /* game/actor_flyer.c */
void flyer_pose(gaddr obj);                           /* game/actor_flyer.c */
void flyer_draw(gaddr obj);                           /* game/actor_flyer.c */
void resting_actor_init(gaddr actor, gaddr placement); /* game/actor_bodies.c */
/* S18 b05: game/actor_type10.c */
void actor_type10_init(gaddr obj, gaddr placement);
void actor_type10_update(gaddr obj);
void actor_type10_draw(gaddr obj);
/* S18 b05: game/actor_type26.c */
void actor_type26_update(gaddr obj);
void actor_type26_draw(gaddr obj);
void actor_type26_step_particles(gaddr obj);
void actor_type26_draw_particles(gaddr obj);
void actor_type26_spawn_particle(gaddr obj);
/* S18 b05: game/actor_type27.c */
void actor_type27_init(gaddr obj, gaddr placement);
void actor_type27_update(gaddr obj);
void actor_type27_draw(gaddr obj);
void actor_type27_draw_parts(gaddr obj);
void actor_type27_draw_part(gaddr obj, gaddr node);
void material_set_89b8(f32 r, f32 g, f32 b, f32 a);
/* game/scenery_b07.c */
gaddr actor_b_delete(gaddr obj, int32_t free_memory); /* game/actor.c: exported there, declared here */
void sign_destroy(gaddr obj);                          /* game/actor_sign.c: exported there, declared here */
gaddr flyer_base_construct(gaddr obj);                 /* game/actor_flyer.c: exported there, declared here */
void bumper_draw(gaddr obj);
gaddr flap_delete(gaddr obj, int32_t free_memory);
gaddr flap_destroy_self(gaddr obj);
void flap_destroy(gaddr obj);
void flap_draw(gaddr obj);
void flock_destroy(gaddr obj);
gaddr flock_delete(gaddr obj, int32_t free_memory);
gaddr flock_destroy_self(gaddr obj);
void flock_draw(gaddr obj);
void turning_gate_destroy(gaddr obj);
gaddr turning_gate_delete(gaddr obj, int32_t free_memory);
gaddr turning_gate_destroy_self(gaddr obj);
void turning_gate_draw(gaddr obj);
void geyser_destroy(gaddr obj);
gaddr geyser_delete(gaddr obj, int32_t free_memory);
gaddr geyser_destroy_self(gaddr obj);
void swimmer_destroy(gaddr obj);
gaddr swimmer_delete(gaddr obj, int32_t free_memory);
gaddr swimmer_destroy_self(gaddr obj);
gaddr sign_destroy_self(gaddr obj);
void buoy_destroy(gaddr obj);
gaddr buoy_delete(gaddr obj, int32_t free_memory);
gaddr buoy_destroy_self(gaddr obj);

#endif
