/* Pen Pen port, S17 b03: the CastScene's destruction. The CastScene is a Task (constructor func_8C02004C, vtable
 * 0x8C23DA14, body func_8C01FE58, which fades the screen and so stays translated) that shows a penguin per
 * character: it makes g_cast_count MenuPenguins (MotionPlayers, game/motion.h; func_8C01F6DA with the character
 * number) into `penguins`, and a TrapSet (game/trap_set.c) at 0x7C. The class name is this file's. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/inline.h"
#include "../game/motion.h"

typedef struct CastScene {
  Task task;                /* 0x00 */
  uint8_t pad_024[0x5C - 0x24];
  gaddr penguins[8];        /* 0x5C: g_cast_count of them used (0 = none) */
  gaddr trap_set;           /* 0x7C */
} CastScene;
HW_OFFSET(CastScene, penguins, 0x5C);
HW_OFFSET(CastScene, trap_set, 0x7C);

#define g_cast_count GUEST_VAR(int32_t, 0x8C072644u) /* the penguins the scene makes (8) */

static inline CastScene *cast_scene_at(gaddr a) { return (CastScene *)hw_ptr(a); }

/* was func_8C020062: the destructor's body: each penguin deleted (and freed), then the Task's (empty) body. The
 * loop runs to g_cast_count as it is now (no bound by the array's 8: the original has none). */
void cast_scene_destroy(gaddr scene) {
  int32_t i;
  for (i = 0; i < g_cast_count; i++) {
    gaddr penguin = cast_scene_at(scene)->penguins[i];
    if (penguin != 0)
      motion_player_delete(penguin, 1);
  }
  method_empty();
}
HW_EXPORT(8c020062, cast_scene_destroy(ARG0))

/* was func_8C0200A6: the virtual destructor (frees the scene when asked) */
gaddr cast_scene_delete(gaddr scene, int32_t free_memory) {
  cast_scene_destroy(scene);
  if (free_memory != 0)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c0200a6, RET(cast_scene_delete(ARG0, (int32_t)ARG1)))

/* was func_8C0200CA: the destructor without freeing */
gaddr cast_scene_destroy_self(gaddr scene) {
  cast_scene_destroy(scene);
  return scene;
}
HW_EXPORT(8c0200ca, RET(cast_scene_destroy_self(ARG0)))
