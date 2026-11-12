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

/* the ribbon: its first word points to an object whose first word is the array of 4 * g_ribbon_length points */
typedef struct Ribbon {
  gaddr points_holder; /* 0x00 */
  uint32_t word4;      /* 0x04 */
} Ribbon;
HW_SIZE(Ribbon, 8);

/* what ribbon_push is handed: a matrix and, after it, the two heights the edges are placed between (a local of the
 * racer's update, func_8C0351C8) */
typedef struct RibbonFrame {
  Matrix m;            /* 0x00 */
  uint8_t pad_40[0x70 - 0x40];
  f32 low;             /* 0x70 */
  f32 high;            /* 0x74 */
} RibbonFrame;
HW_OFFSET(RibbonFrame, low, 0x70);
HW_OFFSET(RibbonFrame, high, 0x74);

/* a 0x18-byte record of the ribbon's (func_8C038788 clears one) */
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
typedef struct RibbonSet {
  uint8_t pad_00[0x14];
  uint32_t word14;     /* 0x14: 0 */
  uint32_t shared_word; /* 0x18 */
  gaddr data;          /* 0x1C: RIBBON_SET_DATA */
  uint8_t pad_20[0x24 - 0x20];
  gaddr model;         /* 0x24: from the shared data's table (by g_ribbon_kind) */
  gaddr texture;       /* 0x28 */
  int32_t count;       /* 0x2C: g_ribbon_count */
  uint8_t pad_30[0x34 - 0x30];
  gaddr arrays[RIBBON_SET_ARRAYS]; /* 0x34: a word per ribbon each (the third, 0x3C, cleared) */
  gaddr bodies;        /* 0x54: a Body per ribbon */
} RibbonSet;
HW_OFFSET(RibbonSet, word14, 0x14);
HW_OFFSET(RibbonSet, model, 0x24);
HW_OFFSET(RibbonSet, count, 0x2C);
HW_OFFSET(RibbonSet, arrays, 0x34);
HW_OFFSET(RibbonSet, bodies, 0x54);
HW_SIZE(RibbonSet, 0x58);

/* set A (constructor func_8C039CF2): `count` at 0x20, arrays from 0x30 */
typedef struct EffectSetA {
  uint8_t pad_00[0x30];
  gaddr arrays[8];     /* 0x30 */
} EffectSetA;
HW_OFFSET(EffectSetA, arrays, 0x30);
HW_SIZE(EffectSetA, 0x50);

/* set B (constructor func_8C03AF80) */
typedef struct EffectSetB {
  uint8_t pad_00[0x14];
  uint32_t word14;     /* 0x14: 0 */
  uint32_t shared_word; /* 0x18 */
  gaddr data;          /* 0x1C: RIBBON_SET_DATA */
  uint8_t pad_20[0x2C - 0x20];
  int32_t count;       /* 0x2C */
  uint32_t word30;     /* 0x30: 1 */
  gaddr arrays[7];     /* 0x34: a word per particle each (the second, 0x38, cleared) */
  gaddr bodies;        /* 0x50: a Body per particle */
} EffectSetB;
HW_OFFSET(EffectSetB, word14, 0x14);
HW_OFFSET(EffectSetB, count, 0x2C);
HW_OFFSET(EffectSetB, arrays, 0x34);
HW_OFFSET(EffectSetB, bodies, 0x50);
HW_SIZE(EffectSetB, 0x54);

/* set C (constructor func_8C03BE38; g_object_set_c_count particles) */
typedef struct EffectSetC {
  uint8_t pad_00[0x28];
  gaddr counters;      /* 0x28: int32 per particle */
  gaddr states;        /* 0x2C: int32 per particle: 1 = waiting, 2 = growing, 3 = ending, 0 = off */
  gaddr array30;       /* 0x30 */
  gaddr array34;       /* 0x34 */
  gaddr values;        /* 0x38: f32 per particle */
  gaddr array3c;       /* 0x3C */
  gaddr bodies;        /* 0x40: a Body per particle */
} EffectSetC;
HW_OFFSET(EffectSetC, counters, 0x28);
HW_OFFSET(EffectSetC, bodies, 0x40);
HW_SIZE(EffectSetC, 0x44);

static inline RacerEffects *racer_effects_at(gaddr a) { return (RacerEffects *)hw_ptr(a); }

/* game/model.c */
void racer_effects_reset_ribbon(gaddr fx, f32 drop, gaddr m);

#endif
