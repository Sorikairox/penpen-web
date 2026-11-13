/* Pen Pen port, S17 b07: more scenery actor classes (placement types 0x0A, 0x0B, 0x0F, 0x10, 0x26 and 0x27, built by
 * the factory func_8C043D4C, game/actor_factory.c), and declarations of the functions this batch exports. To be
 * merged into game/actor.h. */
#ifndef PENPEN_ACTOR_B07_H
#define PENPEN_ACTOR_B07_H

#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
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
  uint8_t pad_028[0x034 - 0x028];
  Body body;                /* 0x034: the contacts' owner */
  Body1 values[3];          /* 0x064: (0, 0, 0, 1) at construction */
  uint8_t pad_094[0x098 - 0x094];
  Matrix matrix;            /* 0x098: the identity at construction */
  uint8_t pad_0d8[0x0F4 - 0x0D8];
  int32_t shown;            /* 0x0F4: not 0 = drawn (model 9 of the list, by the matrix) */
  Contacts contacts;        /* 0x0F8 */
  int32_t contacts_word;    /* 0x14C */
  gaddr second_vtable;      /* 0x150 */
} ActorType0F;
HW_OFFSET(ActorType0F, body, 0x034);
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
  uint8_t pad_028[0x030 - 0x028];
  Body1 value30;            /* 0x030: (0, 0, 0, 1) at construction */
  uint8_t pad_040[0x04C - 0x040];
  Vec3 point;               /* 0x04C: the contacts' owner */
  Body1 value58;            /* 0x058 */
  uint8_t pad_068[0x06C - 0x068];
  Body1 value6c;            /* 0x06C */
  uint8_t pad_07c[0x0A0 - 0x07C];
  Matrix matrix;            /* 0x0A0: the identity at construction */
  Contacts contacts;        /* 0x0E0 */
  int32_t contacts_word;    /* 0x134 */
  gaddr second_vtable;      /* 0x138 */
} ActorType10;
HW_OFFSET(ActorType10, value30, 0x030);
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
typedef struct ActorType26 {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x030 - 0x028];
  int32_t word30;           /* 0x030: picks the reset's table (0: 0x8C099428 and word100 = word88) */
  uint8_t pad_034[0x03C - 0x034];
  f32 f3c;                  /* 0x03C: the reset's counter (as a float) */
  uint8_t pad_040[0x048 - 0x040];
  Vec3 point;               /* 0x048: the contacts' owner */
  Body body;                /* 0x054: at rest at construction */
  uint8_t pad_084[0x088 - 0x084];
  uint32_t word88;          /* 0x088 */
  uint8_t pad_08c[0x090 - 0x08C];
  Contacts contacts;        /* 0x090: radius g_type26_radius */
  int32_t contacts_word;    /* 0x0E4 */
  uint8_t pad_0e8[0x0FC - 0x0E8];
  int32_t counter;          /* 0x0FC: 0, then f3c at reset */
  uint32_t word100;         /* 0x100: word88, or a global word, at reset */
  gaddr table;              /* 0x104: a static table at reset */
  gaddr blocks[3];          /* 0x108: 0x20 bytes each (syMalloc) */
  gaddr bodies;             /* 0x114: Body[8] or 0 */
  gaddr second_vtable;      /* 0x118 */
} ActorType26;
HW_OFFSET(ActorType26, word30, 0x030);
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
  Quat turns[TYPE27_PARTS]; /* 0x464: (0, 0, 0, 1) at construction */
  Matrix matrix_a;          /* 0x5B4: the identity at construction */
  Matrix matrix_b;          /* 0x5F4: the identity at construction */
  Contacts contacts;        /* 0x634 */
  int32_t contacts_word;    /* 0x688 */
  uint8_t pad_68c[0x694 - 0x68C];
  int32_t state;            /* 0x694: 0 at init */
  gaddr third_vtable;       /* 0x698 */
} ActorType27;
HW_OFFSET(ActorType27, parts, 0x074);
HW_OFFSET(ActorType27, turns, 0x464);
HW_OFFSET(ActorType27, matrix_a, 0x5B4);
HW_OFFSET(ActorType27, matrix_b, 0x5F4);
HW_OFFSET(ActorType27, contacts, 0x634);
HW_OFFSET(ActorType27, contacts_word, 0x688);
HW_OFFSET(ActorType27, state, 0x694);
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
