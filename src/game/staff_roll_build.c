/* Pen Pen port, S18 b01: mode 9's scene (StaffRoll, game/staff_roll.h): its body (the constructor's work after the
 * vtable: files, camera, light, the first phase and line, the eight penguins) and its penguins' constructor. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/task.h"
#include "../core/inline.h"
#include "../core/contact.h"
#include "game.h"
#include "save.h"
#include "racer.h"
#include "effects.h"
#include "motion.h"
#include "staff_roll.h"

#define STAFF_ROLL_TEXLIST    0x8C08CDACu /* the scene sprite's texture list (staff_roll_draw) */
#define STAFF_ROLL_MODEL_FILE 0x8C072620u /* "7010end.dat": texlist, model, motion */
#define STAFF_ROLL_TEXTURES   0x8C07262Cu /* "7011end.pvm" */
#define STAFF_ROLL_PENGUINS   8
#define STAFF_ROLL_FIRST_FADE 120         /* frames of the first phase's fade in */

#define ROLL_PENGUIN_SIZE     0x1C0u
#define ROLL_PENGUIN_VTABLE   0x8C23D9B4u /* its Task part's (at 0xE0) */
#define ROLL_PENGUIN_VTABLE2  0x8C23D9CCu /* at 0x1A0 */
#define ROLL_PENGUIN_PART     0x1A4u      /* the first word points at its part there */
#define ROLL_PENGUIN_CONTACTS 4
#define RACER_EFFECTS_SIZE    0x3Cu

/* the camera: where it stands and what it looks at */
#define ROLL_CAMERA_X F32_BITS(0xC264D547u) /* -57.21 */
#define ROLL_CAMERA_Y F32_BITS(0x42594C78u) /* 54.32 */
#define ROLL_CAMERA_Z F32_BITS(0x4318C26Eu) /* 152.76 */
#define ROLL_TARGET_X F32_BITS(0xC2621827u) /* -56.52 */
#define ROLL_TARGET_Y F32_BITS(0x4258E69Cu) /* 54.23 */
#define ROLL_TARGET_Z F32_BITS(0x43180998u) /* 152.04 */
#define ROLL_CAMERA_FOV F32_BITS(0x46354F87u) /* 11603.88 (Ninja angle units) */

/* func_8C0120A0 (core/texture_list.c, not declared in a header): load a texture list's file */
static void texlist_load(gaddr texlist, gaddr dir_name) { CALL2(0x8C0120A0u, texlist, dir_name); }
/* func_8C0120CA (core/texture.c, not declared in a header): load a .pvm into a texture list */
static void texture_load(gaddr name, gaddr texlist, gaddr dir_name) { CALL3(0x8C0120CAu, name, texlist, dir_name); }
/* func_8C011F50 (core/view_setup.c, not declared in a header): a view's camera (position, target, roll, field of
 * view; applied at once or not) */
static void view_set_camera(gaddr view, gaddr position, gaddr target, f32 roll, f32 fov, int32_t apply) {
  FARGS2(roll, fov);
  CALL4(0x8C011F50u, view, position, target, apply);
}
/* func_8C010F04 (core/gfx_init.c, not declared in a header): the background colour and the fog table's range */
static void background_set(uint32_t argb, f32 fog_near, f32 fog_far) {
  FARGS2(fog_near, fog_far);
  CALL1(0x8C010F04u, argb);
}

/* was func_8C01E364: a penguin of the scene: its first word points at its part at 0x1A4, its two vtables; the
 * MotionPlayer's construction (character, variant, container, table); the word at 0xDC, its Task part (no parent),
 * its Body and heading; its contacts (owner the Body, the character's radius, 4 of them); then its effects object
 * (0x3C bytes, or 0 when the allocation fails) hung under its Task part */
gaddr roll_penguin_construct(gaddr penguin, int32_t character, int32_t variant, gaddr container, int32_t table) {
  RollPenguin *p = (RollPenguin *)hw_ptr(penguin);
  p->player.unknown00 = penguin + ROLL_PENGUIN_PART;
  p->member_e0.vtable = ROLL_PENGUIN_VTABLE;
  p->vtable_1a0 = ROLL_PENGUIN_VTABLE2;
  motion_player_build(penguin, character, variant, container, table);
  word_construct(member_addr(penguin, RollPenguin, word_dc), 0);
  gaddr part = member_addr(penguin, RollPenguin, member_e0);
  task_construct(part, 0);
  body_init(member_addr(penguin, RollPenguin, body));
  quat_identity(member_addr(penguin, RollPenguin, turn));
  const CharacterParams *params = (const CharacterParams *)hw_ptr(CHARACTER_PARAMS) + character;
  gaddr owner = method_self(member_addr(penguin, RollPenguin, body));
  contacts_construct(member_addr(penguin, RollPenguin, contacts), owner, params->radius, ROLL_PENGUIN_CONTACTS);
  p->extra = 0;
  gaddr fx = syMalloc(RACER_EFFECTS_SIZE);
  fx = fx != 0 ? racer_effects_construct(fx, 0) : 0;
  p->extra = fx;
  task_add_children(task_at(part), task_at(fx));
  return penguin;
}
HW_EXPORT(8c01e364, RET(roll_penguin_construct(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3, (int32_t)ARG4)))

/* was func_8C01E50C: the scene's body (its constructor func_8C01E8DA sets the vtable first): the Task header (no
 * parent yet), the V-sync background and soft-reset flags off while it loads; the scene sprite's texture list, the
 * model file (its entries: texture list, model, motion) and its textures; one view; the camera; a black background;
 * the first view's fog off and the current view's light colour black; phase 0 fading in over 120 frames, line 0
 * fading in over 180 frames for its time from the language's table; the eight penguins (one per character) hung
 * under it and reset; then the V-sync background on again. The camera's two vectors are locals of the original's
 * frame (kept on the guest stack). */
gaddr staff_roll_build(gaddr scene) {
  StaffRoll *s = staff_roll_at(scene);
  s->task.linger = 0;
  s->task.active = 1;
  s->task.stops_on_pause = 1;
  s->task.child = 0;
  s->task.next = 0;
  s->task.parent = 0;
  s->task.owner = 0;
  g_video_running = 0;
  g_soft_reset = 0;

  /* the files */
  texlist_load(STAFF_ROLL_TEXLIST, 0);
  s->file = container_load(STAFF_ROLL_MODEL_FILE, 0);
  const ContainerEntry *entries = (const ContainerEntry *)hw_ptr(s->file);
  s->texlist = (gaddr)entries[0].offset;
  s->object = (gaddr)entries[1].offset;
  s->motion = (gaddr)entries[2].offset;
  texture_load(STAFF_ROLL_TEXTURES, s->texlist, 0);

  /* the view and its camera */
  view_set_split(SPLIT_FULL);
  gaddr frame = hw_stack_alloc(0x1C);
  gaddr target = frame + 0x04, position = frame + 0x10;
  vec3_set(ROLL_TARGET_X, ROLL_TARGET_Y, ROLL_TARGET_Z, target);
  vec3_set(ROLL_CAMERA_X, ROLL_CAMERA_Y, ROLL_CAMERA_Z, position);
  gaddr view = hw_addr(&g_views[0]);
  view_set_camera(view, position, target, F32(0.0f), ROLL_CAMERA_FOV, 0);
  hw_stack_free(0x1C);
  background_set(0xFF000000u, F32(0.0f), F32(-1.0f));
  view_at(view)->fog = 0; /* func_8C01E1CE (an inline copy) */
  view_at(g_cur_view)->light_color.x = F32(0.0f);
  view_at(g_cur_view)->light_color.y = F32(0.0f);
  view_at(g_cur_view)->light_color.z = F32(0.0f);

  /* the first phase and line */
  s->started = 0;
  s->phase_fade_frames = F32(120.0f);
  s->countdown = STAFF_ROLL_FIRST_FADE;
  s->phase = 0;
  s->prev_phase = -1;
  s->phase_fade = F32(0.0f);
  s->line = 0;
  s->prev_line = -1;
  int32_t language = game_language();
  const StaffRollLine *line = (const StaffRollLine *)hw_ptr(STAFF_ROLL_LINES +
      (uint32_t)language * STAFF_ROLL_LANGUAGE_SIZE + (uint32_t)s->line * (uint32_t)sizeof(StaffRollLine));
  s->line_timer = f32_from_int(line->seconds);
  s->line_fade = F32(0.0f);
  s->line_fade_frames = F32(180.0f);
  s->frames = F32(0.0f);

  /* the penguins */
  for (int32_t i = 0; i < STAFF_ROLL_PENGUINS; i++) {
    gaddr p = syMalloc(ROLL_PENGUIN_SIZE);
    p = p != 0 ? roll_penguin_construct(p, i, 0, 0, 0) : 0;
    s->penguins[i] = p;
    task_add_children(task_at(scene), task_at(p != 0 ? member_addr(p, RollPenguin, member_e0) : 0));
  }
  for (int32_t i = 0; i < STAFF_ROLL_PENGUINS; i++)
    roll_penguin_reset(s->penguins[i], 1);
  g_video_running = 1;
  return scene;
}
HW_EXPORT(8c01e50c, RET(staff_roll_build(ARG0)))
