/* Pen Pen port, S18 b02: mode 9's scene (StaffRoll, game/staff_roll.h): its constructor and draw, and its penguin's
 * (RollPenguin) reset, update and draw. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/ninja.h"
#include "../core/view.h"
#include "../core/inline.h"
#include "../core/inline_b02.h"
#include "../core/motion_frame.h"
#include "save.h"
#include "motion.h"
#include "effects.h"
#include "staff_roll.h"

#define STAFF_ROLL_VTABLE    0x8C23D9E4u
#define STAFF_ROLL_TLIST     0x8C08CDACu /* the scene sprite's texture list and animation */
#define STAFF_ROLL_TANIM     0x8C08CDB4u
#define STAFF_ROLL_ATTR      0x22u
#define CONTROL3D_FLAG_10    0x10u       /* the njControl3D flag the sprites are drawn with */
#define ROLL_CELL_BEHIND     13          /* drawn behind cells 11 and 12 */

/* per phase: the light's direction and colour it adds (by its fade) */
typedef struct RollPhaseLight {
  Vec3 dir;
  f32 r, g, b;
} RollPhaseLight;
HW_SIZE(RollPhaseLight, 24);
#define g_roll_phase_lights ((const RollPhaseLight *)hw_ptr(0x8C0714CCu))

/* where the scene's model (and the penguin) stands, its heading (-85 degrees in Ninja units) */
#define ROLL_PLACE_X F32_BITS(0x4289AEBEu) /* 68.84 */
#define ROLL_PLACE_Y F32_BITS(0xC05E5C91u) /* -3.474 */
#define ROLL_PLACE_Z F32_BITS(0x4266374Bu) /* 57.55 */
#define ROLL_HEADING F32_BITS(0xC671C71Cu) /* -15473.8 */

/* func_8C010514 (core/matrix_axis.c, not declared in a header): turn matrix m by the rotation from `from` to `to` */
static void matrix_turn_toward(gaddr m, gaddr from, gaddr to) { CALL3(0x8C010514u, m, from, to); }
/* SDK njColorBlendingMode (0x8C0A23A8) */
static inline void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }
/* func_8C01E50C (still translated): the scene's body (it builds the scene; returns it) */
static gaddr staff_roll_init(gaddr scene) { return CALL1(0x8C01E50Cu, scene); }

/* was func_8C01E8DA: the constructor (the vtable, then the body) */
gaddr staff_roll_construct(gaddr scene) {
  staff_roll_at(scene)->task.vtable = STAFF_ROLL_VTABLE;
  staff_roll_init(scene);
  return scene;
}
HW_EXPORT(8c01e8da, RET(staff_roll_construct(ARG0)))

/* ---- the draw ---- */

/* line `index` of the game's language: each of its labels drawn at the line's fade (or, fading out, 1 - the fade:
 * computed again for each as the original does), then its cell (and cell 13 behind cells 11 and 12) in white at
 * that alpha */
static void staff_roll_line(gaddr scene, int32_t index, gaddr sprite, int fade_out) {
  StaffRoll *s = staff_roll_at(scene);
  int32_t language = game_language();
  const StaffRollLine *line = (const StaffRollLine *)hw_ptr(STAFF_ROLL_LINES +
      (uint32_t)language * STAFF_ROLL_LANGUAGE_SIZE + (uint32_t)index * (uint32_t)sizeof(StaffRollLine));
  gaddr label = line->labels;
  if (label != 0) {
    while (text_label_at(label)->string != 0) {
      text_label_at(label)->a = fade_out ? fadd(fneg(s->line_fade), F32(1.0f)) : s->line_fade;
      text_label_draw(label);
      label += sizeof(TextLabel);
    }
  }
  int32_t cell = line->cell;
  if (cell < 0)
    return;
  control3d_push();
  control3d_set(CONTROL3D_FLAG_10);
  material_set_e258(F32(1.0f), F32(1.0f), F32(1.0f), fade_out ? fadd(fneg(s->line_fade), F32(1.0f)) : s->line_fade);
  if (cell == 12 || cell == 11)
    njDrawSprite2D(sprite, ROLL_CELL_BEHIND, F32(-2.0f), STAFF_ROLL_ATTR);
  njDrawSprite2D(sprite, cell, F32(-1.0f), STAFF_ROLL_ATTR);
  control3d_pop();
}

/* a phase's two cells (phase, phase + 5) at fade `alpha` */
static void staff_roll_phase_cells(gaddr sprite, int32_t phase, f32 alpha) {
  material_set_e258(F32(1.0f), F32(1.0f), F32(1.0f), alpha);
  njDrawSprite2D(sprite, phase, F32_BITS(0xC61C4000u) /* -10000 */, STAFF_ROLL_ATTR);
  njDrawSprite2D(sprite, phase + 5, F32(-100.0f), STAFF_ROLL_ATTR);
}

/* the light's direction and colour summed from the phase's entry (by `weight`) */
static void staff_roll_light_add(Vec3 *dir, f32 *color, int32_t phase, f32 weight) {
  const RollPhaseLight *l = &g_roll_phase_lights[phase];
  dir->x = fmac(weight, l->dir.x, dir->x);
  dir->y = fmac(weight, l->dir.y, dir->y);
  dir->z = fmac(weight, l->dir.z, dir->z);
  color[0] = fmac(weight, l->r, color[0]);
  color[1] = fmac(weight, l->g, color[1]);
  color[2] = fmac(weight, l->b, color[2]);
}

/* was func_8C01EAC2: the scene's draw: the blending modes; the scene sprite (a local of the original's frame: built
 * there with its position, scale and lists; its depth and angle are what that stack held); the line fading in and
 * the one fading out; the phase's cells fading in and the previous phase's fading out; the light mixed from both
 * phases' entries by their fades; then the scene's model with its shape motion at its place. The frame (0x50
 * bytes, the sprite 0x28 above its bottom) is kept at the original's place. */
void staff_roll_draw(gaddr scene) {
  StaffRoll *s = staff_roll_at(scene);
  gaddr frame = hw_stack_alloc(0x50);
  gaddr sprite = frame + 0x28;
  NjSprite *sp = (NjSprite *)hw_ptr(sprite);
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  sp->p.x = F32(0.0f);
  sp->p.y = F32(0.0f);
  sp->sx = F32(1.0f);
  sp->sy = F32(1.0f);
  sp->tlist = STAFF_ROLL_TLIST;
  sp->tanim = STAFF_ROLL_TANIM;

  /* the lines */
  if (s->line >= 0)
    staff_roll_line(scene, s->line, sprite, 0);
  if (s->prev_line >= 0)
    staff_roll_line(scene, s->prev_line, sprite, 1);

  /* the phases' cells */
  control3d_push();
  control3d_set(CONTROL3D_FLAG_10);
  if (s->phase >= 0)
    staff_roll_phase_cells(sprite, s->phase, s->phase_fade);
  if (s->prev_phase >= 0)
    staff_roll_phase_cells(sprite, s->prev_phase, fadd(fneg(s->phase_fade), F32(1.0f)));
  control3d_pop();

  /* the light */
  Vec3 dir = {F32(0.0f), F32(0.0f), F32(0.0f)};
  f32 color[3] = {F32(0.0f), F32(0.0f), F32(0.0f)};
  if (s->phase >= 0)
    staff_roll_light_add(&dir, color, s->phase, s->phase_fade);
  if (s->prev_phase >= 0)
    staff_roll_light_add(&dir, color, s->prev_phase, fadd(fneg(s->phase_fade), F32(1.0f)));
  view_current_set_light_dir(dir.x, dir.y, dir.z);
  view_current_set_light_color(color[0], color[1], color[2]);

  /* the scene's model */
  matrix_push();
  matrix_translate(ROLL_PLACE_X, ROLL_PLACE_Y, ROLL_PLACE_Z);
  matrix_rotate_y(ROLL_HEADING);
  njSetTexture(s->texlist);
  const MotionEntry *m = (const MotionEntry *)hw_ptr(s->motion);
  shape_motion_draw_l3(s->object, m->motion, m->word4, member_addr(scene, StaffRoll, frames), 1);
  matrix_pop();
  hw_stack_free(0x50);
}
HW_EXPORT(8c01eac2, staff_roll_draw(ARG0))

/* ---- the penguin ---- */

/* the place of each character (and its heading in .w) */
typedef struct RollPlace {
  f32 x, y, z, heading;
} RollPlace;
HW_SIZE(RollPlace, 16);
#define g_roll_places ((const RollPlace *)hw_ptr(0x8C07155Cu))
#define ROLL_PENGUIN_X F32_BITS(0x4289AEBEu) /* 68.84 */
#define ROLL_PENGUIN_Z F32_BITS(0x4266374Bu) /* 57.55 */
#define ROLL_DAMPING   F32(0.75f)
enum { ROLL_CHARACTER_RIBBONS = 6 }; /* the character whose motion pushes particles */

/* was func_8C01EFDE: the penguin's start: no request, frame 0, speed 1; its motion set by character (0: 29, 2: 30,
 * 5: 32, 7: 33, else 31) at speed 1; its looks from the save; its body at the scene's place, still, damped by 0.75
 * on x and z; its heading 0, damped by 0.75; its effects' ribbon reset at an identity matrix (a local of the
 * original's frame, 0x50 bytes at its bottom). The second argument is stored and not read. */
void roll_penguin_reset(gaddr penguin, int32_t unused) {
  RollPenguin *p = (RollPenguin *)hw_ptr(penguin);
  gaddr frame = hw_stack_alloc(0x50);
  gaddr m = frame;
  (void)unused;
  p->player.request = 0;
  p->player.frame = F32(0.0f);
  p->player.speed = F32(1.0f);
  if (p->player.character == 0)
    motion_player_play_set(penguin, 0x1D, F32(1.0f));
  else if (p->player.character == 2)
    motion_player_play_set(penguin, 0x1E, F32(1.0f));
  else if (p->player.character == 5)
    motion_player_play_set(penguin, 0x20, F32(1.0f));
  else if (p->player.character == 7)
    motion_player_play_set(penguin, 0x21, F32(1.0f));
  else
    motion_player_play_set(penguin, 0x1F, F32(1.0f));
  motion_player_looks_from_save(penguin);
  gaddr body = member_addr(penguin, RollPenguin, body);
  vec3_set(ROLL_PENGUIN_X, F32(0.0f), ROLL_PENGUIN_Z, member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(ROLL_DAMPING, F32(0.0f), ROLL_DAMPING, member_addr(body, Body, damping));
  p->turn.value = F32(0.0f);
  p->turn.velocity = F32(0.0f);
  p->turn.force = F32(0.0f);
  p->turn.damping = ROLL_DAMPING;
  matrix_identity(m);
  njTranslate(m, F32(0.0f), F32(0.0f), F32(0.0f));
  njRotateY(m, f32_to_int(F32(0.0f)));
  racer_effects_reset_ribbon(p->extra, F32(0.0f), m);
  hw_stack_free(0x50);
}
HW_EXPORT(8c01efde, roll_penguin_reset(ARG0, (int32_t)ARG1))

/* the original's frame of roll_penguin_update (0xEC bytes): a Vec3, the particles' RibbonFrame, a matrix */
typedef struct RollUpdateFrame {
  Vec3 drop;                /* 0x00: (0, -2.5, 0) */
  RibbonFrame ribbon;       /* 0x0C */
  Matrix m;                 /* 0xA4 */
  uint8_t spill[0xEC - 0xE4]; /* 0xE4: the saved `this` and padding */
} RollUpdateFrame;
HW_OFFSET(RollUpdateFrame, ribbon, 0x0C);
HW_OFFSET(RollUpdateFrame, m, 0xA4);
HW_SIZE(RollUpdateFrame, 0xEC);

/* was func_8C01F1F0: the penguin's update: its body and heading at its character's place; for character 6 a
 * particle frame is built (its matrix: the place turned by the heading; its position; no velocity; 2.5 below the
 * place; up; no height range; its heading; word84 1, word94 3) and from frame 20 on, every 15th whole frame, its
 * effects push a particle of set B with it; then the motion's update */
void roll_penguin_update(gaddr penguin) {
  RollPenguin *p = (RollPenguin *)hw_ptr(penguin);
  gaddr frame = hw_stack_alloc(sizeof(RollUpdateFrame));
  RollUpdateFrame *f = (RollUpdateFrame *)hw_ptr(frame);
  gaddr body = member_addr(penguin, RollPenguin, body);
  const RollPlace *place = &g_roll_places[p->player.character];
  vec3_set(place->x, place->y, place->z, body);
  p->turn.value = place->heading;
  if (p->player.character == ROLL_CHARACTER_RIBBONS) {
    gaddr m = member_addr(frame, RollUpdateFrame, m);
    gaddr rf = member_addr(frame, RollUpdateFrame, ribbon);
    matrix_identity(m);
    njTranslateV(m, method_self(body));
    njRotateY(m, f32_to_int(p->turn.value));
    matrix_identity(member_addr(rf, RibbonFrame, m));
    memmove(&f->ribbon.m, &f->m, sizeof(Matrix));
    memmove(&f->ribbon.position, hw_ptr(method_self(body)), sizeof(Vec3));
    vec3_zero(member_addr(rf, RibbonFrame, velocity));
    vec3_set(F32(0.0f), F32(-2.5f), F32(0.0f), member_addr(frame, RollUpdateFrame, drop));
    vec3_sum(member_addr(rf, RibbonFrame, vec58), method_self(body), member_addr(frame, RollUpdateFrame, drop));
    vec3_set(F32(0.0f), F32(1.0f), F32(0.0f), member_addr(rf, RibbonFrame, vec64));
    f->ribbon.low = F32(0.0f);
    f->ribbon.high = F32(0.0f);
    f->ribbon.f78 = F32(0.0f);
    f->ribbon.heading = p->turn.value;
    f->ribbon.word80 = 0;
    f->ribbon.word84 = 1;
    f->ribbon.word88 = 0;
    f->ribbon.word8c = 0;
    f->ribbon.index = 0;
    f->ribbon.word94 = 3;
    if (fgt(p->player.frame, F32(20.0f)) && f32_to_int(p->player.frame) % 15 == 0)
      racer_effects_push_set_b(p->extra, rf);
  }
  motion_player_advance(penguin);
  hw_stack_free(sizeof(RollUpdateFrame));
}
HW_EXPORT(8c01f1f0, roll_penguin_update(ARG0))

/* was func_8C01F3EA: the penguin's draw: a matrix turned from straight ahead (0, 0, -1) toward (0.686, 0, -0.722);
 * then at its body's place, turned by its heading, by that matrix, the motion player's draw (the vectors and the
 * matrix are locals of the original's frame: 0x60 bytes, the vectors at its bottom, the matrix 0x18 above) */
void roll_penguin_draw(gaddr penguin) {
  RollPenguin *p = (RollPenguin *)hw_ptr(penguin);
  gaddr frame = hw_stack_alloc(0x60);
  gaddr toward = frame;
  gaddr ahead = frame + sizeof(Vec3);
  gaddr m = frame + 2 * sizeof(Vec3);
  matrix_identity(m);
  vec3_set(F32_BITS(0x3F2F47F9u) /* 0.686 */, F32(0.0f), F32_BITS(0xBF38D5C7u) /* -0.722 */, toward);
  vec3_set(F32(0.0f), F32(0.0f), F32(-1.0f), ahead);
  matrix_turn_toward(m, ahead, toward);
  matrix_push();
  matrix_translate_v(method_self(member_addr(penguin, RollPenguin, body)));
  matrix_rotate_y(p->turn.value);
  matrix_multiply(m);
  motion_player_draw(penguin);
  matrix_pop();
  hw_stack_free(0x60);
}
HW_EXPORT(8c01f3ea, roll_penguin_draw(ARG0))
