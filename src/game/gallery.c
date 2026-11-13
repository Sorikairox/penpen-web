/* Pen Pen port, S17 b03: the picture gallery, the scene of game mode 11 (game/mode_scenes.h).
 *
 * It shows one of 42 pictures over the gallery's frame: Right / Up step to the next picture, Left / Down to the one
 * before (wrapping round), each loading the new picture's textures when it has been unlocked (a save flag); A shows
 * or hides the picture's number, Start goes back to the title. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/pad.h"
#include "../core/ninja.h"
#include "../core/view.h"
#include "../game/game.h"
#include "../game/mode_scenes.h"

#define GALLERY_PVM           0x8C072D34u /* "gallery.pvm" */
#define GALLERY_PICTURE_TLIST 0x8C092E40u /* the picture's NJS_TEXLIST */
#define GALLERY_FRAME_TLIST   0x8C092E54u
#define GALLERY_PICTURE_PVMS  ((const gaddr *)hw_ptr(0x8C092E70u)) /* a texture file name per picture */
#define GALLERY_FRAME_SPRITE  0x8C092F40u /* NJS_SPRITE templates (copied to the stack, the texlist set) */
#define GALLERY_PICTURE_SPRITE 0x8C092F20u
#define SPRITE_BIG            0x8C1B3B08u /* the big text sprite (core/sprite_text.c) */
#define GALLERY_LOCK_CELL     14          /* its cell drawn beside the number */

/* func_8C0120CA (core/texture.c, not declared in a header): loads a texture file into a texlist */
static void texture_load(gaddr name, gaddr texlist, int32_t flags) { CALL3(0x8C0120CAu, name, texlist, flags); }
/* func_8C010F04 (core/gfx_init.c, not declared in a header): the background colour and the fog's range */
static void background_set(uint32_t argb, int32_t mode, f32 fog_near, f32 fog_far) {
  FARGS2(fog_near, fog_far);
  CALL2(0x8C010F04u, argb, mode);
}
/* func_8C0197C4 (still translated): save flag n is set (bit n % 8 of byte n / 8 at 0x8C1B7F50) */
static int32_t save_flag_test(int32_t n) { return (int32_t)CALL1(0x8C0197C4u, n); }
/* func_8C0158CA (still translated): the number `value` at (x, y), `size` pixels */
static void number_draw(int32_t x, int32_t y, int32_t value, int32_t size) { CALL4(0x8C0158CAu, x, y, value, size); }

/* ---- the pad step (the config scene has the same, game/config_menu.c) ---- */

/* the repeat lock counts down every update; once it is below 0, Start, A and B are read and any of them sets it
 * again; while it runs their presses are cleared. The directions are read every update. */
void mode_scene_read_pad(gaddr pad, ModeScenePresses *press, int32_t *repeat_lock) {
  const Pad *p = (const Pad *)hw_ptr(pad);
  int32_t lock = *repeat_lock - 1;
  *repeat_lock = lock;
  if (lock < 0) {
    press->start = pad_pressed(p, PAD_START);
    press->a = pad_pressed(p, PAD_A);
    press->b = pad_pressed(p, PAD_B);
    if (press->start != 0 || press->a != 0 || press->b != 0)
      *repeat_lock = MODE_SCENE_REPEAT_FRAMES;
  } else {
    press->b = 0;
    press->a = 0;
    press->start = 0;
  }
  press->right = pad_pressed(p, PAD_RIGHT);
  press->left = pad_pressed(p, PAD_LEFT);
  press->up = pad_pressed(p, PAD_UP);
  press->down = pad_pressed(p, PAD_DOWN);
}

/* ---- construction and destruction ---- */

/* was func_8C0219A0: the scene's body: the Task part by hand (its pause word and vtable left as they are), the pad
 * on port 0, the display stopped while one view and a black background are set and the frame's textures load */
gaddr gallery_init(gaddr scene) {
  GalleryScene *g = gallery_at(scene);
  g->task.linger = 0;
  g->task.active = 1;
  g->task.stops_on_pause = 1;
  g->task.child = 0;
  g->task.next = 0;
  g->task.parent = 0;
  g->task.owner = 0;
  g->pad.port = 0;
  g_video_running = 0;
  view_set_split(0);
  background_set(0xFF000000u, 0, F32(0.0f), F32(-1.0f));
  g->picture_texlist = GALLERY_PICTURE_TLIST;
  g->frame_texlist = GALLERY_FRAME_TLIST;
  texture_load(GALLERY_PVM, GALLERY_FRAME_TLIST, 0);
  g->step = 0;
  g->word2c = 0;
  g->picture = 0;
  g->repeat_lock = MODE_SCENE_REPEAT_FRAMES;
  g->number_hidden = 0;
  g_video_running = 1;
  return scene;
}
HW_EXPORT(8c0219a0, RET(gallery_init(ARG0)))

/* was func_8C021A10: the constructor (the vtable, then the body) */
gaddr gallery_construct(gaddr scene) {
  gallery_at(scene)->task.vtable = GALLERY_VTABLE;
  gallery_init(scene);
  return scene;
}
HW_EXPORT(8c021a10, RET(gallery_construct(ARG0)))

/* was func_8C021A26: the destructor's body: both texlists' textures released */
void gallery_destroy(gaddr scene) {
  njReleaseTexture(gallery_at(scene)->picture_texlist);
  njReleaseTexture(gallery_at(scene)->frame_texlist);
}
HW_EXPORT(8c021a26, gallery_destroy(ARG0))

/* was func_8C021A42: the virtual destructor (frees the scene when asked) */
gaddr gallery_delete(gaddr scene, int32_t free_memory) {
  gallery_destroy(scene);
  if (free_memory != 0)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c021a42, RET(gallery_delete(ARG0, (int32_t)ARG1)))

/* was func_8C021A66: the destructor without freeing */
gaddr gallery_destroy_self(gaddr scene) {
  gallery_destroy(scene);
  return scene;
}
HW_EXPORT(8c021a66, RET(gallery_destroy_self(ARG0)))

/* ---- update and draw ---- */

/* was func_8C021A78: the update: the pad, A's toggle, then the step */
void gallery_update(gaddr scene) {
  GalleryScene *g = gallery_at(scene);
  mode_scene_read_pad(member_addr(scene, GalleryScene, pad), &g->press, &g->repeat_lock);
  if (g->press.a != 0)
    g->number_hidden = g->number_hidden == 0;
  switch (g->step) {
  case 0: /* the picture's textures, when it is unlocked */
    if (save_flag_test(g->picture) != 0) {
      njReleaseTexture(g->picture_texlist);
      texture_load(GALLERY_PICTURE_PVMS[g->picture], g->picture_texlist, 0);
    }
    g->step++;
    break;
  case 1:
    if (g->press.right != 0 || g->press.up != 0) {
      g->picture++;
      if (g->picture >= GALLERY_PICTURES)
        g->picture -= GALLERY_PICTURES;
      g->step--;
    } else if (g->press.left != 0 || g->press.down != 0) {
      g->picture--;
      if (g->picture < 0)
        g->picture += GALLERY_PICTURES;
      g->step--;
    } else if (g->press.start != 0) {
      g->step++;
    }
    break;
  case 2: { /* back to the title */
    Game *game = game_at(g->task.owner);
    game->next_mode = MODE_TITLE;
    game->mode_arg = 0;
    break;
  }
  }
}
HW_EXPORT(8c021a78, gallery_update(ARG0))

/* was func_8C021C10: the draw: the frame, the picture when it is unlocked, then (unless A hid it) the picture's
 * number, the big sprite's cell 14 and the number 38 beside it. The two sprites are built on the stack from
 * their templates (the frame's at the frame's address, the picture's 0x20 above it). */
void gallery_draw(gaddr scene) {
  GalleryScene *g = gallery_at(scene);
  gaddr frame = hw_stack_alloc(2 * sizeof(NjSprite));
  gaddr picture = frame + sizeof(NjSprite);
  memmove(hw_ptr(picture), hw_ptr(GALLERY_PICTURE_SPRITE), sizeof(NjSprite));
  ((NjSprite *)hw_ptr(picture))->tlist = g->picture_texlist;
  memmove(hw_ptr(frame), hw_ptr(GALLERY_FRAME_SPRITE), sizeof(NjSprite));
  ((NjSprite *)hw_ptr(frame))->tlist = g->frame_texlist;
  njDrawSprite2D(frame, 0, F32(-300.0f), 0);
  if (save_flag_test(g->picture) != 0)
    njDrawSprite2D(picture, 0, F32(-200.0f), 0);
  if (g->number_hidden == 0) {
    NjSprite *big;
    number_draw(520, 378, g->picture, 32);
    big = (NjSprite *)hw_ptr(SPRITE_BIG);
    big->p.x = F32(550.0f);
    big->p.y = F32(388.0f);
    big->sx = F32(0.71875f);
    big->sy = F32(0.71875f);
    njDrawSprite2D(SPRITE_BIG, GALLERY_LOCK_CELL, F32(-1.0f), 0x20);
    number_draw(570, 408, 38, 32);
  }
  hw_stack_free(2 * sizeof(NjSprite));
}
HW_EXPORT(8c021c10, gallery_draw(ARG0))
