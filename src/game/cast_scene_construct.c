/* Pen Pen port, S18 (lead): the CastScene's construction (game/cast_scene.h): the cast viewer's constructor and body.
 * Nothing in the shipped game builds this scene (no call, no pointer: a debug viewer); the synthetic samples run it
 * as a frame-passing prelude (DECOMP.md 19). Resumable: the body shows a loading screen (two frames). */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "cast_scene.h"
#include "trap_set.h"
#include "effects.h"
#include "motion.h"

void loading_screen_show(int32_t picture);                 /* game/loading.c */
void background_set(uint32_t argb, f32 fog_near, f32 fog_far); /* core/gfx_init.c */

#define CAST_SCENE_VTABLE   0x8C23DA14u
#define CAST_LOADING_PICTURE 0x2A
#define CAST_SPLIT          3           /* view_set_split: the cast's view layout */
#define CAST_BACKGROUND     0xFF0000FFu /* opaque blue */
#define MENU_PENGUIN_SIZE   0xF8
#define RACER_EFFECTS_SIZE  0x3C
#define TRAP_SET_SIZE       0x558

/* was func_8C01FE58: the scene's body: the Task (no parent), the pad on port 0, the camera's Vec3 and the effects slot;
 * the loading screen; a MenuPenguin per character (g_cast_count, 0 where the allocation failed); four views lit
 * (0, 0, 1); the camera at its start, level; the main controls at speed 1, the first motion set; the looks taken
 * from penguin 0 (the last first); the fog off and a blue background; a RacerEffects child and a TrapSet child,
 * which places its objects */
gaddr cast_scene_init(gaddr scene) {
  HW_RESUMABLE(0x8C01FE58u, scene);
  HW_RESUME_AT(0);
  task_construct(scene, 0);
  word_construct(member_addr(scene, CastScene, pad), 0);
  method_self(member_addr(scene, CastScene, camera)); /* the Vec3's (empty) constructor */
  cast_scene_at(scene)->effects = 0;
  HW_YIELDING_RET(0, loading_screen_show(CAST_LOADING_PICTURE), 0);
  CastScene *s = cast_scene_at(scene);
  s->penguin = 0;
  s->page = 0;
  /* the original's count is read again every pass (no bound by the array's 8) */
  for (int32_t i = 0; i < g_cast_count; i++) {
    gaddr block = syMalloc(MENU_PENGUIN_SIZE);
    cast_scene_at(scene)->penguins[i] = block != 0 ? menu_penguin_construct(block, i, 0, 0, 0) : 0;
  }
  view_set_split(CAST_SPLIT);
  for (int32_t v = 0; v < 4; v++)
    view_set_light_intensity(F32(0.0f), F32(0.0f), F32(1.0f), v);
  s = cast_scene_at(scene);
  s->word3c = 0;
  vec3_set(g_cast_start_x, g_cast_start_y, g_cast_start_z, member_addr(scene, CastScene, camera));
  s->pitch = F32(0.0f);
  s->yaw = F32(0.0f);
  s->controls = 1;
  s->motion = 0;
  s->fixed_speed = 1;
  s->trap_mode = 0;
  s->word58 = 0;
  for (int32_t i = CAST_LOOKS - 1; i >= 0; i--)
    cast_scene_at(scene)->looks[i] = motion_player_look(cast_scene_at(scene)->penguins[0], i);
  view_fog_off(0);
  background_set(CAST_BACKGROUND, F32(0.0f), fneg(F32(1.0f)));
  gaddr fx = syMalloc(RACER_EFFECTS_SIZE);
  fx = fx != 0 ? racer_effects_construct(fx, 0) : 0;
  cast_scene_at(scene)->effects = fx;
  task_add_children(task_at(scene), task_at(fx));
  gaddr traps = syMalloc(TRAP_SET_SIZE);
  traps = traps != 0 ? trap_set_construct(traps) : 0;
  cast_scene_at(scene)->trap_set = traps;
  task_add_children(task_at(scene), task_at(traps));
  trap_set_populate(cast_scene_at(scene)->trap_set);
  return scene;
}
HW_EXPORT_RESUMABLE(8c01fe58, RET(cast_scene_init(ARG0)))

/* was func_8C02004C: the constructor (the vtable, then the body) */
gaddr cast_scene_construct(gaddr scene) {
  HW_RESUMABLE(0x8C02004Cu, scene);
  HW_RESUME_AT(0);
  task_at(scene)->vtable = CAST_SCENE_VTABLE;
  HW_YIELDING_RET(0, cast_scene_init(scene), 0);
  return scene;
}
HW_EXPORT_RESUMABLE(8c02004c, RET(cast_scene_construct(ARG0)))
