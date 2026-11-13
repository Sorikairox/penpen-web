/* Pen Pen port, S17 b06: three more scenery actor classes (placement types 0x06, 0x09 and 0x28, built by the factory
 * func_8C043D4C, game/actor_factory.c), and declarations of the functions this batch exports. To be merged into
 * game/actor.h. */
#ifndef PENPEN_ACTOR_B06_H
#define PENPEN_ACTOR_B06_H

#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"

/* placement type 0x06 (constructor func_8C04BBEE, 0xFC bytes; vtable 0x8C23DF54, second 0x8C23DF74): two Bodies,
 * contacts owned by the first (radius 10, room for 2) and a Burst of 20 particles */
typedef struct ActorType06 {
  Actor base;               /* 0x00 */
  Body body;                /* 0x28: the contacts' owner */
  uint8_t pad_58[0x64 - 0x58];
  gaddr burst;              /* 0x64: Burst (20 particles) or 0 */
  Body body2;               /* 0x68 */
  uint8_t pad_98[0xA0 - 0x98];
  Contacts contacts;        /* 0xA0 */
  int32_t contacts_word;    /* 0xF4 */
  gaddr second_vtable;      /* 0xF8 */
} ActorType06;
HW_OFFSET(ActorType06, body, 0x28);
HW_OFFSET(ActorType06, burst, 0x64);
HW_OFFSET(ActorType06, body2, 0x68);
HW_OFFSET(ActorType06, contacts, 0xA0);
HW_OFFSET(ActorType06, contacts_word, 0xF4);
HW_OFFSET(ActorType06, second_vtable, 0xF8);
HW_SIZE(ActorType06, 0xFC);

/* placement type 0x09 (constructor func_8C04CDA0, 0xB4 bytes; vtable 0x8C23E01C, second 0x8C23E03C): a model at
 * the placement's position that bursts (a Burst of 20 particles) the first time something touches its contacts
 * (radius 7, room for 2) */
typedef struct ActorType09 {
  Actor base;               /* 0x00 */
  int32_t state;            /* 0x28: 0 whole (drawn, its contacts in the race scene's slot 8), 1 burst */
  Vec3 position;            /* 0x2C: the contacts' owner */
  f32 angle_y;              /* 0x38: the placement's angle about y */
  gaddr model;              /* 0x3C: model 2 of the actor's list */
  gaddr burst;              /* 0x40: Burst (20 particles) or 0 */
  uint8_t pad_44[0x58 - 0x44];
  Contacts contacts;        /* 0x58 */
  int32_t contacts_word;    /* 0xAC: 4 at set-up */
  gaddr second_vtable;      /* 0xB0 */
} ActorType09;
HW_OFFSET(ActorType09, state, 0x28);
HW_OFFSET(ActorType09, position, 0x2C);
HW_OFFSET(ActorType09, angle_y, 0x38);
HW_OFFSET(ActorType09, model, 0x3C);
HW_OFFSET(ActorType09, burst, 0x40);
HW_OFFSET(ActorType09, contacts, 0x58);
HW_OFFSET(ActorType09, contacts_word, 0xAC);
HW_OFFSET(ActorType09, second_vtable, 0xB0);
HW_SIZE(ActorType09, 0xB4);

/* placement type 0x28 (constructor func_8C04E528, 0x27C bytes; vtable 0x8C23E0DC, second 0x8C23E0FC): five Body1s,
 * three Bodies, two contact sets, and a particle spray like the Effect's (per-particle arrays of
 * g_type28_particles entries: countdown, state, value, spin, Body) */
typedef struct ActorType28 {
  Actor base;               /* 0x000 */
  int32_t word28;           /* 0x028: 0: the draw also draws the particles (func_8C04F3A2) */
  int32_t motion;           /* 0x02C: the entry of the motion table it is drawn with */
  uint8_t pad_030[0x04C - 0x030];
  f32 frame;                /* 0x04C: the motion's frame (handed by address) */
  uint8_t pad_050[0x054 - 0x050];
  Body1 pulls[5];           /* 0x054: (0, 0, 0, 1) at construction */
  Quat turn_a;              /* 0x0A4 */
  Quat turn_b;              /* 0x0B4 */
  Vec3 point;               /* 0x0C4: the first contacts' owner */
  Body body;                /* 0x0D0 */
  Vec3 point100;            /* 0x100 */
  uint8_t pad_10c[0x110 - 0x10C];
  gaddr object;             /* 0x110: NJS_OBJECT */
  gaddr texlist;            /* 0x114 */
  gaddr motions;            /* 0x118: MotionEntry table */
  Body body_b;              /* 0x11C */
  Body body_c;              /* 0x14C: the second contacts' owner */
  Quat turn_c;              /* 0x17C */
  Contacts contacts;        /* 0x18C: radius g_type28_radius_a, room for 2 */
  int32_t contacts_word;    /* 0x1E0 */
  Contacts contacts_b;      /* 0x1E4: radius g_type28_radius_b, room for 2 */
  int32_t contacts_b_word;  /* 0x238 */
  f32 scale_a;              /* 0x23C: 0 at reset */
  f32 scale_b;              /* 0x240: 0 at reset */
  f32 f244;                 /* 0x244: 0 at reset */
  uint8_t pad_248[0x250 - 0x248];
  int32_t counter;          /* 0x250: 0 at reset */
  uint32_t word254;         /* 0x254: a global word at reset */
  gaddr table;              /* 0x258: a static table at reset */
  gaddr words25c;           /* 0x25C: 0x20-byte block */
  gaddr countdowns;         /* 0x260: int32 per particle */
  gaddr states;             /* 0x264: int32 per particle: 1 counting down, 2 growing, 3 ending, 0 off */
  gaddr values;             /* 0x268: f32 per particle: grows by 1 up to g_type28_value_limit */
  gaddr spins;              /* 0x26C: f32 per particle: + g_type28_spin_step every step it moves */
  gaddr words270;           /* 0x270: 0x20-byte block */
  gaddr bodies;             /* 0x274: Body per particle (8, from operator new[]) */
  gaddr second_vtable;      /* 0x278 */
} ActorType28;
HW_OFFSET(ActorType28, pulls, 0x054);
HW_OFFSET(ActorType28, motion, 0x02C);
HW_OFFSET(ActorType28, frame, 0x04C);
HW_OFFSET(ActorType28, object, 0x110);
HW_OFFSET(ActorType28, motions, 0x118);
HW_OFFSET(ActorType28, turn_a, 0x0A4);
HW_OFFSET(ActorType28, turn_b, 0x0B4);
HW_OFFSET(ActorType28, point, 0x0C4);
HW_OFFSET(ActorType28, body, 0x0D0);
HW_OFFSET(ActorType28, point100, 0x100);
HW_OFFSET(ActorType28, body_b, 0x11C);
HW_OFFSET(ActorType28, body_c, 0x14C);
HW_OFFSET(ActorType28, turn_c, 0x17C);
HW_OFFSET(ActorType28, contacts, 0x18C);
HW_OFFSET(ActorType28, contacts_word, 0x1E0);
HW_OFFSET(ActorType28, contacts_b, 0x1E4);
HW_OFFSET(ActorType28, scale_a, 0x23C);
HW_OFFSET(ActorType28, f244, 0x244);
HW_OFFSET(ActorType28, counter, 0x250);
HW_OFFSET(ActorType28, table, 0x258);
HW_OFFSET(ActorType28, words25c, 0x25C);
HW_OFFSET(ActorType28, countdowns, 0x260);
HW_OFFSET(ActorType28, states, 0x264);
HW_OFFSET(ActorType28, values, 0x268);
HW_OFFSET(ActorType28, spins, 0x26C);
HW_OFFSET(ActorType28, words270, 0x270);
HW_OFFSET(ActorType28, bodies, 0x274);
HW_OFFSET(ActorType28, second_vtable, 0x278);
HW_SIZE(ActorType28, 0x27C);

/* ---- S17 b06: exported functions ---- */
/* game/actor_type06.c */
gaddr actor_type06_base_construct(gaddr obj);
gaddr actor_type06_construct(gaddr obj);
void actor_type06_destroy(gaddr obj);
gaddr actor_type06_delete(gaddr obj, int32_t free_memory);
gaddr actor_type06_destroy_self(gaddr obj);
/* game/actor_type09.c */
gaddr actor_type09_base_construct(gaddr obj);
gaddr actor_type09_construct(gaddr obj);
void actor_type09_destroy(gaddr obj);
gaddr actor_type09_delete(gaddr obj, int32_t free_memory);
gaddr actor_type09_destroy_self(gaddr obj);
void actor_type09_init(gaddr obj, gaddr placement);
void actor_type09_update(gaddr obj);
void actor_type09_draw(gaddr obj);
/* game/actor_type28.c */
gaddr actor_type28_base_construct(gaddr obj);
gaddr actor_type28_construct(gaddr obj);
void actor_type28_destroy(gaddr obj);
gaddr actor_type28_delete(gaddr obj, int32_t free_memory);
gaddr actor_type28_destroy_self(gaddr obj);
void actor_type28_reset(gaddr obj);
void actor_type28_free_blocks(gaddr obj);
void actor_type28_step_particles(gaddr obj);
void material_set_e028(f32 r, f32 g, f32 b, f32 a);
void actor_type28_draw(gaddr obj);
int32_t race_scene_course_ground(gaddr scene, gaddr pos, int32_t cell, gaddr distance, int32_t flags);
/* game/actor_motes_b06.c */
void bubbles_draw(gaddr obj);
gaddr member_array_delete(gaddr obj, int32_t free_memory);
void motes_draw(gaddr obj);
void spring_destroy(gaddr obj);
gaddr spring_delete(gaddr obj, int32_t free_memory);
gaddr spring_destroy_self(gaddr obj);
void spring_draw(gaddr obj);
/* game/actor_bodies_b06.c */
void post_destroy(gaddr obj);
gaddr post_delete(gaddr obj, int32_t free_memory);
gaddr post_destroy_self(gaddr obj);
void post_draw(gaddr obj);
void balls_destroy(gaddr obj);
gaddr balls_delete(gaddr obj, int32_t free_memory);
gaddr balls_destroy_self(gaddr obj);
void balls_draw(gaddr obj);
/* game/actor_type19_b06.c */
f32 f32_quotient(f32 a, f32 b);
void wobbler_destroy(gaddr obj);
gaddr wobbler_delete(gaddr obj, int32_t free_memory);
void wobbler_draw(gaddr obj);
void actor_type19_destroy(gaddr obj);
gaddr actor_type19_delete(gaddr obj, int32_t free_memory);
gaddr actor_type19_destroy_self(gaddr obj);
void actor_type19_draw(gaddr obj);
void actor_type19_path_start(gaddr obj);
int32_t actor_type19_path_step(gaddr obj);
/* game/actor_type24_b06.c */
void actor_type24_destroy(gaddr obj);
gaddr actor_type24_delete(gaddr obj, int32_t free_memory);
gaddr actor_type24_destroy_self(gaddr obj);
void actor_type24_draw(gaddr obj);

#endif
