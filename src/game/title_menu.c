/* Pen Pen port, S19 (u): the title scene's draw method and the draw of its intro / menu states (game/title.h's
 * TitleScene). The draw (vtable 0x8C23D96C, slot 2) binds the title's texlist and hands over by state: the fade-in
 * (game/title_fade.c), the logo (game/title_draw.c), or the menu below (states 2, 3 and 4). */
#include "hw.h"
#include "sdk.h"
#include "title.h"
#include "../core/ninja.h"
#include "../core/inline_b02.h"
#include "../core/motion_frame.h"

/* the menu's items: the first page's column (x, then the y of its four rows), as integers */
#define g_title_menu_x GUEST_VAR(int32_t, 0x8C070A2Cu)
#define TITLE_MENU_Y   0x8C070A30u /* int32_t[4] */
static inline int32_t title_menu_y(int32_t row) { return ((const int32_t *)hw_ptr(TITLE_MENU_Y))[row]; }

/* the sprites' texture animation table (NJS_TEXANIM of the title's sprites) */
#define TITLE_SPRITE_ANIM 0x8C08C824u
#define MENU_CONTROL3D 0x10
#define MENU_ATTR 0x22
#define MENU_PRI F32(-1.0f)
#define POINTER_DY 19 /* the pointer stands 19 pixels below a first-page row */

/* the second page's places (whole numbers, as floats) */
#define PAGE1_TITLE_X F32(366.0f)
#define PAGE1_TITLE_Y F32(256.0f)
#define PAGE1_ROW_X   F32(472.0f)
#define PAGE1_MARK_X  F32(514.0f)
#define PAGE1_ROW0_Y  F32(302.0f)
#define PAGE1_ROW1_Y  F32(345.0f)
#define PAGE1_ROW2_Y  F32(388.0f)
/* the second page's pointer places (x, then the y of each row) */
#define PAGE1_POINTER_X 0x1D8
#define PAGE1_POINTER_Y0 0x141
#define PAGE1_POINTER_Y1 0x16C
#define PAGE1_POINTER_Y2 0x197

/* the menu draw's frame on the guest stack (the original's: 0x50 bytes under its saved PR); the sprite's address is
 * handed to the draw, and the pointer's target is read from it even when no case set it (a choice not on a page) */
typedef struct TitleMenuFrame {
  NjSprite sprite;   /* 0x00 */
  int32_t target_y;  /* 0x20: the pointer's target */
  int32_t target_x;  /* 0x24 */
  f32 dim[4];        /* 0x28: a, b, g, r of an item not chosen (0.5) */
  f32 bright[4];     /* 0x38: a, b, g, r of the chosen item (1) */
  uint32_t attr;     /* 0x48 */
  gaddr scene;       /* 0x4C */
  uint32_t saved_pr; /* 0x50: the original's PR slot */
} TitleMenuFrame;
HW_OFFSET(TitleMenuFrame, target_y, 0x20);
HW_OFFSET(TitleMenuFrame, dim, 0x28);
HW_OFFSET(TitleMenuFrame, bright, 0x38);
HW_OFFSET(TitleMenuFrame, scene, 0x4C);
HW_SIZE(TitleMenuFrame, 0x54);

/* func_8C015AA8 (core/sprite_pointer.c, not declared in a header): the pointer from (x, y) toward a target */
static void pointer_draw(int32_t x, int32_t y, int32_t target_x, int32_t target_y) {
  CALL4(0x8C015AA8u, x, y, target_x, target_y);
}

static void menu_color(const f32 *c) { material_set_b072(c[3], c[2], c[1], c[0]); }

/* one layer of the back model: scaled in x and y by its scroll, at its own frame */
static void title_menu_layer(const TitleScene *t, f32 scale, gaddr frame) {
  matrix_push();
  matrix_scale(scale, scale, F32(1.0f));
  motion_draw_l3b(t->model[2], t->motion[2], frame, 1);
  matrix_pop();
}

/* a row of the second page: dim, or bright when it is the choice and enough controllers are in; its item, then the
 * mark item 6 at x = 514 */
static void title_menu_row(TitleMenuFrame *f, gaddr sp, const TitleScene *t, f32 y, int32_t choice, int32_t players,
                           int32_t item) {
  menu_color(f->dim);
  f->sprite.p.x = PAGE1_ROW_X;
  f->sprite.p.y = y;
  if (t->choice == choice && t->controllers >= players)
    menu_color(f->bright);
  njDrawSprite2D(sp, item, MENU_PRI, f->attr);
  f->sprite.p.x = PAGE1_MARK_X;
  njDrawSprite2D(sp, 6, MENU_PRI, f->attr);
}

/* was func_8C01CA24: the menu states' draw: the logo and the star at the star's place, the back model's two layers at
 * their alphas (3D flags 0x10 pushed), the pointer from the logo's position toward the chosen item, then the page's
 * items (dim, the chosen one bright; on the first page the third needs two controllers, on the second the rows need
 * 2, 3, 4) */
void title_draw_menu(gaddr scene) {
  gaddr sp = hw_stack_alloc(sizeof(TitleMenuFrame));
  TitleMenuFrame *f = (TitleMenuFrame *)hw_ptr(sp);
  TitleScene *t = title_at(scene);
  f->scene = scene;

  /* the logo and the star at the star's place */
  matrix_push();
  matrix_translate_v(member_addr(scene, TitleScene, mover));
  matrix_rotate_x(t->angle[0].value);
  matrix_rotate_y(t->angle[1].value);
  matrix_rotate_z(t->angle[2].value);
  nj_ModelL1(t->model[0]);
  nj_ModelL1(t->model[1]);
  matrix_pop();

  /* the back model's layers */
  control3d_push();
  control3d_set(MENU_CONTROL3D);
  material_set_b072(F32(1.0f), F32(1.0f), F32(1.0f), t->blend_a);
  title_menu_layer(t, t->scroll_a, member_addr(scene, TitleScene, frame[1]));
  material_set_b072(F32(1.0f), F32(1.0f), F32(1.0f), t->blend_b);
  title_menu_layer(t, t->scroll_b, member_addr(scene, TitleScene, frame[2]));

  f->attr = MENU_ATTR;
  for (int i = 3; i >= 0; i--)
    f->bright[i] = F32(1.0f);
  for (int i = 3; i >= 0; i--)
    f->dim[i] = F32(0.5f);

  /* the pointer's target: the chosen item (any other choice leaves the frame's words as they are) */
  switch (t->choice) {
  case 0:
  case 1:
  case 2:
  case 3:
    f->target_x = g_title_menu_x;
    f->target_y = title_menu_y(t->choice) + POINTER_DY;
    break;
  case 10:
    f->target_x = PAGE1_POINTER_X;
    f->target_y = PAGE1_POINTER_Y0;
    break;
  case 11:
    f->target_x = PAGE1_POINTER_X;
    f->target_y = PAGE1_POINTER_Y1;
    break;
  case 12:
    f->target_x = PAGE1_POINTER_X;
    f->target_y = PAGE1_POINTER_Y2;
    break;
  default:
    break;
  }
  int32_t from_y = f32_to_int(t->logo.position.y);
  int32_t from_x = f32_to_int(t->logo.position.x);
  pointer_draw(from_x, from_y, f->target_x, f->target_y);

  /* the items' sprite */
  f->sprite.tlist = t->texlist;
  f->sprite.tanim = TITLE_SPRITE_ANIM;
  f->sprite.ang = 0;
  f->sprite.sx = F32(1.0f);
  f->sprite.sy = F32(1.0f);
  f->sprite.p.z = F32(0.0f);

  if (t->menu_page == 0) {
    /* the first page: three rows of the column (the third needs two controllers) */
    f->sprite.p.x = f32_from_int(g_title_menu_x);
    f->sprite.p.y = f32_from_int(title_menu_y(0));
    menu_color(f->dim);
    if (t->choice == 0)
      menu_color(f->bright);
    njDrawSprite2D(sp, 0, MENU_PRI, f->attr);
    menu_color(f->dim);
    f->sprite.p.y = f32_from_int(title_menu_y(1));
    if (t->choice == 1)
      menu_color(f->bright);
    njDrawSprite2D(sp, 1, MENU_PRI, f->attr);
    menu_color(f->dim);
    f->sprite.p.y = f32_from_int(title_menu_y(2));
    if (t->choice == 2 && t->controllers >= 2)
      menu_color(f->bright);
    njDrawSprite2D(sp, 2, MENU_PRI, f->attr);
    menu_color(f->dim);
  } else if (t->menu_page == 1) {
    /* the second page: its title (bright), then three rows with their marks */
    f->sprite.p.x = PAGE1_TITLE_X;
    f->sprite.p.y = PAGE1_TITLE_Y;
    menu_color(f->bright);
    njDrawSprite2D(sp, 2, MENU_PRI, f->attr);
    title_menu_row(f, sp, t, PAGE1_ROW0_Y, 10, 2, 4);
    title_menu_row(f, sp, t, PAGE1_ROW1_Y, 11, 3, 5);
    title_menu_row(f, sp, t, PAGE1_ROW2_Y, 12, 4, 7);
    menu_color(f->dim);
  }
  control3d_pop();
  hw_stack_free(sizeof(TitleMenuFrame));
}
HW_EXPORT(8c01ca24, title_draw_menu(ARG0))

/* was func_8C01BB0C: the title scene's draw: its texlist bound, then by state the fade-in (0), the logo (1) or the
 * menu (2, 3, 4) */
void title_draw(gaddr scene) {
  TitleScene *t = title_at(scene);
  njSetTexture(t->texlist);
  switch (t->state) {
  case 0:
    title_draw_fade(scene);
    break;
  case 1:
    title_draw_logo(scene);
    break;
  case 2:
  case 3:
  case 4:
    title_draw_menu(scene);
    break;
  default:
    break;
  }
}
HW_EXPORT(8c01bb0c, title_draw(ARG0))
