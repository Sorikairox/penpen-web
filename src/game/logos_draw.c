/* Pen Pen port, S19 (u): the draw method of Game mode 0's scene (the logos; game/logos.h). */
#include "hw.h"
#include "sdk.h"
#include "logos.h"
#include "../core/ninja.h"

#define LOGO_SPRITE_TEMPLATE 0x8C08C74Cu /* the picture's NJS_SPRITE as the data holds it (its texlist set here) */
#define LOGO_COLOR_TEMPLATE  0x8C08C714u /* an NJS_ARGB the colour starts from (all four words are then set) */
#define LOGO_CONTROL3D 0x10
#define LOGO_ATTR 0x22
#define LOGO_PRI F32(-1.0f)

/* the draw's frame on the guest stack (the original's: 0x38 bytes under its saved PR and r14): the colour and the
 * sprite go to Ninja by address */
typedef struct LogoDrawFrame {
  gaddr scene;       /* 0x00 */
  uint32_t flags;    /* 0x04: the 3D flags set */
  f32 color[4];      /* 0x08: a, r, g, b (NJS_ARGB) */
  NjSprite sprite;   /* 0x18 */
  uint32_t saved[2]; /* 0x38: the original's PR and r14 slots */
} LogoDrawFrame;
HW_OFFSET(LogoDrawFrame, color, 0x08);
HW_OFFSET(LogoDrawFrame, sprite, 0x18);
HW_SIZE(LogoDrawFrame, 0x40);

/* was func_8C01A4FE: the scene's draw: the picture (sprite 0 of the scene's texlist) in white at the fade's alpha,
 * with the 3D flags 0x10 pushed */
void logo_scene_draw(gaddr scene) {
  gaddr fp = hw_stack_alloc(sizeof(LogoDrawFrame));
  LogoDrawFrame *f = (LogoDrawFrame *)hw_ptr(fp);
  f->scene = scene;
  memmove(&f->sprite, hw_ptr(LOGO_SPRITE_TEMPLATE), sizeof(NjSprite));
  f->sprite.tlist = logo_scene_at(scene)->texlist;
  control3d_push();
  f->flags = LOGO_CONTROL3D;
  control3d_set(LOGO_CONTROL3D);
  f32 alpha = logo_scene_at(scene)->fade;
  memmove(f->color, hw_ptr(LOGO_COLOR_TEMPLATE), sizeof f->color);
  f->color[0] = alpha;
  f->color[1] = F32(1.0f);
  f->color[2] = F32(1.0f);
  f->color[3] = F32(1.0f);
  njSetConstantMaterial(member_addr(fp, LogoDrawFrame, color));
  njDrawSprite2D(member_addr(fp, LogoDrawFrame, sprite), 0, LOGO_PRI, LOGO_ATTR);
  control3d_pop();
  hw_stack_free(sizeof(LogoDrawFrame));
}
HW_EXPORT(8c01a4fe, logo_scene_draw(ARG0))
