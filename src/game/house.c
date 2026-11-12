/* Pen Pen port, S14 (b02): PenPen House and the selections after it: the SetupMenu (game/menu.h) and the small
 * helpers of its unit (game/house.c).
 *
 * The SetupMenu is the scene a RaceSetup (game/race_scene.h) shows first. Each frame it reads player one's buttons
 * (A and B with a 30-frame repeat lock), runs the update of its state (0 the house, 1 and 2 two selections, 3 and 4
 * two more screens), and when a state asks to leave (5 to 8) tells the set-up what to show next; state 7 hands the
 * choices back to the set-up first. The VMU then shows each player's picture. A state's first update (phase 0)
 * starts its music, places the menu's sprite Bodies and lights the scene. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/inline.h"
#include "../core/ninja.h"
#include "sound.h"
#include "motion.h"
#include "menu.h"
#include "race_scene.h"

/* data of the original */
#define g_house_step  GUEST_VAR(f32, 0x8C0739FCu) /* 8.0: the push on place_a when the cursor moves */
#define g_house_lift  GUEST_VAR(f32, 0x8C073A00u) /* 2.0: the push on place_a when the house starts */
#define CAMERA_FOV    0x462AAAAAu                 /* 60 degrees in Ninja angle units, as a float */
#define LIGHT_BLUE    0x3ECCCCCCu                 /* 0.4f */
#define LIGHT_DOWN    0xBF3504F6u                 /* -sqrt(1/2) */
#define MATERIAL_DEFAULT 0x8C0932F0u              /* the NJS_ARGB a material starts from (overwritten) */

/* the text style of core/sprite_text.c (its .bss) */
#define TEXT_SPRITE_SMALL 0x8C1B3AE8u /* NJS_SPRITE */
#define g_text_scale_x    GUEST_VAR(f32, 0x8C1B3B28u)
#define g_text_scale_y    GUEST_VAR(f32, 0x8C1B3B2Cu)
#define g_text_style_30   GUEST_VAR(f32, 0x8C1B3B30u)
#define g_text_color      ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b */

/* the house's choices (cursor) and what each leads to */
enum { HOUSE_PLAY, HOUSE_CHOICE_1, HOUSE_CHOICE_2, HOUSE_CHOICE_3, HOUSE_GOOD_NIGHT };
/* what the set-up is asked to show when the menu leaves (RaceSetup.requested), by the leaving state */
enum { MENU_LEAVE_5 = 5, MENU_LEAVE_6 = 6, MENU_LEAVE_TO_RACE = 7, MENU_LEAVE_8 = 8 };

/* ---- still translated ---- */
static void text_style_reset(void) { CALL0(0x8C022A78u); } /* func_8C022A78 (core/sprite_text.c, no header) */
/* func_8C0107C4 (core/random.c, no header): a random float in [0, range) */
static f32 random_scaled(f32 range) {
  FARGS1(range);
  CALL0(0x8C0107C4u);
  return hw_fret();
}
/* func_8C011F50 (core/view_setup.c, no header): a view's camera from an eye and a target (roll and field of view
 * in angle units), applied at once or not */
static void view_set_camera(gaddr view, gaddr eye, gaddr target, f32 roll, f32 fov, int32_t apply) {
  FARGS2(roll, fov);
  CALL4(0x8C011F50u, view, eye, target, apply);
}
/* func_8C0117F8 (core/motion.c, no header): a shape motion drawn at *frame, its frame kept in range */
static int32_t shape_motion_draw(gaddr object, gaddr motion, gaddr shape, gaddr frame, int32_t loop) {
  return (int32_t)CALL5(0x8C0117F8u, object, motion, shape, frame, loop);
}
/* func_8C01529A: draws a string at (x, y) with the text style */
static int32_t text_draw(int32_t x, int32_t y, gaddr string) { return (int32_t)CALL3(0x8C01529Au, x, y, string); }
/* func_8C018C20 (game/vmu.c, no header): picture `picture` on the VMU of the player's slot */
static void vmu_show_picture(int32_t slot, uint32_t picture) { CALL2(0x8C018C20u, slot, picture); }
/* func_8C027DC2: builds (or resets) the menu's penguins, posing or not */
static void house_penguins_reset(gaddr menu, int32_t posing) { CALL2(0x8C027DC2u, menu, posing); }
static void house_draw_27aa8(gaddr menu, int32_t n) { CALL2(0x8C027AA8u, menu, n); }               /* func_8C027AA8 */
static void house_draw_27c58(gaddr menu, int32_t n, int32_t k) { CALL3(0x8C027C58u, menu, n, k); } /* func_8C027C58 */
static void house_penguins_update(gaddr menu) { CALL1(0x8C027E98u, menu); }                        /* func_8C027E98 */
/* the other states' updates and draws */
static void selection_update(gaddr menu) { CALL1(0x8C023D16u, menu); }  /* func_8C023D16: state 1 */
static void selection2_update(gaddr menu) { CALL1(0x8C02478Cu, menu); } /* func_8C02478C: state 2 */
static void state3_update(gaddr menu) { CALL1(0x8C025CECu, menu); }     /* func_8C025CEC */
static void state4_update(gaddr menu) { CALL1(0x8C0270D2u, menu); }     /* func_8C0270D2 */
static void selection2_draw(gaddr menu) { CALL1(0x8C0250E4u, menu); }   /* func_8C0250E4: states 2 and 7 */
static void state3_draw(gaddr menu) { CALL1(0x8C0265ECu, menu); }       /* func_8C0265EC */
static void state4_draw(gaddr menu) { CALL1(0x8C027234u, menu); }       /* func_8C027234 */

/* func_8C015AA8: the frame drawn between two corners (in pixels) */
static void frame_draw(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { CALL4(0x8C015AA8u, x0, y0, x1, y1); }
static void menu_draw_27ef0(gaddr menu) { CALL1(0x8C027EF0u, menu); } /* func_8C027EF0: drawn last by each screen */

/* ---- small helpers of the unit ---- */

/* was func_8C02257A (and func_8C028E32, not exported here): a thunk to the C runtime's one-float function
 * func_8C062988 (the callers use it as floorf) */
f32 rt_floor(f32 x) {
  FARGS1(x);
  CALL0(0x8C062988u);
  return hw_fret();
}
HW_EXPORT(8c02257a, RETF(rt_floor(FARG0)))

/* was func_8C022730: view `index`'s camera from an eye and a target (not applied yet) */
void house_view_camera(int32_t index, gaddr eye, gaddr target, f32 roll, f32 fov) {
  view_set_camera(hw_addr(&g_views[index]), eye, target, roll, fov, 0);
}
HW_EXPORT(8c022730, house_view_camera((int32_t)ARG0, ARG1, ARG2, FARG0, FARG1))

/* was func_8C0227FC: the constant material (r, g, b, a): an NJS_ARGB on the stack, copied from the default one and
 * then filled */
typedef struct NjArgb {
  f32 a, r, g, b;
} NjArgb;
HW_SIZE(NjArgb, 16);
void material_set(f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(sizeof(NjArgb));
  memmove(hw_ptr(argb), hw_ptr(MATERIAL_DEFAULT), sizeof(NjArgb));
  NjArgb *c = (NjArgb *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(sizeof(NjArgb));
}
HW_EXPORT(8c0227fc, material_set(FARG0, FARG1, FARG2, FARG3))

/* was func_8C022904: a shape motion of a {motion, shape} pair drawn at *frame */
typedef struct ShapeMotion {
  gaddr motion; /* 0x00 */
  gaddr shape;  /* 0x04 */
} ShapeMotion;
int32_t shape_motion_pair_draw(gaddr object, gaddr pair, gaddr frame, int32_t loop) {
  const ShapeMotion *p = (const ShapeMotion *)hw_ptr(pair);
  return shape_motion_draw(object, p->motion, p->shape, frame, loop);
}
HW_EXPORT(8c022904, RET(shape_motion_pair_draw(ARG0, ARG1, ARG2, (int32_t)ARG3)))

/* was func_8C022AAE: draws a text item: its colour, its scale (in 32-pixel units), its third style value, then its
 * string at its position */
typedef struct TextItem {
  f32 r, g, b, a;   /* 0x00 */
  f32 x, y;         /* 0x10 */
  f32 sx, sy;       /* 0x18: in pixels (32 = scale 1) */
  f32 style;        /* 0x20: the text style's third value */
  gaddr string;     /* 0x24 */
} TextItem;
HW_OFFSET(TextItem, string, 0x24);
int32_t text_item_draw(gaddr item) {
  const TextItem *t = (const TextItem *)hw_ptr(item);
  f32 *color = g_text_color;
  NjSprite *small = (NjSprite *)hw_ptr(TEXT_SPRITE_SMALL);
  color[1] = t->r;
  color[2] = t->g;
  color[3] = t->b;
  color[0] = t->a;
  f32 sx = fdiv(t->sx, F32(32.0f)), sy = fdiv(t->sy, F32(32.0f));
  g_text_scale_x = sx;
  small->sx = sx;
  g_text_scale_y = sy;
  small->sy = sy;
  g_text_style_30 = t->style;
  return text_draw(f32_to_int(t->x), f32_to_int(t->y), t->string);
}
HW_EXPORT(8c022aae, RET(text_item_draw(ARG0)))

/* ---- the SetupMenu ---- */

static inline RaceSetup *menu_setup(const SetupMenu *m) { return race_setup_at(m->setup); }

/* func_8C0229BE: a new state, from its first phase */
static void menu_set_state(gaddr menu, int32_t state) {
  SetupMenu *m = setup_menu_at(menu);
  m->state = state;
  m->phase = 0;
}

/* the light every state sets up when it starts */
static void menu_lights(void) {
  view_set_light_intensity(F32(1.0f), F32(1.0f), F32_BITS(LIGHT_BLUE), 0);
  view_set_light_dir(F32(0.0f), F32_BITS(LIGHT_DOWN), F32_BITS(LIGHT_DOWN), 0);
  view_set_light_color(F32(1.0f), F32(1.0f), F32(1.0f), 0);
}

/* one of the house's tracks: 6, 7 or 8 */
static void menu_music_random(void) { music_play(f32_to_int(fadd(random_scaled(F32(3.0f)), F32(6.0f))), 0, 1); }

/* was func_8C0232DC: the destructor's body: the owner's textures and file, the penguins (deleted), their file, the
 * select file's two texture lists and the file; the text style back to its default, the music stopped, the drive
 * left to stop; then the Task's (empty) destructor */
void setup_menu_destroy(gaddr menu) {
  njReleaseTexture(setup_menu_at(menu)->owner_texlist);
  file_free(setup_menu_at(menu)->owner_file);
  for (int variant = 0; variant < 2; variant++)
    for (int character = 0; character < 8; character++) {
      gaddr penguin = setup_menu_at(menu)->penguins[character][variant];
      if (penguin)
        motion_player_delete(penguin, 1);
    }
  file_free(setup_menu_at(menu)->penguin_file);
  njReleaseTexture(setup_menu_at(menu)->map_texlist);
  njReleaseTexture(setup_menu_at(menu)->texlist);
  file_free(setup_menu_at(menu)->select_file);
  text_style_reset();
  music_stop();
  cd_wait_stopped();
  method_empty();
}
HW_EXPORT(8c0232dc, setup_menu_destroy(ARG0))

/* was func_8C0233B4: the deleting destructor */
gaddr setup_menu_delete(gaddr menu, int32_t free_memory) {
  setup_menu_destroy(menu);
  if (free_memory)
    syFree(menu);
  return menu;
}
HW_EXPORT(8c0233b4, RET(setup_menu_delete(ARG0, (int32_t)ARG1)))

/* was func_8C02362E: the house's start: a random track, the sprite Bodies at rest and placed by entry 0, the cursor
 * on the first choice, the light */
void house_enter(gaddr menu) {
  menu_music_random();
  icon_bodies_init(menu);
  icon_place(menu, 0);
  setup_menu_at(menu)->cursor = 0;
  menu_lights();
}
HW_EXPORT(8c02362e, house_enter(ARG0))

/* was func_8C023BEE: the first selection's start: a random track unless the set-up's mode is 0, the sprite Bodies
 * at rest and placed by entry 0, the players' words from the set-up, the order 0..7 and nothing picked yet, the
 * light */
void selection_enter(gaddr menu) {
  if (menu_setup(setup_menu_at(menu))->mode != 0)
    menu_music_random();
  icon_bodies_init(menu);
  icon_place(menu, 0);
  for (int i = 0; i < 8; i++) {
    SetupMenu *m = setup_menu_at(menu);
    m->player_a[i] = menu_setup(m)->words68[i];
    m->player_b[i] = menu_setup(m)->words88[i];
    m->order[i] = i;
    m->picked[i] = 0;
  }
  menu_lights();
}
HW_EXPORT(8c023bee, selection_enter(ARG0))

/* was func_8C0246FE: the second selection's start: track 9, the sprite Bodies at rest and placed by entry 1, the
 * four fades at 0, the value at 0x1E0 at 0, the light */
void selection2_enter(gaddr menu) {
  music_play(9, 0, 1);
  icon_bodies_init(menu);
  icon_place(menu, 1);
  for (int i = 0; i < 4; i++)
    setup_menu_at(menu)->fades[i].value = F32(0.0f);
  setup_menu_at(menu)->f1e0 = F32(0.0f);
  menu_lights();
}
HW_EXPORT(8c0246fe, selection2_enter(ARG0))

/* was func_8C02368E: the house (state 0): started on its first frame (with a lift of the placed sprite); up and
 * down move the cursor round the choices (with a click and a push on the sprite), A takes the choice, B leaves
 * (state 6); then the camera, the sprites and the penguins */
void house_update(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  if (m->phase == 0) {
    house_enter(menu);
    m->phase++;
    f32_add_to(g_house_lift, member_addr(menu, SetupMenu, place_a.force.y));
  }
  if (m->pressed[MENU_PRESS_UP]) {
    sound_play_port_sound(0);
    m->cursor -= 1;
    if (m->cursor < 0)
      m->cursor = m->cursor_count - 1;
    f32_add_to(fneg(g_house_step), member_addr(menu, SetupMenu, place_a.force.x));
  } else if (m->pressed[MENU_PRESS_DOWN]) {
    sound_play_port_sound(0);
    m->cursor += 1;
    if (m->cursor >= m->cursor_count)
      m->cursor = 0;
    f32_add_to(fneg(g_house_step), member_addr(menu, SetupMenu, place_a.force.x));
  } else if (m->pressed[MENU_PRESS_A]) {
    switch (m->cursor) {
    case HOUSE_PLAY:
      sound_play_port_sound(1);
      menu_set_state(menu, 1);
      break;
    case HOUSE_CHOICE_1:
      sound_play_port_sound(1);
      house_penguins_reset(menu, 0);
      menu_set_state(menu, 3);
      break;
    case HOUSE_CHOICE_2:
      sound_play_port_sound(1);
      house_penguins_reset(menu, 0);
      menu_set_state(menu, 4);
      break;
    case HOUSE_CHOICE_3:
      sound_play_port_sound(2);
      menu_set_state(menu, MENU_LEAVE_5);
      break;
    case HOUSE_GOOD_NIGHT:
      sound_play_port_sound(2);
      menu_set_state(menu, MENU_LEAVE_8);
      break;
    default:
      break;
    }
  } else if (m->pressed[MENU_PRESS_B]) {
    menu_set_state(menu, MENU_LEAVE_6);
  }

  /* the camera: from (0, 21.18, 50.25) at (0, 20.86, 49.30) */
  gaddr frame = hw_stack_alloc(0x18);
  gaddr target = frame, eye = frame + (gaddr)sizeof(Vec3);
  vec3_set(F32(0.0f), F32_BITS(0x41A6E739u), F32_BITS(0x424532CCu), target);
  vec3_set(F32(0.0f), F32_BITS(0x41A96A7Eu), F32_BITS(0x4248FEFAu), eye);
  house_view_camera(0, eye, target, F32(0.0f), F32_BITS(CAMERA_FOV));
  hw_stack_free(0x18);
  house_draw_27aa8(menu, 0);
  house_draw_27c58(menu, 0, setup_menu_at(menu)->cursor);
  house_penguins_update(menu);
}
HW_EXPORT(8c02368e, house_update(ARG0))

/* was func_8C0233EA: the update: the buttons (A and B locked for 30 frames once pressed), the state's update or what
 * a leaving state asks of the set-up, then each player's VMU picture by the set-up's mode */
void setup_menu_update(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  m->repeat_lock -= 1;
  if (m->repeat_lock < 0) {
    m->pressed[MENU_PRESS_A] = (uint32_t)pad_pressed(&m->pad, PAD_A);
    m->pressed[MENU_PRESS_B] = (uint32_t)pad_pressed(&m->pad, PAD_B);
    if (m->pressed[MENU_PRESS_A] || m->pressed[MENU_PRESS_B])
      m->repeat_lock = MENU_REPEAT_FRAMES;
  } else {
    m->pressed[MENU_PRESS_RIGHT] = 0;
    m->pressed[MENU_PRESS_B] = 0;
    m->pressed[MENU_PRESS_A] = 0;
  }
  m->pressed[MENU_PRESS_RIGHT] = (uint32_t)pad_pressed(&m->pad, PAD_RIGHT);
  m->pressed[MENU_PRESS_LEFT] = (uint32_t)pad_pressed(&m->pad, PAD_LEFT);
  m->pressed[MENU_PRESS_UP] = (uint32_t)pad_pressed(&m->pad, PAD_UP);
  m->pressed[MENU_PRESS_DOWN] = (uint32_t)pad_pressed(&m->pad, PAD_DOWN);

  switch (m->state) {
  case 0: house_update(menu); break;
  case 1: selection_update(menu); break;
  case 2: selection2_update(menu); break;
  case 3: state3_update(menu); break;
  case 4: state4_update(menu); break;
  case MENU_LEAVE_5: menu_setup(m)->requested = 3; break;
  case MENU_LEAVE_6: menu_setup(m)->requested = 6; break;
  case MENU_LEAVE_8: menu_setup(m)->requested = 4; break;
  case MENU_LEAVE_TO_RACE:
    for (int32_t i = 0; i < setup_menu_at(menu)->player_count; i++) {
      menu_setup(m)->words68[i] = m->player_a[i];
      menu_setup(m)->words88[i] = m->player_b[i];
    }
    menu_setup(m)->course = m->course;
    menu_setup(m)->variant = m->variant;
    menu_setup(m)->requested = 1;
    break;
  default:
    break;
  }

  /* the VMU pictures: one per player of the mode (3 = four players, down to 0 and 4 = one) */
  switch (menu_setup(m)->mode) {
  case 3: vmu_show_picture(0x13, m->player_a[3]); /* fall through */
  case 2: vmu_show_picture(0x0D, m->player_a[2]); /* fall through */
  case 1: vmu_show_picture(0x07, m->player_a[1]); /* fall through */
  case 0:
  case 4: vmu_show_picture(0x01, m->player_a[0]); break;
  default: break;
  }
}
HW_EXPORT(8c0233ea, setup_menu_update(ARG0))

/* the sprite cells of the house's screen */
enum {
  CELL_BACK = 0x19, CELL_FRONT = 0x1A,
  CELL_SLOT_FIRST = 0x23,   /* + 2 * (slot % 4): a player's slot (the picked one one cell on) */
  CELL_SLOT_PANEL_A = 0x21, CELL_SLOT_PANEL_B = 0x22, CELL_SLOT_TAG_A = 0x2B, CELL_SLOT_TAG_B = 0x2C,
};
/* the house's choices, top to bottom: their label cells */
static const int32_t house_choice_cell[5] = {0x1C, 0x1D, 0x1F, 0x1E, 0x20};
#define HOUSE_CHOICES_SHORT 4 /* the fifth only when cursor_count is 5 or more */
#define LIGHT_GREY   0x3F666666u /* 0.9f */
#define LIGHT_YELLOW 0x3E999999u /* 0.3f */

static void menu_sprite_draw(gaddr menu, int32_t cell, f32 priority, uint32_t attr) {
  njDrawSprite2D(member_addr(menu, SetupMenu, sprite), cell, priority, attr);
}

/* was func_8C02386C: the house's screen (from its second phase): the backdrop's two cells, the menu's icon with the
 * course map's textures, the choices' labels in black, then again in colour (the cursor's choice yellow, the others
 * pale blue), a frame between the two placed Bodies, the closing draw */
void house_draw(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  if (m->phase == 0)
    return;
  m->sprite.tlist = m->texlist;
  m->sprite.p.x = F32(0.0f);
  m->sprite.p.y = F32(0.0f);
  menu_sprite_draw(menu, CELL_BACK, F32(-1000.0f), 0);
  menu_sprite_draw(menu, CELL_FRONT, F32(-50.0f), 0x20);
  m->sprite.tlist = m->map_texlist;
  icon_draw(menu, 0);
  control3d_push();
  control3d_set(0x10);

  int32_t choices = m->cursor_count >= 5 ? 5 : HOUSE_CHOICES_SHORT;
  /* the labels' shadow, one pixel down and right of them */
  material_set(F32(0.0f), F32(0.0f), F32(0.0f), F32(1.0f));
  m->sprite.p.x = F32(61.0f);
  m->sprite.p.y = F32(41.0f);
  m->sprite.sx = F32(1.0f);
  m->sprite.sy = F32(1.0f);
  for (int32_t k = 0; k < choices; k++) {
    if (k > 0)
      m->sprite.p.y = fadd(m->sprite.p.y, F32(32.0f));
    menu_sprite_draw(menu, house_choice_cell[k], F32(-2.0f), 0x22);
  }
  /* the labels */
  m->sprite.p.x = F32(58.0f);
  m->sprite.p.y = F32(38.0f);
  for (int32_t k = 0; k < choices; k++) {
    if (m->cursor == k)
      material_set(F32(1.0f), F32(1.0f), F32_BITS(LIGHT_YELLOW), F32(1.0f));
    else
      material_set(F32(0.0f), F32_BITS(LIGHT_GREY), F32(1.0f), F32(1.0f));
    menu_sprite_draw(menu, house_choice_cell[k], F32(-1.5f), 0x22);
    if (k < HOUSE_CHOICES_SHORT)
      m->sprite.p.y = fadd(m->sprite.p.y, F32(32.0f));
  }
  global_color_set(F32(1.0f), F32(1.0f), F32(1.0f), F32(1.0f));
  /* (the corners are read b.y, b.x, a.y, a.x) */
  int32_t by = f32_to_int(m->place_b.position.y), bx = f32_to_int(m->place_b.position.x);
  int32_t ay = f32_to_int(m->place_a.position.y), ax = f32_to_int(m->place_a.position.x);
  frame_draw(ax, ay, bx, by);
  control3d_pop();
  menu_draw_27ef0(menu);
}
HW_EXPORT(8c02386c, house_draw(ARG0))

/* was func_8C0244DE: the first selection's screen (from its second phase): the backdrop, the icon with the course
 * map's textures, then for each player of the set-up's mode (1 to 4) its slot at its Body's position: tinted green
 * once it has picked (player_b), faded by its alpha; three cells, the set chosen by whether its order is past 4 */
void selection_draw(gaddr menu) {
  if (setup_menu_at(menu)->phase == 0)
    return;
  backdrop_draw(menu);
  SetupMenu *m = setup_menu_at(menu);
  m->sprite.tlist = m->map_texlist;
  icon_draw(menu, 0);
  int32_t players;
  switch (menu_setup(m)->mode) {
  case 1: players = 2; break;
  case 2: players = 3; break;
  case 3: players = 4; break;
  default: players = 1; break;
  }
  control3d_push();
  control3d_set(0x10);
  for (int32_t i = 0; i < players; i++) {
    f32 alpha = m->alphas[i].value;
    if (m->player_b[i] == 0)
      material_set(F32(1.0f), F32(1.0f), F32(1.0f), alpha);
    else
      material_set(F32(0.5f), F32(1.0f), F32(0.5f), alpha);
    memmove(&m->sprite.p, &m->slots[i].position, sizeof(Vec3));
    int32_t cell = CELL_SLOT_FIRST + 2 * (i % 4);
    if (m->order[i] > 4) {
      menu_sprite_draw(menu, cell + 1, F32(-5.0f), 0x20);
      menu_sprite_draw(menu, CELL_SLOT_PANEL_B, F32(-6.0f), 0x20);
      menu_sprite_draw(menu, CELL_SLOT_TAG_B, F32(-7.0f), 0x22);
    } else {
      menu_sprite_draw(menu, cell, F32(-5.0f), 0x20);
      menu_sprite_draw(menu, CELL_SLOT_PANEL_A, F32(-6.0f), 0x20);
      menu_sprite_draw(menu, CELL_SLOT_TAG_A, F32(-7.0f), 0x22);
    }
  }
  control3d_pop();
  menu_draw_27ef0(menu);
}
HW_EXPORT(8c0244de, selection_draw(ARG0))

/* was func_8C0235D8: the draw: the state's screen (a leaving state keeps drawing the screen it left) */
void setup_menu_draw(gaddr menu) {
  switch (setup_menu_at(menu)->state) {
  case 0: case MENU_LEAVE_5: house_draw(menu); break;
  case 1: case MENU_LEAVE_6: selection_draw(menu); break;
  case 2: case MENU_LEAVE_TO_RACE: selection2_draw(menu); break;
  case 3: state3_draw(menu); break;
  case 4: state4_draw(menu); break;
  default: break;
  }
}
HW_EXPORT(8c0235d8, setup_menu_draw(ARG0))
