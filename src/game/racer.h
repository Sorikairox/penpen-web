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

/* S14 b05: a collider (func_8C0308BC constructs it, func_8C030896 destroys it): the racer's is at 0x1B0. Its first
 * word points to its own storage at 0x6C; the Contacts' owner is the position it collides at. */
typedef struct Collider {
  gaddr data;         /* 0x00: = &store */
  int32_t word4;      /* 0x04: -1 */
  Contacts contacts;  /* 0x08: capacity 8 */
  uint8_t pad_5c[0x64 - 0x5C];
  gaddr position;     /* 0x64: the Vec3 it collides at (the Contacts' owner) */
  gaddr anchor;       /* 0x68: a Vec3 given at construction (the racer's vec2ac) */
  uint8_t store[0x24]; /* 0x6C: (the racer's: up to its 0x240) */
} Collider;
HW_OFFSET(Collider, contacts, 0x08);
HW_OFFSET(Collider, position, 0x64);
HW_OFFSET(Collider, store, 0x6C);
HW_SIZE(Collider, 0x90);

/* S14 b08: a button's edges of one update */
typedef struct ButtonEdges {
  int32_t pressed;  /* held now, not before */
  int32_t released; /* held before, not now */
} ButtonEdges;
HW_SIZE(ButtonEdges, 8);

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
  uint8_t pad_0f4[0x100 - 0x0F4];
  /* S14 b08: the racer's controls (its pad, or the computer player's choices; func_8C030F00 clears them) */
  int32_t button_prev[2];    /* 0x100: button[] of the previous update */
  int32_t button[2];         /* 0x108: the two buttons held */
  f32 stick_x;               /* 0x110 */
  f32 stick_y;               /* 0x114 */
  f32 stick_length;          /* 0x118: |stick| (the update computes it) */
  f32 stick_angle;           /* 0x11C: the stick's direction in Ninja angle units (kept while |stick| <= 0.1) */
  ButtonEdges edges[2];      /* 0x120: per button, pressed / released this update */
  int32_t button_lock[2];    /* 0x130: updates a button stays ignored after a release (15) */
  uint8_t pad_138[0x148 - 0x138];
  int32_t mark_a;            /* 0x148: achievement flags (racer_check_marks) */
  int32_t mark_b;            /* 0x14C */
  int32_t mark_c;            /* 0x150 */
  uint8_t pad_154[0x184 - 0x154];
  int32_t stick_damped;      /* 0x184: S14 b08: the update scales the stick down (x by 0.5, y by 0.75) */
  uint8_t pad_188[0x18C - 0x188];
  int32_t had_contacts;      /* 0x18C: racer_handle_contacts saw contacts this frame */
  uint8_t pad_190[0x194 - 0x190];
  gaddr scene;               /* 0x194: the RaceScene */
  uint8_t pad_198[0x19C - 0x198];
  int32_t state;             /* 0x19C */
  int32_t effects_shown;     /* 0x1A0: 0 = its effects are not drawn (racer_effects_draw) */
  uint8_t pad_1a4[0x1AC - 0x1A4];
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
  Body1 value35c[3];         /* 0x35C: (Quat-constructed; reset to value 1, damping 1) */
  Body1 value38c;            /* 0x38C */
  Vec3 vec39c;               /* 0x39C */
  Matrix matrix3a8;          /* 0x3A8 */
  Matrix matrix3e8;          /* 0x3E8 */
  Vec3 vec428;               /* 0x428 */
  f32 f434;                  /* 0x434: 0 at reset */
  int32_t index;             /* 0x438: from the constructor's arguments */
  gaddr params;              /* 0x43C: CharacterParams */
  Contacts contacts2;        /* 0x440: capacity 4, no owner */
  uint8_t pad_494[0x498 - 0x494];
  Vec3 camera_vec498;        /* 0x498 */
  Body1 camera_spring[3];    /* 0x4A4 */
  Vec3 camera_offset;        /* 0x4D4: (0, 0, camera_distance) at reset */
  Vec3 camera_vec4e0;        /* 0x4E0 */
  Body camera_eye;           /* 0x4EC */
  union {
    Vec3 camera_vec51c;      /* 0x51C: (its y and z words are cleared with the state, racer_set_state) */
    struct {                 /* S14 b08: how the camera functions use these words */
      f32 camera_f51c;       /* 0x51C: 0 after each camera step */
      int32_t camera_step;   /* 0x520: the intro camera's step (0: set up, 1: running) */
      int32_t camera_frames; /* 0x524: frames the intro camera has run */
    };
  };
  uint32_t word528;          /* 0x528 */
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
HW_OFFSET(Racer, button_prev, 0x100);
HW_OFFSET(Racer, button, 0x108);
HW_OFFSET(Racer, stick_x, 0x110);
HW_OFFSET(Racer, stick_angle, 0x11C);
HW_OFFSET(Racer, edges, 0x120);
HW_OFFSET(Racer, button_lock, 0x130);
HW_OFFSET(Racer, effects_shown, 0x1A0);
HW_OFFSET(Racer, stick_damped, 0x184);
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
HW_OFFSET(Racer, value35c, 0x35C);
HW_OFFSET(Racer, value38c, 0x38C);
HW_OFFSET(Racer, matrix3a8, 0x3A8);
HW_OFFSET(Racer, vec428, 0x428);
HW_OFFSET(Racer, f434, 0x434);
HW_OFFSET(Racer, index, 0x438);
HW_OFFSET(Racer, contacts2, 0x440);
HW_OFFSET(Racer, camera_vec498, 0x498);
HW_OFFSET(Racer, camera_spring, 0x4A4);
HW_OFFSET(Racer, camera_offset, 0x4D4);
HW_OFFSET(Racer, camera_vec4e0, 0x4E0);
HW_OFFSET(Racer, camera_eye, 0x4EC);
HW_OFFSET(Racer, camera_vec51c, 0x51C);
HW_OFFSET(Racer, camera_step, 0x520);
HW_OFFSET(Racer, camera_frames, 0x524);
HW_OFFSET(Racer, camera_f578, 0x578);
HW_OFFSET(Racer, camera_f580, 0x580);
HW_OFFSET(Racer, word528, 0x528);
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

/* A racer class larger than Racer: the computer player (S14 b08: constructor func_8C03DB9A -> func_8C03DB60, vtable
 * 0x8C23DB34, destructor body func_8C03DBC4, deleting destructor func_8C03DBEA; reset func_8C03DC54, update
 * func_8C03DE18). Its 0x000-0x58B are the Racer's (racer_at), built by the same base constructor (func_8C030BF4);
 * from 0x58C it follows the course's lanes: the course hands it, per lane (4) and per branch (2: the main way and a
 * side way), a table of vertex pairs (a row per step along the lane; func_8C03FA44 tests a point against the quad
 * between two rows) and the lane's motion, texture list and one more word. Its second base is at 0x6A4. */
typedef struct LaneRacer {
  uint8_t racer[0x58C];      /* 0x000: the Racer part */
  gaddr lanes[4][2];         /* 0x58C: [lane][branch]: the vertex pairs */
  gaddr lane_extra[4][2];    /* 0x5AC: the model set's fourth word */
  gaddr lane_motion[4][2];   /* 0x5CC */
  gaddr lane_texlist[4][2];  /* 0x5EC */
  int32_t lane_last[4];      /* 0x60C: the course's row count of the lane, - 1: its quads */
  gaddr lane_points;         /* 0x61C: lanes[lane][branch], the table in use */
  int32_t branch;            /* 0x620: 0 = the main way, 1 = the side way */
  int32_t branch_checked;    /* 0x624: the quad the branch test last ran at */
  int32_t branch_length;     /* 0x628: the side way's quads */
  int32_t lane;              /* 0x62C: the lane of the ground (the cell kinds 1-5 set it) */
  int32_t prev_lane;         /* 0x630 */
  int32_t quad;              /* 0x634: the quad of the lane the racer is on */
  int32_t prev_quad;         /* 0x638 */
  int32_t word63c;           /* 0x63C: 0 when the lane changes */
  int32_t word640;           /* 0x640 */
  uint8_t pad_644[0x64C - 0x644];
  int32_t range_index;       /* 0x64C: the constructor's last argument: a row of the range tables */
  f32 range1_min;            /* 0x650: from the first range table */
  f32 range1_max;            /* 0x654: min + the table's width (+ 10 for the characters 1, 4, 6) */
  f32 range2_min;            /* 0x658: the second table's */
  f32 range2_max;            /* 0x65C */
  f32 f660;                  /* 0x660 */
  int32_t word664;           /* 0x664 */
  int32_t word668;           /* 0x668 */
  Vec3 reset_position;       /* 0x66C: where the reset put it */
  f32 f678;                  /* 0x678 */
  int32_t word67c;           /* 0x67C */
  int32_t word680;           /* 0x680 */
  int32_t word684;           /* 0x684 */
  int32_t word688;           /* 0x688: -1 at reset */
  int32_t word68c;           /* 0x68C */
  int32_t word690;           /* 0x690: -1 at reset */
  f32 f694;                  /* 0x694: 0.5 at reset */
  uint8_t pad_698[0x6A4 - 0x698];
  uint8_t second_base_at[0x1C]; /* 0x6A4: Racer.second_base points here */
} LaneRacer;
HW_OFFSET(LaneRacer, lanes, 0x58C);
HW_OFFSET(LaneRacer, lane_extra, 0x5AC);
HW_OFFSET(LaneRacer, lane_motion, 0x5CC);
HW_OFFSET(LaneRacer, lane_texlist, 0x5EC);
HW_OFFSET(LaneRacer, lane_last, 0x60C);
HW_OFFSET(LaneRacer, lane_points, 0x61C);
HW_OFFSET(LaneRacer, branch, 0x620);
HW_OFFSET(LaneRacer, lane, 0x62C);
HW_OFFSET(LaneRacer, quad, 0x634);
HW_OFFSET(LaneRacer, word640, 0x640);
HW_OFFSET(LaneRacer, range_index, 0x64C);
HW_OFFSET(LaneRacer, range2_max, 0x65C);
HW_OFFSET(LaneRacer, word664, 0x664);
HW_OFFSET(LaneRacer, reset_position, 0x66C);
HW_OFFSET(LaneRacer, f678, 0x678);
HW_OFFSET(LaneRacer, word690, 0x690);
HW_OFFSET(LaneRacer, f694, 0x694);
HW_OFFSET(LaneRacer, second_base_at, 0x6A4);
#define LANE_RACER_VTABLE 0x8C23DB34u
static inline LaneRacer *lane_racer_at(gaddr a) { return (LaneRacer *)hw_ptr(a); }

static inline Racer *racer_at(gaddr a) { return (Racer *)hw_ptr(a); }

/* core/accessors.c (S14 b05 declares them for their callers) */
int32_t racer_state(gaddr racer);
int32_t racer_action(gaddr racer);
uint32_t racer_word55c(gaddr racer);
gaddr racer_velocity(gaddr racer);

/* game/racer_body.c (S14 b05) */
gaddr collider_construct(gaddr self, gaddr position, f32 radius, gaddr unused, gaddr anchor);
void racer_body_destroy(gaddr racer);
void racer_set_state(gaddr racer, int32_t state);
void racer_reset_at(gaddr racer, gaddr position, f32 b, f32 heading, f32 c);
static inline FrameBody *frame_body_at(gaddr a) { return (FrameBody *)hw_ptr(a); }

/* game/course.c (S13; S14 b08 declares it) */
int32_t lane_quad_contains(gaddr racer, gaddr point, int32_t quad);
/* game/stage.c (S13; S14 b08 declares them for their callers) */
void body_pull_axes(gaddr b, gaddr target, gaddr rate);
uint32_t racer_word1ac(gaddr racer);
/* game/racer_camera.c (S14 b08) */
void racer_camera_intro(gaddr racer);
void racer_camera_follow(gaddr racer);
void racer_camera_high(gaddr racer);
/* game/lane_racer.c (S14 b08) */
gaddr lane_racer_construct(gaddr self, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                           int32_t range_index, f32 radius);
void lane_racer_destroy(gaddr self);
void lane_racer_reset(gaddr self, gaddr position, uint32_t word, f32 value);
void lane_racer_update(gaddr self);
int32_t lane_racer_find_quad(gaddr self, gaddr point, int32_t near);
f32 racer_x_fraction(gaddr racer);

/* game/racer_control.c */
gaddr racer_init_base(gaddr racer, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, f32 f);
gaddr racer_construct(gaddr racer, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, f32 f);
void racer_read_controls(gaddr racer);
void racer_draw_thunk(gaddr racer);

#endif
