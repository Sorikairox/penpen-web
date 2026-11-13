/* Pen Pen port, S18 b02: the CastScene (S17 b03 declared it in game/cast_scene.c; moved here and extended).
 *
 * The CastScene is a Task (constructor func_8C02004C, vtable 0x8C23DA14, body func_8C01FE58, which shows a loading
 * picture and so stays on the lead's resumable list) that shows the cast: g_cast_count MenuPenguins (MotionPlayers,
 * game/motion.h) of which one is shown at a time, drawn four times in a square; a TrapSet (game/trap_set.h) of the
 * course objects; and a RacerEffects child (game/effects.h) whose ribbons the update fires when the penguin's motion
 * passes some frames. The player picks the penguin, its motion set and its four looks, turns the view and moves the
 * camera with the pad (game/cast_scene_update.c); START cycles the trap mode (1 = the scene is frozen). */
#ifndef PENPEN_CAST_SCENE_H
#define PENPEN_CAST_SCENE_H
#include "hw.h"
#include "../core/task.h"
#include "../core/pad.h"
#include "../core/vec.h"

#define CAST_LOOKS 4

typedef struct CastScene {
  Task task;                /* 0x00 */
  Pad pad;                  /* 0x24: the controller (port 0 at construction) */
  int32_t controls;         /* 0x28: 1 at construction: the main control set; 0 = the alternate set */
  int32_t fixed_speed;      /* 0x2C: 1 at construction: the penguin plays at speed 1 (0 = at the R trigger's) */
  int32_t trap_mode;        /* 0x30: 0 at construction, 0..2 by START; handed to the TrapSet; 1 = frozen */
  int32_t penguin;          /* 0x34: the one shown (0..7) */
  int32_t page;             /* 0x38: 0 at construction: the penguin is penguins[g_cast_count * page + penguin] */
  int32_t word3c;           /* 0x3C: 0 at construction */
  int32_t motion;           /* 0x40: the motion set played (0..28, a 29th..33rd by penguin) */
  uint32_t word44;          /* 0x44 */
  int32_t looks[CAST_LOOKS]; /* 0x48: the penguin's four style numbers as the scene edits them */
  int32_t word58;           /* 0x58: 0 whenever a motion set is started */
  gaddr penguins[8];        /* 0x5C: g_cast_count of them used (0 = none) */
  gaddr trap_set;           /* 0x7C: TrapSet */
  Vec3 camera;              /* 0x80: where the penguin is drawn (the ribbons' origin too) */
  f32 pitch;                /* 0x8C: Ninja angle units, within +-15 degrees in the main controls */
  f32 yaw;                  /* 0x90: Ninja angle units */
  gaddr effects;            /* 0x94: RacerEffects (0x3C bytes, func_8C038C1C), a child task */
} CastScene;
HW_OFFSET(CastScene, pad, 0x24);
HW_OFFSET(CastScene, controls, 0x28);
HW_OFFSET(CastScene, fixed_speed, 0x2C);
HW_OFFSET(CastScene, trap_mode, 0x30);
HW_OFFSET(CastScene, penguin, 0x34);
HW_OFFSET(CastScene, page, 0x38);
HW_OFFSET(CastScene, motion, 0x40);
HW_OFFSET(CastScene, looks, 0x48);
HW_OFFSET(CastScene, word58, 0x58);
HW_OFFSET(CastScene, penguins, 0x5C);
HW_OFFSET(CastScene, trap_set, 0x7C);
HW_OFFSET(CastScene, camera, 0x80);
HW_OFFSET(CastScene, pitch, 0x8C);
HW_OFFSET(CastScene, yaw, 0x90);
HW_OFFSET(CastScene, effects, 0x94);

#define g_cast_count GUEST_VAR(int32_t, 0x8C072644u) /* the penguins the scene makes (8) */
/* where the camera starts and goes back to (0, -1.5, -40) */
#define g_cast_start_x GUEST_VAR(f32, 0x8C072638u)
#define g_cast_start_y GUEST_VAR(f32, 0x8C07263Cu)
#define g_cast_start_z GUEST_VAR(f32, 0x8C072640u)
/* 1 while the ribbons are waiting to be fired: the update fires them (and clears it) when the motion enters one of
 * its frame windows, and fires them again (and sets it) when it leaves (1 at start) */
#define g_cast_ribbons_armed GUEST_VAR(int32_t, 0x8C092D5Cu)
/* the draw's four places (x: -10 / 10, y: 6.99 / -6.99, z: -50) */
#define g_cast_draw_left   GUEST_VAR(f32, 0x8C072650u)
#define g_cast_draw_right  GUEST_VAR(f32, 0x8C072654u)
#define g_cast_draw_top    GUEST_VAR(f32, 0x8C072660u)
#define g_cast_draw_bottom GUEST_VAR(f32, 0x8C072664u)
#define g_cast_draw_depth  GUEST_VAR(f32, 0x8C07266Cu)
/* the captions: the penguin's name by CastScene.penguin, the motion set's name by CastScene.motion */
#define g_cast_penguin_names ((const gaddr *)hw_ptr(0x8C092D60u))
#define g_cast_motion_names  ((const gaddr *)hw_ptr(0x8C092D84u))

static inline CastScene *cast_scene_at(gaddr a) { return (CastScene *)hw_ptr(a); }

/* the penguin shown: penguins[g_cast_count * page + penguin] (the original's 32-bit index arithmetic, no bound) */
static inline gaddr cast_scene_penguin(gaddr scene) {
  const CastScene *s = cast_scene_at(scene);
  uint32_t i = (uint32_t)g_cast_count * (uint32_t)s->page + (uint32_t)s->penguin;
  return GUEST_VAR(gaddr, member_addr(scene, CastScene, penguins) + i * (uint32_t)sizeof(gaddr));
}

/* game/cast_scene.c */
void cast_scene_destroy(gaddr scene);
gaddr cast_scene_delete(gaddr scene, int32_t free_memory);
gaddr cast_scene_destroy_self(gaddr scene);
/* S18 b02: game/cast_scene_update.c */
void cast_scene_update(gaddr scene);
void cast_scene_draw(gaddr scene);

#endif
