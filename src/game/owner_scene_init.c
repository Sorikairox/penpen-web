/* Pen Pen port, S18 (owner): the OwnerScene's construction (game/owner_scene.h), called by its constructor
 * owner_scene_construct (func_8C02FC12). Resumable: it shows a loading screen (two frames) before it loads the owner
 * file, builds the penguins, loads the result screen's textures and places the camera. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/inline.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/ninja.h"
#include "game.h"
#include "sound.h"
#include "motion.h"
#include "race_scene.h"
#include "owner_scene.h"

void loading_screen_show(int32_t picture);                     /* game/loading.c */
void texture_load(gaddr name, gaddr texlist, gaddr dir_name);  /* core/texture.c (no header) */
void view_set_camera(gaddr view, gaddr position, gaddr target, f32 roll, f32 fov, int32_t apply); /* core/view_setup.c */

#define OWNER_LOADING_PICTURE 2
#define OWNER_FILE_NAME     0x8C080694u /* "5110Owner.bin" */
#define OWNER_PVM_NAME      0x8C0806A4u /* "5111Owner.pvm" */
#define RESULT_PVM_NAME     0x8C0806B4u /* "6010result.pvm" */
#define RESULT_TEXLIST      0x8C0966E4u /* the result screen's NJS_TEXLIST (guest data) */
#define RESULT_TEXANIM      0x8C0966ECu /* its full-screen NJS_TEXANIM (640 x 480) */
#define OWNER_PENGUIN_SIZE  0xF8
#define OWNER_PENGUIN_COUNT 3        /* penguin_count: 3 penguins, 2 for variant 4 */
#define OWNER_VARIANT_TWO   4
#define OWNER_SOUND_SET     0
#define OWNER_SPLIT         0           /* view_set_split: one full-screen view */
#define OWNER_START_FOV     F32_BITS(0x462AAAAAu) /* 10922.67: 60 degrees in angle units */

/* a penguin's start: motion set 0x22 of its table, no request, motion 0x18 from frame 0 at speed 1 */
#define PENGUIN_MOTION_SET  0x22
#define PENGUIN_MOTION      0x18

/* the camera of each course (guest data, 0x40 bytes apart) */
typedef struct OwnerCamera {
  Vec3 eye;        /* 0x00 */
  Vec3 target;     /* 0x0C */
  int32_t roll;    /* 0x18: angle units */
  int32_t fov;     /* 0x1C: angle units */
  uint8_t unknown20[0x40 - 0x20];
} OwnerCamera;
HW_OFFSET(OwnerCamera, roll, 0x18);
HW_SIZE(OwnerCamera, 0x40);
#define g_owner_cameras ((const OwnerCamera *)hw_ptr(0x8C0965B4u))

/* the original's frame (0x30 bytes of locals, then PR, fr14, fr15, r8-r14): the cameras' vectors go to
 * view_set_camera by address */
typedef struct OwnerInitFrame {
  Vec3 target;       /* 0x00: the course's camera target */
  Vec3 eye;          /* 0x0C: the course's camera eye */
  Vec3 start_target; /* 0x18: (0, 0, -1) */
  Vec3 start_eye;    /* 0x24: the origin */
  uint32_t saved[10]; /* 0x30 */
} OwnerInitFrame;
HW_OFFSET(OwnerInitFrame, start_target, 0x18);
HW_OFFSET(OwnerInitFrame, start_eye, 0x24);
HW_SIZE(OwnerInitFrame, 0x58);

/* the motion set a penguin starts with, by its character */
static int32_t penguin_start_set(int32_t character) {
  switch (character) {
  case 0: return 0x1D;
  case 2: return 0x1E;
  case 5: return 0x20;
  case 7: return 0x21;
  default: return 0x1F;
  }
}

/* penguin i: its slot, character and model file from the race set-up, a MotionPlayer (0 where the allocation
 * failed: the original then writes through 0), its start motion, its looks */
static void owner_scene_build_penguin(gaddr scene, int32_t i) {
  OwnerScene *s = owner_scene_at(scene);
  s->penguin_slot[i] = race_setup_at(g_race_setup)->words44[i];
  s->penguin_character[i] = (int32_t)race_setup_at(g_race_setup)->words68[s->penguin_slot[i]];
  s->penguin_variant[i] = (int32_t)race_setup_at(g_race_setup)->words88[s->penguin_slot[i]];
  gaddr block = syMalloc(OWNER_PENGUIN_SIZE);
  s->penguins[i] = block != 0 ? owner_penguin_construct(block, s->penguin_character[i], s->penguin_variant[i], 0) : 0;
  gaddr penguin = s->penguins[i];
  MotionPlayer *p = motion_player_at(penguin);
  p->motion_count = PENGUIN_MOTION_SET;
  p->request = 0;
  p->index = PENGUIN_MOTION;
  p->frame = F32(0.0f);
  p->speed = F32(1.0f);
  motion_player_play_set(penguin, penguin_start_set(p->character), F32(1.0f));
  menu_penguin_set_looks(penguin); /* (inline in the original) */
}

/* the camera: first at the origin looking down -z (60 degrees), then the course's own; neither applied yet */
static void owner_scene_place_camera(gaddr scene, gaddr frame) {
  OwnerInitFrame *f = (OwnerInitFrame *)hw_ptr(frame);
  gaddr view = hw_addr(&g_views[0]);
  f->start_target.z = F32(-1.0f);
  f->start_target.y = F32(0.0f);
  f->start_target.x = F32(0.0f);
  f->start_eye.z = F32(0.0f);
  f->start_eye.y = F32(0.0f);
  f->start_eye.x = F32(0.0f);
  view_set_camera(view, member_addr(frame, OwnerInitFrame, start_eye), member_addr(frame, OwnerInitFrame, start_target),
                  F32(0.0f), OWNER_START_FOV, 0);
  const OwnerCamera *cam = &g_owner_cameras[owner_scene_at(scene)->course];
  f->target.z = cam->target.z;
  f->target.y = cam->target.y;
  f->target.x = cam->target.x;
  f->eye.z = cam->eye.z;
  f->eye.y = cam->eye.y;
  f->eye.x = cam->eye.x;
  view_set_camera(view, member_addr(frame, OwnerInitFrame, eye), member_addr(frame, OwnerInitFrame, target),
                  f32_from_int(cam->roll), f32_from_int(cam->fov), 0);
}

/* was func_8C02F850: the scene's construction (the RaceSetup in r5): the Task (no parent), its pad on port 0, the
 * set-up, the owner's Body at rest (no damping); the loading screen (picture 2); the video and soft-reset flags
 * cleared; the set-up's course and variant; 5110Owner.bin and the course's owner (object and motion pairs) and
 * 5111Owner.pvm into its texture list; the owner's motion reset; three penguins (two for variant 4); the result
 * screen's textures, the sound set 0 and one full-screen view; the camera; the full-screen sprite; state 0, and
 * the video flag set. Returns the scene. */
gaddr owner_scene_init(gaddr scene, gaddr setup) {
  gaddr frame = 0;
  HW_RESUMABLE(0x8C02F850u, scene, frame);
  HW_RESUME_AT(0);
  /* the frame first, as the original's prologue (a rewind comes back with r15 still lowered and `frame` saved) */
  frame = hw_stack_alloc(sizeof(OwnerInitFrame));
  task_init(task_at(scene), 0);
  word_construct(member_addr(scene, OwnerScene, pad), 0);
  owner_scene_at(scene)->setup = setup;
  gaddr body = member_addr(scene, OwnerScene, body);
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, position));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, velocity));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(body, Body, force));
  vec3_set(F32(1.0f), F32(1.0f), F32(1.0f), member_addr(body, Body, damping));
  HW_YIELDING_RET(0, loading_screen_show(OWNER_LOADING_PICTURE), 0);

  OwnerScene *s = owner_scene_at(scene);
  g_video_running = 0;
  g_soft_reset = 0;
  s->course = race_setup_at(s->setup)->course;
  s->variant = race_setup_at(s->setup)->variant;

  /* the owner file: its texture list, then per course an object and its motion pairs */
  s->file = container_load(OWNER_FILE_NAME, 0);
  const ContainerEntry *entries = (const ContainerEntry *)hw_ptr(s->file);
  s->texlist = (gaddr)entries[0].offset;
  s->object = (gaddr)entries[1 + 2 * s->course].offset;
  s->pairs = (gaddr)entries[2 + 2 * s->course].offset;
  texture_load(OWNER_PVM_NAME, s->texlist, 0);
  owner_scene_reset_owner(scene);

  s->penguin_count = OWNER_PENGUIN_COUNT;
  if (s->variant == OWNER_VARIANT_TWO) s->penguin_count = 2;
  /* the count is read again every pass (the arrays hold 3) */
  for (int32_t i = 0; i < s->penguin_count; i++)
    owner_scene_build_penguin(scene, i);

  s->word108 = race_setup_at(g_race_setup)->word38;
  s->texlist64 = RESULT_TEXLIST;
  texture_load(RESULT_PVM_NAME, RESULT_TEXLIST, 0);
  sound_select_set(OWNER_SOUND_SET);
  view_set_split(OWNER_SPLIT);
  owner_scene_place_camera(scene, frame);

  s->sprite.sx = F32(1.0f);
  s->sprite.sy = F32(1.0f);
  s->sprite.tlist = s->texlist64;
  s->sprite.tanim = RESULT_TEXANIM;
  s->state = 0;
  g_video_running = 1;
  hw_stack_free(sizeof(OwnerInitFrame));
  return scene;
}
HW_EXPORT_RESUMABLE(8c02f850, RET(owner_scene_init(ARG0, ARG1)))
