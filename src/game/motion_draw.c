/* Pen Pen port, S15 (b06): drawing a penguin (game/motion.h): its body in its motion, the parts it wears, the
 * extra pieces of character 7, and the racer's whole draw (0x8C0373B8, 0x8C0381C4, 0x8C0385CC).
 *
 * The body is drawn in the motion playing, or blended from the previous one while MotionPlayer.blend is below 1.
 * While Ninja draws it, a node callback (func_8C037CD6) watches for the four nodes the parts hang on (the
 * player's texlist word points at them): at each one it saves the current matrix (g_attach_matrices) and sets the
 * player's attached[] flag. The parts (picked by the player's looks) are then drawn at those matrices, the first
 * two with a placement of their own per character and look. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "course.h"
#include "racer.h"
#include "scenery.h" /* motion_draw_l1, shape_motion_draw_l1 (core/motion.c) */
#include "motion.h"

/* ---- what the draw shares with its node callback ---- */
#define g_attach_nodes   GUEST_VAR(gaddr, 0x8C096BA4u) /* the four nodes the parts hang on (the player's texlist) */
#define g_attach_flags   GUEST_VAR(gaddr, 0x8C096BA8u) /* &player->attached[0] */
#define ATTACH_MATRICES  0x8C1BAB18u                  /* four Ninja matrices (0x40 bytes) the callback fills */
#define ATTACH_NODE_CALLBACK 0x8C037CD6u              /* func_8C037CD6: the callback (node -> matrix, flag) */
#define ATTACH_POINTS    4
#define NJ_MATRIX_SIZE   0x40
static gaddr attach_matrix(int32_t i) { return ATTACH_MATRICES + (gaddr)i * NJ_MATRIX_SIZE; }

/* the texture list the penguins are drawn with */
#define g_draw_texlist   GUEST_VAR(gaddr, 0x8C096B9Cu)

/* the parts: per attach point, a table by look of {model, motion, word} */
typedef struct PartModel {
  gaddr model;    /* 0x00: 0 = nothing worn there */
  gaddr motion;   /* 0x04: 0 = drawn still */
  uint32_t word8; /* 0x08 */
} PartModel;
HW_SIZE(PartModel, 12);
#define g_part_models    (&GUEST_VAR(gaddr, 0x8C1BAC1Cu)) /* four tables of PartModel, by look */

/* the placement of the first two parts, per character a table by look */
typedef struct PartPlacement {
  f32 angle[3];   /* 0x00: x, y, z (Ninja angle units, truncated to integers) */
  f32 position[3]; /* 0x0C */
  f32 scale[3];   /* 0x18 */
} PartPlacement;
HW_OFFSET(PartPlacement, position, 0x0C);
HW_OFFSET(PartPlacement, scale, 0x18);
HW_SIZE(PartPlacement, 0x24);
#define g_part0_placements (&GUEST_VAR(gaddr, 0x8C096B7Cu)) /* by character: PartPlacement[] by look */
#define g_part1_placements (&GUEST_VAR(gaddr, 0x8C096B5Cu))

/* character 7's extra pieces: an object with two models (entries 2 and 3), drawn at attach matrices 2 and 3, and its
 * motions (entries 0 and 2 of a second table) */
#define g_extra_parts    GUEST_VAR(gaddr, 0x8C096BB0u) /* gaddr[]: [0] the object, [2], [3] models */
#define g_extra_motions  GUEST_VAR(gaddr, 0x8C096BB4u) /* gaddr[]: [0] for motion 0x10, [2] for motion 0x14 */
enum {
  EXTRA_CHARACTER = 7,
  EXTRA_MOTION_A = 0x10, /* plays extra motion 0 */
  EXTRA_MOTION_B = 0x14, /* plays extra motion 2 */
  EXTRA_FIRST_MODEL = 2,
  EXTRA_END_MODEL = 4,
};
static gaddr extra_part(int32_t i) { return ((const gaddr *)hw_ptr(g_extra_parts))[i]; }
static gaddr extra_motion(int32_t i) { return ((const gaddr *)hw_ptr(g_extra_motions))[i]; }

/* Ninja's motion link (NJS_MOTION_LINK): two motions (or shape motions) and their frames, blended by a rate */
typedef struct MotionLink {
  gaddr motion[2]; /* 0x00: from, to */
  f32 frame[2];    /* 0x08 */
} MotionLink;
HW_SIZE(MotionLink, 0x10);

/* ---- Ninja (SDK) ---- */
/* nj_SetCallback: the function Ninja calls at each node it draws (0: none) */
static inline void nj_SetCallback(gaddr fn) { CALL1(0x8C0CB12Cu, fn); }
static inline void nj_MotionL1(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0CC84Eu, object, motion); }
static inline void nj_MotionL2(gaddr object, gaddr motion, f32 frame) { FARGS1(frame); CALL2(0x8C0CB7FCu, object, motion); }
/* the blend of two motions and two shape motions (links) at `rate` */
static inline void nj_ShapeMotionL1b(gaddr object, gaddr motion_link, gaddr shape_link, f32 rate) {
  FARGS1(rate);
  CALL3(0x8C0C94CAu, object, motion_link, shape_link);
}
static inline void nj_ShapeMotionL2b(gaddr object, gaddr motion_link, gaddr shape_link, f32 rate) {
  FARGS1(rate);
  CALL3(0x8C0C74AAu, object, motion_link, shape_link);
}
static inline void njCnkModDrawModel(gaddr model) { CALL1(0x8C0D0EFEu, model); }

/* ---- core/motion.c's drawers still translated ---- */
/* func_8C0113E8: motion_draw_l1 with nj_MotionL2 (keeps *frame in the motion, draws) */
static void motion_draw_l2(gaddr object, gaddr motion, gaddr frame, int32_t loop) {
  CALL4(0x8C0113E8u, object, motion, frame, loop);
}
/* func_8C011948: shape_motion_draw_l1 with the L2 shape-motion drawer */
static void shape_motion_draw_l2(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  CALL5(0x8C011948u, object, motion, shape, frame, loop);
}

/* ---- the racer's matrix helper (game/racer_body.c, not in a header) ---- */
/* func_8C0304F2: scale the current matrix along `dir` by sx, sy, sz */
static void matrix_scale_along(gaddr dir, f32 sx, f32 sy, f32 sz) {
  FARGS3(sx, sy, sz);
  CALL1(0x8C0304F2u, dir);
}

/* the racer's shadow (a modifier-volume model, static) */
#define RACER_SHADOW_MODEL 0x8C096AD8u

/* the draw's frame (0x24 bytes): the blend's two links, or later a part's placement copied out of its table */
typedef struct MotionDrawFrame {
  union {
    struct {
      uint32_t word0;    /* 0x00 */
      MotionLink shapes; /* 0x04: the shape motions */
      MotionLink motions; /* 0x14: the motions */
    } blend;
    PartPlacement placement; /* 0x00 */
  };
} MotionDrawFrame;
HW_OFFSET(MotionDrawFrame, blend.shapes, 0x04);
HW_OFFSET(MotionDrawFrame, blend.motions, 0x14);
HW_SIZE(MotionDrawFrame, 0x24);

static const MotionEntry *motion_entry(const MotionPlayer *p, int32_t index) {
  return &((const MotionEntry *)hw_ptr(p->motions))[index];
}

/* the body in its current motion (with its shape motion), by the player's L1 / L2 choice */
static void motion_player_draw_current(gaddr player) {
  MotionPlayer *p = motion_player_at(player);
  const MotionEntry *e = motion_entry(p, p->index);
  if (p->draw_l2 != 0)
    shape_motion_draw_l2(p->model, e->motion, e->word4, member_addr(player, MotionPlayer, frame), p->loop);
  else
    shape_motion_draw_l1(p->model, e->motion, e->word4, member_addr(player, MotionPlayer, frame), p->loop);
}

/* a part's placement: translate, rotate, scale the current matrix */
static void part_place(gaddr frame, gaddr table, int32_t look) {
  const PartPlacement *t = (const PartPlacement *)hw_ptr(frame);
  memmove(hw_ptr(frame), hw_ptr(table + (gaddr)look * sizeof(PartPlacement)), sizeof(PartPlacement));
  njTranslate(0, t->position[0], t->position[1], t->position[2]);
  njRotateXYZ(0, f32_to_int(t->angle[0]), f32_to_int(t->angle[1]), f32_to_int(t->angle[2]));
  njScale(0, t->scale[0], t->scale[1], t->scale[2]);
}

/* was func_8C0381C4: draw the penguin: the body (blending from the previous motion while blend < 1; once it has
 * blended in, the previous motion becomes the current one), then each part that its attach node was passed for */
void motion_player_draw(gaddr player) {
  MotionPlayer *p = motion_player_at(player);
  gaddr frame = hw_stack_alloc(sizeof(MotionDrawFrame));
  MotionDrawFrame *f = (MotionDrawFrame *)hw_ptr(frame);

  g_attach_nodes = p->texlist;
  for (int32_t i = ATTACH_POINTS - 1; i >= 0; i--)
    p->attached[i] = 0;
  g_attach_flags = member_addr(player, MotionPlayer, attached);
  nj_SetCallback(ATTACH_NODE_CALLBACK);
  njSetTexture(g_draw_texlist);

  if (p->index != p->prev_index) {
    f->blend.motions.motion[0] = motion_entry(p, p->prev_index)->motion;
    f->blend.motions.motion[1] = motion_entry(p, p->index)->motion;
    f->blend.shapes.motion[0] = motion_entry(p, p->prev_index)->word4;
    f->blend.shapes.motion[1] = motion_entry(p, p->index)->word4;
    f->blend.motions.frame[0] = p->prev_frame;
    f->blend.motions.frame[1] = p->frame;
    f->blend.shapes.frame[0] = p->prev_frame;
    f->blend.shapes.frame[1] = p->frame;
    if (fgt(F32(1.0f), p->blend)) {
      gaddr motions = member_addr(frame, MotionDrawFrame, blend.motions);
      gaddr shapes = member_addr(frame, MotionDrawFrame, blend.shapes);
      if (p->draw_l2 != 0)
        nj_ShapeMotionL2b(p->model, motions, shapes, p->blend);
      else
        nj_ShapeMotionL1b(p->model, motions, shapes, p->blend);
    } else {
      p->prev_index = p->index;
      motion_player_draw_current(player);
    }
  } else {
    motion_player_draw_current(player);
  }
  nj_SetCallback(0);

  njPushMatrix(0);
  for (int32_t i = 0; i < ATTACH_POINTS; i++) {
    if (p->attached[i] == 0)
      continue;
    njSetMatrix(0, attach_matrix(i));
    int32_t look = p->looks[i];
    if (i == 0)
      part_place(frame, g_part0_placements[p->character], look);
    else if (i == 1)
      part_place(frame, g_part1_placements[p->character], look);
    const PartModel *m = &((const PartModel *)hw_ptr(g_part_models[i]))[look];
    gaddr model = m->model;
    gaddr motion = m->motion;
    if (model == 0)
      continue;
    if (motion != 0) {
      gaddr timer = member_addr(player, MotionPlayer, timers[i]);
      if (p->draw_l2 != 0)
        motion_draw_l2(model, motion, timer, 1);
      else
        motion_draw_l1(model, motion, timer, 1);
    } else {
      nj_ModelL1(model);
    }
  }
  njPopMatrix(1);
  hw_stack_free(sizeof(MotionDrawFrame));
}
HW_EXPORT(8c0381c4, motion_player_draw(ARG0))

/* the frame of motion_player_draw_extra (0x20 bytes): the shape link (no shapes) and the motion link */
typedef struct ExtraDrawFrame {
  MotionLink shapes;  /* 0x00 */
  MotionLink motions; /* 0x10 */
} ExtraDrawFrame;
HW_OFFSET(ExtraDrawFrame, motions, 0x10);
HW_SIZE(ExtraDrawFrame, 0x20);

/* was func_8C0385CC: character 7's extra pieces, in motions 0x10 and 0x14 only: the object in the extra motion that
 * goes with the body's (blended like the body), then its two models at attach matrices 2 and 3 */
void motion_player_draw_extra(gaddr player) {
  MotionPlayer *p = motion_player_at(player);
  if (p->character != EXTRA_CHARACTER)
    return;
  int32_t which;
  if (p->index == EXTRA_MOTION_B)
    which = 2;
  else if (p->index == EXTRA_MOTION_A)
    which = 0;
  else
    return;
  gaddr object = extra_part(0);
  gaddr motion = extra_motion(which);
  if (p->index == p->prev_index) {
    if (p->draw_l2 != 0)
      nj_MotionL2(object, motion, p->frame);
    else
      nj_MotionL1(object, motion, p->frame);
  } else {
    gaddr frame = hw_stack_alloc(sizeof(ExtraDrawFrame));
    ExtraDrawFrame *f = (ExtraDrawFrame *)hw_ptr(frame);
    if (which == 0) {
      f->motions.motion[0] = extra_motion(2);
      f->motions.motion[1] = extra_motion(0);
    } else {
      f->motions.motion[0] = extra_motion(0);
      f->motions.motion[1] = extra_motion(2);
    }
    f->shapes.motion[0] = 0;
    f->shapes.motion[1] = 0;
    f->motions.frame[0] = p->prev_frame;
    f->motions.frame[1] = p->frame;
    f->shapes.frame[0] = p->prev_frame;
    f->shapes.frame[1] = p->frame;
    if (fgt(F32(1.0f), p->blend)) {
      gaddr motions = member_addr(frame, ExtraDrawFrame, motions);
      if (p->draw_l2 != 0)
        nj_ShapeMotionL2b(object, motions, frame, p->blend);
      else
        nj_ShapeMotionL1b(object, motions, frame, p->blend);
    } else if (p->draw_l2 != 0) {
      nj_MotionL2(object, motion, p->frame);
    } else {
      nj_MotionL1(object, motion, p->frame);
    }
    hw_stack_free(sizeof(ExtraDrawFrame));
  }
  njPushMatrix(0);
  for (int32_t i = EXTRA_FIRST_MODEL; i < EXTRA_END_MODEL; i++) {
    njSetMatrix(0, attach_matrix(i));
    gaddr model = extra_part(i);
    if (model != 0)
      nj_ModelL1(model);
  }
  njPopMatrix(1);
}
HW_EXPORT(8c0385cc, motion_player_draw_extra(ARG0))

/* the racer's scale (value35c[0..2]) along its vec39c, on the current matrix */
static void racer_scale_along(Racer *r, gaddr racer) {
  matrix_scale_along(member_addr(racer, Racer, vec39c), r->value35c[0].value, r->value35c[1].value,
                     r->value35c[2].value);
}

/* was func_8C0373B8: the racer's draw (a method the Racer and the LaneRacer share). Nothing when its cell is not in
 * the view; else the penguin (L2 drawers on cells with attribute bit 0x20) in its matrix and scale, then its shadow
 * at its position and heading */
void racer_draw(gaddr racer) {
  Racer *r = racer_at(racer);
  gaddr character = member_addr(racer, Racer, second_base);
  int32_t shown = race_scene_view_bit(r->scene, (int32_t)racer_cell(racer));
  r->effects_shown = shown;
  if (shown == 0)
    return;
  int32_t attr = race_scene_cell_attr(r->scene, (int32_t)racer_cell(racer));
  motion_player_at(character)->draw_l2 = (attr & 0x20) != 0 ? 1 : 0;
  matrix_push();
  matrix_multiply(member_addr(racer, Racer, matrix3a8));
  racer_scale_along(r, racer);
  motion_player_draw(character);
  motion_player_draw_extra(character);
  matrix_pop();

  matrix_push();
  matrix_translate_v(racer_body(racer));
  matrix_rotate_xyz(F32(0.0f), racer_heading(racer), F32(0.0f));
  racer_scale_along(r, racer);
  njCnkModDrawModel(RACER_SHADOW_MODEL);
  matrix_pop();
}
HW_EXPORT(8c0373b8, racer_draw(ARG0))
