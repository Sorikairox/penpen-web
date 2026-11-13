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
#include "../core/ninja.h" /* S14 b11: NjSprite (Sparkles) */

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
  VMethod draw_view; /* 0x18: (this) S15 b08: once per view, when the actor is visible in it (func_8C04489A) */
} ActorVtbl;
HW_OFFSET(ActorVtbl, draw, 0x10);
HW_OFFSET(ActorVtbl, draw_view, 0x18);
HW_SIZE(ActorVtbl, 0x20);

typedef struct Actor {
  int32_t index;    /* 0x00: its number in the group */
  int32_t cell;     /* 0x04: the course cell it stands on */
  uint32_t flags;   /* 0x08: 0 at construction (bit 1 set by the group's re-init) */
  uint32_t word0c;  /* 0x0C: the group's word at 0x434 (S15 b08: the number of views) */
  gaddr scene;      /* 0x10: the RaceScene */
  gaddr model_set;  /* 0x14: S15 b08: a placement with param bit 16: the group file's ContainerEntry 2k+1 (k: param
                     * bits 12-15); else not set by the group */
  gaddr texlist;    /* 0x18: the group's texture list; with param bit 16 the group's container file (its first
                     * entry's offset is its texture list) */
  gaddr word1c;     /* 0x1C: with param bit 16 the group file's ContainerEntry 2k+2 (a motion list); else not set by
                     * the group */
  gaddr models;     /* 0x20: a list of models (NJS_CNK_OBJECT / NJS_OBJECT addresses): the group's blocks (S15) */
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
  uint32_t word428;         /* 0x428: S15 b08: 0 at construction */
  uint8_t pad_42c[0x430 - 0x42C];
  gaddr file;               /* 0x430: the group's data file (its texture list at +4) */
  uint32_t word434;         /* 0x434: S15 b08: the number of views (1, 2 or 4 by g_split_mode) */
  gaddr texlist;            /* 0x438 */
  gaddr blocks[(0x554 - 0x43C) / 4]; /* 0x43C: model blocks, 0-terminated */
} ActorGroup;
HW_OFFSET(ActorGroup, actors, 0x024);
HW_OFFSET(ActorGroup, count, 0x424);
HW_OFFSET(ActorGroup, word428, 0x428);
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
  int32_t kind;             /* 0x28: S16 b06: the placement's param: which of the three pairs (0-2) */
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
HW_OFFSET(ActorPair, kind, 0x28);
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
  uint32_t word50;          /* 0x50: S16 b06: set: its first landing scatters it sideways (by its index) */
  uint32_t alt_model;       /* 0x54: model 8 of the list instead of 9 */
  uint32_t word58;          /* 0x58: S16 b06: updates before it falls (counted down while the actor is not held) */
  union {
    uint32_t size;          /* 0x5C: handed to the race scene's visibility test */
    int32_t cell;           /* 0x5C: S16 b06: the course cell it is on (pieces_update refreshes one per update) */
  };
  int32_t sound_timer;      /* 0x60: S16 b06: updates before a contact may sound again (60) */
  Contacts contacts;        /* 0x64: S14 b10: owned by the body, radius 10, capacity 3 */
  uint8_t pad_b8[0xBC - 0xB8];
} Piece;
HW_OFFSET(Piece, tip, 0x3C);
HW_OFFSET(Piece, state, 0x4C);
HW_OFFSET(Piece, size, 0x5C);
HW_OFFSET(Piece, sound_timer, 0x60);
HW_OFFSET(Piece, contacts, 0x64);
HW_SIZE(Piece, 0xBC);

#define PIECE_COUNT 15
/* type 5 (constructor func_8C046C50) */
typedef struct Pieces {
  Actor base;               /* 0x00 */
  int32_t state;            /* 0x28: S16 b06: 0 waits for a player on cell 0x77, 1 the pieces fall */
  uint8_t pad_2c[0x38 - 0x2C];
  Piece pieces[PIECE_COUNT]; /* 0x38 */
  int32_t cell_turn;        /* 0xB3C: S16 b06: the piece whose cell is refreshed next */
  Body body;                /* 0xB40: S14 b10 */
  Contacts contacts;        /* 0xB70: owned by the body, radius 16, capacity 4 */
  uint8_t pad_bc4[0xBCC - 0xBC4];
  int32_t held;             /* 0xBCC: S16 b06: while set, the pieces' fall delays do not count down */
  gaddr second_vtable;      /* 0xBD0 */
} Pieces;
HW_OFFSET(Pieces, pieces, 0x38);
HW_OFFSET(Pieces, body, 0xB40);
HW_OFFSET(Pieces, contacts, 0xB70);
HW_OFFSET(Pieces, state, 0x28);
HW_OFFSET(Pieces, cell_turn, 0xB3C);
HW_OFFSET(Pieces, held, 0xBCC);
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
  Vec3 position;            /* 0x28: S17 b05: where it is drawn (func_8C04764E) */
  f32 tilt;                 /* 0x34: S17 b05: about x (Ninja units, as a float) */
  f32 angle;                /* 0x38: Ninja units (about y) */
  gaddr model;              /* 0x3C: S17 b05: NJS_CNK_OBJECT drawn with nj_ModelL1 */
  gaddr second_vtable;      /* 0x40: S17 b05 */
} Spinner;
HW_OFFSET(Spinner, angle, 0x38);
HW_OFFSET(Spinner, tilt, 0x34);
HW_OFFSET(Spinner, model, 0x3C);
HW_SIZE(Spinner, 0x44);

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
  union {
    struct {
      uint8_t pad_14c[0x158 - 0x14C];
      f32 scale_a, scale_b; /* 0x158 */
      int32_t counter;      /* 0x160 */
      uint32_t word164;     /* 0x164: = word_ec at reset */
      gaddr table;          /* 0x168 */
    };
    NjSprite sprite;        /* 0x14C: S18 b04: the particles' sprite (p: the particle's position, ang (counter) turns
                             * 15 degrees a drawn particle, tlist (word164), tanim (table)) */
  };
  uint8_t pad_16c[0x170 - 0x16C];
  gaddr frames;             /* 0x170: int32 per particle: its animation frame, 0..6 */
  gaddr countdowns;         /* 0x174: int32 per particle */
  gaddr states;             /* 0x178: int32 per particle: 1 = waiting, 2 = growing, 3 = ending, 0 = off */
  gaddr values;             /* 0x17C: f32 per particle */
  gaddr extra;              /* 0x180: a word per particle (S14 b10: allocated by the constructor); S18 b04: f32: its
                             * alpha (1 - value / 20) */
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
HW_OFFSET(Effect, sprite, 0x14C);
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
/* S16 b06: an entry of type 0x2B's motion table (12 bytes) */
typedef struct Type2BMotion {
  gaddr motion;             /* 0x00: NJS_MOTION (core/file.h: NjMotion) */
  uint32_t word4;           /* 0x04: copied with it into the blend */
  uint32_t word8;           /* 0x08 */
} Type2BMotion;
HW_SIZE(Type2BMotion, 12);

typedef struct ActorType2B {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x02C - 0x028];
  int32_t motion_index;     /* 0x02C: S16 b06: the entry of `motions` playing (0 idle, 1 hit, 2 reach) */
  int32_t active;           /* 0x030: S16 b06: 0: it never reaches out and its body stays */
  uint8_t pad_034[0x038 - 0x034];
  int32_t state;            /* 0x038: S16 b06: 0-2 the reach cycle, 10-15 hit by a racer */
  uint8_t pad_03c[0x040 - 0x03C];
  int32_t timer;            /* 0x040: S16 b06 */
  int32_t reach;            /* 0x044: S16 b06: how far out the body is (0 to reach_max) */
  int32_t reach_max;        /* 0x048: S16 b06 */
  int32_t blending;         /* 0x04C: S16 b06: 1 while two motions blend */
  int32_t side;             /* 0x050: S16 b06: which pair of points the body moves between (flips each cycle) */
  f32 frame;                /* 0x054: S16 b06: the motion's frame */
  f32 frame_end;            /* 0x058: S16 b06: its last frame */
  Body1 values[6];          /* 0x05C: S18 b04: [0..2] the scale in x, y, z, [3..5] the angles about x, y, z */
  Body body;                /* 0x0BC */
  Vec3 points_a[2];         /* 0x0EC */
  Vec3 points_b[2];         /* 0x104 */
  f32 angle_a;              /* 0x11C: S16 b06: the turn for side 0 */
  f32 angle_b;              /* 0x120: S16 b06: for side 1 */
  f32 angle;                /* 0x124: S16 b06: where values[4] turns to (a hit adds -0.25..0.25 turn) */
  gaddr object;             /* 0x128: S18 b04: NJS_OBJECT */
  gaddr texlist;            /* 0x12C: S18 b04 */
  gaddr motions;            /* 0x130: S16 b06: Type2BMotion[3] */
  f32 blend_time;           /* 0x134: S16 b06 */
  f32 blend_length;         /* 0x138: S16 b06 */
  gaddr blend_motion[2];    /* 0x13C: S16 b06: from, to */
  f32 blend_frame[2];       /* 0x144: S16 b06 */
  uint32_t blend_word[2];   /* 0x14C: S16 b06 */
  f32 blend_frame_b[2];     /* 0x154: S16 b06 */
  Contacts contacts;        /* 0x15C: owned by the body, radius 5, capacity 2 */
  int32_t contacts_word;    /* 0x1B0 */
  gaddr second_vtable;      /* 0x1B4 */
} ActorType2B;
HW_OFFSET(ActorType2B, body, 0x0BC);
HW_OFFSET(ActorType2B, points_b, 0x104);
HW_OFFSET(ActorType2B, motion_index, 0x02C);
HW_OFFSET(ActorType2B, state, 0x038);
HW_OFFSET(ActorType2B, frame_end, 0x058);
HW_OFFSET(ActorType2B, values, 0x05C);
HW_OFFSET(ActorType2B, angle_a, 0x11C);
HW_OFFSET(ActorType2B, motions, 0x130);
HW_OFFSET(ActorType2B, object, 0x128);
HW_OFFSET(ActorType2B, texlist, 0x12C);
HW_OFFSET(ActorType2B, blend_motion, 0x13C);
HW_OFFSET(ActorType2B, blend_word, 0x14C);
HW_OFFSET(ActorType2B, blend_frame_b, 0x154);
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
  uint8_t pad_028[0x02C - 0x028];
  int32_t motion;           /* 0x02C: S17 b06: the entry of the motion table it is drawn with */
  uint8_t pad_030[0x040 - 0x030];
  f32 clock;                /* 0x040: + 1 each step (S17 b06: the motion's frame, handed by address) */
  uint8_t pad_044[0x048 - 0x044];
  f32 rest;                 /* 0x048: the value its three Body1s are pulled toward */
  Vec3 angle;               /* 0x04C: S17 b06: drawn turned about z, y, then x by it */
  Vec3 position;            /* 0x058: the contacts' owner */
  Body1 wobble[3];          /* 0x064: S17 b06: their values scale the model in x, y, z */
  Body body;                /* 0x094: S17 b06: drawn at its position */
  gaddr object;             /* 0x0C4: S17 b06: NJS_OBJECT */
  gaddr texlist;            /* 0x0C8: S17 b06 */
  gaddr motions;            /* 0x0CC: S17 b06: MotionEntry table */
  Contacts contacts;        /* 0x0D0: owned by the position, radius 6, capacity 2 */
  int32_t contacts_word;    /* 0x124 */
  gaddr second_vtable;      /* 0x128 */
} Wobbler;
HW_OFFSET(Wobbler, clock, 0x040);
HW_OFFSET(Wobbler, motion, 0x02C);
HW_OFFSET(Wobbler, angle, 0x04C);
HW_OFFSET(Wobbler, object, 0x0C4);
HW_OFFSET(Wobbler, motions, 0x0CC);
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
  int32_t slow;             /* 0x030: 0 = the clock runs at double speed (S17 b06: also the entry of the motion
                             * table it is drawn with) */
  int32_t state;            /* 0x034: picks the step (func_8C050BAE, func_8C05106C, func_8C0512A4, func_8C051548) */
  uint8_t pad_038[0x03C - 0x038];
  int32_t phase;            /* 0x03C: S16 b06: state 2's phase: 0 waits for a player on trigger_cell, 1 counts
                             * delay down, 2 flies */
  int32_t delay;            /* 0x040: S16 b06: updates before it leaps (5 to 65, at random) */
  int32_t trigger_cell;     /* 0x044: S16 b06 */
  int32_t first_player;     /* 0x048: S16 b06: the players before it (in focus order) are not watched */
  int32_t waiting;          /* 0x04C: S16 b06: 1 while it waits, 0 while it flies */
  f32 clock;                /* 0x050 */
  uint8_t pad_054[0x058 - 0x054];
  f32 radius;               /* 0x058: S18 b04: state 1 circles points[0] at this distance */
  f32 roll_target;          /* 0x05C: S18 b04: turns[2] (its roll) is pulled to it: 0, or half a turn */
  f32 circle_angle;         /* 0x060: S18 b04: state 1's angle about points[0] (Ninja units) */
  Vec3 points[3];           /* 0x064: S18 b04: [0] its home, [1] the point its body is pulled to */
  Quat turns[3];            /* 0x088 */
  Body body;                /* 0x0B8 */
  gaddr object;             /* 0x0E8: S17 b06: NJS_OBJECT */
  gaddr texlist;            /* 0x0EC: S17 b06 */
  gaddr motions;            /* 0x0F0: S17 b06: MotionEntry table */
  Contacts contacts;        /* 0x0F4: owned by the body, radius 2, capacity 2 */
  int32_t contacts_word;    /* 0x148 */
  Vec3 point14c;            /* 0x14C: S17 b06: the first path's point (from + half - cos(t) half) */
  Vec3 path_from;           /* 0x158: S17 b06: two paths run from `from` to `to` by a half cosine as path_t goes
                             * 0 to 0x8000 */
  Vec3 path_half;           /* 0x164: S17 b06: (to - from) / 2 */
  Vec3 path_to;             /* 0x170 */
  Vec3 point17c;            /* 0x17C: S17 b06: the second path's point */
  Vec3 path_b_from;         /* 0x188 */
  Vec3 path_b_half;         /* 0x194 */
  Vec3 path_b_to;           /* 0x1A0 */
  f32 path_t;               /* 0x1AC: S17 b06: Ninja angle units, 0 to 0x8000 */
  f32 path_step;            /* 0x1B0: S17 b06: 0x8000 / path_length */
  f32 path_length;          /* 0x1B4: S17 b06: steps the paths take */
  gaddr second_vtable;      /* 0x1B8 */
} ActorType19;
HW_OFFSET(ActorType19, state, 0x034);
HW_OFFSET(ActorType19, clock, 0x050);
HW_OFFSET(ActorType19, phase, 0x03C);
HW_OFFSET(ActorType19, waiting, 0x04C);
HW_OFFSET(ActorType19, points, 0x064);
HW_OFFSET(ActorType19, radius, 0x058);
HW_OFFSET(ActorType19, roll_target, 0x05C);
HW_OFFSET(ActorType19, circle_angle, 0x060);
HW_OFFSET(ActorType19, turns, 0x088);
HW_OFFSET(ActorType19, body, 0x0B8);
HW_OFFSET(ActorType19, contacts, 0x0F4);
HW_OFFSET(ActorType19, point14c, 0x14C);
HW_OFFSET(ActorType19, object, 0x0E8);
HW_OFFSET(ActorType19, motions, 0x0F0);
HW_OFFSET(ActorType19, path_from, 0x158);
HW_OFFSET(ActorType19, path_half, 0x164);
HW_OFFSET(ActorType19, path_to, 0x170);
HW_OFFSET(ActorType19, point17c, 0x17C);
HW_OFFSET(ActorType19, path_b_from, 0x188);
HW_OFFSET(ActorType19, path_b_half, 0x194);
HW_OFFSET(ActorType19, path_b_to, 0x1A0);
HW_OFFSET(ActorType19, path_t, 0x1AC);
HW_OFFSET(ActorType19, path_length, 0x1B4);
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
  NjSprite sprite;          /* 0x4C: S18 b04: the particles' sprite (tlist = texlist, tanim = texture at each draw) */
  union {
    struct {
      gaddr block_a, block_b; /* 0x6C */
    };
    struct {
      int32_t sprite_n;     /* 0x6C: S18 b04: the sprite number drawn when there is no list */
      gaddr sprite_list;    /* 0x70: S18 b04: int32 per drawn particle, in order (0: none) */
    };
  };
  gaddr vtable;             /* 0x74 */
} Burst;
HW_OFFSET(Burst, bodies, 0x10);
HW_OFFSET(Burst, origin, 0x14);
HW_OFFSET(Burst, depth, 0x20);
HW_OFFSET(Burst, velocity_min, 0x30);
HW_OFFSET(Burst, gravity, 0x40);
HW_OFFSET(Burst, block_b, 0x70);
HW_OFFSET(Burst, sprite, 0x4C);
HW_OFFSET(Burst, sprite_list, 0x70);
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
  Vec3 points_b[4];         /* 0x094: copies of points_a (S16 b06: the curve's four control points, re-picked
                             * from points_a each time the curve is run through) */
  Vec3 curve_point;         /* 0x0C4: S16 b06: the point of the curve at f54 (body2 is pulled toward it) */
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
HW_OFFSET(ActorType24, curve_point, 0x0C4);
HW_OFFSET(ActorType24, model, 0x180);
HW_OFFSET(ActorType24, motions, 0x188);
HW_OFFSET(ActorType24, values, 0x0DC);
HW_OFFSET(ActorType24, body, 0x11C);
HW_OFFSET(ActorType24, body2, 0x14C);
HW_OFFSET(ActorType24, contacts, 0x190);
HW_SIZE(ActorType24, 0x1EC);

/* ---- S14 b11: the scenery actor classes of 0x8C052FCE-0x8C05C268 ---- */

/* type 0x0C (constructor func_8C053008, 0xB4 bytes): a bumper: a touch pushes the toucher out and bounces it back,
 * swells the bumper (its pulse) and plays a sound (func_8C05309A sets it up, func_8C053126 steps it) */
typedef struct Bumper {
  Actor base;               /* 0x00 */
  uint32_t param;           /* 0x28: the placement's param: 0-2 small (sound 0x24), 3 and more big (sound 0x30) */
  int32_t size;             /* 0x2C: param + 1, or 42 for a big one */
  Vec3 position;            /* 0x30: the placement's position */
  Vec3 center;              /* 0x3C: the contacts' owner (a big one's 5 higher) */
  Body1 pulse;              /* 0x48: pulled back to 1, pushed by a touch */
  Contacts contacts;        /* 0x58: owned by the centre, radius 10, capacity 8 */
  int32_t contacts_word;    /* 0xAC */
  gaddr second_vtable;      /* 0xB0 */
} Bumper;
HW_OFFSET(Bumper, size, 0x2C);
HW_OFFSET(Bumper, center, 0x3C);
HW_OFFSET(Bumper, pulse, 0x48);
HW_OFFSET(Bumper, contacts, 0x58);
HW_OFFSET(Bumper, contacts_word, 0xAC);
HW_SIZE(Bumper, 0xB4);

/* type 0x0D (constructor func_8C053600, 0x10C bytes): a flap hanging from a pivot that swings about z; when
 * touched it drops, waits, then swings back up (func_8C0536A0 sets it up, func_8C0537E4 steps it) */
typedef struct Flap {
  Actor base;               /* 0x000 */
  int32_t state;            /* 0x028: 0 resting, 1 down (waiting), 2 coming back */
  int32_t countdown;        /* 0x02C */
  Vec3 position;            /* 0x030: the placement's position */
  Body pivot;               /* 0x03C */
  Body1 swing;              /* 0x06C: the angle about z (Ninja units): 40 degrees at rest */
  Body tip;                 /* 0x07C: the flap's end, 55.6 below the pivot turned by the swing: the contacts' owner */
  uint32_t param;           /* 0x0AC: the placement's param: 0 = it swings */
  Contacts contacts;        /* 0x0B0: owned by the tip, radius 7.5, capacity 8 */
  int32_t contacts_word;    /* 0x104 */
  gaddr second_vtable;      /* 0x108 */
} Flap;
HW_OFFSET(Flap, countdown, 0x02C);
HW_OFFSET(Flap, pivot, 0x03C);
HW_OFFSET(Flap, swing, 0x06C);
HW_OFFSET(Flap, tip, 0x07C);
HW_OFFSET(Flap, param, 0x0AC);
HW_OFFSET(Flap, contacts, 0x0B0);
HW_OFFSET(Flap, contacts_word, 0x104);
HW_SIZE(Flap, 0x10C);

/* type 0x29 (constructor func_8C055132, 0x68 bytes): a flock of bodies that waits until a player reaches a cell,
 * then each flies on until it is 500 above the start (func_8C0553B8 steps it) */
typedef struct Flock {
  Actor base;               /* 0x00 */
  int32_t state;            /* 0x28: 0 waiting, 1 flying, 2 gone */
  int32_t count;            /* 0x2C: bodies */
  int32_t trigger_cell;     /* 0x30: the course cell a player must reach */
  uint8_t pad_34[0x38 - 0x34];
  f32 height;               /* 0x38: a body 500 above it stops */
  uint8_t pad_3c[0x40 - 0x3C];
  gaddr bodies;             /* 0x40: Body[count] (operator new[], each built by func_8C0551CE) */
  NjSprite sprite;          /* 0x44: S17 b07: drawn twice at each body (func_8C0554AE) */
  gaddr second_vtable;      /* 0x64 */
} Flock;
HW_OFFSET(Flock, trigger_cell, 0x30);
HW_OFFSET(Flock, height, 0x38);
HW_OFFSET(Flock, bodies, 0x40);
HW_OFFSET(Flock, sprite, 0x44);
HW_OFFSET(Flock, second_vtable, 0x64);
HW_SIZE(Flock, 0x68);

/* type 0x17 (constructor func_8C055AE2, 0xA8 bytes): a gate that turns about y in two half turns: it rests 180
 * steps, turns one degree a step to 210 degrees, rests, turns on round to between 30 and 180 degrees
 * (func_8C055B48 sets it up, func_8C055C20 steps it) */
typedef struct TurningGate {
  Actor base;               /* 0x00 */
  Vec3 position;            /* 0x28: the contacts' owner */
  int32_t state;            /* 0x34: 0 resting, 1 turning, 2 resting, 3 turning, 4 stopped (param != 0) */
  Body1 turn;               /* 0x38: the angle about y (Ninja units), 30 degrees at the start */
  f32 timer;                /* 0x48: steps rested */
  Contacts contacts;        /* 0x4C: owned by the position, radius 45, no room */
  int32_t contacts_word;    /* 0xA0 */
  gaddr second_vtable;      /* 0xA4 */
} TurningGate;
HW_OFFSET(TurningGate, state, 0x34);
HW_OFFSET(TurningGate, turn, 0x38);
HW_OFFSET(TurningGate, timer, 0x48);
HW_OFFSET(TurningGate, contacts, 0x4C);
HW_OFFSET(TurningGate, contacts_word, 0xA0);
HW_SIZE(TurningGate, 0xA8);

/* type 0x18 (constructor func_8C0560E2, 0xC0 bytes): a geyser that swells (its scale a Body pulled toward targets),
 * then may send ten motes up and sound (func_8C05616C sets it up, func_8C056304 steps it) */
typedef struct Geyser {
  Actor base;               /* 0x00 */
  int32_t state;            /* 0x28: 0 waiting, 1 swelling, 2 settling, 3 again; 4 still (param bit 13) */
  uint32_t param;           /* 0x2C: the placement's param */
  int32_t cell_index;       /* 0x30: (param & 0x1F) + 17: kind 2 takes its cell from g_geyser_cells */
  int32_t kind;             /* 0x34: param bits 5-7: 0, 1 swell (1 and 2 with motes); 2 a still model */
  int32_t delay;            /* 0x38: 20 + 40 (param bits 8-11) steps first */
  int32_t flag12;           /* 0x3C: param bit 12 */
  Vec3 position;            /* 0x40 */
  f32 angle_y;              /* 0x4C: the placement's angle about y */
  Body scale;               /* 0x50: (1, 1, 1) at rest */
  MemberArray motes;        /* 0x80: 10 motes, 87.5 above the position, top 65 */
  int32_t motes_on;         /* 0xB8: kind 1 */
  gaddr second_vtable;      /* 0xBC */
} Geyser;
HW_OFFSET(Geyser, cell_index, 0x30);
HW_OFFSET(Geyser, delay, 0x38);
HW_OFFSET(Geyser, position, 0x40);
HW_OFFSET(Geyser, angle_y, 0x4C);
HW_OFFSET(Geyser, scale, 0x50);
HW_OFFSET(Geyser, motes, 0x80);
HW_OFFSET(Geyser, motes_on, 0xB8);
HW_SIZE(Geyser, 0xC0);

/* type 0x21 (constructor func_8C056886, 0x270 bytes): a swimmer that goes round a circle (radius 80) about its
 * placement, turning its heading toward the point ahead and pushed along it, with contact spheres on its body
 * (kind 0: three, placed by its heading; else one at its position) and eight bubbles that rise from it in turn
 * (func_8C056960 sets it up, func_8C056CAC steps it) */
#define SWIMMER_BUBBLES 8
typedef struct Swimmer {
  Actor base;               /* 0x000 */
  int32_t kind;             /* 0x028: param bits 0-3: 0 the big one (three spheres), else one sphere */
  int32_t still;            /* 0x02C: param bits 4-7: not 0 = it stays where it is */
  Body body;                /* 0x030: damping 0.8 */
  Body1 heading;            /* 0x060: about y (Ninja units), damping 0.9 */
  Vec3 center;              /* 0x070: the circle's centre (the placement's position) */
  Vec3 target;              /* 0x07C: the circle's point at the phase */
  f32 phase;                /* 0x088: Ninja units */
  f32 radius;               /* 0x08C: 80 */
  f32 phase_step;           /* 0x090: per step: 1 degree (0.5 for the big one) */
  f32 thrust;               /* 0x094: 0.6 (0.1) */
  f32 turn_rate;            /* 0x098: half a degree (0.2) a step */
  f32 wag;                  /* 0x09C: + 17 degrees a step while it moves */
  Body bubbles[SWIMMER_BUBBLES]; /* 0x0A0: built by func_8C0567C8 */
  int32_t bubble_state[SWIMMER_BUBBLES]; /* 0x220: 0 waiting, 1 rising */
  int32_t bubble_delay[SWIMMER_BUBBLES]; /* 0x240: 1, 11, 21 ... steps; then 1 */
  int32_t sphere_count;     /* 0x260: 3 (kind 0) or 1 */
  gaddr spheres;            /* 0x264: ContactsSlot[sphere_count] (operator new[]) */
  gaddr points;             /* 0x268: Vec3[sphere_count]: each sphere's owner */
  gaddr second_vtable;      /* 0x26C */
} Swimmer;
HW_OFFSET(Swimmer, body, 0x030);
HW_OFFSET(Swimmer, heading, 0x060);
HW_OFFSET(Swimmer, center, 0x070);
HW_OFFSET(Swimmer, target, 0x07C);
HW_OFFSET(Swimmer, phase, 0x088);
HW_OFFSET(Swimmer, wag, 0x09C);
HW_OFFSET(Swimmer, bubbles, 0x0A0);
HW_OFFSET(Swimmer, bubble_state, 0x220);
HW_OFFSET(Swimmer, bubble_delay, 0x240);
HW_OFFSET(Swimmer, sphere_count, 0x260);
HW_OFFSET(Swimmer, spheres, 0x264);
HW_OFFSET(Swimmer, points, 0x268);
HW_SIZE(Swimmer, 0x270);

/* type 0x22 (constructor func_8C057784, 0x124 bytes): a sign of seven modes (the placement's param): a part that
 * swings (by its timer, modes 0, 2, 3, 5), a still one (4), with motes (5), or a board that slides in and out with
 * the first player's state (6) (func_8C057AF2 steps it, func_8C057ED2 draws it) */
typedef struct Sign {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x02C - 0x028];
  int32_t mode;             /* 0x02C: 0 to 6 */
  int32_t state;            /* 0x030: mode 0: 0 = it has contacts; mode 6: 0 in, 1 out, 2 hidden, 3 gone */
  int32_t timer;            /* 0x034 */
  Body body;                /* 0x038: mode 6: the board's place (2D) */
  Vec3 position;            /* 0x068: the contacts' owner */
  Vec3 angle;               /* 0x074: about x, y, z (drawn turned z, y, x) */
  Body1 swing;              /* 0x080: an angle (Ninja units) */
  MemberArray motes;        /* 0x090: 5 motes (mode 5) */
  Contacts contacts;        /* 0x0C8: owned by the position, radius 10, capacity 2 */
  int32_t contacts_word;    /* 0x11C */
  gaddr second_vtable;      /* 0x120 */
} Sign;
HW_OFFSET(Sign, mode, 0x02C);
HW_OFFSET(Sign, timer, 0x034);
HW_OFFSET(Sign, body, 0x038);
HW_OFFSET(Sign, position, 0x068);
HW_OFFSET(Sign, angle, 0x074);
HW_OFFSET(Sign, swing, 0x080);
HW_OFFSET(Sign, motes, 0x090);
HW_OFFSET(Sign, contacts, 0x0C8);
HW_OFFSET(Sign, contacts_word, 0x11C);
HW_SIZE(Sign, 0x124);

/* type 0x2A (constructor func_8C058538, 0xDC bytes): a buoy: a body pulled back home, bumped by contacts (kind 0);
 * kind 1 has no contacts (func_8C0585C0 sets it up, func_8C0586DE steps it) */
typedef struct Buoy {
  Actor base;               /* 0x00 */
  int32_t kind;             /* 0x28: param bits 0-3 */
  int32_t subkind;          /* 0x2C: param bits 4-7 */
  Vec3 home;                /* 0x30: the placement's position */
  Body body;                /* 0x3C: damping (0, 0.98, 0); kind 1: (1, 1, 1), half a unit higher */
  f32 angle_x;              /* 0x6C: the placement's angles */
  f32 angle_y;              /* 0x70 */
  Vec3 position;            /* 0x74: a copy of the body's: the contacts' owner */
  Contacts contacts;        /* 0x80: radius 15.58 (kind 1: 3), capacity 2 */
  int32_t contacts_word;    /* 0xD4 */
  gaddr second_vtable;      /* 0xD8 */
} Buoy;
HW_OFFSET(Buoy, home, 0x30);
HW_OFFSET(Buoy, body, 0x3C);
HW_OFFSET(Buoy, angle_x, 0x6C);
HW_OFFSET(Buoy, position, 0x74);
HW_OFFSET(Buoy, contacts, 0x80);
HW_OFFSET(Buoy, contacts_word, 0xD4);
HW_SIZE(Buoy, 0xDC);

/* type 0x25 (constructor func_8C0595D0, 0x9C bytes): only its construction and a clock tick (func_8C059756) are
 * written */
typedef struct ActorType25 {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x30 - 0x28];
  int32_t motion;           /* 0x30: S18 b05: the entry of `motions` it is drawn with */
  uint8_t pad_34[0x44 - 0x34];
  f32 clock;                /* 0x44: + 1 a tick; S18 b05: the motion's frame (handed by address) */
  uint8_t pad_48[0x4C - 0x48];
  f32 scale;                /* 0x4C: S18 b05: drawn scaled by it on all three axes */
  Vec3 angle;               /* 0x50: S18 b05: about x, y, z (drawn turned z, x, y) */
  Body body;                /* 0x5C */
  gaddr object;             /* 0x8C: S18 b05: the NJS_OBJECT drawn */
  gaddr textures;           /* 0x90: S18 b05: its NJS_TEXLIST */
  gaddr motions;            /* 0x94: S18 b05: MotionEntry[] */
  gaddr second_vtable;      /* 0x98 */
} ActorType25;
HW_OFFSET(ActorType25, motion, 0x30);
HW_OFFSET(ActorType25, scale, 0x4C);
HW_OFFSET(ActorType25, angle, 0x50);
HW_OFFSET(ActorType25, object, 0x8C);
HW_OFFSET(ActorType25, motions, 0x94);
HW_OFFSET(ActorType25, clock, 0x44);
HW_OFFSET(ActorType25, body, 0x5C);
HW_OFFSET(ActorType25, second_vtable, 0x98);
HW_SIZE(ActorType25, 0x9C);

/* a Ninja object node with its typed transform (NJS_OBJECT: file.h's NjObject keeps only the pointers) */
typedef struct NjObjectPose {
  uint32_t evalflags;       /* 0x00: bit 3 = NJD_EVAL_HIDE */
  gaddr model;              /* 0x04 */
  Vec3 pos;                 /* 0x08 */
  int32_t ang[3];           /* 0x14: Ninja units */
  Vec3 scl;                 /* 0x20 */
  gaddr child;              /* 0x2C */
  gaddr sibling;            /* 0x30 */
} NjObjectPose;
HW_OFFSET(NjObjectPose, ang, 0x14);
HW_OFFSET(NjObjectPose, child, 0x2C);
HW_SIZE(NjObjectPose, 0x34);

/* type 0x11 (constructor func_8C059C30, 0x74 bytes): a flyer: once a player reaches cell 193 it waits 120 steps,
 * then flies off toward a far point; its model's nodes from the seventh on take a pose by its kind
 * (func_8C059D46 steps it, func_8C059F26 draws it) */
typedef struct Flyer {
  Actor base;               /* 0x00 */
  Body body;                /* 0x28 */
  int32_t kind;             /* 0x58: picks the pose tables */
  int32_t timer;            /* 0x5C */
  int32_t state;            /* 0x60: 1 waiting for a player, 2 counting, 3 flying */
  Vec3 angle;               /* 0x64: about x, y, z (drawn turned z, y, x) */
  gaddr second_vtable;      /* 0x70 */
} Flyer;
HW_OFFSET(Flyer, body, 0x28);
HW_OFFSET(Flyer, kind, 0x58);
HW_OFFSET(Flyer, state, 0x60);
HW_OFFSET(Flyer, angle, 0x64);
HW_SIZE(Flyer, 0x74);

/* type 0x12 (constructor func_8C05A72E, 0x110 bytes): a row of 53 models placed from static tables, each drawn when
 * the current view's course row has its cell (func_8C05A7AC sets it up, func_8C05A834 draws it) */
#define FENCE_POSTS 53
typedef struct Fence {
  Actor base;               /* 0x000 */
  Vec3 position;            /* 0x028: the placement's */
  uint32_t param;           /* 0x034 */
  int32_t cells[FENCE_POSTS]; /* 0x038: the course cell of each post */
  gaddr second_vtable;      /* 0x10C */
} Fence;
HW_OFFSET(Fence, param, 0x034);
HW_OFFSET(Fence, cells, 0x038);
HW_SIZE(Fence, 0x110);

/* a placed model of the wave field (32 bytes) */
typedef struct WaveItem {
  int32_t kind;             /* 0x00: picks the scale (and the model: kind / 2) */
  int32_t cell;             /* 0x04 */
  Vec3 pos;                 /* 0x08 */
  Vec3 angle;               /* 0x14: about x, y, z */
} WaveItem;
HW_SIZE(WaveItem, 0x20);

/* a vertex of a chunk model's vertex list (16 bytes) */
typedef struct WaveVertex {
  f32 x, y, z;              /* 0x00 */
  uint32_t extra;           /* 0x0C */
} WaveVertex;
HW_SIZE(WaveVertex, 16);

/* type 0x13 (constructor func_8C05AA76, 0xD34 bytes): a wave field: 82 placed models from a static table and six
 * meshes whose first ten vertices are moved by two sets of five waves (func_8C05AB00 sets it up, func_8C05AC58
 * steps the waves, func_8C05AD58 draws the placed models) */
#define WAVE_ITEMS_MAX 90
#define WAVE_ITEMS 82
#define WAVE_MESHES 6
#define WAVE_MESH_VERTICES 10
#define WAVE_COUNT 5
typedef struct WaveField {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x02C - 0x028];
  int32_t count;            /* 0x02C: placed models */
  WaveItem items[WAVE_ITEMS_MAX]; /* 0x030 */
  gaddr meshes[WAVE_MESHES]; /* 0xB70: models 15-20 of the list (NJS_CNK_OBJECT) */
  gaddr vertices[WAVE_MESHES]; /* 0xB88: each mesh's first vertex (WaveVertex) */
  f32 rest_x[WAVE_MESHES][WAVE_MESH_VERTICES]; /* 0xBA0: the vertices' x at rest */
  Body1 waves_a[WAVE_COUNT]; /* 0xC90: move meshes 0 and 1 */
  Body1 waves_b[WAVE_COUNT]; /* 0xCE0: move meshes 2-5 */
  gaddr second_vtable;      /* 0xD30 */
} WaveField;
HW_OFFSET(WaveField, count, 0x02C);
HW_OFFSET(WaveField, items, 0x030);
HW_OFFSET(WaveField, meshes, 0xB70);
HW_OFFSET(WaveField, vertices, 0xB88);
HW_OFFSET(WaveField, rest_x, 0xBA0);
HW_OFFSET(WaveField, waves_a, 0xC90);
HW_OFFSET(WaveField, waves_b, 0xCE0);
HW_OFFSET(WaveField, second_vtable, 0xD30);
HW_SIZE(WaveField, 0xD34);

/* type 0x14 (constructor func_8C05B194, 0x10C bytes): an ornament of six kinds (param bits 0-3: its model and
 * offsets; kinds 1, 2 with two swinging parts) (func_8C05B230 sets it up) */
typedef struct Ornament {
  Actor base;               /* 0x000 */
  uint32_t param;           /* 0x028 */
  int32_t style;            /* 0x02C: by kind: 4, 0, 0, 1, 2, 3 */
  int32_t subkind;          /* 0x030: param bits 4-7 */
  int32_t model;            /* 0x034: the model of the list: 1, 2, 2, 11, 13, 11 */
  int32_t word38;           /* 0x038 */
  int32_t word3c;           /* 0x03C */
  Vec3 position;            /* 0x040: the placement's */
  Body body;                /* 0x04C: the contacts' owner, offset from the position by kind */
  Vec3 angle;               /* 0x07C: the placement's angles */
  f32 f88;                  /* 0x088 */
  f32 f8c;                  /* 0x08C: by kind: 0.35, 0.4, 0.3 */
  Body1 swing_a;            /* 0x090: kinds 1, 2: from 25 (40) degrees */
  Body1 swing_b;            /* 0x0A0: kinds 1, 2: from -35 (-25) degrees */
  Contacts contacts;        /* 0x0B0: owned by the body, radius 5, capacity 2 */
  int32_t contacts_word;    /* 0x104 */
  gaddr second_vtable;      /* 0x108 */
} Ornament;
HW_OFFSET(Ornament, model, 0x034);
HW_OFFSET(Ornament, position, 0x040);
HW_OFFSET(Ornament, body, 0x04C);
HW_OFFSET(Ornament, angle, 0x07C);
HW_OFFSET(Ornament, f8c, 0x08C);
HW_OFFSET(Ornament, swing_a, 0x090);
HW_OFFSET(Ornament, swing_b, 0x0A0);
HW_OFFSET(Ornament, contacts, 0x0B0);
HW_OFFSET(Ornament, contacts_word, 0x104);
HW_SIZE(Ornament, 0x10C);

/* type 0x15 (constructor func_8C05B942, 0x68 bytes): a part that turns to a fixed angle by its kind (the param, 0-6)
 * (func_8C05B9C0 sets it up, func_8C05BA7E steps it) */
typedef struct Swinger {
  Actor base;               /* 0x00 */
  uint32_t kind;            /* 0x28: the placement's param */
  int32_t word2c;           /* 0x2C */
  Vec3 position;            /* 0x30 */
  Vec3 angle;               /* 0x3C */
  Body1 swing;              /* 0x48: about its axis (Ninja units) */
  uint8_t pad_58[0x60 - 0x58];
  int32_t style;            /* 0x60: by kind: 1, 1, 2, 2, 2, 2, 4 */
  gaddr second_vtable;      /* 0x64 */
} Swinger;
HW_OFFSET(Swinger, position, 0x30);
HW_OFFSET(Swinger, angle, 0x3C);
HW_OFFSET(Swinger, swing, 0x48);
HW_OFFSET(Swinger, style, 0x60);
HW_SIZE(Swinger, 0x68);

/* type 0x16 (constructor func_8C05BC60, 0xEC bytes): 40 sprites placed from a static table, drawn in 3D
 * (func_8C05BCB6 sets it up, func_8C05BD44 draws it) */
#define SPARKLES 40
typedef struct Sparkles {
  Actor base;               /* 0x00 */
  NjSprite sprite;          /* 0x28: the one sprite each is drawn with */
  int32_t cells[SPARKLES];  /* 0x48 */
  gaddr second_vtable;      /* 0xE8 */
} Sparkles;
HW_OFFSET(Sparkles, sprite, 0x28);
HW_OFFSET(Sparkles, cells, 0x48);
HW_SIZE(Sparkles, 0xEC);

/* type 0x2C (constructor func_8C05BFC6, 0x3F0 bytes): a body with wide contacts (radius 40) and 40 rings that
 * shrink from 40 to 2.5 and start again, each turning 6 degrees a step (func_8C05C1E8 steps it) */
#define RINGS 40
typedef struct Rings {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x02C - 0x028];
  Body body;                /* 0x02C: the contacts' owner */
  Vec3 angle;               /* 0x05C: S18 b05: the rings' plane, about x, y, z (Ninja units, as floats) */
  Contacts contacts;        /* 0x068: radius 40, capacity 8 */
  int32_t contacts_word;    /* 0x0BC */
  uint8_t pad_0c0[0x0CC - 0x0C0];
  Body1 sizes[RINGS];       /* 0x0CC */
  f32 turns[RINGS];         /* 0x34C: Ninja units */
  gaddr second_vtable;      /* 0x3EC */
} Rings;
HW_OFFSET(Rings, body, 0x02C);
HW_OFFSET(Rings, angle, 0x05C);
HW_OFFSET(Rings, contacts, 0x068);
HW_OFFSET(Rings, contacts_word, 0x0BC);
HW_OFFSET(Rings, sizes, 0x0CC);
HW_OFFSET(Rings, turns, 0x34C);
HW_SIZE(Rings, 0x3F0);

/* ---- end of S14 b11 ---- */

/* ---- S15 b08: the factory's types 1, 2, 0x20 and 0x2D (game/actor_types.c) ---- */

/* type 1 (constructor func_8C045020, 0x110 bytes; vtable 0x8C23DBCC, its second base's 0x8C23DBEC): a body with
 * contacts (radius 10) whose response is the class's own (func_8C0455B0), a Body1 that turns (damping 0.9) and four
 * direction vectors; while the camera's cell is 0x3A..0x3D it pushes the course's wind (func_8C045514) */
typedef struct ActorType01 {
  Actor base;               /* 0x000 */
  Vec3 position;            /* 0x028: the contacts' owner */
  f32 heading;              /* 0x034: the placement's angle about y */
  f32 word38;               /* 0x038: 0 */
  Body1 spin;               /* 0x03C: (0, 0, 0, 1) at construction */
  f32 word4c;               /* 0x04C: S16 b06: the wind's strength (level times 0.0194), push = point times it */
  Vec3 push;                /* 0x050: the wind it adds (func_8C045514: 40 long) */
  int32_t word5c;           /* 0x05C: S16 b06: the step in its level table */
  int32_t timer;            /* 0x060: S16 b06: updates in the step (a step lasts 121) */
  int32_t word64;           /* 0x064: 0, or 3 when kind_b is not 0 (S16 b06: the state: 0 waits for a player on
                             * cell 0x39, 1 steps through the level table, 2 stops, 3 spins) */
  int32_t kind_a;           /* 0x068: the placement's param bits 0-3 (S16 b06: picks the level table) */
  int32_t word6c;           /* 0x06C: S16 b06: the level (from the table; 0 ends it, -1 starts it over) */
  int32_t kind_b;           /* 0x070: param bits 4-7 */
  Contacts contacts;        /* 0x074 */
  int32_t contacts_word;    /* 0x0C8: 4 */
  Vec3 dir[4];              /* 0x0CC: (0,0,0), (0,0,-1), (0,0,0), (0,0,-1) at construction */
  Vec3 point;               /* 0x0FC */
  f32 reach;                /* 0x108: 40 */
  gaddr second_vtable;      /* 0x10C */
} ActorType01;
HW_OFFSET(ActorType01, position, 0x028);
HW_OFFSET(ActorType01, spin, 0x03C);
HW_OFFSET(ActorType01, push, 0x050);
HW_OFFSET(ActorType01, word64, 0x064);
HW_OFFSET(ActorType01, timer, 0x060);
HW_OFFSET(ActorType01, word6c, 0x06C);
HW_OFFSET(ActorType01, kind_b, 0x070);
HW_OFFSET(ActorType01, contacts, 0x074);
HW_OFFSET(ActorType01, contacts_word, 0x0C8);
HW_OFFSET(ActorType01, dir, 0x0CC);
HW_OFFSET(ActorType01, point, 0x0FC);
HW_OFFSET(ActorType01, reach, 0x108);
HW_OFFSET(ActorType01, second_vtable, 0x10C);
HW_SIZE(ActorType01, 0x110);

/* type 2 (constructor func_8C0458FC, 0xE0 bytes; vtable 0x8C23DC0C, second 0x8C23DC2C): a Body with contacts
 * (radius 5, two slots), a target point picked by the placement's param (bits 0-5), a model (param bit 7: the list's
 * first, else its second) and a Burst of 10 particles (func_8C0459BA sets it up) */
typedef struct ActorType02 {
  Actor base;               /* 0x00 */
  int32_t state;            /* 0x28: 0 at set-up */
  int32_t flag40;           /* 0x2C: param bit 6 */
  Body body;                /* 0x30 */
  Vec3 target;              /* 0x60: by param bits 0-5 (kinds 0-3; other kinds leave it) */
  f32 angle_x;              /* 0x6C: the placement's angles */
  f32 angle_y;              /* 0x70 */
  Contacts contacts;        /* 0x74 */
  int32_t contacts_word;    /* 0xC8: 4 */
  gaddr model;              /* 0xCC */
  int32_t delay;            /* 0xD0: param bits 8-15, times 5 */
  int32_t wordd4;           /* 0xD4: 0 at set-up (S16 b06: updates it has rolled; past 300 it bursts) */
  gaddr burst;              /* 0xD8: Burst (10 particles) or 0 */
  gaddr second_vtable;      /* 0xDC */
} ActorType02;
HW_OFFSET(ActorType02, body, 0x30);
HW_OFFSET(ActorType02, target, 0x60);
HW_OFFSET(ActorType02, angle_y, 0x70);
HW_OFFSET(ActorType02, contacts, 0x74);
HW_OFFSET(ActorType02, contacts_word, 0xC8);
HW_OFFSET(ActorType02, model, 0xCC);
HW_OFFSET(ActorType02, wordd4, 0xD4);
HW_OFFSET(ActorType02, burst, 0xD8);
HW_OFFSET(ActorType02, second_vtable, 0xDC);
HW_SIZE(ActorType02, 0xE0);

/* type 0x2D (constructor func_8C05C6AA, 0xB8 bytes; vtable 0x8C23E77C, second 0x8C23E79C): something that turns
 * about y by steps (a Body1, its speed +-341.33 by param bit 4), waits between them (param bits 0-3 times 144), with
 * contacts (radius 20) whose response is the class's own (func_8C05CAA8); func_8C05C738 sets it up, func_8C05C81E
 * steps it */
typedef struct ActorType2D {
  Actor base;               /* 0x00 */
  Vec3 position;            /* 0x28: the contacts' owner */
  f32 heading;              /* 0x34: the placement's angle about y */
  int32_t state;            /* 0x38: 0, or 4 with param bit 5 */
  int32_t reverse;          /* 0x3C: param bit 4 */
  Body1 turn;               /* 0x40 */
  f32 speed;                /* 0x50 */
  f32 timer;                /* 0x54 */
  f32 wait;                 /* 0x58 */
  Contacts contacts;        /* 0x5C */
  int32_t contacts_word;    /* 0xB0: 4 */
  gaddr second_vtable;      /* 0xB4 */
} ActorType2D;
HW_OFFSET(ActorType2D, heading, 0x34);
HW_OFFSET(ActorType2D, turn, 0x40);
HW_OFFSET(ActorType2D, wait, 0x58);
HW_OFFSET(ActorType2D, contacts, 0x5C);
HW_OFFSET(ActorType2D, contacts_word, 0xB0);
HW_OFFSET(ActorType2D, second_vtable, 0xB4);
HW_SIZE(ActorType2D, 0xB8);

/* type 0x20 (constructor func_8C061964, 0xE8 bytes; vtable 0x8C23E87C, second 0x8C23E89C): a Body that sways about
 * its home by two random phases, two Body1s; kinds 0 and 1 (param bits 0-3) differ in their speeds
 * (func_8C0619DC sets it up) */
typedef struct ActorType20 {
  Actor base;               /* 0x00 */
  uint32_t tex_word;        /* 0x28: the second word of the actor's texlist field */
  uint8_t pad_2c[0x30 - 0x2C];
  int32_t frame;            /* 0x30: random 0..15 at set-up */
  int32_t kind;             /* 0x34 */
  uint8_t pad_38[0x40 - 0x38];
  Vec3 home;                /* 0x40: S15 b09: the body's position, copied every update */
  f32 word4c;               /* 0x4C: 0.625 */
  f32 word50;               /* 0x50: 0.625 */
  int32_t word54;           /* 0x54: 0 */
  uint32_t word58;          /* 0x58: tex_word */
  gaddr table;              /* 0x5C: static data at 0x8C09975C */
  Body body;                /* 0x60 */
  f32 pull;                 /* 0x90: 0.05 (S15 b09: the two Body1s' pull rate) */
  f32 damping94;            /* 0x94: 0.8 (S15 b09: the two Body1s' damping, set every update) */
  f32 radius;               /* 0x98: 20 (kind 0), 5 (kind 1) */
  f32 phase_a;              /* 0x9C: random 0..65536; S15 b09: + rate_a every update */
  f32 phase_b;              /* 0xA0: S15 b09: + rate_b every update */
  f32 rate_a;               /* 0xA4: 36.4 + random 0..18.2 (kinds 0, 1) */
  f32 rate_b;               /* 0xA8: +-546.13 (kind 0), +-273.07 (kind 1) */
  Vec3 start;               /* 0xAC */
  Vec3 target;              /* 0xB8: S15 b09: where the body is pulled: start + the turned offset (0, 0, radius) */
  Body1 c4;                 /* 0xC4: S15 b09: the tilt (njRotateX), pulled toward phase_a */
  Body1 d4;                 /* 0xD4: S15 b09: the turn (njRotateY), pulled toward phase_b */
  gaddr second_vtable;      /* 0xE4 */
} ActorType20;
HW_OFFSET(ActorType20, frame, 0x30);
HW_OFFSET(ActorType20, home, 0x40);
HW_OFFSET(ActorType20, table, 0x5C);
HW_OFFSET(ActorType20, body, 0x60);
HW_OFFSET(ActorType20, pull, 0x90);
HW_OFFSET(ActorType20, rate_b, 0xA8);
HW_OFFSET(ActorType20, start, 0xAC);
HW_OFFSET(ActorType20, target, 0xB8);
HW_OFFSET(ActorType20, c4, 0xC4);
HW_OFFSET(ActorType20, d4, 0xD4);
HW_OFFSET(ActorType20, second_vtable, 0xE4);
HW_SIZE(ActorType20, 0xE8);

/* ---- end of S15 b08 ---- */

/* game/actor.c */
gaddr course_actor_placements(gaddr course); /* S17 b05: exported there, declared here */
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
void actor_group_release(gaddr group);   /* S15 b08: exported there, declared here */
void race_scene_env_push(gaddr scene, int32_t i, gaddr v); /* S15 b08 */
void effect_defaults(gaddr fx);          /* S15 b08 */
/* game/burst.c (S15 b08: exported there, declared here) */
gaddr burst_construct(gaddr obj, uint32_t count);
/* S15 b08: game/actor_group.c */
void actor_set_cell(gaddr actor, gaddr pos, int32_t hint);
/* game/actor.c (S16 b06 declares it for actor_pieces_update.c) */
void piece_kick(gaddr obj, int32_t i);
/* game/burst.c (S16 b06 declares it for actor_type02.c) */
void burst_update(gaddr obj);
int32_t actor_in_view(gaddr actor);
gaddr actor_group_init(gaddr group);
gaddr actor_group_construct(gaddr group);
gaddr actor_group_delete(gaddr group, int32_t free_memory);
void actor_group_populate(gaddr group);
void actor_group_attach(gaddr group, gaddr actor, gaddr placement, int32_t index);
void actor_group_draw_view(gaddr group);
/* S15 b08: game/actor_types.c */
void body_damping_none(gaddr body);
/* S18 b02: game/actor_factory.c (declared for game/trap_set_objects.c, whose factory is a copy) */
gaddr actor_create(gaddr group, uint32_t type);
/* S18 b02: game/trap_set_objects.c (the Actor flags word's getter, func_8C01F6AC) */
uint32_t actor_flags(gaddr actor);
/* S18 b02: game/actor_factory.c (declared for game/trap_set_objects.c, whose factory is a copy) */
gaddr actor_create(gaddr group, uint32_t type);
/* S18 b02: game/trap_set_objects.c (the Actor flags word's getter, func_8C01F6AC) */
uint32_t actor_flags(gaddr actor);

/* S18 b04: game/actor_type19_b04.c (they supersede the static CALL wrappers type19_step0/1/3 in actor_bodies.c) */
void type19_step0(gaddr obj);
void type19_step1(gaddr obj);
void type19_step3(gaddr obj);

/* S18 b04: game/actor_effect_b04.c (effect_burst supersedes the static CALL wrapper of that name in actor_effect.c) */
void effect_free_blocks(gaddr obj);
void effect_destroy(gaddr obj);
gaddr effect_delete(gaddr obj, int32_t free_memory);
gaddr effect_destroy_self(gaddr obj);
void effect_draw(gaddr obj);
void effect_draw_particles(gaddr obj);
void effect_burst(gaddr obj, gaddr pos, f32 angle);
/* S18 b04: game/actor_misc_b04.c */
void burst_draw(gaddr obj);
void material_set_ac6a(f32 r, f32 g, f32 b, f32 a);

#endif
