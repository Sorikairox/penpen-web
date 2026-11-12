/* Pen Pen port, S14: a racer's effects (game/model.c, game/stage.c).
 *
 * Each Racer has a child task (Racer.child, 0x3C bytes, constructor func_8C038C1C -> func_8C038B32, vtable
 * 0x8C23DB1C) that owns its effects: a ribbon (the trail behind the racer: an 8-byte holder, func_8C038D94) and four
 * particle sets kept as per-particle arrays (one block per field, `count` entries each, and a Body per particle).
 * Every set starts with the same three words at 0x14: 0, the shared data's word (g_shared_word) and a pointer to
 * static data; the sets' other fields are named by what the written functions do with them. */
#ifndef PENPEN_EFFECTS_H
#define PENPEN_EFFECTS_H

#include "../core/task.h"
#include "../core/vec.h"
#include "../core/ninja.h" /* b07: NjSprite */

typedef struct RacerEffects {
  Task task;       /* 0x00: vtable 0x8C23DB1C */
  gaddr racer;     /* 0x24: the owner */
  gaddr ribbon;    /* 0x28: Ribbon (8 bytes) */
  gaddr ribbons;   /* 0x2C: RibbonSet (0x58) */
  gaddr set_a;     /* 0x30: EffectSetA (0x50, 0x20 particles) */
  gaddr set_b;     /* 0x34: EffectSetB (0x54, 0x20 particles) */
  gaddr set_c;     /* 0x38: EffectSetC (0x44) */
} RacerEffects;
HW_SIZE(RacerEffects, 0x3C);

/* the ribbon: its first word points to an object whose first word is the array of 4 * g_ribbon_length points
 * (b07: a Ninja basic model, NJS_MODEL, that the constructor func_8C038D94 builds: a quad per pair) */
typedef struct Ribbon {
  gaddr points_holder; /* 0x00: b07: the NJS_MODEL (core/file.h NjModel, 0x28 bytes) */
  uint32_t word4;      /* 0x04 */
} Ribbon;
HW_SIZE(Ribbon, 8);

/* what ribbon_push is handed: a matrix and, after it, the two heights the edges are placed between (a local of the
 * racer's update, func_8C0351C8). S15 b05: a snapshot of the racer (0x98 bytes; func_8C033A88 fills one for set C):
 * its matrix at 0x3A8, its moving body's position and velocity, its vectors at 0x534 / 0x540, its contact radius,
 * the character's word at +4, ... its index */
typedef struct RibbonFrame {
  Matrix m;            /* 0x00 */
  Vec3 position;       /* 0x40: b07: where set C's particles start (effect_set_c_emit) */
  Vec3 velocity;       /* 0x4C: S15 b05 */
  Vec3 vec58;          /* 0x58: S15 b05: Racer.vec534 */
  Vec3 vec64;          /* 0x64: S15 b05: Racer.vec540 */
  f32 low;             /* 0x70: (S15 b05: Racer.contacts.radius) */
  f32 high;            /* 0x74: (S15 b05: CharacterParams.f04) */
  f32 f78;             /* 0x78: S15 b05: Racer.f434 */
  f32 heading;         /* 0x7C: S15 b05 */
  int32_t word80;      /* 0x80: S15 b05: Racer.word144 */
  int32_t word84;      /* 0x84: S15 b05: Racer.word174 */
  int32_t word88;      /* 0x88: S15 b05: Racer.word17c */
  int32_t word8c;      /* 0x8C: S15 b05: Racer.word180 */
  int32_t index;       /* 0x90: S15 b05: Racer.index */
  uint32_t word94;     /* 0x94 */
} RibbonFrame;
HW_OFFSET(RibbonFrame, position, 0x40);
HW_OFFSET(RibbonFrame, velocity, 0x4C);
HW_OFFSET(RibbonFrame, vec64, 0x64);
HW_OFFSET(RibbonFrame, low, 0x70);
HW_OFFSET(RibbonFrame, high, 0x74);
HW_OFFSET(RibbonFrame, heading, 0x7C);
HW_OFFSET(RibbonFrame, word8c, 0x8C);
HW_OFFSET(RibbonFrame, index, 0x90);
HW_SIZE(RibbonFrame, 0x98);

/* a 0x18-byte record of the ribbon's (func_8C038788 clears one; b07: the first 0x18 bytes of the ribbon's NJS_MODEL) */
typedef struct RibbonRecord {
  uint32_t words[5];   /* 0x00 */
  uint16_t half14;     /* 0x14 */
  uint16_t half16;     /* 0x16 */
} RibbonRecord;
HW_SIZE(RibbonRecord, 0x18);

/* an entry of the shared data's second table (12 bytes, by g_ribbon_kind) */
typedef struct RibbonKind {
  gaddr model;         /* 0x00 */
  gaddr texture;       /* 0x04 */
  uint32_t word8;      /* 0x08 */
} RibbonKind;
HW_SIZE(RibbonKind, 12);

#define RIBBON_SET_ARRAYS 8
/* b07: every set starts with the NJS_SPRITE it draws its particles with (position, scale, angle, texture list =
 * the shared data's word, texture animation = a static record): the three words at 0x14 are its ang, tlist and tanim.
 * A particle has a kind (the sprite's texture-animation entry, or 8/10 / 12-15: a motion model), a state (0 = off,
 * 1 = waiting out its counter / playing, 2 = growing, 3 = ending), a size counted up to a limit (its alpha falls with
 * it), a scale and a Body. */
typedef struct RibbonSet {
  union {
    NjSprite sprite;     /* 0x00: b07 */
    struct {
      uint8_t pad_00[0x14];
      uint32_t word14;     /* 0x14: 0 (sprite.ang) */
      uint32_t shared_word; /* 0x18 (sprite.tlist) */
      gaddr data;          /* 0x1C: RIBBON_SET_DATA (sprite.tanim) */
    };
  };
  gaddr object;        /* 0x20: b07: the model tree of the motion-model particles (kinds 8 and 10) */
  gaddr model;         /* 0x24: from the shared data's table (by g_ribbon_kind); b07: an NJS_MOTION (nb_frames) */
  gaddr texture;       /* 0x28 */
  int32_t count;       /* 0x2C: g_ribbon_count */
  uint8_t pad_30[0x34 - 0x30];
  union {
    gaddr arrays[RIBBON_SET_ARRAYS]; /* 0x34: a word per ribbon each (the third, 0x3C, cleared) */
    struct {             /* b07: what the update and the draw do with them */
      gaddr kinds;       /* 0x34: int32: 8, 10 = a motion model; 0, 3, 4, 5 = a sprite of that texture entry */
      gaddr counters;    /* 0x38: int32: frames to wait in state 1 */
      gaddr states;      /* 0x3C: int32 */
      gaddr alphas;      /* 0x40: f32 (the draw computes it) */
      gaddr sizes;       /* 0x44: f32: counted up to g_ribbon_size_limit */
      gaddr scales;      /* 0x48: f32 */
      gaddr frames;      /* 0x4C: f32: the motion's frame */
      gaddr angles;      /* 0x50: f32: the model's turn (Ninja angle units) */
    };
  };
  gaddr bodies;        /* 0x54: a Body per ribbon */
} RibbonSet;
HW_OFFSET(RibbonSet, word14, 0x14);
HW_OFFSET(RibbonSet, object, 0x20);
HW_OFFSET(RibbonSet, model, 0x24);
HW_OFFSET(RibbonSet, count, 0x2C);
HW_OFFSET(RibbonSet, arrays, 0x34);
HW_OFFSET(RibbonSet, angles, 0x50);
HW_OFFSET(RibbonSet, bodies, 0x54);
HW_SIZE(RibbonSet, 0x58);

/* set A (constructor func_8C039CF2): `count` at 0x20, arrays from 0x30. b07: puffs that rise from an origin
 * (positions are origin + offset; drawn while above the course's floor) */
typedef struct EffectSetA {
  NjSprite sprite;     /* 0x00: b07: scale 0.05 (g_set_a_scale), tanim 0x8C096CD0 */
  int32_t count;       /* 0x20: b07 */
  uint32_t word24;     /* 0x24: b07: 0 */
  int32_t cell;        /* 0x28: b07: the course cell whose floor height hides the puffs */
  f32 heading;         /* 0x2C: b07: the drift's direction (Ninja angle units) */
  union {
    gaddr arrays[8];   /* 0x30 */
    struct {           /* b07 */
      gaddr counters;  /* 0x30: int32 */
      gaddr states;    /* 0x34: int32 */
      gaddr types;     /* 0x38: int32: the sprite's texture entry (1: rises faster) */
      gaddr origins;   /* 0x3C: Vec3 */
      gaddr offsets;   /* 0x40: Vec3 */
      gaddr scales;    /* 0x44: f32 (0.05 at construction) */
      gaddr sizes;     /* 0x48: f32: counted up to g_set_a_size_limit */
      gaddr alphas;    /* 0x4C: f32 */
    };
  };
} EffectSetA;
HW_OFFSET(EffectSetA, count, 0x20);
HW_OFFSET(EffectSetA, heading, 0x2C);
HW_OFFSET(EffectSetA, arrays, 0x30);
HW_OFFSET(EffectSetA, alphas, 0x4C);
HW_SIZE(EffectSetA, 0x50);

/* set B (constructor func_8C03AF80) */
typedef struct EffectSetB {
  union {
    NjSprite sprite;     /* 0x00: b07 */
    struct {
      uint8_t pad_00[0x14];
      uint32_t word14;     /* 0x14: 0 (sprite.ang) */
      uint32_t shared_word; /* 0x18 */
      gaddr data;          /* 0x1C: RIBBON_SET_DATA */
    };
  };
  gaddr object;        /* 0x20: b07: as RibbonSet's (kinds 12-15) */
  gaddr motion;        /* 0x24: b07: NJS_MOTION */
  gaddr texture;       /* 0x28: b07 */
  int32_t count;       /* 0x2C */
  int32_t word30;      /* 0x30: 1 (S15 c05: a gate for the sprites effect_set_b_push starts: when it is positive,
                        * it is counted down and the sprite starts; at 0 the particle stays off and it goes back
                        * to 1, so they start every other time) */
  union {
    gaddr arrays[7];   /* 0x34: a word per particle each (the second, 0x38, cleared) */
    struct {           /* b07 */
      gaddr counters;  /* 0x34: int32 */
      gaddr states;    /* 0x38: int32 */
      gaddr kinds;     /* 0x3C: int32: 0-7 a sprite, 8-11 a flat sprite turned by sprite.ang, 12-15 a motion model */
      gaddr scales;    /* 0x40: f32 */
      gaddr alphas;    /* 0x44: f32 */
      gaddr sizes;     /* 0x48: f32 */
      gaddr frames;    /* 0x4C: f32 */
    };
  };
  gaddr bodies;        /* 0x50: a Body per particle */
} EffectSetB;
HW_OFFSET(EffectSetB, word14, 0x14);
HW_OFFSET(EffectSetB, object, 0x20);
HW_OFFSET(EffectSetB, count, 0x2C);
HW_OFFSET(EffectSetB, word30, 0x30);
HW_OFFSET(EffectSetB, arrays, 0x34);
HW_OFFSET(EffectSetB, frames, 0x4C);
HW_OFFSET(EffectSetB, bodies, 0x50);
HW_SIZE(EffectSetB, 0x54);

/* set C (constructor func_8C03BE38; g_object_set_c_count particles) */
typedef struct EffectSetC {
  NjSprite sprite;     /* 0x00: b07: scale 0.15625, a random angle, tanim 0x8C096CF8 */
  uint8_t pad_20[0x28 - 0x20];
  gaddr counters;      /* 0x28: int32 per particle */
  gaddr states;        /* 0x2C: int32 per particle: 1 = waiting, 2 = growing, 3 = ending, 0 = off */
  gaddr array30;       /* 0x30: b07: f32 scale (0.078125 at construction) */
  gaddr array34;       /* 0x34: b07: f32 alpha */
  gaddr values;        /* 0x38: f32 per particle */
  gaddr array3c;       /* 0x3C: b07: f32 (0 at construction) */
  gaddr bodies;        /* 0x40: a Body per particle */
} EffectSetC;
HW_OFFSET(EffectSetC, counters, 0x28);
HW_OFFSET(EffectSetC, bodies, 0x40);
HW_SIZE(EffectSetC, 0x44);

static inline RacerEffects *racer_effects_at(gaddr a) { return (RacerEffects *)hw_ptr(a); }

/* game/model.c */
void racer_effects_reset_ribbon(gaddr fx, f32 drop, gaddr m);
void ribbon_push(gaddr ribbon, gaddr frame); /* func_8C038FFE (S14 b05 declares it) */

/* game/racer_body.c (S14 b05): the racer's frame handed to each effect */
void racer_effects_push_ribbon(gaddr fx, gaddr frame);
void racer_effects_push_ribbons(gaddr fx, gaddr frame);
void racer_effects_push_set_b(gaddr fx, gaddr frame);
void racer_effects_push_set_c(gaddr fx, gaddr frame);

/* game/model.c (b07 declares them for their callers) */
gaddr ribbon_record_clear(gaddr rec);
void shared_data_load(void);
void model_draw(gaddr obj);
gaddr ribbon_set_construct(gaddr set);
void ribbon_set_free(gaddr set);
gaddr effect_set_b_construct(gaddr set, int32_t count);
/* game/stage.c (b07 declares them for their callers) */
void effect_set_a_free(gaddr set);
void effect_set_b_free(gaddr set);
void effect_set_c_free(gaddr set);
void effect_set_c_update(gaddr set);

/* b07, game/effects.c */
void effect_material_set(f32 r, f32 g, f32 b, f32 a);
gaddr racer_effects_init(gaddr fx, gaddr racer);
gaddr racer_effects_construct(gaddr fx, gaddr racer);
void racer_effects_free(gaddr fx);
gaddr racer_effects_delete(gaddr fx, int32_t free_memory);
void racer_effects_update(gaddr fx);
void racer_effects_draw(gaddr fx);
gaddr ribbon_construct(gaddr ribbon);
void ribbon_free(gaddr ribbon);
void ribbon_set_update(gaddr set);
void ribbon_set_draw(gaddr set);
gaddr effect_set_a_construct(gaddr set, int32_t count);
void effect_set_a_update(gaddr set);
void effect_set_a_draw(gaddr set);
void effect_set_b_update(gaddr set);
void effect_set_b_draw(gaddr set);
gaddr effect_set_c_construct(gaddr set);
void effect_set_c_draw(gaddr set);
void effect_set_c_emit(gaddr set, gaddr frame);

/* S15 c05, game/effects_push_b.c */
void effect_set_b_push(gaddr set, gaddr frame);

#endif
