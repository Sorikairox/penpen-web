/* Pen Pen port, S16 (b06): the draw of the actor of placement type 0x22 (Sign, game/actor.h; func_8C057ED2).
 *
 * The sign is drawn at its body's position turned by its angles (z, y, x); each mode draws its models of the
 * actor's list (modes 0-3 turned by the swing; mode 4 by its timer; mode 5 sets its model's child's z angle to the
 * swing and then draws its motes). Mode 6 is a 2D board: while it is in or out (state 0 or 1) three lines of text
 * in the game's language (a title, two lines whose size and place follow the board's x and y) and a
 * sprite (frame 11 in, 12 out). */
#include "hw.h"
#include "../core/sprite_text.h" /* text_draw (S16: hand-written) */
#include "sdk.h"
#include "../core/vec.h"
#include "../core/file.h"     /* NjObject */
#include "../core/ninja.h"    /* NjSprite, matrix_* */
#include "actor.h"
#include "actor_b06.h"   /* motes_draw (S17 b06) */

enum { SIGN_MODE_SWING_X = 0, SIGN_MODE_TWO_PARTS = 1, SIGN_MODE_SWING_Z = 2, SIGN_MODE_SWING_Y = 3,
       SIGN_MODE_STILL = 4, SIGN_MODE_MOTES = 5, SIGN_MODE_BOARD = 6 };

#define SIGN_STILL_FRAMES 15        /* mode 4: its first model before, its second after */
#define BOARD_TEXT_SIZE   F32(32.0f)
#define BOARD_TEXLIST     0x8C09703Cu /* the board sprite's texture list */
#define BOARD_TEXANIM     0x8C097044u /* its texture animations */
#define BOARD_SPRITE_X    F32(272.0f)
#define BOARD_SPRITE_Y    F32(396.0f)
#define BOARD_SPRITE_ATTR 0x30u

/* func_8C01529A (core/sprite_text.c, still translated): draws a string at (x, y) with the text style */
/* func_8C05769E (core/system.c, no header): the text's size (x, y) and a third setting */
static void text_size_set(f32 x, f32 y, f32 z) {
  FARGS3(x, y, z);
  CALL0(0x8C05769Eu);
}
/* func_8C05768C: the text's colour (red, green, blue, alpha) */
static void text_color_set(f32 r, f32 g, f32 b, f32 a) {
  FARGS4(r, g, b, a);
  CALL0(0x8C05768Cu);
}
/* func_8C019860 (game/save.c, no header): the game's language (0-6) */
static int32_t game_language(void) { return (int32_t)CALL0(0x8C019860u); }
/* func_8C04CA86: draw a MemberArray of motes */

/* the original's frame of the draw (0x34 bytes under r8 and PR): the board sprite */
typedef struct SignFrame {
  int32_t frame;         /* 0x00: the sprite's texture animation */
  NjSprite sprite;       /* 0x04 */
  f32 y;                 /* 0x24: a text line's place */
  f32 x;                 /* 0x28 */
  uint8_t rest[0x3C - 0x2C]; /* 0x2C: the board's x or y, the sign, r8, PR */
} SignFrame;
HW_OFFSET(SignFrame, sprite, 0x04);
HW_OFFSET(SignFrame, y, 0x24);
HW_SIZE(SignFrame, 0x3C);

/* the board's three lines and its sprite (mode 6) */
static void sign_draw_board(gaddr obj, gaddr frame) {
  const Sign *s = (const Sign *)hw_ptr(obj);
  SignFrame *f = (SignFrame *)hw_ptr(frame);
  text_size_set(BOARD_TEXT_SIZE, BOARD_TEXT_SIZE, F32(0.0f));
  text_color_set(F32(1.0f), F32(1.0f), F32(0.5f), F32(1.0f));
  switch (game_language()) { /* the title */
  case 0:
  case 1: text_draw(0xF0, 0x17C, 0x8C0884DCu); break;
  case 2: text_draw(0xF0, 0x17C, 0x8C0884ECu); break;
  case 3: text_draw(0xD2, 0x17C, 0x8C0884FCu); break;
  case 4: text_draw(0xF0, 0x17C, 0x8C088514u); break;
  case 5: text_draw(0x112, 0x17C, 0x8C088524u); break;
  case 6: text_draw(0xF0, 0x17C, 0x8C0884DCu); break;
  default: break;
  }

  /* the second line: sized by the board's x, centred on (180, 430) */
  f32 size = s->body.position.x;
  text_size_set(size, size, F32(0.0f));
  f->x = fsub(F32(180.0f), fdiv(size, fadd(F32(1.0f), F32(1.0f))));
  f->y = fsub(F32(430.0f), fdiv(size, fadd(F32(1.0f), F32(1.0f))));
  text_color_set(F32(0.0f), F32(1.0f), F32(0.5f), F32(1.0f));
  switch (game_language()) {
  case 0:
  case 1: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C088530u); break;
  case 2: text_draw(0x8C, f32_to_int(f->y), 0x8C08853Cu); break;
  case 3: text_draw(0x8C, f32_to_int(f->y), 0x8C08854Cu); break;
  case 4: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C08855Cu); break;
  case 5: text_draw(0x84, f32_to_int(f->y), 0x8C088568u); break;
  case 6: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C08857Cu); break;
  default: break;
  }

  /* the third line: sized by the board's y, centred on (354, 430) */
  size = s->body.position.y;
  text_size_set(size, size, F32(0.0f));
  f->x = fsub(F32(354.0f), fdiv(size, fadd(F32(1.0f), F32(1.0f))));
  f->y = fsub(F32(430.0f), fdiv(size, fadd(F32(1.0f), F32(1.0f))));
  text_color_set(F32(1.0f), F32(0.5f), F32(0.5f), F32(1.0f));
  switch (game_language()) {
  case 0:
  case 1: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C088588u); break;
  case 2: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C088598u); break;
  case 3: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C0885ACu); break;
  case 4: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C0885C0u); break;
  case 5: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C0885D0u); break;
  case 6: text_draw(f32_to_int(f->x), f32_to_int(f->y), 0x8C0885E4u); break;
  default: break;
  }

  f->sprite.tlist = BOARD_TEXLIST;
  f->sprite.tanim = BOARD_TEXANIM;
  f->sprite.ang = 0;
  f->sprite.sx = F32(1.0f);
  f->sprite.sy = F32(1.0f);
  f->sprite.p.x = BOARD_SPRITE_X;
  f->sprite.p.y = BOARD_SPRITE_Y;
  f->sprite.p.z = F32(0.0f);
  f->frame = s->state == 0 ? 11 : 12;
  njDrawSprite2D(member_addr(frame, SignFrame, sprite), f->frame, F32(0.0f), BOARD_SPRITE_ATTR);
}

/* was func_8C057ED2: the draw (see the top of the file) */
void sign_draw(gaddr obj) {
  Sign *s = (Sign *)hw_ptr(obj);
  gaddr frame = hw_stack_alloc(sizeof(SignFrame));
  matrix_push();
  matrix_translate_v(member_addr(obj, Sign, body));
  matrix_rotate_z(s->angle.z);
  matrix_rotate_y(s->angle.y);
  matrix_rotate_x(s->angle.x);
  switch (s->mode) {
  case SIGN_MODE_SWING_X:
    nj_ModelL1(actor_model(&s->base, 46));
    matrix_translate(F32_BITS(0x4080A3D7u) /* 4.02 */, F32(12.0f), F32(0.0f));
    matrix_rotate_x(s->swing.value);
    nj_ModelL1(actor_model(&s->base, 47));
    break;
  case SIGN_MODE_TWO_PARTS:
    matrix_rotate_y(s->swing.value);
    nj_ModelL1(actor_model(&s->base, 44));
    matrix_translate(F32_BITS(0x42B2CCCCu) /* 89.4 */, F32_BITS(0x42163D70u) /* 37.56 */, F32(0.0f));
    nj_ModelL1(actor_model(&s->base, 43));
    break;
  case SIGN_MODE_SWING_Z:
    matrix_rotate_z(s->swing.value);
    nj_ModelL1(actor_model(&s->base, 45));
    break;
  case SIGN_MODE_SWING_Y:
    matrix_rotate_y(s->swing.value);
    nj_ModelL1(actor_model(&s->base, 41));
    break;
  case SIGN_MODE_STILL:
    if (s->timer >= SIGN_STILL_FRAMES)
      nj_ModelL1(actor_model(&s->base, 40));
    else
      nj_ModelL1(actor_model(&s->base, 39));
    break;
  case SIGN_MODE_MOTES: {
    gaddr model = actor_model(&s->base, 4);
    NjObject *child = (NjObject *)hw_ptr(((const NjObject *)hw_ptr(model))->child);
    child->pos_ang_scl[5] = (uint32_t)f32_to_int(s->swing.value); /* its angle about z */
    nj_ModelL1(model);
    break;
  }
  case SIGN_MODE_BOARD:
    if (s->state > 1) break;
    sign_draw_board(obj, frame);
    break;
  default:
    break;
  }
  matrix_pop();
  if (s->mode == SIGN_MODE_MOTES)
    motes_draw(member_addr(obj, Sign, motes));
  hw_stack_free(sizeof(SignFrame));
}
HW_EXPORT(8c057ed2, sign_draw(ARG0))
