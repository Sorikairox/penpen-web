/* Pen Pen port, S14: the scenery actors of a race (game/actor.c).
 *
 * The race scene's ActorGroup (0x554 bytes, constructor func_8C0445B0, vtable 0x8C23DB74, a Task at the scene's 0x48)
 * reads the course's actor placements (32-byte records, -1-terminated) and builds one actor per record with the
 * factory func_8C043D4C (by the record's type: 0x34 types, each its own class and size, 0x44 to 0xD34 bytes); it
 * hands each actor the scene, its texture list and model list (func_8C0447BC), then calls its init method with the
 * record, and later draws them all. Every actor class starts with the Actor base (constructor func_8C044424); its
 * vtable is at 0x24, and every class's last word is the vtable of a second base (the constructors store both, then
 * run the Actor constructor).
 * The classes below are the ones whose methods are written; their names say what they do (the types are not mapped
 * to the classes yet). */
#ifndef PENPEN_ACTOR_H
#define PENPEN_ACTOR_H

#include "../core/task.h"
#include "../core/vec.h"
#include "../core/contact.h"

/* an actor placement of the course (Course.actors) */
typedef struct ActorPlacement {
  int32_t type;     /* 0x00: the factory's type; -1 ends the list */
  uint32_t param;   /* 0x04: per class (bit 16: the actor's models are the group's file's; bits 8-11, 12-15: kinds) */
  Vec3 position;    /* 0x08 */
  uint8_t pad_14[0x20 - 0x14];
} ActorPlacement;
HW_OFFSET(ActorPlacement, position, 0x08);
HW_SIZE(ActorPlacement, 0x20);

/* an actor's vtable: {function, this-adjustment} pairs (core/task.h) */
typedef struct ActorVtbl {
  VMethod destroy;  /* 0x00: (this, int free_memory) */
  VMethod init;     /* 0x08: (this, const ActorPlacement *) */
  VMethod draw;     /* 0x10: (this) */
} ActorVtbl;
HW_OFFSET(ActorVtbl, draw, 0x10);
HW_SIZE(ActorVtbl, 0x18);

typedef struct Actor {
  int32_t index;    /* 0x00: its number in the group */
  int32_t cell;     /* 0x04: the course cell it stands on */
  uint32_t flags;   /* 0x08: 0 at construction (bit 1 set by the group's re-init) */
  uint32_t word0c;  /* 0x0C: the group's word at 0x434 */
  gaddr scene;      /* 0x10: the RaceScene */
  gaddr model_set;  /* 0x14 */
  gaddr texlist;    /* 0x18 */
  gaddr word1c;     /* 0x1C */
  gaddr models;     /* 0x20: a list of models (NJS_CNK_OBJECT / NJS_OBJECT addresses) */
  gaddr vtable;     /* 0x24: ActorVtbl */
} Actor;
HW_OFFSET(Actor, scene, 0x10);
HW_OFFSET(Actor, models, 0x20);
HW_OFFSET(Actor, vtable, 0x24);
HW_SIZE(Actor, 0x28);

#define ACTOR_MAX 256

typedef struct ActorGroup {
  Task task;                /* 0x000: vtable 0x8C23DB74; task.parent is the RaceScene */
  gaddr actors[ACTOR_MAX];  /* 0x024: Actor */
  int32_t count;            /* 0x424 */
  uint8_t pad_428[0x430 - 0x428];
  gaddr file;               /* 0x430: the group's data file (its texture list at +4) */
  uint32_t word434;         /* 0x434 */
  gaddr texlist;            /* 0x438 */
  gaddr blocks[(0x554 - 0x43C) / 4]; /* 0x43C: model blocks, 0-terminated */
} ActorGroup;
HW_OFFSET(ActorGroup, actors, 0x024);
HW_OFFSET(ActorGroup, count, 0x424);
HW_OFFSET(ActorGroup, file, 0x430);
HW_OFFSET(ActorGroup, texlist, 0x438);
HW_OFFSET(ActorGroup, blocks, 0x43C);
HW_SIZE(ActorGroup, 0x554);

static inline Actor *actor_at(gaddr a) { return (Actor *)hw_ptr(a); }
static inline const ActorVtbl *actor_vtbl(gaddr a) { return (const ActorVtbl *)hw_ptr(actor_at(a)->vtable); }
static inline ActorGroup *actor_group_at(gaddr a) { return (ActorGroup *)hw_ptr(a); }
/* model i of an actor's model list (a signed index: the original's 32-bit address arithmetic) */
static inline gaddr actor_model(const Actor *a, int32_t i) { return ((const gaddr *)hw_ptr(a->models))[i]; }

/* ---- actor classes ---- */

/* type 3 (constructor func_8C046028): two models side by side, the first turned by an angle, the second by a Body1
 * (func_8C046564 draws it) */
typedef struct ActorPair {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x2C - 0x28];
  Vec3 position_a;          /* 0x2C */
  Vec3 position_b;          /* 0x38 */
  uint8_t pad_44[0x5C - 0x44];
  f32 angle_a;              /* 0x5C: Ninja units */
  uint8_t pad_60[0x6C - 0x60];
  Body1 angle_b;            /* 0x6C */
  gaddr model_a;            /* 0x7C: NJS_OBJECT */
  gaddr model_b;            /* 0x80 */
  uint32_t word84;          /* 0x84 */
  gaddr second_vtable;      /* 0x88 */
} ActorPair;
HW_OFFSET(ActorPair, position_a, 0x2C);
HW_OFFSET(ActorPair, angle_a, 0x5C);
HW_OFFSET(ActorPair, angle_b, 0x6C);
HW_OFFSET(ActorPair, model_b, 0x80);
HW_SIZE(ActorPair, 0x8C);

/* type 4 (constructor func_8C0466F2): a scrolling strip, a model whose texture coordinates scroll (func_8C046778
 * sets it up, func_8C046842 scrolls) */
typedef struct Strip {
  Actor base;               /* 0x00 */
  Vec3 position;            /* 0x28 */
  uint16_t quad_count;      /* 0x34: the model's first mesh set's mesh count */
  uint8_t pad_36[2];
  gaddr uv;                 /* 0x38: the mesh set's vertex_uvs: {u, v} 16-bit pairs, 4 per quad */
  int16_t u0, u1;           /* 0x3C */
  int16_t v0;               /* 0x40 */
  int16_t quad_height;      /* 0x42: v1 - v0 */
  int16_t scroll;           /* 0x44: 0..255 */
  int16_t scroll_speed;     /* 0x46 */
  gaddr model;              /* 0x48: NJS_OBJECT */
  uint32_t sound_a;         /* 0x4C: by kind */
  uint32_t sound_b;         /* 0x50 */
  gaddr second_vtable;      /* 0x54 */
} Strip;
HW_OFFSET(Strip, position, 0x28);
HW_OFFSET(Strip, quad_count, 0x34);
HW_OFFSET(Strip, uv, 0x38);
HW_OFFSET(Strip, scroll_speed, 0x46);
HW_OFFSET(Strip, sound_b, 0x50);
HW_SIZE(Strip, 0x58);

/* a piece of the type-5 actor's 15 (func_8C0473C2 draws them, func_8C046FA0 kicks one) */
typedef struct Piece {
  Body body;                /* 0x00 */
  Vec3 angle;               /* 0x30: about x, y, z */
  Body1 tip;                /* 0x3C: a falling piece tips by it */
  int32_t state;            /* 0x4C: 0 falling, 1, 2 and more: gone */
  uint32_t word50;          /* 0x50 */
  uint32_t alt_model;       /* 0x54: model 8 of the list instead of 9 */
  uint32_t word58;          /* 0x58 */
  uint32_t size;            /* 0x5C: handed to the race scene's visibility test */
  uint8_t pad_60[0xBC - 0x60];
} Piece;
HW_OFFSET(Piece, tip, 0x3C);
HW_OFFSET(Piece, state, 0x4C);
HW_OFFSET(Piece, size, 0x5C);
HW_SIZE(Piece, 0xBC);

#define PIECE_COUNT 15
/* type 5 (constructor func_8C046C50) */
typedef struct Pieces {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x38 - 0x28];
  Piece pieces[PIECE_COUNT]; /* 0x38 */
  uint8_t pad_b3c[0xBD0 - 0xB3C];
  gaddr second_vtable;      /* 0xBD0 */
} Pieces;
HW_OFFSET(Pieces, pieces, 0x38);
HW_SIZE(Pieces, 0xBD4);

/* type 0x1F (constructor func_8C048476): three parts: part 0 (drawn twice, the second time turned half a turn) and
 * parts 1, 2 (func_8C0485D0) */
typedef struct Triple {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x2C - 0x28];
  uint32_t model_a;         /* 0x2C: model of part 0 (in the actor's model list) */
  uint32_t model_b;         /* 0x30: model of parts 1 and 2 */
  Vec3 position[3];         /* 0x34 */
  f32 angle[3];             /* 0x58: about y, Ninja units */
  gaddr second_vtable;      /* 0x64 */
} Triple;
HW_OFFSET(Triple, model_a, 0x2C);
HW_OFFSET(Triple, position, 0x34);
HW_OFFSET(Triple, angle, 0x58);
HW_SIZE(Triple, 0x68);

/* types 0x0E (constructor func_8C047576) and 0x1C (constructor func_8C0494F6): only their constructors are written */
typedef struct ActorType0E {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x40 - 0x28];
  gaddr second_vtable;      /* 0x40 */
} ActorType0E;
HW_SIZE(ActorType0E, 0x44);
typedef struct ActorType1C {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x8C - 0x28];
  gaddr second_vtable;      /* 0x8C */
} ActorType1C;
HW_SIZE(ActorType1C, 0x90);

/* an actor that turns (func_8C04763E; by its address, probably type 0x0E: constructor func_8C047576, 0x44 bytes) */
typedef struct Spinner {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x38 - 0x28];
  f32 angle;                /* 0x38: Ninja units */
} Spinner;
HW_OFFSET(Spinner, angle, 0x38);

/* type 0x1D (constructor func_8C04795C): a chain, a Body with its contacts, a swing and 20 links (func_8C0479EC sets
 * it up) */
#define CHAIN_LINKS 20
typedef struct Chain {
  Actor base;               /* 0x000 */
  Body body;                /* 0x028 */
  Contacts contacts;        /* 0x058: capacity 0: the chain only takes part in others' contacts */
  int32_t contacts_word;    /* 0x0AC: 4 after field_reset_2c (the word after a Contacts) */
  uint32_t param;           /* 0x0B0: the placement's param */
  f32 word_b4;              /* 0x0B4 */
  Body1 swing;              /* 0x0B8 */
  Body1 links[CHAIN_LINKS]; /* 0x0C8 */
  f32 link_a[CHAIN_LINKS];  /* 0x208: from the static table */
  f32 link_b[CHAIN_LINKS];  /* 0x258 */
  gaddr second_vtable;      /* 0x2A8 */
} Chain;
HW_OFFSET(Chain, body, 0x028);
HW_OFFSET(Chain, contacts, 0x058);
HW_OFFSET(Chain, contacts_word, 0x0AC);
HW_OFFSET(Chain, param, 0x0B0);
HW_OFFSET(Chain, swing, 0x0B8);
HW_OFFSET(Chain, links, 0x0C8);
HW_OFFSET(Chain, link_a, 0x208);
HW_OFFSET(Chain, link_b, 0x258);
HW_SIZE(Chain, 0x2AC);

/* type 0x1A (constructor func_8C04B072): a particle effect, per-particle arrays (frame, countdown, state, value,
 * Body), g_effect_slots particles (func_8C04B4DE resets it, func_8C04B5AC steps it) */
typedef struct Effect {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x0EC - 0x028];
  uint32_t word_ec;         /* 0x0EC */
  uint8_t pad_0f0[0x158 - 0x0F0];
  f32 scale_a, scale_b;     /* 0x158 */
  int32_t counter;          /* 0x160 */
  uint32_t word164;         /* 0x164: = word_ec at reset */
  gaddr table;              /* 0x168 */
  uint8_t pad_16c[0x170 - 0x16C];
  gaddr frames;             /* 0x170: int32 per particle: its animation frame, 0..6 */
  gaddr countdowns;         /* 0x174: int32 per particle */
  gaddr states;             /* 0x178: int32 per particle: 1 = waiting, 2 = growing, 3 = ending, 0 = off */
  gaddr values;             /* 0x17C: f32 per particle */
  uint8_t pad_180[0x184 - 0x180];
  gaddr bodies;             /* 0x184: Body per particle */
  gaddr second_vtable;      /* 0x188 */
} Effect;
HW_OFFSET(Effect, word_ec, 0x0EC);
HW_OFFSET(Effect, scale_a, 0x158);
HW_OFFSET(Effect, frames, 0x170);
HW_OFFSET(Effect, values, 0x17C);
HW_OFFSET(Effect, bodies, 0x184);
HW_SIZE(Effect, 0x18C);

/* an effect's settings (func_8C047F38 gives the defaults) */
typedef struct EffectParams {
  uint32_t on;              /* 0x00 */
  uint32_t word04;          /* 0x04 */
  uint32_t word08;          /* 0x08 */
  uint8_t pad_0c[0x14 - 0x0C];
  Vec3 velocity;            /* 0x14 */
  f32 gravity;              /* 0x20 */
  Vec3 scale_a;             /* 0x24 */
  Vec3 scale_b;             /* 0x30 */
  f32 damping;              /* 0x3C */
  f32 spin;                 /* 0x40 */
  gaddr texlist;            /* 0x44 */
  gaddr texture;            /* 0x48 */
  uint8_t pad_4c[0x6C - 0x4C];
  gaddr block_a, block_b;   /* 0x6C */
} EffectParams;
HW_OFFSET(EffectParams, velocity, 0x14);
HW_OFFSET(EffectParams, damping, 0x3C);
HW_OFFSET(EffectParams, texture, 0x48);
HW_OFFSET(EffectParams, block_a, 0x6C);
HW_OFFSET(EffectParams, block_b, 0x70);

/* a holder of two blocks of a size (func_8C047EAA, func_8C047ED0) */
typedef struct BufferPair {
  uint32_t word00;          /* 0x00 */
  uint32_t size;            /* 0x04 */
  uint32_t word08;          /* 0x08 */
  gaddr block_a;            /* 0x0C */
  gaddr block_b;            /* 0x10 */
} BufferPair;
HW_OFFSET(BufferPair, block_b, 0x10);

/* an array of 0x48-byte members built with operator new[] and a rotation (func_8C04C708, func_8C04C780) */
typedef struct MemberArray {
  int32_t count;            /* 0x00 */
  gaddr array;              /* 0x04 */
  uint8_t pad_08[0x14 - 0x08];
  Quat rotation;            /* 0x14 */
} MemberArray;
HW_OFFSET(MemberArray, array, 0x04);
HW_OFFSET(MemberArray, rotation, 0x14);

#endif
