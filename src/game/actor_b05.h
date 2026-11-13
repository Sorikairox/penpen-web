/* Pen Pen port, S17 b05: three more scenery actor classes (placement types 0x00, 0x1B and 0x2F, built by the factory
 * func_8C043D4C, game/actor_factory.c), and declarations of functions other files export with no header yet. To be
 * merged into game/actor.h. */
#ifndef PENPEN_ACTOR_B05_H
#define PENPEN_ACTOR_B05_H

#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"

/* placement type 0x00 (constructor func_8C044AEC, 0xC0 bytes): an unseen sphere (radius 10) that other bodies bump;
 * its Body is pulled back toward its home every update (a debug draw shows the sphere) */
typedef struct ActorType00 {
  Actor base;               /* 0x00 */
  Vec3 home;                /* 0x28: the placement's position */
  Body body;                /* 0x34: starts 3 to the +x of home */
  Contacts contacts;        /* 0x64: owned by the body, radius 10, room for 8 */
  int32_t contacts_word;    /* 0xB8 */
  gaddr second_vtable;      /* 0xBC */
} ActorType00;
HW_OFFSET(ActorType00, home, 0x28);
HW_OFFSET(ActorType00, body, 0x34);
HW_OFFSET(ActorType00, contacts, 0x64);
HW_OFFSET(ActorType00, second_vtable, 0xBC);
HW_SIZE(ActorType00, 0xC0);

/* placement type 0x1B (constructor func_8C04928A, 0x90 bytes; its destructor body is empty). S20 W3: the same
 * layout as type 0x1C (ActorType1C, game/actor.h): a model of the group file's model set with one of its motions
 * (its init func_8C049308, game/actor_type1b_init.c) */
typedef struct ActorType1B {
  Actor base;               /* 0x00 */
  uint8_t pad_28[0x2C - 0x28];
  int32_t kind;             /* 0x2C: S20 W3: the placement's param bits 8-11: which model of the model set */
  uint8_t pad_30[0x34 - 0x30];
  int32_t motion;           /* 0x34: S20 W3: the entry of the motion table (the param's bits 0-3) */
  f32 frame;                /* 0x38: S20 W3: the motion's frame (0 at init) */
  f32 last_frame;           /* 0x3C: S20 W3: the motion's frame count - 1 (when the entry has a motion) */
  uint8_t pad_40[0x48 - 0x40];
  Vec3 angle;               /* 0x48: S20 W3: the placement's angle */
  gaddr object;             /* 0x54: S20 W3: NJS_OBJECT (the kind's model of the model set) */
  gaddr texlist;            /* 0x58: S20 W3 */
  gaddr motions;            /* 0x5C: S20 W3: MotionEntry table */
  uint8_t pad_60[0x80 - 0x60];
  Vec3 position;            /* 0x80: S20 W3: the placement's position */
  gaddr second_vtable;      /* 0x8C */
} ActorType1B;
HW_OFFSET(ActorType1B, kind, 0x2C);
HW_OFFSET(ActorType1B, motion, 0x34);
HW_OFFSET(ActorType1B, frame, 0x38);
HW_OFFSET(ActorType1B, last_frame, 0x3C);
HW_OFFSET(ActorType1B, angle, 0x48);
HW_OFFSET(ActorType1B, object, 0x54);
HW_OFFSET(ActorType1B, texlist, 0x58);
HW_OFFSET(ActorType1B, motions, 0x5C);
HW_OFFSET(ActorType1B, position, 0x80);
HW_OFFSET(ActorType1B, second_vtable, 0x8C);
HW_SIZE(ActorType1B, 0x90);

/* an NJS_ARGB (alpha first; argb_set fills it from (r, g, b, a)) */
typedef struct NjArgb {
  f32 a, r, g, b;
} NjArgb;
HW_SIZE(NjArgb, 16);

/* placement type 0x2F (constructor func_8C048C58, 0xE4 bytes): a Body with contacts (radius 10, room for 2) and a
 * state, drawn in a colour */
typedef struct ActorType2F {
  Actor base;               /* 0x00 */
  uint32_t param;           /* 0x28: the placement's param */
  int32_t state;            /* 0x2C: 0 at init (the update switches on 0, 1, 2) */
  int32_t word30;           /* 0x30: 0 at init */
  Vec3 home;                /* 0x34: the placement's position */
  Body body;                /* 0x40: at rest at the origin; at init, home + 2 in y */
  union {
    uint32_t word70;        /* 0x70 */
    f32 lift;               /* 0x70: S18 b04: in state 4, how far above the player it rides (150, less 2 an update) */
  };
  Contacts contacts;        /* 0x74: owned by the body, cell = the actor's, response func_8C048B56 */
  int32_t contacts_word;    /* 0xC8: 4 at init */
  NjArgb color;             /* 0xCC: (a 0.6, r 0, g 1, b 0) at init */
  union {
    int32_t worddc;         /* 0xDC: -1 at init */
    int32_t player;         /* 0xDC: S18 b04: the player it marks (one of its contacts' others, below 8) */
  };
  gaddr second_vtable;      /* 0xE0 */
} ActorType2F;
HW_OFFSET(ActorType2F, param, 0x28);
HW_OFFSET(ActorType2F, home, 0x34);
HW_OFFSET(ActorType2F, body, 0x40);
HW_OFFSET(ActorType2F, contacts, 0x74);
HW_OFFSET(ActorType2F, contacts_word, 0xC8);
HW_OFFSET(ActorType2F, color, 0xCC);
HW_OFFSET(ActorType2F, worddc, 0xDC);
HW_OFFSET(ActorType2F, player, 0xDC);
HW_OFFSET(ActorType2F, lift, 0x70);
HW_OFFSET(ActorType2F, second_vtable, 0xE0);
HW_SIZE(ActorType2F, 0xE4);

/* a sphere as the debug draw takes it: a centre and a radius */
typedef struct Sphere {
  Vec3 center;              /* 0x00 */
  f32 radius;               /* 0x0C */
} Sphere;
HW_SIZE(Sphere, 0x10);

/* exported elsewhere, declared here (merge into actor.h) */
gaddr actor_a_delete(gaddr obj, int32_t free_memory);  /* game/actor.c */
gaddr empty_delete(gaddr obj, int32_t free_memory);    /* game/actor.c */
void actor_type02_destroy(gaddr obj);                  /* game/actor_types.c */
void pieces_destroy(gaddr obj);                        /* game/actor_pieces.c */

/* game/actor_lifetimes.c (S17 b05) */
gaddr actor_type00_base_construct(gaddr obj);
gaddr actor_type00_construct(gaddr obj);
void actor_type00_destroy(gaddr obj);
gaddr actor_type2f_base_construct(gaddr obj);
gaddr actor_type2f_construct(gaddr obj);
gaddr actor_type1b_construct(gaddr obj);
void actor_type2b_destroy(gaddr obj);

/* S17: the rest of b05's functions (game/scenery_inline.c, course_positions.c, the lifetimes; the copies call them) */
void course_env_set_push(gaddr env, gaddr v);
void burst_set_origin(gaddr burst, gaddr v);
void burst_clear(gaddr burst);
void burst_trigger(gaddr burst);
f32 body_velocity_y(gaddr body);
void body_set_damping_y(gaddr body, f32 d);
void argb_set(f32 r, f32 g, f32 b, f32 a, gaddr c);
void vec3_mul(gaddr out, gaddr a, gaddr b);
void object_pair_draw(gaddr pair);
gaddr race_scene_course_words(gaddr scene);
f32 race_scene_finish_value(gaddr scene);
int32_t race_scene_course_variant(gaddr scene);
void course_set_word28_one(gaddr course);
int32_t course_current_row_bit(gaddr course, int32_t bit);
void attach_matrices_init(void);
int32_t course_segment_kind_marked(gaddr course, int32_t cell);
void body_rest(gaddr body);
void course_finish_position(gaddr course, int32_t i, gaddr out);
void race_scene_finish_position(gaddr scene, int32_t i, gaddr out);
void course_cell_corners(gaddr course, int32_t cell, gaddr a, gaddr b);
gaddr racer_effects_destroy_self(gaddr fx);
gaddr lane_racer_destroy_self(gaddr self);
gaddr course_destroy_self(gaddr course);
gaddr actor_construct_self(gaddr actor);
gaddr actor_group_destroy_self(gaddr group);
gaddr actor_a_destroy_self(gaddr obj);
gaddr actor_type02_destroy_self(gaddr obj);
gaddr empty_destroy_self(gaddr obj);
gaddr pieces_destroy_self(gaddr obj);
gaddr actor_b_destroy_self(gaddr obj);
gaddr buffer_pair_destroy_self(gaddr obj);
gaddr actor_type00_delete(gaddr obj, int32_t free_memory);
gaddr actor_type00_destroy_self(gaddr obj);
gaddr actor_type2b_delete(gaddr obj, int32_t free_memory);
gaddr actor_type2b_destroy_self(gaddr obj);
gaddr motion_player_destroy_self(gaddr player);
void burst_reset_particles(gaddr obj);
void actor_type00_init(gaddr obj, gaddr placement);
void actor_type00_update(gaddr obj);
void actor_type00_draw(gaddr obj);
void actor_type2f_init(gaddr obj, gaddr placement);
void actor_type2f_draw(gaddr obj);
void spinner_draw(gaddr obj);
void actor_group_reinit(gaddr group);
void matrix_scale_v(gaddr v);

#endif
