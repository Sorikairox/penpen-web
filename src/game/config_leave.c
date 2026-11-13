/* Pen Pen port, S18 b06: the language set-up's last entry, the one that leaves (ConfigScene, game/mode_scenes.h;
 * the language entries are game/config_entries.c's config_scene_draw_entry, which this one copies but for the
 * name's cell). */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/ninja.h"
#include "mode_scenes.h"
#include "config_leave.h"

/* Ninja's NJS_ARGB */
typedef struct NjArgb {
  f32 a, r, g, b;
} NjArgb;
HW_SIZE(NjArgb, 16);
#define CONFIG_ARGB_TEMPLATE 0x8C092F78u /* the material the colours below are built on (all four fields replaced) */

/* where an entry's two sprites go (by entry - 1): its name, then its box (game/config_entries.c) */
typedef struct ConfigEntryPlace {
  f32 name_x, name_y;
  f32 box_x, box_y;
} ConfigEntryPlace;
HW_SIZE(ConfigEntryPlace, 16);
#define CONFIG_ENTRY_PLACES ((const ConfigEntryPlace *)hw_ptr(0x8C072D44u))
#define CONFIG_BOX_CELL     12
#define CONFIG_ENTRY_ATTR   0x22u
/* the original's frame: 0x18 bytes of locals (n, selected, the colour at +8) under r12-r14, fr15 and PR */
#define LEAVE_FRAME         0x2C
#define LEAVE_FRAME_ARGB    8

/* the constant material from the template, then a, r, g, b */
static void leave_material(gaddr argb, f32 a, f32 r, f32 g, f32 b) {
  memmove(hw_ptr(argb), hw_ptr(CONFIG_ARGB_TEMPLATE), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
}

/* was func_8C02232C: draw the leave entry (n = 8) like a language entry, with njControl3D's flags pushed: its
 * name (the scene's sprite's cell language - 1: the word in the language the scene started with; pink when
 * selected, else mauve) and its box (cell 12, white when selected, else yellow), each at entry n's place. The
 * colour is a local of the original's frame (kept on the guest stack at the same place). */
void config_scene_draw_leave(gaddr scene, int32_t selected, int32_t n) {
  ConfigScene *c = config_scene_at(scene);
  gaddr frame = hw_stack_alloc(LEAVE_FRAME);
  gaddr argb = frame + LEAVE_FRAME_ARGB;
  control3d_push();
  if (selected != 0)
    leave_material(argb, F32(1.0f), F32_BITS(0x3F781062u) /* 0.97 */, F32_BITS(0x3F44DD2Fu) /* 0.769 */,
                   F32_BITS(0x3F63D70Au) /* 0.89 */);
  else
    leave_material(argb, F32(1.0f), F32_BITS(0x3F781062u) /* 0.97 */, F32_BITS(0x3ECAC083u) /* 0.396 */,
                   F32_BITS(0x3F51EB85u) /* 0.82 */);
  const ConfigEntryPlace *place = &CONFIG_ENTRY_PLACES[n - 1];
  c->sprite.p.x = place->name_x;
  c->sprite.p.y = place->name_y;
  njDrawSprite2D(member_addr(scene, ConfigScene, sprite), c->language - 1, F32(-2.0f), CONFIG_ENTRY_ATTR);
  if (selected != 0)
    leave_material(argb, F32(1.0f), F32(1.0f), F32(1.0f), F32(0.0f));
  else
    leave_material(argb, F32(1.0f), F32(1.0f), F32_BITS(0x3F3D70A3u) /* 0.74 */, F32_BITS(0x3E926E97u) /* 0.286 */);
  place = &CONFIG_ENTRY_PLACES[n - 1];
  c->sprite.p.x = place->box_x;
  c->sprite.p.y = place->box_y;
  njDrawSprite2D(member_addr(scene, ConfigScene, sprite), CONFIG_BOX_CELL, F32(-3.0f), CONFIG_ENTRY_ATTR);
  control3d_pop();
  hw_stack_free(LEAVE_FRAME);
}
HW_EXPORT(8c02232c, config_scene_draw_leave(ARG0, (int32_t)ARG1, (int32_t)ARG2))
