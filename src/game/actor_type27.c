/* Pen Pen port, S18 b05: the scenery actor of placement type 0x27 (game/actor_b07.h: ActorType27): a Flyer whose
 * model comes apart. Until something touches its contacts it is the Flyer (stepped and drawn as one); then its 21
 * parts (one per node of its model, in the order of a depth-first walk) are thrown up and out with a spin about x,
 * fall for 180 steps, and the actor is gone. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "actor.h"
#include "actor_b07.h"
#include "race_scene.h" /* race_scene_view_cell, race_scene_slot_insert */
#include "game.h"       /* panel_get */
#include "scenery.h"    /* race_scene_sound */

/* func_8C0D02FE (nj_ObjectL1 in port/symbols/sdk_calls.tsv): draw a node's model with the current matrix */
static void nj_ObjectL1(gaddr model) { CALL1(0x8C0D02FEu, model); }

#define TYPE27_CONTACT_MASS  F32(0.5f)
#define TYPE27_CONTACT_WORD  4                     /* the Contacts' word after it (field_reset_2c) */
#define TYPE27_PART_DAMPING  F32_BITS(0x3F7D70A3u) /* 0.99 */
#define TYPE27_TURN_DAMPING  F32_BITS(0x3F7AE147u) /* 0.98 */
#define TYPE27_THROW_Z0      F32_BITS(0xBE4CCCCCu) /* -0.2: part i's z speed -0.2 + 0.06 i */
#define TYPE27_THROW_DZ      F32_BITS(0x3D75C28Eu) /* 0.06 */
#define TYPE27_THROW_UP      F32_BITS(0x3FC8F5C1u) /* 1.57: every part's y speed */
#define TYPE27_THROW_DX      F32_BITS(0x3DCCCCCCu) /* 0.1: part i's x speed -0.1 i */
#define TYPE27_SPIN0         F32_BITS(0x44B60B60u) /* 1456.355 (8 degrees): part i's spin 8 + 0.4 i degrees */
#define TYPE27_SPIN_STEP     F32_BITS(0x4291A2B3u) /* 72.81777 (0.4 degrees) */
#define TYPE27_GRAVITY       F32_BITS(0xBD5F0122u) /* -0.05444444: the parts' force each step */
#define TYPE27_FALL_STEPS    180
#define TYPE27_SOUND_RANGE   F32(1.0f)             /* players within one course cell hear it */
#define TYPE27_SOUND_BANK    8
#define TYPE27_SOUND_ID      0x24
#define NJ_EVAL_UNIT_POS     1u                    /* NJD_EVAL_UNIT_POS of a node's evalflags */

static gaddr type27_part(gaddr obj, int32_t i) { return member_addr(obj, ActorType27, parts[i]); }
static gaddr type27_turn(gaddr obj, int32_t i) { return member_addr(obj, ActorType27, turns[i]); }

/* was func_8C05A148 (the init method: this, the placement): the Flyer's resting init; its first node's
 * NJD_EVAL_UNIT_POS cleared; the contacts empty, in its cell, mass 0.5; phase and timer 0; every part at rest at
 * the origin (damping 0.99), every spin at rest at 0 (damping 0.98) */
void actor_type27_init(gaddr obj, gaddr placement) {
  ActorType27 *a = (ActorType27 *)hw_ptr(obj);
  resting_actor_init(obj, placement);
  NjObjectPose *root = (NjObjectPose *)hw_ptr(actor_model(&a->flyer.base, 0));
  root->evalflags &= ~NJ_EVAL_UNIT_POS;
  Contacts *c = &a->contacts;
  c->count = 0; /* field_reset_2c's copy (func_8C059B02): the count and the word after the Contacts */
  a->contacts_word = TYPE27_CONTACT_WORD;
  c->cell = a->flyer.base.cell;
  c->mass = TYPE27_CONTACT_MASS;
  c->unknown18 = F32(1.0f);
  a->phase = 0;
  a->timer = 0;
  for (int32_t i = 0; i < TYPE27_PARTS; i++) {
    gaddr part = type27_part(obj, i);
    body_reset(part);
    vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), part);
    vec3_fill(member_addr(part, Body, damping), TYPE27_PART_DAMPING);
    vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(part, Body, velocity)); /* func_8C059AB2 */
    gaddr turn = type27_turn(obj, i);
    quat_identity(turn);
    Body1 *t = body1_at(turn);
    t->value = F32(0.0f);
    t->damping = TYPE27_TURN_DAMPING;
    t->velocity = F32(0.0f);
  }
}
HW_EXPORT(8c05a148, actor_type27_init(ARG0, ARG1))

/* phase 0, touched: every part thrown (x speed -0.1 i, y 1.57, z -0.2 + 0.06 i) and spun (8 + 0.4 i degrees); every
 * player within a course cell of the actor hears it */
static void type27_break(gaddr obj) {
  ActorType27 *a = (ActorType27 *)hw_ptr(obj);
  for (int32_t i = 0; i < TYPE27_PARTS; i++) {
    f32 fi = f32_from_int(i);
    f32 z = fmac(TYPE27_THROW_DZ, fi, TYPE27_THROW_Z0);
    f32 x = fneg(fmul(f32_from_int(i), TYPE27_THROW_DX));
    vec3_add_xyz(x, TYPE27_THROW_UP, z, member_addr(type27_part(obj, i), Body, velocity)); /* func_8C059ACE */
    f32_add_to(fmac(TYPE27_SPIN_STEP, f32_from_int(i), TYPE27_SPIN0),
               member_addr(type27_turn(obj, i), Body1, velocity)); /* func_8C059A2A */
  }
  for (int32_t j = 0; j < (int32_t)a->flyer.base.word0c; j++) {
    int32_t cell = a->flyer.base.cell;
    int32_t near = race_scene_view_cell(panel_get(), j);
    if (fgt(f32_abs(f32_from_int(cell - near)), TYPE27_SOUND_RANGE)) continue;
    race_scene_sound(panel_get(), j, 0, TYPE27_SOUND_BANK, TYPE27_SOUND_ID);
  }
}

/* was func_8C05A27C: the update method. Phase 0: the Flyer's step; touched (a contact in its list) it breaks
 * (phase 1), else its contacts go into the race scene's slot 8. Phase 1: every part falls and steps, every spin
 * steps; after 180 steps phase 2 (nothing more). */
void actor_type27_update(gaddr obj) {
  ActorType27 *a = (ActorType27 *)hw_ptr(obj);
  switch (a->phase) {
  case 0:
    flyer_update(obj);
    if (a->contacts.count != 0) {
      a->phase += 1;
      type27_break(obj);
    } else {
      gaddr contacts = member_addr(obj, ActorType27, contacts);
      race_scene_slot_insert(panel_get(), contacts, 8);
    }
    break;
  case 1:
    for (int32_t i = 0; i < TYPE27_PARTS; i++) {
      f32_add_to(TYPE27_GRAVITY, member_addr(type27_part(obj, i), Body, force.y)); /* func_8C059AE8 */
      body_step(type27_part(obj, i));
      body1_step(type27_turn(obj, i));
    }
    a->timer += 1;
    if (a->timer > TYPE27_FALL_STEPS) a->phase += 1;
    break;
  default: /* 2: gone */
    break;
  }
}
HW_EXPORT(8c05a27c, actor_type27_update(ARG0))

/* the original's frame of actor_type27_draw_part (0x90 bytes under PR): its matrices go to Ninja */
typedef struct PartFrame {
  Matrix saved;      /* 0x00: matrix_b as it was, for the siblings */
  Matrix local;      /* 0x40: the node's own transform */
  f32 angle;         /* 0x80: the part's spin */
  gaddr part;        /* 0x84 */
  gaddr node;        /* 0x88 */
  gaddr self;        /* 0x8C */
  uint32_t saved_pr; /* 0x90 */
} PartFrame;
HW_OFFSET(PartFrame, local, 0x40);
HW_OFFSET(PartFrame, angle, 0x80);
HW_OFFSET(PartFrame, self, 0x8C);
HW_SIZE(PartFrame, 0x94);

/* was func_8C05A4CC (with its tail at 0x8C05A614, after the copy back): draw a node of the model and, recursively,
 * its children and siblings. The node takes the next part: drawn at matrix_a, moved to the part's Body, turned
 * about x by its spin, then by the tree matrix and the node's own transform (translate, rotate z, y, x); the tree
 * matrix gets the node's transform for the children, and is put back for the siblings. */
void actor_type27_draw_part(gaddr obj, gaddr node) {
  ActorType27 *a = (ActorType27 *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(PartFrame));
  PartFrame *f = (PartFrame *)hw_ptr(frame);
  gaddr saved = member_addr(frame, PartFrame, saved);
  gaddr local = member_addr(frame, PartFrame, local);
  gaddr matrix_b = member_addr(obj, ActorType27, matrix_b);
  f->self = obj;
  f->node = node;
  matrix_push();
  matrix_load(member_addr(obj, ActorType27, matrix_a));
  f->part = type27_part(obj, a->part_next);
  f->angle = body1_at(type27_turn(obj, a->part_next))->value; /* func_8C059A00 */
  a->part_next += 1;
  matrix_translate_v(method_self(f->part));
  matrix_rotate_x(f->angle);
  matrix_multiply(matrix_b);
  matrix_identity(local); /* func_8C0598F2: the matrix's constructor */
  njUnitMatrix(local);
  const NjObjectPose *n = (const NjObjectPose *)hw_ptr(node);
  njTranslate(local, n->pos.x, n->pos.y, n->pos.z);
  /* the unit's rotators take the angle as a float (func_8C05996E, ...) */
  njRotateZ(local, f32_to_int(f32_from_int(n->ang[2])));
  njRotateY(local, f32_to_int(f32_from_int(n->ang[1])));
  njRotateX(local, f32_to_int(f32_from_int(n->ang[0])));
  matrix_multiply(local);
  nj_ObjectL1(n->model);
  matrix_assign(saved, matrix_b);
  matrix_multiply_on(matrix_b, local);
  if (n->child != 0) actor_type27_draw_part(obj, n->child);
  memmove(hw_ptr(matrix_b), hw_ptr(saved), sizeof(Matrix)); /* the SHC copy helper 0x8C0625E0 */
  n = (const NjObjectPose *)hw_ptr(f->node);
  if (n->sibling != 0) actor_type27_draw_part(f->self, n->sibling);
  matrix_pop();
  hw_stack_free(sizeof(PartFrame));
}
HW_EXPORT(8c05a4cc, actor_type27_draw_part(ARG0, ARG1))

/* was func_8C05A498: draw the parts: the Flyer's pose set, the walk from part 0, matrix_a the current matrix, the
 * tree matrix the unit; then the model's root node (a tail call) */
void actor_type27_draw_parts(gaddr obj) {
  ActorType27 *a = (ActorType27 *)hw_ptr(obj);
  flyer_pose(obj);
  gaddr root = actor_model(&a->flyer.base, 0);
  a->part_next = 0;
  njGetMatrix(member_addr(obj, ActorType27, matrix_a));
  njUnitMatrix(member_addr(obj, ActorType27, matrix_b));
  actor_type27_draw_part(obj, root);
}
HW_EXPORT(8c05a498, actor_type27_draw_parts(ARG0))

/* was func_8C05A428: the draw method: phase 0 the Flyer's draw; phase 1 the parts at the Flyer's Body turned z, y,
 * x by its angles; phase 2 nothing */
void actor_type27_draw(gaddr obj) {
  ActorType27 *a = (ActorType27 *)hw_ptr(obj);
  switch (a->phase) {
  case 0:
    flyer_draw(obj);
    break;
  case 1:
    matrix_push();
    matrix_translate_v(method_self(member_addr(obj, Flyer, body)));
    matrix_rotate_z(a->flyer.angle.z);
    matrix_rotate_y(a->flyer.angle.y);
    matrix_rotate_x(a->flyer.angle.x);
    actor_type27_draw_parts(obj);
    matrix_pop();
    break;
  default:
    break;
  }
}
HW_EXPORT(8c05a428, actor_type27_draw(ARG0))
