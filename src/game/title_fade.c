/* Pen Pen port, S17 (b02): the title scene's fade-in step and its draw (game/title.c keeps a static copy of the step, title_fade_in,
 * which the lead may replace by this one). */
#include "hw.h"
#include "sdk.h"
#include "title.h"
#include "../core/ninja.h"
#include "../core/inline_b02.h"

#define FADE_STEP F32_BITS(0x3C888888u) /* 1/60: the fade-in lasts a second */

/* was func_8C01BB52: state 0, the fade-in: one second, then the logo (state 1) */
void title_fade_in_step(gaddr scene) {
  TitleScene *t = title_at(scene);
  t->fade = fadd(t->fade, FADE_STEP);
  if (fgt(t->fade, F32(1.0f)))
    t->state = 1;
}
HW_EXPORT(8c01bb52, title_fade_in_step(ARG0))

/* the sprite drawn over the title while it fades in, and the flag of its one-time set-up */
#define TITLE_FADE_SPRITE 0x8C08C950u
#define g_title_fade_sprite GUEST_VAR(NjSprite, TITLE_FADE_SPRITE)
#define g_title_fade_ready GUEST_VAR(int32_t, 0x8C1BAAB0u) /* 0 until the first draw gives it the texlist */
#define FADE_CONTROL3D 0x10
#define FADE_ATTR 0x22
#define FADE_PRI F32_BITS(0xC77FFF00u) /* -65535 */

/* was func_8C01C5E8: the fade-in's draw: the sprite (the title's texlist the first time) at the fade's alpha, with
 * the 3D flags 0x10 pushed */
void title_draw_fade(gaddr scene) {
  if (g_title_fade_ready == 0) {
    g_title_fade_ready++;
    g_title_fade_sprite.tlist = title_at(scene)->texlist;
  }
  control3d_push();
  control3d_set(FADE_CONTROL3D);
  material_set_b072(F32(1.0f), F32(1.0f), F32(1.0f), title_at(scene)->fade);
  njDrawSprite2D(TITLE_FADE_SPRITE, 0, FADE_PRI, FADE_ATTR);
  control3d_pop();
}
HW_EXPORT(8c01c5e8, title_draw_fade(ARG0))
