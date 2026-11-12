/* Pen Pen port, S14: the loading screen (game/loading.c).
 *
 * Shown while a scene loads: one picture (a PVM file of one texture, by number; a random one of seven for most
 * numbers, offset by the console's language) drawn as a full-screen sprite for two frames, then released. Its
 * texture list, sprite and texture animation live in the function's frame on the guest stack (the SDK writes the
 * texture handle into the list), initialised from static templates. */
#include "hw.h"
#include "sdk.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "../core/inline.h"
#include "game.h"

/* functions of other files (no header yet) */
void background_set(uint32_t argb, f32 near_z, f32 far_z); /* core/gfx_init.c */
f32 random_timer_scaled(f32 range);                        /* core/random.c */
void texture_load(gaddr name, gaddr texlist, gaddr dir_name); /* core/texture.c */
int32_t game_language(void);                               /* game/save.c */

#define LOADING_FILES     0x8C08C368u /* the picture files' names, by number */
#define TEXNAME_TEMPLATE  0x8C08C418u /* NJS_TEXNAME (12 bytes) */
#define TEXLIST_TEMPLATE  0x8C08C424u /* NJS_TEXLIST {textures, count} */
#define TEXANIM_TEMPLATE  0x8C08C42Cu /* NJS_TEXANIM (0x14 bytes) */
#define SPRITE_TEMPLATE   0x8C08C440u /* NJS_SPRITE (0x20 bytes) */
#define LOADING_PRIORITY  F32(-100.0f)

/* the original's frame (0x54 bytes deep with its saved registers: the SDK writes into it, DECOMP.md 15.1) */
typedef struct LoadingFrame {
  NjSprite sprite;          /* 0x00: its texture list and animation point below */
  uint8_t texanim[0x14];    /* 0x20 */
  gaddr texlist[2];         /* 0x34: {textures, count} */
  uint8_t texname[12];      /* 0x3C */
  uint32_t saved[3];        /* 0x48: r14, r13, PR */
} LoadingFrame;
HW_OFFSET(LoadingFrame, texanim, 0x20);
HW_OFFSET(LoadingFrame, texlist, 0x34);
HW_OFFSET(LoadingFrame, texname, 0x3C);
HW_SIZE(LoadingFrame, 0x54);

/* the first picture of each language's set of seven (languages 0 and 1 share the first) */
static int32_t language_base(int32_t language) {
  switch (language) {
  case 2: return 7;
  case 3: return 14;
  case 4: return 21;
  case 5: return 28;
  case 6: return 35;
  default: return 0; /* 0 and 1 (another value would leave the original's register as the caller had it; the
                       * game's language is always 1 here: game/save.c) */
  }
}

/* was func_8C01630E (with func_8C0163CE: Ghidra split it at a literal pool; one function): show loading picture
 * `picture` (below 0 counts as 0) for two frames. Resumable: it waits for two V-blanks */
void loading_screen_show(int32_t picture) {
  gaddr frame = 0;
  HW_RESUMABLE(0x8C01630Eu, frame);
  HW_RESUME_AT(0, 1);
  /* the frame first, as the original's prologue (the registers, r15 too, stay as they are across the frames that
   * end in njWaitVSync: a rewind comes back with r15 still lowered, and the saved `frame`) */
  frame = hw_stack_alloc(sizeof(LoadingFrame));
  view_set_split(0);
  field_set(hw_addr(&g_views[0]), offsetof(View, fog), 0);
  background_set(0xFF000000u, F32(0.0f), F32(-1.0f));
  if (picture < 0) picture = 0;
  int32_t base = language_base(game_language());
  /* 3-9: that picture; 10-17: picture - 10 of the language's set; else a random one of the set */
  if (picture < 3) {
    picture = base + f32_to_int(random_timer_scaled(F32_BITS(0x40DFAE14u))); /* 6.99 */
  } else if (picture > 9) {
    picture -= 10;
    if (picture > 7) picture = f32_to_int(random_timer_scaled(F32_BITS(0x40DFAE14u)));
    picture += base;
  }
  LoadingFrame *f = (LoadingFrame *)hw_ptr(frame);
  memmove(f->texname, hw_ptr(TEXNAME_TEMPLATE), sizeof f->texname);
  memmove(f->texlist, hw_ptr(TEXLIST_TEMPLATE), sizeof f->texlist);
  f->texlist[0] = member_addr(frame, LoadingFrame, texname);
  memmove(f->texanim, hw_ptr(TEXANIM_TEMPLATE), sizeof f->texanim);
  memmove(&f->sprite, hw_ptr(SPRITE_TEMPLATE), sizeof f->sprite);
  f->sprite.tlist = member_addr(frame, LoadingFrame, texlist);
  f->sprite.tanim = member_addr(frame, LoadingFrame, texanim);
  texture_load(((const gaddr *)hw_ptr(LOADING_FILES))[picture], member_addr(frame, LoadingFrame, texlist), 0);
  njSetTexture(member_addr(frame, LoadingFrame, texlist));
  view_frame_begin();
  view_select(0);
  njDrawSprite2D(frame, 0, LOADING_PRIORITY, 0);
  view_finish();
  view_frame_end();
  njFogDisable();
  HW_YIELDING(0, njWaitVSync());
  njReleaseTexture(member_addr(frame, LoadingFrame, texlist));
  njFogDisable();
  HW_YIELDING(1, njWaitVSync());
  hw_stack_free(sizeof(LoadingFrame));
}
HW_EXPORT_RESUMABLE(8c01630e, loading_screen_show((int32_t)ARG0))
