/* Pen Pen port, S17 b08: more scenery actor classes (placement types 0x30, 0x31 and 0x32, built by the factory
 * func_8C043D4C, game/actor_factory.c), and declarations of the functions this batch exports. To be merged into
 * game/actor.h. */
#ifndef PENPEN_ACTOR_B08_H
#define PENPEN_ACTOR_B08_H

#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "actor.h"

/* the object a contact's unknown4c word names, as type 0x2C's contact handler (func_8C05C424) reads it: only its
 * Vec3 at 0xC0 is known */
typedef struct ContactOther {
  uint8_t pad_00[0xC0];
  Vec3 velocity;            /* 0xC0: copied into the body's velocity by the handler */
} ContactOther;
HW_OFFSET(ContactOther, velocity, 0xC0);

/* placement type 0x30 (constructor func_8C05D190, 0x3AC bytes; vtable 0x8C23E7BC, second 0x8C23E7DC): a Body, six
 * Quats, five matrices and a sixth one, two contact sets (radius 1.9999999 at the point at 0x74, 0.99999994 at the
 * point at 0x80) and up to four puffs (each in six 4-word blocks and a Vec3 block: the puffs' state per field) */
#define TYPE30_PUFF_BLOCK 0x10      /* bytes per puff block (4 words) */
#define TYPE30_MATRICES 5
typedef struct ActorType30 {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x074 - 0x028];
  Vec3 point_a;             /* 0x074: the first contacts' owner */
  Vec3 point_b;             /* 0x080: the second contacts' owner */
  Quat quats_a[3];          /* 0x08C: (0, 0, 0, 1) at construction */
  Quat quats_b[3];          /* 0x0BC: (0, 0, 0, 1) at construction */
  Body body;                /* 0x0EC: at rest at construction; where a new puff starts */
  uint8_t pad_11c[0x130 - 0x11C];
  Matrix matrices[TYPE30_MATRICES]; /* 0x130: the identity at construction */
  Matrix matrix;            /* 0x270: the identity at construction */
  Contacts contacts_a;      /* 0x2B0: owned by point_a, room for 2 */
  int32_t contacts_a_word;  /* 0x304 */
  Contacts contacts_b;      /* 0x308: owned by point_b, room for 2 */
  int32_t contacts_b_word;  /* 0x35C */
  uint32_t word360;         /* 0x360 */
  gaddr entry364;           /* 0x364: a 12-byte entry of a static table */
  uint32_t word368;         /* 0x368 */
  f32 f36c;                 /* 0x36C: 0 at reset */
  f32 f370;                 /* 0x370: 0 at reset */
  f32 f374;                 /* 0x374: 0 at reset */
  uint8_t pad_378[0x384 - 0x378];
  uint32_t word384;         /* 0x384: a global word at reset */
  gaddr table;              /* 0x388: a static table at reset */
  gaddr puff_kind;          /* 0x38C: int32_t[4]: 0 or 1 (which spawn made it) */
  gaddr puff_timer;         /* 0x390: int32_t[4]: g_type30_puff_time at spawn */
  gaddr puff_state;         /* 0x394: int32_t[4]: 0 free, 1-3 the phases (1 at spawn) */
  gaddr puff_fade;          /* 0x398: f32[4]: 0 at spawn */
  gaddr puff_scale;         /* 0x39C: f32[4]: 1.25 (kind 0) or g_type30_puff_scale1 (kind 1) at spawn */
  gaddr puff_frame;         /* 0x3A0: f32[4]: 0 at spawn */
  gaddr puff_pos;           /* 0x3A4: Vec3[4] (operator new[]) or 0: the body's position at spawn */
  gaddr second_vtable;      /* 0x3A8 */
} ActorType30;
HW_OFFSET(ActorType30, point_a, 0x074);
HW_OFFSET(ActorType30, point_b, 0x080);
HW_OFFSET(ActorType30, quats_a, 0x08C);
HW_OFFSET(ActorType30, quats_b, 0x0BC);
HW_OFFSET(ActorType30, body, 0x0EC);
HW_OFFSET(ActorType30, matrices, 0x130);
HW_OFFSET(ActorType30, matrix, 0x270);
HW_OFFSET(ActorType30, contacts_a, 0x2B0);
HW_OFFSET(ActorType30, contacts_b, 0x308);
HW_OFFSET(ActorType30, word360, 0x360);
HW_OFFSET(ActorType30, f36c, 0x36C);
HW_OFFSET(ActorType30, word384, 0x384);
HW_OFFSET(ActorType30, table, 0x388);
HW_OFFSET(ActorType30, puff_kind, 0x38C);
HW_OFFSET(ActorType30, puff_pos, 0x3A4);
HW_OFFSET(ActorType30, second_vtable, 0x3A8);
HW_SIZE(ActorType30, 0x3AC);

/* placement type 0x31 (constructor func_8C05EFB4, 0x1FC bytes; vtable 0x8C23E7FC, second 0x8C23E81C): three Body1s,
 * a Body, a matrix, two Quats, contacts with no owner (radius 0, room for 4) and seven blocks (two raw, five arrays of
 * 26 from operator new[]: three of Body1s, Vec3s, Bodies) */
#define TYPE31_ITEMS 26
typedef struct ActorType31 {
  Actor base;               /* 0x000 */
  uint8_t pad_028[0x038 - 0x028];
  int32_t state;            /* 0x038: 11, 12, ... set by the steps that start a phase */
  int32_t timer;            /* 0x03C: the phase's length (480 for 11, 60 for 12) */
  int32_t player;           /* 0x040: the player it follows (race_scene_nearest_player) */
  uint8_t pad_044[0x048 - 0x044];
  f32 distance;             /* 0x048: to that player (phase 11 starts nearer than 40) */
  uint8_t pad_04c[0x068 - 0x04C];
  Vec3 v68;                 /* 0x068: copied to v164 when phase 12 starts */
  Body1 b74;                /* 0x074: (0, 0, 0, 1) at construction */
  Body1 b84;                /* 0x084 */
  Body1 b94;                /* 0x094 */
  Body body;                /* 0x0A4: at rest at construction */
  gaddr object;             /* 0x0D4: an NJS_OBJECT: its child is the model drawn */
  gaddr texlist;            /* 0x0D8: set before the draw */
  gaddr block_dc;           /* 0x0DC: 0x1AC bytes (syMalloc); with block_e0 the pair nj_ObjectL1 is given */
  gaddr block_e0;           /* 0x0E0: 0xD2 bytes (syMalloc) */
  uint8_t pad_0e4[0x0F4 - 0x0E4];
  gaddr turns_a;            /* 0x0F4: Body1[26] or 0 ((0, 0, 0, 1) by the Quat constructor; damping 0.6 at reset) */
  gaddr turns_b;            /* 0x0F8: Body1[26] or 0 */
  gaddr turns_c;            /* 0x0FC: Body1[26] or 0 */
  gaddr vecs;               /* 0x100: Vec3[26] or 0 */
  gaddr bodies;             /* 0x104: Body[26] or 0 */
  Matrix matrix;            /* 0x108: the identity at construction */
  uint8_t pad_148[0x150 - 0x148];
  f32 f150;                 /* 0x150: 15 in phase 11, 20 in phase 12 */
  f32 angle_a;              /* 0x154: + rate_a every step (Ninja units) */
  f32 angle_b;              /* 0x158: + rate_b every step */
  f32 rate_a;               /* 0x15C */
  f32 rate_b;               /* 0x160: its sign is kept when a phase sets its size */
  Vec3 v164;                /* 0x164 */
  Vec3 v170;                /* 0x170 */
  Quat q17c;                /* 0x17C: (0, 0, 0, 1) at construction */
  Quat q18c;                /* 0x18C: (0, 0, 0, 1) at construction */
  Contacts contacts;        /* 0x19C */
  int32_t contacts_word;    /* 0x1F0 */
  gaddr scene;              /* 0x1F4: the RaceScene */
  gaddr second_vtable;      /* 0x1F8 */
} ActorType31;
HW_OFFSET(ActorType31, state, 0x038);
HW_OFFSET(ActorType31, player, 0x040);
HW_OFFSET(ActorType31, distance, 0x048);
HW_OFFSET(ActorType31, v68, 0x068);
HW_OFFSET(ActorType31, f150, 0x150);
HW_OFFSET(ActorType31, rate_b, 0x160);
HW_OFFSET(ActorType31, v164, 0x164);
HW_OFFSET(ActorType31, scene, 0x1F4);
HW_OFFSET(ActorType31, b74, 0x074);
HW_OFFSET(ActorType31, body, 0x0A4);
HW_OFFSET(ActorType31, object, 0x0D4);
HW_OFFSET(ActorType31, texlist, 0x0D8);
HW_OFFSET(ActorType31, block_dc, 0x0DC);
HW_OFFSET(ActorType31, turns_a, 0x0F4);
HW_OFFSET(ActorType31, bodies, 0x104);
HW_OFFSET(ActorType31, matrix, 0x108);
HW_OFFSET(ActorType31, v170, 0x170);
HW_OFFSET(ActorType31, q17c, 0x17C);
HW_OFFSET(ActorType31, contacts, 0x19C);
HW_OFFSET(ActorType31, second_vtable, 0x1F8);
HW_SIZE(ActorType31, 0x1FC);

/* placement type 0x32 (constructor func_8C0603B6, 0x1D0 bytes; vtable 0x8C23E83C, second 0x8C23E85C): six Body1s, a
 * Body owning contacts (radius g_type32_radius, room for 2), five 0x80-byte blocks and 32 Bodies (operator new[]) */
#define TYPE32_BODIES 32
typedef struct ActorType32 {
  Actor base;               /* 0x000 */
  int32_t word28;           /* 0x028: S17 b09: 0 at the init */
  int32_t motion;           /* 0x02C: S17 b09: the motion entry whose length sets `frames` (0 at the init) */
  int32_t kind;             /* 0x030: S17 b09: the placement's param bits 0-3 */
  uint8_t pad_034[0x038 - 0x034];
  int32_t state;            /* 0x038: S17 b09: 0 at the init (the update switches on 0, 1, 2) */
  int32_t wait;             /* 0x03C: S17 b09: random 120..539 at the init */
  int32_t word40;           /* 0x040: S17 b09: 0 at the init */
  int32_t word44;           /* 0x044: S17 b09: 0 at the init */
  uint8_t pad_048[0x04C - 0x048];
  int32_t alternate;        /* 0x04C: S17 b09: not 0: drawn with the second model and motion pair */
  uint8_t pad_050[0x054 - 0x050];
  f32 frame;                /* 0x054: S17 b09: the motions' frame (handed by address) */
  uint8_t pad_058[0x05C - 0x058];
  f32 frames;               /* 0x05C: S17 b09: the motion's frame count - 1 */
  f32 scale;                /* 0x060: S17 b09: the model's scale */
  Vec3 v64;                 /* 0x064: S17 b09: 0 at the init */
  uint8_t pad_070[0x074 - 0x070];
  f32 radius;               /* 0x074: S17 b09: g_type32_radius at the init, the contacts' radius */
  uint8_t pad_078[0x088 - 0x078];
  Vec3 home;                /* 0x088: S17 b09: the Body's position at the init */
  Body1 turns[6];           /* 0x094: (0, 0, 0, 1) at construction; S17 b09: [3] about x, [4] about y, [5] about z */
  Body body;                /* 0x0F4: at rest at construction; the contacts' owner */
  uint8_t pad_124[0x12C - 0x124];
  gaddr textures;           /* 0x12C: S17 b09: the init's copy of the texlist entry's data word (an NJS_TEXLIST) */
  gaddr motions;            /* 0x130: S17 b09: MotionEntry[] (the word1c entry's data word) */
  gaddr objects;            /* 0x134: S17 b09: gaddr[] of NJS_OBJECTs (the model_set entry's data word) */
  Contacts contacts;        /* 0x138 */
  int32_t contacts_word;    /* 0x18C */
  NjSprite sprite;          /* 0x190: S17 b09: the sprite every ripple is drawn with (textures, a static texanim) */
  uint8_t pad_1b0[0x1B4 - 0x1B0];
  /* S17 b09: 32 ripples (g_type32_ripples), one word or float each in these blocks */
  union {
    gaddr blocks_a[3];      /* 0x1B4: 0x80 bytes each (syMalloc) */
    struct {
      gaddr cells;          /* 0x1B4: int32_t: the sprite's cell (0: the spreading ring; 1-3: the splash, cycled) */
      gaddr timers;         /* 0x1B8: int32_t: counts down in stage 1 */
      gaddr stages;         /* 0x1BC: int32_t: 0 free, 1 waiting, 2 growing, 3 done (back to 0) */
    };
  };
  union {
    gaddr blocks_b[2];      /* 0x1C0: 0x80 bytes each (syMalloc, after the Bodies) */
    struct {
      gaddr sizes;          /* 0x1C0: f32: grows in stage 2 */
      gaddr alphas;         /* 0x1C4: f32: 1 at the spawn */
    };
  };
  gaddr bodies;             /* 0x1C8: Body[32] or 0 */
  gaddr second_vtable;      /* 0x1CC */
} ActorType32;
HW_OFFSET(ActorType32, turns, 0x094);
HW_OFFSET(ActorType32, body, 0x0F4);
HW_OFFSET(ActorType32, contacts, 0x138);
HW_OFFSET(ActorType32, contacts_word, 0x18C);
HW_OFFSET(ActorType32, textures, 0x12C);
HW_OFFSET(ActorType32, alternate, 0x04C);
HW_OFFSET(ActorType32, kind, 0x030);
HW_OFFSET(ActorType32, state, 0x038);
HW_OFFSET(ActorType32, word44, 0x044);
HW_OFFSET(ActorType32, frames, 0x05C);
HW_OFFSET(ActorType32, v64, 0x064);
HW_OFFSET(ActorType32, radius, 0x074);
HW_OFFSET(ActorType32, home, 0x088);
HW_OFFSET(ActorType32, frame, 0x054);
HW_OFFSET(ActorType32, scale, 0x060);
HW_OFFSET(ActorType32, motions, 0x130);
HW_OFFSET(ActorType32, objects, 0x134);
HW_OFFSET(ActorType32, sprite, 0x190);
HW_OFFSET(ActorType32, stages, 0x1BC);
HW_OFFSET(ActorType32, alphas, 0x1C4);
HW_OFFSET(ActorType32, blocks_a, 0x1B4);
HW_OFFSET(ActorType32, blocks_b, 0x1C0);
HW_OFFSET(ActorType32, bodies, 0x1C8);
HW_OFFSET(ActorType32, second_vtable, 0x1CC);
HW_SIZE(ActorType32, 0x1D0);

/* game/scenery_b08.c */
gaddr ornament_delete(gaddr obj, int32_t free_memory);
gaddr ornament_destroy_self(gaddr obj);
void rings_destroy(gaddr obj);
gaddr rings_delete(gaddr obj, int32_t free_memory);
gaddr rings_destroy_self(gaddr obj);
void contact_take_other_velocity(gaddr contacts, int32_t i, gaddr body);
void actor_type2d_destroy(gaddr obj);
gaddr actor_type2d_delete(gaddr obj, int32_t free_memory);
gaddr actor_type2d_destroy_self(gaddr obj);
void actor_type2d_draw(gaddr obj);
void object_draw_model_l1(gaddr self, gaddr object);
void material_set_cd7e(f32 r, f32 g, f32 b, f32 a);
void material_set_ff24(f32 r, f32 g, f32 b, f32 a);
void ornament_draw_model(gaddr obj);
void swinger_draw(gaddr obj);
void contact_push_out_soft(gaddr contacts, int32_t i, gaddr body);
void contact_push_out_hard(gaddr contacts, int32_t i, gaddr body);
/* game/actor_types_b08.c */
gaddr actor_type30_base_construct(gaddr obj);
gaddr actor_type30_construct(gaddr obj);
void actor_type30_free_puffs(gaddr obj);
void actor_type30_destroy(gaddr obj);
gaddr actor_type30_delete(gaddr obj, int32_t free_memory);
gaddr actor_type30_destroy_self(gaddr obj);
void actor_type30_reset(gaddr obj);
void actor_type30_spawn_puff(gaddr obj);
void actor_type30_spawn_puff_b(gaddr obj);
gaddr actor_type31_base_construct(gaddr obj);
gaddr actor_type31_construct(gaddr obj);
void actor_type31_destroy(gaddr obj);
gaddr actor_type31_delete(gaddr obj, int32_t free_memory);
gaddr actor_type31_destroy_self(gaddr obj);
void actor_type31_draw(gaddr obj);
void actor_type31_reset_items(gaddr obj);
void actor_type31_start_near(gaddr obj);
void actor_type31_start_release(gaddr obj);
gaddr actor_type32_base_construct(gaddr obj);
/* S17 b09: game/actor_type32.c */
gaddr actor_type32_construct(gaddr obj);
void actor_type32_free_ripples(gaddr obj);
void actor_type32_destroy(gaddr obj);
gaddr actor_type32_delete(gaddr obj, int32_t free_memory);
gaddr actor_type32_destroy_self(gaddr obj);
void actor_type32_reset_ripples(gaddr obj);
void actor_type32_spawn_ripple(gaddr obj, int32_t wait);
void actor_type32_contact_push(gaddr contacts, int32_t i, gaddr body);
void actor_type32_update_ripples(gaddr obj);
void actor_type32_draw_ripples(gaddr obj);
void actor_type32_spawn_splash(gaddr obj);
void actor_type32_draw(gaddr obj);
void actor_type32_init(gaddr obj, gaddr placement);

#endif
