/* Pen Pen port, S14: the racer, a player of a race (game/racer.c, game/race_scene.c, core/accessors.c).
 *
 * The race scene builds each player with syMalloc(0x5A8) and func_8C03755C (vtable 0x8C23DACC; the base
 * construction is func_8C0374C2 -> func_8C030BF4, the deleting destructor func_8C03758C, the destructor body
 * func_8C0309A4). A Racer is a Task (updated by the scene's tree) holding:
 *  - at 0x24 its character: the penguin's model and motion player (func_8C037D24, freed by func_8C037F44: the model
 *    file at +0x40), its character and costume, its speed;
 *  - at 0x1B0 its collider: the racer's Contacts (core/contact.h; its cell is the racer's cell on the course);
 *  - at 0x2B8 its moving frame (FrameBody: a Body pushed and turned by two matrices), then a Body, the heading and
 *    other Body1 values, two matrices;
 *  - at 0x43C its character's parameters (a 0x434-byte record of the table at 0x8C085648: the mass at +8);
 *  - from 0x498 the camera that follows it: springs (Body1 at 0x4A4, 0x4B4, 0x4C4), the eye (a Body at 0x4EC).
 * Offsets are the original's; names follow what the written functions and the constructor do with them. */
#ifndef PENPEN_RACER_H
#define PENPEN_RACER_H

#include "../core/task.h"
#include "../core/vec.h"
#include "../core/contact.h"

/* a moving frame: a Body with an extra push and two matrices (func_8C03090E constructs it, func_8C037B1E steps it) */
typedef struct FrameBody {
  Body body;      /* 0x00: position, velocity, force, damping */
  Vec3 push;      /* 0x30: added to the velocity on the next step, then cleared */
  gaddr world;    /* 0x3C: the matrix the new velocity is turned by */
  gaddr local;    /* 0x40: the matrix force and velocity are turned by first */
} FrameBody;
HW_OFFSET(FrameBody, push, 0x30);
HW_OFFSET(FrameBody, local, 0x40);
HW_SIZE(FrameBody, 0x44);

/* the character parameters (0x434 bytes per character at 0x8C085648) */
typedef struct CharacterParams {
  uint8_t pad_00[8];
  f32 mass;       /* 0x08: the racer's Contacts' mass */
  uint8_t pad_0c[0x434 - 0x0C];
} CharacterParams;
HW_OFFSET(CharacterParams, mass, 0x08);
HW_SIZE(CharacterParams, 0x434);

typedef struct Racer {
  Task task;                 /* 0x000: vtable 0x8C23DACC */
  /* 0x024-0x1AF: the character (model and motion player, func_8C037D24 on racer + 0x24) */
  gaddr second_base;         /* 0x024: = &second_base_at (racer + 0x58C) */
  uint8_t pad_028[0x050 - 0x028];
  int32_t motion;            /* 0x050: the motion playing (character + 0x2C) */
  f32 motion_frame;          /* 0x054: its frame (character + 0x30) */
  uint8_t pad_058[0x0A4 - 0x058];
  int32_t character;         /* 0x0A4: which penguin (character + 0x80) */
  int32_t costume;           /* 0x0A8: (character + 0x84) */
  uint8_t pad_0ac[0x0F0 - 0x0AC];
  f32 speed;                 /* 0x0F0 */
  uint8_t pad_0f4[0x148 - 0x0F4];
  int32_t mark_a;            /* 0x148: achievement flags (racer_check_marks) */
  int32_t mark_b;            /* 0x14C */
  int32_t mark_c;            /* 0x150 */
  uint8_t pad_154[0x18C - 0x154];
  int32_t had_contacts;      /* 0x18C: racer_handle_contacts saw contacts this frame */
  uint8_t pad_190[0x194 - 0x190];
  gaddr scene;               /* 0x194: the RaceScene */
  uint8_t pad_198[0x19C - 0x198];
  int32_t state;             /* 0x19C */
  uint8_t pad_1a0[0x1AC - 0x1A0];
  uint32_t word1ac;          /* 0x1AC */
  /* 0x1B0-0x23F: the collider (func_8C0308BC) */
  gaddr collider_data;       /* 0x1B0: = racer + 0x21C */
  int32_t collider_word;     /* 0x1B4: -1 */
  Contacts contacts;         /* 0x1B8: capacity 8 */
  uint8_t pad_20c[0x214 - 0x20C];
  uint32_t collider_a;       /* 0x214 */
  uint32_t collider_b;       /* 0x218 */
  uint8_t pad_21c[0x240 - 0x21C];
  Vec3 vec240;               /* 0x240 */
  uint8_t pad_24c[0x258 - 0x24C];
  int32_t mark_a_on;         /* 0x258: mark_a counts */
  int32_t mark_b_on;         /* 0x25C: mark_b counts */
  Vec3 vec260;               /* 0x260 */
  f32 mark_b_value;          /* 0x26C */
  f32 mark_a_value;          /* 0x270 */
  uint8_t pad_274[0x278 - 0x274];
  int32_t hit_strength;      /* 0x278: 1 or 2, for action 3 */
  int32_t action;            /* 0x27C: set with its two arguments cleared (racer_set_action) */
  uint32_t action_arg[2];    /* 0x280 */
  uint8_t pad_288[0x2AC - 0x288];
  Vec3 vec2ac;               /* 0x2AC */
  FrameBody body;            /* 0x2B8 */
  Body body2fc;              /* 0x2FC */
  Body1 heading;             /* 0x32C: Ninja angle units */
  Body1 value33c;            /* 0x33C */
  Body1 value34c;            /* 0x34C */
  Quat quat35c[3];           /* 0x35C */
  Body1 value38c;            /* 0x38C */
  Vec3 vec39c;               /* 0x39C */
  Matrix matrix3a8;          /* 0x3A8 */
  Matrix matrix3e8;          /* 0x3E8 */
  Vec3 vec428;               /* 0x428 */
  uint8_t pad_434[0x438 - 0x434];
  int32_t index;             /* 0x438: from the constructor's arguments */
  gaddr params;              /* 0x43C: CharacterParams */
  Contacts contacts2;        /* 0x440: capacity 4, no owner */
  uint8_t pad_494[0x498 - 0x494];
  Vec3 camera_vec498;        /* 0x498 */
  Body1 camera_spring[3];    /* 0x4A4 */
  Vec3 camera_offset;        /* 0x4D4: (0, 0, camera_distance) at reset */
  Vec3 camera_vec4e0;        /* 0x4E0 */
  Body camera_eye;           /* 0x4EC */
  Vec3 camera_vec51c;        /* 0x51C */
  uint8_t pad_528[0x52C - 0x528];
  gaddr child;               /* 0x52C: a 0x3C-byte child task (func_8C038C1C) */
  uint32_t word530;          /* 0x530 */
  Vec3 vec534;               /* 0x534 */
  Vec3 vec540;               /* 0x540 */
  uint8_t pad_54c[0x55C - 0x54C];
  uint32_t word55c;          /* 0x55C */
  uint8_t pad_560[0x568 - 0x560];
  int32_t variant_on;        /* 0x568 */
  uint8_t pad_56c[0x570 - 0x56C];
  uint32_t variant_word;     /* 0x570: from the table at 0x8C0806FC by variant */
  int32_t variant;           /* 0x574: 1 to 5 (racer_set_variant) */
  f32 camera_f578;           /* 0x578 */
  f32 camera_f57c;           /* 0x57C */
  f32 camera_f580;           /* 0x580 */
  f32 camera_distance;       /* 0x584: -25 at reset */
  uint32_t word588;          /* 0x588 */
  uint8_t second_base_at[0x5A8 - 0x58C]; /* 0x58C */
} Racer;
HW_OFFSET(Racer, motion, 0x050);
HW_OFFSET(Racer, character, 0x0A4);
HW_OFFSET(Racer, speed, 0x0F0);
HW_OFFSET(Racer, mark_a, 0x148);
HW_OFFSET(Racer, had_contacts, 0x18C);
HW_OFFSET(Racer, scene, 0x194);
HW_OFFSET(Racer, state, 0x19C);
HW_OFFSET(Racer, word1ac, 0x1AC);
HW_OFFSET(Racer, contacts, 0x1B8);
HW_OFFSET(Racer, collider_a, 0x214);
HW_OFFSET(Racer, vec240, 0x240);
HW_OFFSET(Racer, mark_a_on, 0x258);
HW_OFFSET(Racer, mark_b_value, 0x26C);
HW_OFFSET(Racer, hit_strength, 0x278);
HW_OFFSET(Racer, action_arg, 0x280);
HW_OFFSET(Racer, vec2ac, 0x2AC);
HW_OFFSET(Racer, body, 0x2B8);
HW_OFFSET(Racer, body2fc, 0x2FC);
HW_OFFSET(Racer, heading, 0x32C);
HW_OFFSET(Racer, quat35c, 0x35C);
HW_OFFSET(Racer, value38c, 0x38C);
HW_OFFSET(Racer, matrix3a8, 0x3A8);
HW_OFFSET(Racer, vec428, 0x428);
HW_OFFSET(Racer, index, 0x438);
HW_OFFSET(Racer, contacts2, 0x440);
HW_OFFSET(Racer, camera_vec498, 0x498);
HW_OFFSET(Racer, camera_spring, 0x4A4);
HW_OFFSET(Racer, camera_offset, 0x4D4);
HW_OFFSET(Racer, camera_vec4e0, 0x4E0);
HW_OFFSET(Racer, camera_eye, 0x4EC);
HW_OFFSET(Racer, camera_vec51c, 0x51C);
HW_OFFSET(Racer, child, 0x52C);
HW_OFFSET(Racer, vec540, 0x540);
HW_OFFSET(Racer, word55c, 0x55C);
HW_OFFSET(Racer, variant_on, 0x568);
HW_OFFSET(Racer, variant_word, 0x570);
HW_OFFSET(Racer, variant, 0x574);
HW_OFFSET(Racer, camera_distance, 0x584);
HW_OFFSET(Racer, word588, 0x588);
HW_OFFSET(Racer, second_base_at, 0x58C);
HW_SIZE(Racer, 0x5A8);
#define RACER_VTABLE 0x8C23DACCu

/* A racer class larger than Racer (its 0x194 is the race scene and its 0xA4 the character, as in Racer; the
 * constructor is not identified): from 0x58C it keeps the course models of its lane (func_8C03DC54, func_8C03DE18
 * set them up), including the lane's vertex pairs at 0x61C (two Vec3 per row; func_8C03FA44 tests a point
 * against them). Only that field is declared. */
typedef struct LaneRacer {
  uint8_t pad_000[0x61C];
  gaddr lane_points;         /* 0x61C: Vec3 pairs, a row per step along the lane */
} LaneRacer;
HW_OFFSET(LaneRacer, lane_points, 0x61C);

static inline Racer *racer_at(gaddr a) { return (Racer *)hw_ptr(a); }
static inline FrameBody *frame_body_at(gaddr a) { return (FrameBody *)hw_ptr(a); }

#endif
