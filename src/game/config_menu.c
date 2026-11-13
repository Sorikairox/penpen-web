/* Pen Pen port, S17 b03: the language set-up, the scene of game mode 12 (game/mode_scenes.h).
 *
 * A cursor (`shown`, 1 to 8: the seven languages, then the entry that leaves) moves through a table of moves with
 * the directions; A on a language sets it and saves, A on the last entry or B leaves without saving. Each entry is
 * drawn by a still-translated drawer, and two lines of help text in the language the scene started with. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/pad.h"
#include "../core/ninja.h"
#include "../core/view.h"
#include "../core/sprite_text.h"
#include "../game/game.h"
#include "../game/sound.h"
#include "../game/mode_scenes.h"

#define CONFIG_PVM          0x8C073464u /* "config.pvm" */
#define CONFIG_TLIST        0x8C092FA0u
#define CONFIG_TANIM        0x8C092FBCu
#define CONFIG_BACK_SPRITE  0x8C0932B8u /* NJS_SPRITE template of the backdrop */
#define CONFIG_ENTRIES      9           /* per language: an entry (0 unused, 1-8) of two help lines */
#define CONFIG_HELP         ((const gaddr (*)[CONFIG_ENTRIES][2])hw_ptr(0x8C0930C0u))
#define CONFIG_LANGUAGES    7           /* entries 1-7 are languages, 8 leaves */
#define CONFIG_LEAVE_ENTRY  8
/* the text style (core/text_draw.c) */
#define SPRITE_SMALL        0x8C1B3AE8u
#define g_text_scale_x      GUEST_VAR(f32, 0x8C1B3B28u)
#define g_text_scale_y      GUEST_VAR(f32, 0x8C1B3B2Cu)
#define g_text_spacing      GUEST_VAR(f32, 0x8C1B3B30u)
#define g_text_color        ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b */

/* func_8C0120CA (core/texture.c, not declared in a header): loads a texture file into a texlist */
static void texture_load(gaddr name, gaddr texlist, int32_t flags) { CALL3(0x8C0120CAu, name, texlist, flags); }
/* func_8C010F04 (core/gfx_init.c, not declared in a header): the background colour and the fog's range */
static void background_set(uint32_t argb, int32_t mode, f32 fog_near, f32 fog_far) {
  FARGS2(fog_near, fog_far);
  CALL2(0x8C010F04u, argb, mode);
}
/* func_8C019860 / func_8C019864 (game/save.c, not declared in a header): the game's language (0-6) / set it */
static int32_t game_language(void) { return (int32_t)CALL0(0x8C019860u); }
static void game_set_language(int32_t language) { CALL1(0x8C019864u, language); }
/* func_8C018F7C (still translated): writes the save file; nonzero once it is done */
static int32_t save_write(void) { return (int32_t)CALL0(0x8C018F7Cu); }
/* func_8C02232C (still translated): draws the leave entry n, highlighted when `selected` (language entry n:
 * config_scene_draw_entry, game/config_entries.c) */
static void config_leave_draw(gaddr scene, int32_t selected, int32_t n) { CALL3(0x8C02232Cu, scene, selected, n); }

/* was func_8C021D6E: the scene's body: the Task part by hand (its pause word and vtable left as they are), the pad
 * on port 0, the display stopped while one view and a black background are set and the textures load; the cursor
 * starts on the game's language; the sprite at scale 1 */
gaddr config_scene_init(gaddr scene) {
  ConfigScene *c = config_scene_at(scene);
  c->task.linger = 0;
  c->task.active = 1;
  c->task.stops_on_pause = 1;
  c->task.child = 0;
  c->task.next = 0;
  c->task.parent = 0;
  c->task.owner = 0;
  c->pad.port = 0;
  g_video_running = 0;
  view_set_split(0);
  background_set(0xFF000000u, 0, F32(0.0f), F32(-1.0f));
  c->texlist = CONFIG_TLIST;
  texture_load(CONFIG_PVM, CONFIG_TLIST, 0);
  c->step = 0;
  c->repeat_lock = MODE_SCENE_REPEAT_FRAMES;
  c->word78 = 0;
  c->language = game_language();
  c->shown = c->language;
  if (c->shown < 4) /* the original tests the value it just stored */
    c->word2c = 1;
  else
    c->word2c = 0;
  c->sprite.sx = F32(1.0f);
  c->sprite.sy = F32(1.0f);
  c->sprite.tlist = c->texlist;
  c->sprite.tanim = CONFIG_TANIM;
  g_video_running = 1;
  return scene;
}
HW_EXPORT(8c021d6e, RET(config_scene_init(ARG0)))

/* was func_8C021E0A: the constructor (the vtable, then the body) */
gaddr config_scene_construct(gaddr scene) {
  config_scene_at(scene)->task.vtable = CONFIG_VTABLE;
  config_scene_init(scene);
  return scene;
}
HW_EXPORT(8c021e0a, RET(config_scene_construct(ARG0)))

/* was func_8C021E20: the destructor's body: the textures released */
void config_scene_destroy(gaddr scene) { njReleaseTexture(config_scene_at(scene)->texlist); }
HW_EXPORT(8c021e20, config_scene_destroy(ARG0))

/* was func_8C021E68: the virtual destructor (frees the scene when asked) */
gaddr config_scene_delete(gaddr scene, int32_t free_memory) {
  config_scene_destroy(scene);
  if (free_memory != 0)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c021e68, RET(config_scene_delete(ARG0, (int32_t)ARG1)))

/* was func_8C021E8C: the destructor without freeing */
gaddr config_scene_destroy_self(gaddr scene) {
  config_scene_destroy(scene);
  return scene;
}
HW_EXPORT(8c021e8c, RET(config_scene_destroy_self(ARG0)))

/* was func_8C021E9E: the update: the pad (game/gallery.c's step), then the step. In step 1 Right or else Left,
 * then Up or else Down move the cursor by the table (each with a click); A on a language plays the confirm sound,
 * sets the language and saves (step 2), A on the leave entry or B plays the cancel sound and leaves (step 3). */
void config_scene_update(gaddr scene) {
  ConfigScene *c = config_scene_at(scene);
  mode_scene_read_pad(member_addr(scene, ConfigScene, pad), &c->press, &c->repeat_lock);
  switch (c->step) {
  case 0:
    c->step++;
    break;
  case 1:
    if (c->press.right != 0) {
      sound_play(0, 5, 0);
      c->shown = CONFIG_MOVES[c->shown - 1].right;
    } else if (c->press.left != 0) {
      sound_play(0, 5, 0);
      c->shown = CONFIG_MOVES[c->shown - 1].left;
    }
    if (c->press.up != 0) {
      sound_play(0, 5, 0);
      c->shown = CONFIG_MOVES[c->shown - 1].up;
    } else if (c->press.down != 0) {
      sound_play(0, 5, 0);
      c->shown = CONFIG_MOVES[c->shown - 1].down;
    }
    if (c->press.a != 0) {
      if (c->shown < CONFIG_LANGUAGES) {
        sound_play(2, 7, 0);
        game_set_language(c->shown);
        c->step = 2;
      } else {
        sound_play(3, 8, 0);
        c->step = 3;
      }
    } else if (c->press.b != 0) {
      sound_play(3, 8, 0);
      c->step = 3;
    }
    break;
  case 2: /* saving */
    if (save_write() != 0)
      c->step = 3;
    break;
  case 3: { /* back to the title */
    Game *game = game_at(c->task.owner);
    game->next_mode = MODE_TITLE;
    game->mode_arg = 0;
    break;
  }
  }
}
HW_EXPORT(8c021e9e, config_scene_update(ARG0))

/* was func_8C022058: the draw: the backdrop (a sprite built on the stack from its template), entries 1-6, entry 7
 * or else 8 by the leave entry's drawer, then the cursor's two help lines in the scene's language, in the text
 * style it sets (scale 0.875, spacing -1, colour a 1, r 0.6, g 1, b 0.38) */
void config_scene_draw(gaddr scene) {
  ConfigScene *c = config_scene_at(scene);
  gaddr back = hw_stack_alloc(sizeof(NjSprite));
  int32_t n;
  memmove(hw_ptr(back), hw_ptr(CONFIG_BACK_SPRITE), sizeof(NjSprite));
  ((NjSprite *)hw_ptr(back))->tlist = c->texlist;
  njDrawSprite2D(back, 0, F32(-300.0f), 0);
  for (n = 1; n <= 6; n++)
    config_scene_draw_entry(scene, c->shown == n, n);
  if (c->shown == 7)
    config_leave_draw(scene, c->shown == 7, 7);
  else
    config_leave_draw(scene, c->shown == CONFIG_LEAVE_ENTRY, CONFIG_LEAVE_ENTRY);
  {
    NjSprite *small = (NjSprite *)hw_ptr(SPRITE_SMALL);
    f32 scale = F32(0.875f);
    g_text_scale_x = scale;
    small->sx = scale;
    g_text_scale_y = scale;
    small->sy = scale;
    g_text_spacing = F32(-1.0f);
    g_text_color[1] = F32_BITS(0x3F199999u); /* 0.6 */
    g_text_color[2] = F32(1.0f);
    g_text_color[3] = F32_BITS(0x3EC28F5Cu); /* 0.38 */
    g_text_color[0] = F32(1.0f);
  }
  text_draw(120, 32, CONFIG_HELP[c->language][c->shown][0]);
  text_draw(106, 66, CONFIG_HELP[c->language][c->shown][1]);
  hw_stack_free(sizeof(NjSprite));
}
HW_EXPORT(8c022058, config_scene_draw(ARG0))
