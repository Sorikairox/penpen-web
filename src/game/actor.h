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
  Vec3 angle;       /* 0x14: S14 b10: about x, y, z (Ninja units, as floats) */
} ActorPlacement;
HW_OFFSET(ActorPlacement, position, 0x08);
HW_OFFSET(ActorPlacement, angle, 0x14);
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
  Vec3 center;              /* 0x44: S14 b10: a player nearer than 60 turns both parts toward their targets */
  f32 rest_angle;           /* 0x50: where both parts turn back to when no player is near */
  f32 target_a;             /* 0x54 */
  f32 target_b;             /* 0x58 */
  union {
    f32 angle_a;            /* 0x5C: Ninja units (the value of turn_a) */
    Body1 turn_a;           /* 0x5C */
  };
  Body1 angle_b;            /* 0x6C */
  gaddr model_a;            /* 0x7C: NJS_OBJECT */
  gaddr model_b;            /* 0x80 */
  int32_t sounded;          /* 0x84: S14 b10: the opening sound was played (cleared while no player is near) */
  gaddr second_vtable;      /* 0x88 */
} ActorPair;
HW_OFFSET(ActorPair, position_a, 0x2C);
HW_OFFSET(ActorPair, center, 0x44);
HW_OFFSET(ActorPair, target_b, 0x58);
HW_OFFSET(ActorPair, angle_a, 0x5C);
HW_OFFSET(ActorPair, turn_a, 0x5C);
HW_OFFSET(ActorPair, sounded, 0x84);
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
  uint32_t sound_a;         /* 0x4C: by kind (S14 b10: the course row bit the draw tests, func_8C0468C4) */
  uint32_t sound_b;         /* 0x50 (S14 b10: the race scene's view bit the draw tests) */
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
  uint8_t pad_60[0x64 - 0x60];
  Contacts contacts;        /* 0x64: S14 b10: owned by the body, radius 10, capacity 3 */
  uint8_t pad_b8[0xBC - 0xB8];
} Piece;
HW_OFFSET(Piece, tip, 0x3C);
HW_OFFSET(Piece, state, 0x4C);
HW_OFFSET(Piece, size, 0x5C);
HW_OFFSET(Piece, contacts, 0x64);
HW_SIZE(Piece, 0xBC);

#define PIECE_COUNT 15
/* type 5 (constructor func_8C046C50) */
typedef struct Pieces {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x38 - 0x28];
  Piece pieces[PIECE_COUNT]; /* 0x38 */
  uint8_t pad_b3c[0xB40 - 0xB3C];
  Body body;                /* 0xB40: S14 b10 */
  Contacts contacts;        /* 0xB70: owned by the body, radius 16, capacity 4 */
  uint8_t pad_bc4[0xBD0 - 0xBC4];
  gaddr second_vtable;      /* 0xBD0 */
} Pieces;
HW_OFFSET(Pieces, pieces, 0x38);
HW_OFFSET(Pieces, body, 0xB40);
HW_OFFSET(Pieces, contacts, 0xB70);
HW_SIZE(Pieces, 0xBD4);

/* type 0x1F (constructor func_8C048476): three parts: part 0 (drawn twice, the second time turned half a turn) and
 * parts 1, 2 (func_8C0485D0) */
typedef struct Triple {
  Actor base;               /* 0x00 */
  uint32_t param;           /* 0x28: S14 b10: the placement's param: 0 at the start segment, else at the finish */
  uint32_t model_a;         /* 0x2C: model of part 0 (in the actor's model list) */
  uint32_t model_b;         /* 0x30: model of parts 1 and 2 */
  Vec3 position[3];         /* 0x34 */
  f32 angle[3];             /* 0x58: about y, Ninja units */
  gaddr second_vtable;      /* 0x64 */
} Triple;
HW_OFFSET(Triple, param, 0x28);
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
/* S14 b10: an entry of a model's motion table (12 bytes) */
typedef struct MotionEntry {
  gaddr motion;             /* 0x00: NJS_MOTION (0 = none) */
  gaddr shape;              /* 0x04: its shape motion (0 = none) */
  uint32_t word08;          /* 0x08 */
} MotionEntry;
HW_SIZE(MotionEntry, 12);
/* S14 b10: type 0x1C draws a model with one of its motions (func_8C049640) */
typedef struct ActorType1C {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x34 - 0x28];
  int32_t motion;           /* 0x34: the entry of the motion table */
  f32 frame;                /* 0x38: the motion's frame (handed by address) */
  uint8_t pad_3c[0x48 - 0x3C];
  Vec3 angle;               /* 0x48: about x, y, z (drawn turned z, x, y) */
  gaddr object;             /* 0x54: NJS_OBJECT */
  gaddr texlist;            /* 0x58 */
  gaddr motions;            /* 0x5C: MotionEntry table */
  uint8_t pad_60[0x80 - 0x60];
  Vec3 position;            /* 0x80 */
  gaddr second_vtable;      /* 0x8C */
} ActorType1C;
HW_OFFSET(ActorType1C, motion, 0x34);
HW_OFFSET(ActorType1C, angle, 0x48);
HW_OFFSET(ActorType1C, motions, 0x5C);
HW_OFFSET(ActorType1C, position, 0x80);
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
  f32 word_b4;              /* 0x0B4: S14 b10: an angle, 5 degrees less every step */
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
  int32_t kind;             /* 0x028: S14 b10: bits 8-11 of the placement's param: picks the model */
  int32_t motion;           /* 0x02C: by kind: 0, 0, 1, 2: the entry of the motion table */
  int32_t subkind;          /* 0x030: bits 0-3 of the param */
  uint8_t pad_034[0x040 - 0x034];
  f32 frame;                /* 0x040: the motion's frame, 0..frame_count */
  f32 frame_count;          /* 0x044: the motion's frame count - 1 */
  Body1 pulse[3];           /* 0x048: pushed by a hit, pulled back to 1 */
  Body1 angle[3];           /* 0x078: about x, y, z (the placement's angles) */
  Body body;                /* 0x0A8: pulled toward home */
  Vec3 home;                /* 0x0D8 */
  uint8_t pad_0e4[0x0E8 - 0x0E4];
  gaddr model;              /* 0x0E8: NJS_OBJECT (model_set's list, by kind) */
  uint32_t word_ec;         /* 0x0EC: texlist's second word */
  gaddr motions;            /* 0x0F0: MotionEntry table (word1c's second word) */
  Contacts contacts;        /* 0x0F4: owned by the body, radius 6, capacity 2 */
  int32_t contacts_word;    /* 0x148 */
  uint8_t pad_14c[0x158 - 0x14C];
  f32 scale_a, scale_b;     /* 0x158 */
  int32_t counter;          /* 0x160 */
  uint32_t word164;         /* 0x164: = word_ec at reset */
  gaddr table;              /* 0x168 */
  uint8_t pad_16c[0x170 - 0x16C];
  gaddr frames;             /* 0x170: int32 per particle: its animation frame, 0..6 */
  gaddr countdowns;         /* 0x174: int32 per particle */
  gaddr states;             /* 0x178: int32 per particle: 1 = waiting, 2 = growing, 3 = ending, 0 = off */
  gaddr values;             /* 0x17C: f32 per particle */
  gaddr extra;              /* 0x180: a word per particle (S14 b10: allocated by the constructor) */
  gaddr bodies;             /* 0x184: Body per particle */
  gaddr second_vtable;      /* 0x188 */
} Effect;
HW_OFFSET(Effect, kind, 0x028);
HW_OFFSET(Effect, frame, 0x040);
HW_OFFSET(Effect, pulse, 0x048);
HW_OFFSET(Effect, angle, 0x078);
HW_OFFSET(Effect, body, 0x0A8);
HW_OFFSET(Effect, home, 0x0D8);
HW_OFFSET(Effect, model, 0x0E8);
HW_OFFSET(Effect, word_ec, 0x0EC);
HW_OFFSET(Effect, motions, 0x0F0);
HW_OFFSET(Effect, contacts, 0x0F4);
HW_OFFSET(Effect, contacts_word, 0x148);
HW_OFFSET(Effect, extra, 0x180);
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

/* S14 b10: a member of a MemberArray (0x48 bytes): a mote that waits, then rises from the array's origin, its
 * sprite frame stepping 0..9 (func_8C04C7DC, func_8C04C986) */
typedef struct Mote {
  Body body;                /* 0x00 */
  int32_t frame_timer;      /* 0x30 */
  int32_t frame;            /* 0x34: 0..9 */
  int32_t loop_frame;       /* 0x38: where the frame goes back to after 9 */
  int32_t state;            /* 0x3C: 0 waiting, 1 rising, 2 done */
  int32_t delay;            /* 0x40: steps left to wait */
  int32_t kind;             /* 0x44 */
} Mote;
HW_OFFSET(Mote, frame_timer, 0x30);
HW_OFFSET(Mote, kind, 0x44);
HW_SIZE(Mote, 0x48);

/* an array of 0x48-byte members built with operator new[] and a rotation (func_8C04C708, func_8C04C780);
 * S14 b10: a class (vtable at 0x34, constructor func_8C04C76A): a set of Motes */
typedef struct MemberArray {
  int32_t count;            /* 0x00 */
  gaddr array;              /* 0x04: Mote[count] */
  Vec3 origin;              /* 0x08 */
  union {
    Quat rotation;          /* 0x14 */
    Body1 fade;             /* 0x14: S14 b10: pulled toward 0 while the motes rise */
  };
  f32 top;                  /* 0x24: a mote above it is done */
  int32_t mode;             /* 0x28: how the kinds are dealt (0: the first 1, the others 3; 1: alternate) */
  gaddr texlist;            /* 0x2C */
  gaddr texanim;            /* 0x30 */
  gaddr vtable;             /* 0x34 */
} MemberArray;
HW_OFFSET(MemberArray, array, 0x04);
HW_OFFSET(MemberArray, origin, 0x08);
HW_OFFSET(MemberArray, rotation, 0x14);
HW_OFFSET(MemberArray, fade, 0x14);
HW_OFFSET(MemberArray, top, 0x24);
HW_OFFSET(MemberArray, texanim, 0x30);
HW_OFFSET(MemberArray, vtable, 0x34);
HW_SIZE(MemberArray, 0x38);

/* ---- S14 b10: more actor classes (the factory's type, its constructor and size) ---- */

/* a Contacts with the word after it (the word field_reset_2c sets to 4): an element of an array */
typedef struct ContactsSlot {
  Contacts contacts;        /* 0x00 */
  int32_t word;             /* 0x54 */
} ContactsSlot;
HW_SIZE(ContactsSlot, 0x58);

/* type 7 (constructor func_8C04C1E8, 0x11C bytes): three bubbles that rise from the origin one after the other
 * (func_8C04C2CC starts one, func_8C04C3BA steps them) */
typedef struct BubbleSprite {
  int16_t w, h;             /* 0x00: 8, 8 */
  int16_t cx, cy;           /* 0x04: 4, 4 */
  int16_t u1, v1;           /* 0x08: 0, 0 */
  int16_t u2, v2;           /* 0x0C: 255, 255 */
  int16_t texid;            /* 0x10 */
  int16_t attr;             /* 0x12 */
} BubbleSprite;
HW_SIZE(BubbleSprite, 20);
#define BUBBLE_COUNT 3
typedef struct Bubbles {
  Actor base;               /* 0x000 */
  Vec3 origin;              /* 0x028 */
  Body bodies[BUBBLE_COUNT]; /* 0x034 */
  BubbleSprite sprites[BUBBLE_COUNT]; /* 0x0C4 */
  int32_t timer[BUBBLE_COUNT]; /* 0x100: counts up from -60 i (minus 20 param); past 240 the bubble starts again */
  int32_t shown[BUBBLE_COUNT]; /* 0x10C: 1 once the timer is past 0 */
  gaddr second_vtable;      /* 0x118 */
} Bubbles;
HW_OFFSET(Bubbles, bodies, 0x034);
HW_OFFSET(Bubbles, sprites, 0x0C4);
HW_OFFSET(Bubbles, timer, 0x100);
HW_OFFSET(Bubbles, shown, 0x10C);
HW_SIZE(Bubbles, 0x11C);

/* type 8 (constructor func_8C04CB26, 0x78 bytes): a spring that waits, then sends five motes up, again and again
 * (func_8C04CC10) */
typedef struct Spring {
  Actor base;               /* 0x00 */
  int32_t state;            /* 0x28: 0 waiting, 1 the motes rise, 2 start again */
  int32_t countdown;        /* 0x2C: the first wait (the placement's param) */
  Vec3 position;            /* 0x30 */
  MemberArray motes;        /* 0x3C: 5 Motes */
  gaddr second_vtable;      /* 0x74 */
} Spring;
HW_OFFSET(Spring, position, 0x30);
HW_OFFSET(Spring, motes, 0x3C);
HW_SIZE(Spring, 0x78);

/* type 0x23 (constructor func_8C0487C4, 0x40 bytes): a model turned about y (kinds 0 and 1: models 6 and 7 of the
 * list); kind 2 turns and plays a sound when a player passes cells 125-129 (func_8C048882) */
typedef struct Turner {
  Actor base;               /* 0x00 */
  Vec3 position;            /* 0x28 */
  int32_t kind;             /* 0x34: the placement's param */
  f32 angle;                /* 0x38: about y */
  gaddr second_vtable;      /* 0x3C */
} Turner;
HW_OFFSET(Turner, kind, 0x34);
HW_OFFSET(Turner, angle, 0x38);
HW_SIZE(Turner, 0x40);

/* type 0x2B (constructor func_8C049CB0, 0x1B8 bytes): only its construction is written */
typedef struct ActorType2B {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x05C - 0x028];
  Body1 values[6];          /* 0x05C */
  Body body;                /* 0x0BC */
  Vec3 points_a[2];         /* 0x0EC */
  Vec3 points_b[2];         /* 0x104 */
  uint8_t pad_11c[0x15C - 0x11C];
  Contacts contacts;        /* 0x15C: owned by the body, radius 5, capacity 2 */
  int32_t contacts_word;    /* 0x1B0 */
  gaddr second_vtable;      /* 0x1B4 */
} ActorType2B;
HW_OFFSET(ActorType2B, body, 0x0BC);
HW_OFFSET(ActorType2B, points_b, 0x104);
HW_OFFSET(ActorType2B, contacts, 0x15C);
HW_SIZE(ActorType2B, 0x1B8);

/* type 0x2E (constructor func_8C04D220, 0xB0 bytes): a post leaning by its angles, its tip computed once
 * (func_8C04D2A4) */
typedef struct Post {
  Actor base;               /* 0x00 */
  Vec3 position;            /* 0x28: the contacts' owner */
  Vec3 angle;               /* 0x34: the placement's angles */
  Vec3 tip;                 /* 0x40 */
  int32_t word4c;           /* 0x4C */
  f32 f50;                  /* 0x50 */
  Contacts contacts;        /* 0x54: radius 25, no capacity */
  int32_t contacts_word;    /* 0xA8 */
  gaddr second_vtable;      /* 0xAC */
} Post;
HW_OFFSET(Post, angle, 0x34);
HW_OFFSET(Post, tip, 0x40);
HW_OFFSET(Post, contacts, 0x54);
HW_OFFSET(Post, contacts_word, 0xA8);
HW_SIZE(Post, 0xB0);

/* type 0x1E (constructor func_8C04D924, 0x3F8 bytes): a big body with eight small contact spheres on points of it
 * (func_8C04D99C sets it up) */
#define BALLS_POINTS 8
typedef struct Balls {
  Actor base;               /* 0x000 */
  Vec3 position;            /* 0x028 */
  f32 f34;                  /* 0x034 */
  uint32_t param;           /* 0x038 */
  int32_t state;            /* 0x03C: 1 when param is 0 */
  gaddr table;              /* 0x040 */
  int32_t word44;           /* 0x044 */
  f32 f48;                  /* 0x048 */
  Body1 swing;              /* 0x04C */
  Contacts contacts;        /* 0x05C: owned by the position, radius 122.35 */
  int32_t contacts_word;    /* 0x0B0 */
  ContactsSlot spheres[BALLS_POINTS]; /* 0x0B4: each owned by its point, radius 5.5 / 4.5, capacity 4 */
  Vec3 points[BALLS_POINTS]; /* 0x374 */
  int32_t cells[BALLS_POINTS]; /* 0x3D4 */
  gaddr second_vtable;      /* 0x3F4 */
} Balls;
HW_OFFSET(Balls, param, 0x038);
HW_OFFSET(Balls, table, 0x040);
HW_OFFSET(Balls, swing, 0x04C);
HW_OFFSET(Balls, contacts, 0x05C);
HW_OFFSET(Balls, contacts_word, 0x0B0);
HW_OFFSET(Balls, spheres, 0x0B4);
HW_OFFSET(Balls, points, 0x374);
HW_OFFSET(Balls, cells, 0x3D4);
HW_SIZE(Balls, 0x3F8);

/* type 0x33 (constructor func_8C04FAB4, 0x12C bytes): something that wobbles when a fast body touches it
 * (func_8C04FE68) */
typedef struct Wobbler {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x040 - 0x028];
  f32 clock;                /* 0x040: + 1 each step */
  uint8_t pad_044[0x048 - 0x044];
  f32 rest;                 /* 0x048: the value its three Body1s are pulled toward */
  uint8_t pad_04c[0x058 - 0x04C];
  Vec3 position;            /* 0x058: the contacts' owner */
  Body1 wobble[3];          /* 0x064 */
  Body body;                /* 0x094 */
  uint8_t pad_0c4[0x0D0 - 0x0C4];
  Contacts contacts;        /* 0x0D0: owned by the position, radius 6, capacity 2 */
  int32_t contacts_word;    /* 0x124 */
  gaddr second_vtable;      /* 0x128 */
} Wobbler;
HW_OFFSET(Wobbler, clock, 0x040);
HW_OFFSET(Wobbler, rest, 0x048);
HW_OFFSET(Wobbler, position, 0x058);
HW_OFFSET(Wobbler, wobble, 0x064);
HW_OFFSET(Wobbler, body, 0x094);
HW_OFFSET(Wobbler, contacts, 0x0D0);
HW_SIZE(Wobbler, 0x12C);

/* type 0x19 (constructor func_8C050504, 0x1BC bytes): S14 b10: its construction and its update (func_8C050A74) */
typedef struct ActorType19 {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x030 - 0x028];
  int32_t slow;             /* 0x030: 0 = the clock runs at double speed */
  int32_t state;            /* 0x034: picks the step (func_8C050BAE, func_8C05106C, func_8C0512A4, func_8C051548) */
  uint8_t pad_038[0x050 - 0x038];
  f32 clock;                /* 0x050 */
  uint8_t pad_054[0x064 - 0x054];
  Vec3 points[3];           /* 0x064 */
  Quat turns[3];            /* 0x088 */
  Body body;                /* 0x0B8 */
  uint8_t pad_0e8[0x0F4 - 0x0E8];
  Contacts contacts;        /* 0x0F4: owned by the body, radius 2, capacity 2 */
  int32_t contacts_word;    /* 0x148 */
  Vec3 point14c;            /* 0x14C */
  uint8_t pad_158[0x1B8 - 0x158];
  gaddr second_vtable;      /* 0x1B8 */
} ActorType19;
HW_OFFSET(ActorType19, state, 0x034);
HW_OFFSET(ActorType19, clock, 0x050);
HW_OFFSET(ActorType19, points, 0x064);
HW_OFFSET(ActorType19, turns, 0x088);
HW_OFFSET(ActorType19, body, 0x0B8);
HW_OFFSET(ActorType19, contacts, 0x0F4);
HW_OFFSET(ActorType19, point14c, 0x14C);
HW_SIZE(ActorType19, 0x1BC);

/* what Actor.model_set, texlist and word1c point at: a word, then the list (S14 b10: the effect reads the second
 * word of each) */
typedef struct ActorList {
  uint32_t word0;           /* 0x00 */
  gaddr list;               /* 0x04 */
} ActorList;
HW_SIZE(ActorList, 8);

/* S14 b10: a burst of particles (0x78 bytes, constructor func_8C047EB6, vtable at 0x74, func_8C048018 steps it):
 * the object BufferPair (its first 0x14 bytes) and EffectParams (the settings) above are views of; embedded in
 * several actors (their constructors func_8C045876, func_8C04BB32, func_8C04CD5A) */
typedef struct Burst {
  int32_t state;            /* 0x00: 0 idle until triggered, 1 the particles fly, 2 over */
  int32_t count;            /* 0x04: particles */
  int32_t trigger;          /* 0x08: set to start the burst */
  gaddr alive;              /* 0x0C: int32 per particle */
  gaddr bodies;             /* 0x10: Body per particle */
  Vec3 origin;              /* 0x14: where the particles start (func_8C048222) */
  f32 depth;                /* 0x20: a particle this far below the origin dies (-10) */
  Vec3 velocity_max;        /* 0x24: each particle's starting velocity is random between these */
  Vec3 velocity_min;        /* 0x30 */
  f32 damping;              /* 0x3C */
  f32 gravity;              /* 0x40: added to the vertical velocity every step */
  gaddr texlist;            /* 0x44 */
  gaddr texture;            /* 0x48 */
  uint8_t pad_4c[0x6C - 0x4C];
  gaddr block_a, block_b;   /* 0x6C */
  gaddr vtable;             /* 0x74 */
} Burst;
HW_OFFSET(Burst, bodies, 0x10);
HW_OFFSET(Burst, origin, 0x14);
HW_OFFSET(Burst, depth, 0x20);
HW_OFFSET(Burst, velocity_min, 0x30);
HW_OFFSET(Burst, gravity, 0x40);
HW_OFFSET(Burst, block_b, 0x70);
HW_OFFSET(Burst, vtable, 0x74);
HW_SIZE(Burst, 0x78);

/* type 0x24 (constructor func_8C051FE0, 0x1EC bytes) */
typedef struct ActorType24 {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x02C - 0x028];
  int32_t kind;             /* 0x02C: bits 0-3 of the placement's param: picks the model */
  int32_t motion;           /* 0x030: the entry of the motion table (= kind) */
  int32_t size;             /* 0x034: = kind: 0 spreads the points by 20, else by 6 */
  uint8_t pad_038[0x044 - 0x038];
  int32_t word44;           /* 0x044 */
  int32_t word48;           /* 0x048 */
  f32 frame;                /* 0x04C: the motion's frame */
  f32 frame_count;          /* 0x050: the motion's frame count - 1 */
  f32 f54;                  /* 0x054 */
  Vec3 angle;               /* 0x058: the placement's angles */
  Vec3 points_a[4];         /* 0x064: random points around 0 */
  Vec3 points_b[4];         /* 0x094: copies of points_a */
  uint8_t pad_0c4[0x0D0 - 0x0C4];
  Vec3 home;                /* 0x0D0: the placement's position */
  Body1 values[4];          /* 0x0DC */
  Body body;                /* 0x11C */
  Body body2;               /* 0x14C */
  uint8_t pad_17c[0x180 - 0x17C];
  gaddr model;              /* 0x180: NJS_OBJECT (model_set's list, by kind) */
  uint32_t word184;         /* 0x184: texlist's second word */
  gaddr motions;            /* 0x188: MotionEntry table (word1c's second word) */
  uint8_t pad_18c[0x190 - 0x18C];
  Contacts contacts;        /* 0x190: owned by the body, radius 3, capacity 2 */
  int32_t contacts_word;    /* 0x1E4 */
  gaddr second_vtable;      /* 0x1E8 */
} ActorType24;
HW_OFFSET(ActorType24, kind, 0x02C);
HW_OFFSET(ActorType24, frame, 0x04C);
HW_OFFSET(ActorType24, angle, 0x058);
HW_OFFSET(ActorType24, points_b, 0x094);
HW_OFFSET(ActorType24, home, 0x0D0);
HW_OFFSET(ActorType24, model, 0x180);
HW_OFFSET(ActorType24, motions, 0x188);
HW_OFFSET(ActorType24, values, 0x0DC);
HW_OFFSET(ActorType24, body, 0x11C);
HW_OFFSET(ActorType24, body2, 0x14C);
HW_OFFSET(ActorType24, contacts, 0x190);
HW_SIZE(ActorType24, 0x1EC);



/* game/actor.c */
void actor_a_destroy(gaddr obj);
void actor_b_destroy(gaddr obj);
gaddr actor_base_construct(gaddr actor);
gaddr actor_construct(gaddr actor);
gaddr body_reset_set3(gaddr b);
int32_t course_start_segment(gaddr course);
int32_t course_finish_segment(gaddr course);
gaddr member_array_init(gaddr obj, int32_t count);
void member_array_free(gaddr obj);
gaddr buffer_pair_init(gaddr obj, uint32_t size);
void buffer_pair_free(gaddr obj);

#endif
