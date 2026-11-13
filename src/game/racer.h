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
  int32_t word4;      /* 0x04: -1 (S15 b07: the cell before the last update, func_8C041880) */
  Contacts contacts;  /* 0x08: capacity 8 (S15 b07: contacts.cell is the course cell it is on) */
  int32_t contacts_word; /* 0x5C: S15 b07: the word after the Contacts (4 after each update) */
  int32_t segment_kind;  /* 0x60: S15 b07: the kind of the course segment its cell is in */
  gaddr position;     /* 0x64: the Vec3 it collides at (the Contacts' owner) */
  gaddr anchor;       /* 0x68: a Vec3 given at construction (the racer's vec2ac) */
  uint8_t store[0x24]; /* 0x6C: (the racer's: up to its 0x240) */
} Collider;
HW_OFFSET(Collider, contacts, 0x08);
HW_OFFSET(Collider, contacts_word, 0x5C);
HW_OFFSET(Collider, segment_kind, 0x60);
HW_OFFSET(Collider, position, 0x64);
HW_OFFSET(Collider, store, 0x6C);
HW_SIZE(Collider, 0x90);

/* S14 b08: a button's edges of one update */
typedef struct ButtonEdges {
  int32_t pressed;  /* held now, not before */
  int32_t released; /* held before, not now */
} ButtonEdges;
HW_SIZE(ButtonEdges, 8);

/* S16 b04: a pair of horizontal dampings (CharacterParams.run_damping) */
typedef struct RunDamping {
  f32 x, z;
} RunDamping;
HW_SIZE(RunDamping, 8);

/* the character parameters (0x434 bytes per character at 0x8C085648) */
typedef struct CharacterParams {
  f32 radius;     /* 0x00: S15 b04: the racer's collider's radius (racer_base_init) */
  f32 f04;        /* 0x04: S15 b05: copied into the frame for set C (RibbonFrame.high) */
  f32 mass;       /* 0x08: the racer's Contacts' mass */
  f32 dash_mass;  /* 0x0C: S15 b05: its mass while action 2 (the dash) starts */
  f32 hit_mass;   /* 0x10: S15 b05: its mass while action 3 (hit) runs */
  uint8_t pad_14[0x18 - 0x14];
  f32 walk_push[3]; /* 0x18: S16 b04: state 3's forward push per stick length, by how far the stick turns from
                     * ahead (racer_run: below 100 degrees, below 150, more) */
  f32 walk_turn[3]; /* 0x24: S16 b04: its turn rate per stick y, the same way */
  f32 jump_push;  /* 0x30: S16 b04: state 3 on special ground: the upward push when button 0 is pressed */
  f32 lift_push;  /* 0x34: S16 b04: state 3 elsewhere: the upward push while button 0 is held */
  uint8_t pad_38[0x44 - 0x38];
  f32 dash_push;  /* 0x44: S15 b05: added to the moving body's push (z) when the dash starts */
  uint8_t pad_48[0x4C - 0x48];
  f32 walk_scale[6]; /* 0x4C: S16 b04: state 3 on special ground: the push scaled by Racer.ground */
  f32 damping_y;  /* 0x64: S15 b05: the moving body's vertical damping in the dash and the hit */
  f32 ground_damping[7]; /* 0x68: S15 b05: its horizontal damping while hit, by Racer.ground (0-4 seen) */
  f32 dash_damping;      /* 0x84: S15 b05: its horizontal damping in the dash */
  uint8_t pad_88[0x90 - 0x88];
  int32_t dash_frames;    /* 0x90: S15 b05: the dash's first part */
  int32_t dash_end_frames; /* 0x94: its second part */
  int32_t hit_frames[3];  /* 0x98: S15 b05: action 3's length by hit_strength: [0] for 2, [1] for 1, [2] for 0 */
  f32 run_push[6][7];     /* 0x0A4: S16 b04: state 4's forward push (z) per update, by Racer.ground and stroke_level */
  f32 run_turn[6][7];     /* 0x14C: S16 b04: its turn rate (times stick_y), the same way */
  f32 slow_turn;          /* 0x1F4: S16 b04: how much more it turns at rest (scaled by how far f434 is below
                           * 0.463) */
  f32 f1f8;       /* 0x1F8: S15 b05: state 4's action 4: push (z) while on the special ground */
  f32 f1fc;       /* 0x1FC: its turn rate (times stick_y) */
  f32 f200;       /* 0x200: its damping (all axes off the special ground; y on it) */
  RunDamping run_damping[6][7]; /* 0x204: S16 b04: state 4's x / z damping on special ground, by ground and
                                 * stroke_level */
  f32 f354;       /* 0x354: its x damping on the special ground */
  f32 f358;       /* 0x358: its z damping on the special ground */
  uint8_t pad_35c[0x360 - 0x35C];
  int32_t hold_levels[5]; /* 0x360: S16 b04: state 4: the stroke level is n + 1 once the button was held longer
                           * than hold_levels[n] (the highest that holds) */
  int32_t release_frames_max; /* 0x374: S16 b04: level 6 while released for 1 to this many updates */
  int32_t frames378; /* 0x378: its length (scaled by the item, func_8C03438C) */
  uint8_t pad_37c[0x434 - 0x37C];
} CharacterParams;
HW_OFFSET(CharacterParams, mass, 0x08);
HW_OFFSET(CharacterParams, hit_mass, 0x10);
HW_OFFSET(CharacterParams, dash_push, 0x44);
HW_OFFSET(CharacterParams, damping_y, 0x64);
HW_OFFSET(CharacterParams, dash_damping, 0x84);
HW_OFFSET(CharacterParams, dash_frames, 0x90);
HW_OFFSET(CharacterParams, hit_frames, 0x98);
HW_OFFSET(CharacterParams, f1f8, 0x1F8);
HW_OFFSET(CharacterParams, f200, 0x200);
HW_OFFSET(CharacterParams, f354, 0x354);
HW_OFFSET(CharacterParams, frames378, 0x378);
HW_OFFSET(CharacterParams, run_push, 0x0A4);
HW_OFFSET(CharacterParams, walk_push, 0x018);
HW_OFFSET(CharacterParams, lift_push, 0x034);
HW_OFFSET(CharacterParams, walk_scale, 0x04C);
HW_OFFSET(CharacterParams, run_turn, 0x14C);
HW_OFFSET(CharacterParams, slow_turn, 0x1F4);
HW_OFFSET(CharacterParams, run_damping, 0x204);
HW_OFFSET(CharacterParams, f354, 0x354);
HW_OFFSET(CharacterParams, hold_levels, 0x360);
HW_OFFSET(CharacterParams, release_frames_max, 0x374);
HW_SIZE(CharacterParams, 0x434);
static inline CharacterParams *character_params_at(gaddr a) { return (CharacterParams *)hw_ptr(a); }

typedef struct Racer {
  Task task;                 /* 0x000: vtable 0x8C23DACC */
  /* 0x024-0x1AF: the character (model and motion player, func_8C037D24 on racer + 0x24) */
  gaddr second_base;         /* 0x024: = &second_base_at (racer + 0x58C) */
  uint8_t pad_028[0x050 - 0x028];
  int32_t motion;            /* 0x050: the motion playing (character + 0x2C) */
  f32 motion_frame;          /* 0x054: its frame (character + 0x30) */
  uint8_t pad_058[0x084 - 0x058];
  int32_t looks[4];          /* 0x084: S15 b04: the character's four looks (character + 0x60; racer_base_init: by
                              * the table at 0x8C085514, or 0, 0, 1, 1 with a costume) */
  uint8_t pad_094[0x0A4 - 0x094];
  int32_t character;         /* 0x0A4: which penguin (character + 0x80) */
  int32_t costume;           /* 0x0A8: (character + 0x84) */
  int32_t racer_contact_prev;  /* 0x0AC: S15 b05: racer_contact of the previous update */
  int32_t object_contact_prev; /* 0x0B0: object_contact of the previous update */
  int32_t racer_contact;     /* 0x0B4: S15 b05: the contact (index in contacts) with another racer, -1: none
                              * (func_8C031D8C) */
  int32_t object_contact;    /* 0x0B8: the contact with something else, -1: none */
  Vec3 vec0bc;               /* 0x0BC: S15 b06: the squash axis that mark_d sets (racer_squash_update) */
  Vec3 object_push;          /* 0x0C8: S16 b04: the same push from the contacts with something else
                              * (racer_sort_contacts) */
  Vec3 racer_velocity;       /* 0x0D4: S16 b04: the other racer's velocity + its own, of the first racer contact */
  Vec3 object_velocity;      /* 0x0E0: S16 b04: the same, of the first object contact */
  f32 racer_speed;           /* 0x0EC: S16 b04: |racer_velocity| */
  f32 speed;                 /* 0x0F0: (S16 b04: |object_velocity|, set by racer_sort_contacts) */
  f32 object_grip;           /* 0x0F4: S16 b04: 0 when the object contact's kind is 0xCD, else 1 */
  uint8_t pad_0f8[0x0FC - 0x0F8];
  uint32_t word0fc;          /* 0x0FC: S16 w3: racer_reset's third argument for players 0-3 (index < 4), else 0 */
  /* S14 b08: the racer's controls (its pad, or the computer player's choices; func_8C030F00 clears them) */
  int32_t button_prev[2];    /* 0x100: button[] of the previous update */
  int32_t button[2];         /* 0x108: the two buttons held */
  f32 stick_x;               /* 0x110 */
  f32 stick_y;               /* 0x114 */
  f32 stick_length;          /* 0x118: |stick| (the update computes it) */
  f32 stick_angle;           /* 0x11C: the stick's direction in Ninja angle units (kept while |stick| <= 0.1) */
  ButtonEdges edges[2];      /* 0x120: per button, pressed / released this update */
  int32_t button_lock[2];    /* 0x130: updates a button stays ignored after a release (15) */
  int32_t on_kind4_prev;     /* 0x138: S16 b04: (read only by racer_sort_contacts here) */
  int32_t on_kind4;          /* 0x13C: S16 b04: an object contact of kind 4 this update */
  int32_t on_kind4_new;      /* 0x140: S16 b04: on_kind4 and not on_kind4_prev: action 4 starts */
  int32_t word144;           /* 0x144: S15 b05: state 2 plays motion set 2 when set, 3 when 0 */
  union {
    struct {
      int32_t mark_a;        /* 0x148: achievement flags (racer_check_marks) */
      int32_t mark_b;        /* 0x14C */
      int32_t mark_c;        /* 0x150 */
      int32_t mark_d;        /* 0x154: S15 b06 */
      int32_t mark_e;        /* 0x158: S15 b06 */
    };
    int32_t marks[5];        /* 0x148: S15 b06: cleared every update after being noted in marks_prev */
  };
  int32_t marks_prev[5];     /* 0x15C: S15 b06: marks[] of the previous update (racer_squash_update) */
  int32_t word170;           /* 0x170: S16 b04: word174 of the previous update (racer_ground) */
  int32_t word174;           /* 0x174: S15 b05 (copied into the frame for set C) */
  int32_t word178;           /* 0x178: S16 b04: word174 is set this update and was not the previous one */
  int32_t word17c;           /* 0x17C: S15 b05 */
  int32_t word180;           /* 0x180: S15 b05 */
  int32_t stick_damped;      /* 0x184: S14 b08: the update scales the stick down (x by 0.5, y by 0.75) */
  f32 f188;                  /* 0x188: S16 w3: 0 at reset */
  int32_t had_contacts;      /* 0x18C: racer_handle_contacts saw contacts this frame */
  int32_t word190;           /* 0x190: S16 w3: 0 at reset */
  gaddr scene;               /* 0x194: the RaceScene */
  union {
    int32_t word198;         /* 0x198: S16 w3: 0 at reset */
    int32_t state_prev;      /* 0x198: S16 w4: state at the start of the update (racer_base_update) */
  };
  int32_t state;             /* 0x19C */
  int32_t effects_shown;     /* 0x1A0: 0 = its effects are not drawn (racer_effects_draw) */
  union {
    int32_t word1a4;         /* 0x1A4: S16 w3: 0 at reset */
    int32_t progress_prev;   /* 0x1A4: S16 w4: word1a8 before this update's ground kind counted (a kind 9 cell
                              * clears it); a time is noted in times298 when the two differ */
  };
  int32_t word1a8;           /* 0x1A8: S15 b04: (func_8C029150; the time trial's timer shows times298[word1a8 - 3]
                              * once the player is in state 2) */
  uint32_t word1ac;          /* 0x1AC */
  /* 0x1B0-0x23F: the collider (func_8C0308BC) */
  gaddr collider_data;       /* 0x1B0: = racer + 0x21C */
  int32_t collider_word;     /* 0x1B4: -1 */
  Contacts contacts;         /* 0x1B8: capacity 8 */
  int32_t contacts_word;     /* 0x20C: S16 w3: the collider's word after its Contacts (Collider.contacts_word): 4 */
  int32_t word210;           /* 0x210: S16 w3: 3 at reset */
  uint32_t collider_a;       /* 0x214 */
  uint32_t collider_b;       /* 0x218 */
  uint8_t pad_21c[0x238 - 0x21C];
  int32_t ground_prev;       /* 0x238: S15 b05: ground of the previous update */
  int32_t ground;            /* 0x23C: S15 b05: the kind of special ground its contacts touch (func_8C03207C: 2,
                              * 3, 4 by the contact's attribute; 0 = none) */
  Vec3 vec240;               /* 0x240: (S15 b05: where the special ground was touched) */
  int32_t ground_touch_prev; /* 0x24C: S15 b05: ground_touch of the previous update */
  int32_t ground_touch;      /* 0x250: S15 b05: its contacts touch special ground this update */
  int32_t word254;           /* 0x254: S16 b04: it left special ground this update (ground_touch_prev and not
                              * ground_touch; racer_run keeps step_timer at 3 while it is set) */
  int32_t mark_a_on;         /* 0x258: mark_a counts */
  int32_t mark_b_on;         /* 0x25C: mark_b counts */
  Vec3 vec260;               /* 0x260 */
  f32 mark_b_value;          /* 0x26C */
  f32 mark_a_value;          /* 0x270 */
  int32_t step_timer;        /* 0x274: S16 b04: racer_run: counts down off special ground; at 0 motion set 0
                              * plays and word144 flips (below 0 it stays -1); 3 while word254 is set */
  int32_t hit_strength;      /* 0x278: 1 or 2, for action 3 */
  int32_t action;            /* 0x27C: set with its two arguments cleared (racer_set_action) */
  union {
    uint32_t action_arg[2];  /* 0x280 */
    struct {                 /* S15 b05: how the actions use them */
      int32_t action_timer;  /* 0x280: frames left in the action's step (it ends when it goes below 0) */
      int32_t action_step;   /* 0x284: the action's step (0: start it) */
    };
  };
  union {
    int32_t word288;         /* 0x288: S15 b05: cleared when state 4 starts (func_8C033DC4) */
    int32_t stroke_level;    /* 0x288: S16 b04: state 4: 0-6, picks the push, turn and damping (racer_state4_run) */
  };
  union {
    int32_t word28c;         /* 0x28C: the same */
    int32_t hold_frames;     /* 0x28C: S16 b04: updates button 0 has been held */
  };
  union {
    int32_t word290;         /* 0x290: the same */
    int32_t last_hold;       /* 0x290: S16 b04: hold_frames of the last hold, noted at the release */
  };
  union {
    int32_t word294;         /* 0x294: the same */
    int32_t release_frames;  /* 0x294: S16 b04: updates since button 0 was released */
  };
  f32 times298[5];           /* 0x298: S15 b04: times (func_8C029168 reads one; the timer shows it) */
  Vec3 vec2ac;               /* 0x2AC */
  FrameBody body;            /* 0x2B8 */
  Body body2fc;              /* 0x2FC */
  Body1 heading;             /* 0x32C: Ninja angle units */
  Body1 value33c;            /* 0x33C */
  Body1 value34c;            /* 0x34C */
  Body1 value35c[3];         /* 0x35C: (Quat-constructed; reset to value 1, damping 1); S15 b06: the draw's scale
                              * along vec39c (racer_draw), springs pulled back to 1 (racer_squash_update) */
  Body1 value38c;            /* 0x38C */
  Vec3 vec39c;               /* 0x39C: S15 b06: the axis of the squash (value35c) */
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
  union {
    uint32_t word530;        /* 0x530 */
    f32 effect_clock;        /* 0x530: S17 r1: f434 is added every update; an effect is emitted (and it is cleared)
                              * once it passes 5 or 10 (racer_emit_effects) */
  };
  Vec3 vec534;               /* 0x534 */
  Vec3 vec540;               /* 0x540 */
  uint8_t pad_54c[0x554 - 0x54C];
  int32_t word554;           /* 0x554: S16 w3: 0 at reset */
  union {
    int32_t word558;         /* 0x558: S16 w3: 0 at reset */
    int32_t backwards_prev;  /* 0x558: S16 w4: backwards of the previous update */
  };
  union {
    uint32_t word55c;        /* 0x55C */
    int32_t backwards;       /* 0x55C: S16 w4: in action 1, it faced against its cell's direction for more than 120
                              * updates in a row (racer_base_update) */
  };
  union {
    int32_t word560;         /* 0x560: S16 w3: 0 at reset */
    int32_t backwards_count; /* 0x560: S16 w4: how often backwards was newly set */
  };
  union {
    int32_t word564;         /* 0x564: S16 w3: 0 at reset */
    int32_t backwards_frames; /* 0x564: S16 w4: updates in a row it has faced backwards (0 outside action 1) */
  };
  union {
    int32_t variant_on;      /* 0x568 */
    int32_t vib_ready;       /* 0x568: S16 w3: its vibration unit is ready (pdVibMxIsReady at reset; 0 for none) */
  };
  int32_t vib_port;          /* 0x56C: S16 w3: the vibration unit's port (2, 8, 14, 20 for players 0-3), 0xFF: none */
  union {
    uint32_t variant_word;   /* 0x570: from the table at 0x8C0806FC by variant */
    int32_t vib_frames;      /* 0x570: S16 w4: updates the variant's vibration still runs (racer_base_update) */
  };
  int32_t variant;           /* 0x574: 1 to 5 (racer_set_variant) */
  f32 camera_f578;           /* 0x578 */
  f32 camera_f57c;           /* 0x57C */
  f32 camera_f580;           /* 0x580 */
  f32 camera_distance;       /* 0x584: -25 at reset */
  uint32_t word588;          /* 0x588 */
  uint8_t second_base_at[0x5A8 - 0x58C]; /* 0x58C */
} Racer;
HW_OFFSET(Racer, motion, 0x050);
HW_OFFSET(Racer, looks, 0x084);
HW_OFFSET(Racer, character, 0x0A4);
HW_OFFSET(Racer, racer_contact, 0x0B4);
HW_OFFSET(Racer, object_contact, 0x0B8);
HW_OFFSET(Racer, word174, 0x174);
HW_OFFSET(Racer, vec0bc, 0x0BC);
HW_OFFSET(Racer, mark_a, 0x148);
HW_OFFSET(Racer, mark_c, 0x150);
HW_OFFSET(Racer, mark_d, 0x154);
HW_OFFSET(Racer, marks_prev, 0x15C);
HW_OFFSET(Racer, word180, 0x180);
HW_OFFSET(Racer, speed, 0x0F0);
HW_OFFSET(Racer, object_push, 0x0C8);
HW_OFFSET(Racer, racer_velocity, 0x0D4);
HW_OFFSET(Racer, object_velocity, 0x0E0);
HW_OFFSET(Racer, racer_speed, 0x0EC);
HW_OFFSET(Racer, object_grip, 0x0F4);
HW_OFFSET(Racer, on_kind4_prev, 0x138);
HW_OFFSET(Racer, on_kind4_new, 0x140);
HW_OFFSET(Racer, button_prev, 0x100);
HW_OFFSET(Racer, button, 0x108);
HW_OFFSET(Racer, stick_x, 0x110);
HW_OFFSET(Racer, stick_angle, 0x11C);
HW_OFFSET(Racer, edges, 0x120);
HW_OFFSET(Racer, button_lock, 0x130);
HW_OFFSET(Racer, effects_shown, 0x1A0);
HW_OFFSET(Racer, stick_damped, 0x184);
HW_OFFSET(Racer, mark_a, 0x148);
HW_OFFSET(Racer, word144, 0x144);
HW_OFFSET(Racer, had_contacts, 0x18C);
HW_OFFSET(Racer, scene, 0x194);
HW_OFFSET(Racer, state, 0x19C);
HW_OFFSET(Racer, word1a8, 0x1A8);
HW_OFFSET(Racer, word1ac, 0x1AC);
HW_OFFSET(Racer, times298, 0x298);
HW_OFFSET(Racer, contacts, 0x1B8);
HW_OFFSET(Racer, collider_a, 0x214);
HW_OFFSET(Racer, vec240, 0x240);
HW_OFFSET(Racer, mark_a_on, 0x258);
HW_OFFSET(Racer, mark_b_value, 0x26C);
HW_OFFSET(Racer, hit_strength, 0x278);
HW_OFFSET(Racer, action_arg, 0x280);
HW_OFFSET(Racer, action_step, 0x284);
HW_OFFSET(Racer, word288, 0x288);
HW_OFFSET(Racer, word294, 0x294);
HW_OFFSET(Racer, stroke_level, 0x288);
HW_OFFSET(Racer, word254, 0x254);
HW_OFFSET(Racer, word170, 0x170);
HW_OFFSET(Racer, word178, 0x178);
HW_OFFSET(Racer, step_timer, 0x274);
HW_OFFSET(Racer, release_frames, 0x294);
HW_OFFSET(Racer, ground, 0x23C);
HW_OFFSET(Racer, ground_touch, 0x250);
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
HW_OFFSET(Racer, word0fc, 0x0FC);
HW_OFFSET(Racer, f188, 0x188);
HW_OFFSET(Racer, word190, 0x190);
HW_OFFSET(Racer, word198, 0x198);
HW_OFFSET(Racer, word1a4, 0x1A4);
HW_OFFSET(Racer, contacts_word, 0x20C);
HW_OFFSET(Racer, word210, 0x210);
HW_OFFSET(Racer, word554, 0x554);
HW_OFFSET(Racer, word558, 0x558);
HW_OFFSET(Racer, word560, 0x560);
HW_OFFSET(Racer, state_prev, 0x198);
HW_OFFSET(Racer, progress_prev, 0x1A4);
HW_OFFSET(Racer, backwards_prev, 0x558);
HW_OFFSET(Racer, backwards, 0x55C);
HW_OFFSET(Racer, backwards_count, 0x560);
HW_OFFSET(Racer, backwards_frames, 0x564);
HW_OFFSET(Racer, vib_frames, 0x570);
HW_OFFSET(Racer, vib_ready, 0x568);
HW_OFFSET(Racer, vib_port, 0x56C);
HW_OFFSET(Racer, variant_word, 0x570);
HW_OFFSET(Racer, variant, 0x574);
HW_OFFSET(Racer, camera_distance, 0x584);
HW_OFFSET(Racer, word588, 0x588);
HW_OFFSET(Racer, second_base_at, 0x58C);
HW_SIZE(Racer, 0x5A8);
#define RACER_VTABLE 0x8C23DACCu

/* S15 b04: the racer's virtual table (the Racer's and the LaneRacer's): the Task's methods, then the reset that
 * puts it at a start position (position, a word (0 at the race's set-up), the heading in fr4) */
typedef struct RacerVtbl {
  TaskVtbl task;  /* 0x00 */
  VMethod reset;  /* 0x18 */
} RacerVtbl;
HW_OFFSET(RacerVtbl, reset, 0x18);

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
  union {
    gaddr lane_motion[4][2]; /* 0x5CC */
    gaddr lane_line[4][2];   /* 0x5CC: S17 r1: an f32 per row: where across the row the racing line runs (-0.5 = the
                              * row's first vertex, 0.5 = its second; lane_racer_steer) */
  };
  union {
    gaddr lane_texlist[4][2];  /* 0x5EC */
    gaddr lane_markers[4][2];  /* 0x5EC: S16 b05: per quad of the lane a LaneMarker list (-1-terminated) or 0
                                * (lane_racer_look_ahead) */
  };
  int32_t lane_last[4];      /* 0x60C: the course's row count of the lane, - 1: its quads */
  gaddr lane_points;         /* 0x61C: lanes[lane][branch], the table in use */
  int32_t branch;            /* 0x620: 0 = the main way, 1 = the side way */
  int32_t branch_checked;    /* 0x624: the quad the branch test last ran at */
  int32_t branch_length;     /* 0x628: the side way's quads */
  int32_t lane;              /* 0x62C: the lane of the ground (the cell kinds 1-5 set it) */
  int32_t prev_lane;         /* 0x630 */
  int32_t quad;              /* 0x634: the quad of the lane the racer is on */
  int32_t prev_quad;         /* 0x638 */
  union {
    int32_t word63c;         /* 0x63C: 0 when the lane changes */
    int32_t manoeuvre;       /* 0x63C: S17 r3: state 3's manoeuvre (lane_racer_drive): 0 racing, 1 working loose
                              * (stuck), 2 holding back (character 7) */
  };
  union {
    int32_t word640;         /* 0x640 */
    int32_t holding_back;    /* 0x640: S17 r1: character 7 waits for player 0 (lane_racer_steer) */
    int32_t manoeuvre_step;  /* 0x640: S17 r3: the manoeuvre's step (racing, character 7: 0 watching, 1 turned
                              * round, 2 a dash soon, 3 a dash once the stick is near straight) */
  };
  union {
    int32_t hold_timer;      /* 0x644: S17 r1: updates character 7 has raced since it last held back or dashed */
    int32_t manoeuvre_frames; /* 0x644: S17 r3: updates in the manoeuvre's step (lane_racer_drive) */
  };
  uint8_t pad_648[0x64C - 0x648];
  int32_t range_index;       /* 0x64C: the constructor's last argument: a row of the range tables */
  f32 range1_min;            /* 0x650: from the first range table */
  f32 range1_max;            /* 0x654: min + the table's width (+ 10 for the characters 1, 4, 6) */
  f32 range2_min;            /* 0x658: the second table's */
  f32 range2_max;            /* 0x65C */
  union {
    f32 f660;                /* 0x660 */
    f32 glide_clock;         /* 0x660: S17 r1: updates the button has stayed released (lane_racer_steer) */
  };
  union {
    int32_t word664;         /* 0x664 */
    int32_t off_ground_frames; /* 0x664: S17 r1: updates in a row off special ground (0 on it) */
  };
  union {
    int32_t word668;         /* 0x668 */
    int32_t stuck_frames;    /* 0x668: S17 r3: updates since reset_position was last noted (every 130, while
                              * mark_b_on; lane_racer_drive) */
  };
  Vec3 reset_position;       /* 0x66C: where the reset put it (S17 r3: then where state 3 last noted it, every
                              * 130 updates while mark_b_on: lane_racer_drive) */
  union {
    f32 f678;                /* 0x678 */
    f32 stroke_clock;        /* 0x678: S17 r1: counted up each update; the button is held from range1_min on, and
                              * the stroke ends past range1_max (lane_racer_steer) */
  };
  union {
    int32_t word67c;         /* 0x67C */
    int32_t stroke_frames;   /* 0x67C: S17 r3: state 3: updates left of the strokes (button 0) a marker or the
                              * ground asked for (lane_racer_drive) */
  };
  int32_t word680;           /* 0x680 */
  int32_t word684;           /* 0x684 */
  union {
    int32_t word688;         /* 0x688: -1 at reset */
    int32_t pending_kind;    /* 0x688: S16 b05: the kind of the marker the racer is acting on (3, 4, 11, 12, 18),
                              * -1: none (a kind 5 marker on the racer's quad clears it) */
  };
  union {
    int32_t word68c;         /* 0x68C */
    gaddr pending_marker;    /* 0x68C: S16 b05: that LaneMarker */
  };
  union {
    int32_t word690;         /* 0x690: -1 at reset */
    int32_t pending_level;   /* 0x690: S16 b05: its priority (1 or 2), -1 with pending_kind */
  };
  union {
    f32 f694;                /* 0x694: 0.5 at reset */
    f32 target_x;            /* 0x694: S16 b05: the side of the lane to steer to (a kind 4 marker sets it) */
  };
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
HW_OFFSET(LaneRacer, lane_markers, 0x5EC);
HW_OFFSET(LaneRacer, pending_kind, 0x688);
HW_OFFSET(LaneRacer, pending_marker, 0x68C);
HW_OFFSET(LaneRacer, pending_level, 0x690);
HW_OFFSET(LaneRacer, second_base_at, 0x6A4);
HW_OFFSET(Racer, effect_clock, 0x530);
HW_OFFSET(LaneRacer, lane_line, 0x5CC);
HW_OFFSET(LaneRacer, holding_back, 0x640);
HW_OFFSET(LaneRacer, hold_timer, 0x644);
HW_OFFSET(LaneRacer, glide_clock, 0x660);
HW_OFFSET(LaneRacer, off_ground_frames, 0x664);
HW_OFFSET(LaneRacer, stroke_clock, 0x678);
HW_OFFSET(LaneRacer, manoeuvre, 0x63C);
HW_OFFSET(LaneRacer, manoeuvre_step, 0x640);
HW_OFFSET(LaneRacer, manoeuvre_frames, 0x644);
HW_OFFSET(LaneRacer, stuck_frames, 0x668);
HW_OFFSET(LaneRacer, stroke_frames, 0x67C);
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
/* S16 w3, game/racer_reset.c */
void racer_reset(gaddr racer, gaddr position, f32 heading, uint32_t word);
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
void lane_racer_drive(gaddr self); /* S17 r3: game/lane_racer_drive.c */
int32_t lane_racer_find_quad(gaddr self, gaddr point, int32_t near);
f32 racer_x_fraction(gaddr racer);

/* game/racer.c (S15 b05 declares it) */
void racer_set_variant(gaddr racer, int32_t n);
/* game/racer_actions.c (S15 b05) */
void racer_dash_start(gaddr racer);
void racer_dash(gaddr racer);
void racer_hit_start_medium(gaddr racer);
void racer_hit_start_strong(gaddr racer);
void racer_hit_contacts(gaddr racer);
void racer_hit(gaddr racer);
void racer_dash_check_hits(gaddr racer);
void racer_update_state2(gaddr racer);
void racer_update_state3(gaddr racer);
void racer_state4_start(gaddr racer);
void racer_state4_action4_start(gaddr racer);
void racer_state4_action4(gaddr racer);
void racer_update_state4(gaddr racer);

/* game/racer_control.c */
gaddr racer_init_base(gaddr racer, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, f32 f);
gaddr racer_construct(gaddr racer, gaddr scene, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, f32 f);
void racer_read_controls(gaddr racer);
void racer_draw_thunk(gaddr racer);

/* S15 b04: game/racer.c */
gaddr frame_body_init(gaddr obj);
/* S16 b04: game/racer.c declares them for racer_run.c */
void racer_contacts_reset(gaddr racer);
void racer_handle_contacts(gaddr racer);
/* game/save.c: entry i of the save's byte table at 0x44 */
int32_t save_byte44(int32_t i);
/* S15 b06: game/motion_draw.c, game/racer_squash.c, game/racer_camera_update.c */
void racer_draw(gaddr racer);
void racer_squash_update(gaddr racer);
void racer_camera_start(gaddr racer);
void racer_camera_update(gaddr racer);
/* S16 w4: hand-written functions the racer's update calls (exported in src/ but not declared before) */
void racer_sort_contacts(gaddr racer);                 /* game/racer_contacts.c */
void frame_body_step(gaddr obj);                       /* game/racer.c */
void racer_check_scene_stop(gaddr racer);              /* game/racer_body.c */
void racer_ground(gaddr racer);                        /* game/racer_ground.c */
f32 race_scene_f74(gaddr scene);                       /* game/racer.c */
int32_t race_scene_cell_kind(gaddr scene, int32_t i);  /* game/racer.c */
void racer_check_marks(gaddr racer);                   /* game/racer.c */
/* S16 w4: game/racer_update.c */
void racer_base_update(gaddr racer);
/* S15 b04: game/racer_base.c (was func_8C030BF4) */
#define CHARACTER_PARAMS 0x8C085648u /* CharacterParams[], by character */
gaddr racer_base_init(gaddr racer, uint32_t index, gaddr unused_b, int32_t character, f32 unused_f, int32_t costume);

/* S17 r1: game/racer_effects_emit.c, game/racer_sounds.c, game/lane_racer_steer.c */
void racer_emit_effects(gaddr self);
void racer_play_sounds(gaddr self);
void lane_racer_steer(gaddr self);

#endif
