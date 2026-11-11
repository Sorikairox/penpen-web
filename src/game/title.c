/* Pen Pen port, S12: the title scene (mode 2's scene; mode 1 builds the same one with variant 0). A Task subclass
 * of 0x1C4 bytes whose constructor loads the title's data and shows the screen meanwhile (it can end frames: the
 * scene base constructor it calls is on the yielding chain). Its update and draw stay translated (floats);
 * so does func_8C01B812 (the destructor without the free: no route calls it). */
#include "game.h"
#include "sound.h"
#include "sdk.h"

#define TITLE_VTABLE 0x8C23D96Cu /* {destroy = title_destroy, update, draw} */

/* the blocks the scene allocated while loading (syMalloc; what each holds is not known yet), its texture list */
typedef struct TitleScene {
  Task task;                  /* 0x000 */
  uint8_t unknown024[0x98 - 0x24];
  gaddr texlist;              /* 0x098: released with njReleaseTexture */
  gaddr data_9c[3];           /* 0x09C */
  gaddr data_a8[3];           /* 0x0A8 */
  gaddr data_b4;              /* 0x0B4 */
  gaddr data_b8;              /* 0x0B8 */
} TitleScene;
HW_OFFSET(TitleScene, texlist, 0x98);
HW_OFFSET(TitleScene, data_a8, 0xA8);
HW_OFFSET(TitleScene, data_b8, 0xB8);
#define TITLE_ARRAY 3

static inline TitleScene *title_at(gaddr a) { return (TitleScene *)hw_ptr(a); }

/* still translated */
static void scene_construct(gaddr scene, int32_t variant) { CALL2(0x8C01B23Cu, scene, variant); } /* func_8C01B23C: loads, shows frames */
static void njReleaseTexture(gaddr texlist) { CALL1(0x8C0A441Cu, texlist); }

/* was func_8C01B77A: the constructor (the vtable first, then the base constructor: it loads the scene) */
gaddr title_construct(gaddr scene, int32_t variant) {
  HW_RESUMABLE(0x8C01B77Au, scene, variant);
  HW_RESUME_AT(0);
  task_at(scene)->vtable = TITLE_VTABLE;
  HW_YIELDING_RET(0, scene_construct(scene, variant), 0);
  return scene;
}
HW_EXPORT_RESUMABLE(8c01b77a, RET(title_construct(ARG0, (int32_t)ARG1)))

/* was func_8C01B790: free what the scene loaded, stop the music, and wait until the drive is no longer playing */
static void title_release(gaddr scene) {
  TitleScene *title = title_at(scene);
  syFree(title->data_b8);
  for (int i = 0; i < TITLE_ARRAY; i++)
    syFree(title->data_a8[i]);
  for (int i = 0; i < TITLE_ARRAY; i++)
    syFree(title->data_9c[i]);
  syFree(title->data_b4);
  njReleaseTexture(title->texlist);
  music_stop();
  while (gdFsGetDrvStat() == GDD_DRVSTAT_PLAY) {
  }
}
HW_EXPORT(8c01b790, title_release(ARG0))

/* was func_8C01B7EE: the virtual destructor (vtable slot 0): release, then free the object when asked */
gaddr title_destroy(gaddr scene, int32_t free_memory) {
  title_release(scene);
  if (free_memory)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c01b7ee, RET(title_destroy(ARG0, (int32_t)ARG1)))
