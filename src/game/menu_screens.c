/* Pen Pen port, S15 (b03): more of the SetupMenu's methods (game/menu.h): its sprite Bodies pulled toward the
 * entries of two tables, the penguins it shows (built, reset and advanced), the course's mascot model (its random
 * motions and their sounds), the three-digit numbers it draws, a frame between its two placed Bodies, and the update
 * of state 4 (player one looks through the penguins). game/menu_screens.c
 *
 * The menu's sprite Bodies: icon_position / icon_scale follow an IconTarget (the menu's icon), place_a / place_b an
 * IconPlace (core of a frame drawn between them; left and right push place_b sideways). */
#include "hw.h"
#include "../core/sprite_text.h" /* text_draw (S16: hand-written) */
#include "sdk.h"
#include "../core/vec.h"
#include "../core/pad.h"
#include "../core/ninja.h"
#include "../core/inline.h"
#include "../core/file.h"
#include "sound.h"
#include "motion.h"
#include "menu.h"

/* functions of other files that no header declares yet */
void body_pull_xyz(gaddr body, f32 x, f32 y, f32 z, f32 rate);                 /* core/accessors.c */
void text_draw_digit(int32_t x, int32_t y, int32_t value, uint32_t attr);      /* core/sprite_text.c */
void text_style_reset(void);                                                   /* core/sprite_text.c */
f32 random_timer_scaled(f32 range);                                            /* core/random.c */

/* ---- still translated ---- */
/* func_8C015AA8: the frame drawn between two corners (in pixels) */
static void frame_draw(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { CALL4(0x8C015AA8u, x0, y0, x1, y1); }
/* func_8C019778 (game/save.c): whether a penguin can be chosen (all but the last one until the save unlocks it) */
static int32_t character_available(int32_t character) { return (int32_t)CALL1(0x8C019778u, character); }

/* ---- data of the original ---- */

/* where the menu's icon goes, per entry: its position Body's target and its scale Body's target */
typedef struct IconTarget {
  Vec3 position; /* 0x00: x, y, priority */
  Vec3 scale;    /* 0x0C */
} IconTarget;
HW_SIZE(IconTarget, 0x18);
#define g_icon_targets ((const IconTarget *)hw_ptr(0x8C073474u))

/* where place_a and place_b go, per entry (the same table icon_place uses): place_a at (x, y + dy * k, z), place_b
 * that plus (dx2, dy2) */
typedef struct IconPlace {
  f32 x, y, z;   /* 0x00 */
  f32 dy;        /* 0x0C: per step k */
  f32 dx2, dy2;  /* 0x10: place_b from place_a */
} IconPlace;
HW_SIZE(IconPlace, 0x18);
#define g_icon_places ((const IconPlace *)hw_ptr(0x8C073C3Cu))

#define g_house_step GUEST_VAR(f32, 0x8C0739FCu) /* 8.0: the push on place_b when left / right is held */

/* the mascot's sounds: per course, one per motion */
#define MASCOT_MOTIONS 4
#define g_mascot_sounds ((const int32_t (*)[MASCOT_MOTIONS])hw_ptr(0x8C0737FCu))
#define MASCOT_SOUND_PORT 4

#define MENU_PENGUIN_SIZE   0xF8u /* a menu penguin (game/motion.h) */
#define PENGUIN_FILE_ENTRY  0x30u /* the penguins' model file: an entry per character */
#define MENU_CHARACTERS     8
#define MENU_VARIANTS       2

#define PULL_HALF   F32(0.5f)
#define PULL_PLACE  0x3DCCCCCCu /* 0.1f */
#define PULL_MASCOT 0x3D4CCCCCu /* 0.05f */

static inline const Pad *menu_pad(gaddr menu) { return &setup_menu_at(menu)->pad; }

/* ---- the sprite Bodies ---- */

/* was func_8C027AA8: the icon's Bodies pulled halfway toward target n (position and scale), then stepped */
void icon_follow(gaddr menu, int32_t n) {
  const IconTarget *t = &g_icon_targets[n];
  gaddr position = member_addr(menu, SetupMenu, icon_position), scale = member_addr(menu, SetupMenu, icon_scale);
  body_pull_xyz(position, t->position.x, t->position.y, t->position.z, PULL_HALF);
  body_pull_xyz(scale, t->scale.x, t->scale.y, t->scale.z, PULL_HALF);
  body_step(position);
  body_step(scale);
}
HW_EXPORT(8c027aa8, icon_follow(ARG0, (int32_t)ARG1))

/* was func_8C027C58: place_b pushed left / right while player one holds left / right; then place_a and place_b
 * pulled (by 0.1) toward entry n of the place table, step k down, and both stepped */
void place_follow(gaddr menu, int32_t n, int32_t k) {
  gaddr a = member_addr(menu, SetupMenu, place_a), b = member_addr(menu, SetupMenu, place_b);
  if (pad_held(menu_pad(menu), 0x40))
    f32_add_to(fneg(g_house_step), member_addr(b, Body, force.x));
  if (pad_held(menu_pad(menu), 0x80))
    f32_add_to(g_house_step, member_addr(b, Body, force.x));
  const IconPlace *p = &g_icon_places[n];
  f32 y = fmac(f32_from_int(k), p->dy, p->y);
  body_pull_xyz(a, p->x, y, p->z, F32_BITS(PULL_PLACE));
  y = fmac(f32_from_int(k), p->dy, p->y);
  body_pull_xyz(b, fadd(p->dx2, p->x), fadd(y, p->dy2), p->z, F32_BITS(PULL_PLACE));
  body_step(a);
  body_step(b);
}
HW_EXPORT(8c027c58, place_follow(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C027D5C: in white, the frame from place_a's position to place_b's */
void place_frame_draw(gaddr menu) {
  const SetupMenu *m = setup_menu_at(menu);
  global_color_set(F32(1.0f), F32(1.0f), F32(1.0f), F32(1.0f));
  int32_t y1 = f32_to_int(m->place_b.position.y);
  int32_t x1 = f32_to_int(m->place_b.position.x);
  int32_t y0 = f32_to_int(m->place_a.position.y);
  int32_t x0 = f32_to_int(m->place_a.position.x);
  frame_draw(x0, y0, x1, y1);
}
HW_EXPORT(8c027d5c, place_frame_draw(ARG0))

/* ---- the penguins ---- */

/* was func_8C027DC2: every penguin (character, variant) built if it is not yet (a menu penguin from the model file's
 * entry of its character; 0 when the memory runs out), then reset, posing or not; the penguins' word and clock 0 */
void menu_penguins_reset(gaddr menu, int32_t posing) {
  SetupMenu *m = setup_menu_at(menu);
  for (int32_t variant = 0; variant < MENU_VARIANTS; variant++) {
    for (int32_t character = 0; character < MENU_CHARACTERS; character++) {
      if (m->penguins[character][variant] == 0) {
        gaddr block = syMalloc(MENU_PENGUIN_SIZE);
        gaddr penguin = 0;
        if (block != 0)
          penguin = menu_penguin_construct(block, character, variant,
                                           m->penguin_file + (gaddr)character * PENGUIN_FILE_ENTRY, 0);
        m->penguins[character][variant] = penguin;
      }
      menu_penguin_reset(m->penguins[character][variant], posing);
    }
  }
  m->held_index = 0;
  m->held_frame = F32(0.0f);
}
HW_EXPORT(8c027dc2, menu_penguins_reset(ARG0, (int32_t)ARG1))

/* was func_8C027E98: the first variant of every penguin advanced one frame; the clock + 1 */
void menu_penguins_advance(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  for (int32_t character = 0; character < MENU_CHARACTERS; character++)
    motion_player_advance(m->penguins[character][0]);
  m->held_frame = fadd(m->held_frame, F32(1.0f));
}
HW_EXPORT(8c027e98, menu_penguins_advance(ARG0))

/* ---- the course's mascot ---- */

static void mascot_sound(const SetupMenu *m, int32_t entry) {
  sound_play(MASCOT_SOUND_PORT, g_mascot_sounds[m->course][entry], 0);
}

/* a random motion in [first, first + count) other than the previous one, which it becomes */
static void mascot_pick(SetupMenu *m, f32 count, int32_t first) {
  do {
    m->motion = f32_to_int(random_timer_scaled(count)) + first;
  } while (m->prev_motion == m->motion);
  m->prev_motion = m->motion;
}

/* was func_8C028404: the mascot back to its start: motion 0, frame 0, angle 0, its Body reset (damping 0.75) at
 * (60, -20, -150), not looping; with `play`, a random motion (course 0: 1 or 2; course 2: 0 to 2; else 0 to 3) and
 * its sound */
void mascot_reset(gaddr menu, int32_t play) {
  SetupMenu *m = setup_menu_at(menu);
  gaddr body = member_addr(menu, SetupMenu, model_body);
  m->motion = 0;
  m->prev_motion = 0;
  m->model_frame = F32(0.0f);
  m->model_angle = F32(0.0f);
  body_reset(body);
  vec3_fill(member_addr(body, Body, damping), F32(0.75f));
  vec3_set(F32(60.0f), F32(-20.0f), F32(-150.0f), body);
  m->model_loop = 0;
  if (play) {
    if (m->course == 0)
      mascot_pick(m, F32(2.0f), 1);
    else if (m->course == 2)
      mascot_pick(m, F32(3.0f), 0);
    else
      mascot_pick(m, F32(4.0f), 0);
    mascot_sound(m, m->motion);
  }
  m->model_frame = F32(0.0f);
  m->model_angle = F32(0.0f);
}
HW_EXPORT(8c028404, mascot_reset(ARG0, (int32_t)ARG1))

/* was func_8C0285CA: the mascot's update: its frame + 1, its Body pulled (by 0.05) toward the origin and stepped,
 * looping; past the motion's last frame the frame goes back to 0 and the next motion starts with its sound:
 * course 0 plays 0, then 3 (silent), then 1, 2, 0, ...; course 2 cycles 0 to 2 with the sounds of 2, 1, 0; the
 * others cycle 0 to 3 */
void mascot_update(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  m->model_frame = fadd(m->model_frame, F32(1.0f));
  gaddr origin = hw_stack_alloc(sizeof(Vec3));
  vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), origin);
  const MotionEntry *entry = &((const MotionEntry *)hw_ptr(m->motion_tables[m->course]))[m->motion];
  gaddr motion = entry->motion;
  body_pull(F32_BITS(PULL_MASCOT), member_addr(menu, SetupMenu, model_body), origin);
  body_step(member_addr(menu, SetupMenu, model_body));
  m->model_loop = 1;
  f32 last = f32_from_uint((uint32_t)(((const NjMotion *)hw_ptr(motion))->nb_frames - 1));
  if (fgt(m->model_frame, last)) {
    m->model_frame = F32(0.0f);
    if (m->course == 0) {
      if (m->motion == 0) {
        m->motion = 3;
      } else if (m->motion == 3) {
        m->motion = 1;
        mascot_sound(m, m->motion);
      } else {
        m->motion += 1;
        if (m->motion > 2) m->motion = 0;
        mascot_sound(m, m->motion);
      }
    } else if (m->course == 2) {
      m->motion += 1;
      if (m->motion > 2) m->motion = 0;
      switch (m->motion) {
      case 0: mascot_sound(m, 2); break;
      case 1: mascot_sound(m, m->motion); break;
      case 2: mascot_sound(m, 0); break;
      }
    } else {
      m->motion += 1;
      if (m->motion > 3) m->motion = 0;
      mascot_sound(m, m->motion);
    }
  }
  hw_stack_free(sizeof(Vec3));
}
HW_EXPORT(8c0285ca, mascot_update(ARG0))

/* ---- numbers ---- */

#define DIGIT_ATTR 0x22

/* was func_8C028C74: `value` (0 to 999) in three digits at (x, y), 24 pixels apart (the tens and units drawn 5 and
 * 10 pixels to the left of that), without leading zeros (the tens drawn when there are hundreds or the tens digit is
 * not 0); in the 3D control mode 0x10, restored after. The first argument is not used. */
void menu_number_draw(gaddr unused, int32_t x, int32_t y, int32_t value) {
  control3d_push();
  control3d_set(0x10);
  int32_t hundreds = value / 100;
  if (hundreds != 0) {
    text_draw_digit(x, y, hundreds, DIGIT_ATTR);
    text_draw_digit(f32_to_int(fadd(f32_from_int(x + 24), F32(-5.0f))), y, value / 10, DIGIT_ATTR);
  } else {
    int32_t tens = value / 10 % 10;
    if (tens != 0)
      text_draw_digit(f32_to_int(fadd(f32_from_int(x + 24), F32(-5.0f))), y, tens, DIGIT_ATTR);
  }
  text_draw_digit(f32_to_int(fadd(f32_from_int(x + 48), F32(-10.0f))), y, value % 10, DIGIT_ATTR);
  control3d_pop();
}
HW_EXPORT(8c028c74, menu_number_draw(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3))

/* ---- a constant material ---- */

#define MATERIAL_DEFAULT_2 0x8C096030u /* the NJS_ARGB this copy of the material setter starts from */
typedef struct ArgbColor {
  f32 a, r, g, b;
} ArgbColor;
HW_SIZE(ArgbColor, 16);

/* was func_8C028F08 (its tail func_8C028F16 is not a function of its own): the constant material (r, g, b, a): an
 * NJS_ARGB on the stack, copied from a default one and then filled (house.c's material_set, from another default) */
void material_set_2(f32 r, f32 g, f32 b, f32 a) {
  gaddr argb = hw_stack_alloc(sizeof(ArgbColor));
  memmove(hw_ptr(argb), hw_ptr(MATERIAL_DEFAULT_2), sizeof(ArgbColor));
  ArgbColor *c = (ArgbColor *)hw_ptr(argb);
  c->a = a;
  c->r = r;
  c->g = g;
  c->b = b;
  njSetConstantMaterial(argb);
  hw_stack_free(sizeof(ArgbColor));
}
HW_EXPORT(8c028f08, material_set_2(FARG0, FARG1, FARG2, FARG3))

/* was func_8C029332: a copy of text_style_reset (core/sprite_text.c, func_8C022A78) */
HW_EXPORT(8c029332, text_style_reset())

/* ---- state 4: player one looks through the penguins ---- */

#define VIEW_TARGET_Y  0xBB8FFB8Bu /* -0.0043942f */
#define VIEW_TARGET_Z  0x450E6C46u /* 2278.77f */
#define VIEW_EYE_Z     0x450E7C46u /* 2279.77f */
#define VIEW_FOV       0x4372AA4Cu /* 242.665f */

/* the camera's two points, on the stack as the original keeps them */
typedef struct ViewPoints {
  Vec3 target; /* 0x00 */
  Vec3 eye;    /* 0x0C */
} ViewPoints;
HW_SIZE(ViewPoints, 0x18);

/* was func_8C0270D2: state 4's update. On its first frame the icon's Bodies go to rest. Then right / left step
 * player one's penguin (player_a[0]) to the next / previous one that can be chosen (with a sound), or B goes back
 * to the house (state 0) with the penguins posing; the frame and the icon follow their entries (4 and 3). In every
 * phase the penguin is updated and view 0 looks at it from just in front. */
void penguin_look_update(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  switch (m->phase) {
  case 0:
    icon_bodies_init(menu);
    m->phase += 1;
    /* fall through */
  case 1:
    if (m->pressed[MENU_PRESS_RIGHT]) {
      sound_play_port_sound(0);
      do {
        m->player_a[0] += 1;
        if ((int32_t)m->player_a[0] >= MENU_CHARACTERS) m->player_a[0] = 0;
      } while (!character_available((int32_t)m->player_a[0]));
    } else if (m->pressed[MENU_PRESS_LEFT]) {
      sound_play_port_sound(0);
      do {
        m->player_a[0] -= 1;
        if ((int32_t)m->player_a[0] < 0) m->player_a[0] = MENU_CHARACTERS - 1;
      } while (!character_available((int32_t)m->player_a[0]));
    } else if (m->pressed[MENU_PRESS_B]) {
      sound_play_port_sound(3);
      menu_penguins_reset(menu, 1);
      m->state = 0;
      m->phase = 0;
    }
    place_follow(menu, 4, 0);
    icon_follow(menu, 3);
    break;
  default:
    break;
  }
  motion_player_update(m->penguins[(int32_t)m->player_a[0]][0]);
  gaddr points = hw_stack_alloc(sizeof(ViewPoints));
  vec3_set(F32(0.0f), F32_BITS(VIEW_TARGET_Y), F32_BITS(VIEW_TARGET_Z), member_addr(points, ViewPoints, target));
  vec3_set(F32(0.0f), F32(0.0f), F32_BITS(VIEW_EYE_Z), member_addr(points, ViewPoints, eye));
  house_view_camera(0, member_addr(points, ViewPoints, eye), member_addr(points, ViewPoints, target), F32(0.0f),
                    F32_BITS(VIEW_FOV));
  hw_stack_free(sizeof(ViewPoints));
}
HW_EXPORT(8c0270d2, penguin_look_update(ARG0))

/* ---- the penguins' screen ---- */

/* func_8C019544 (game/save.c, still translated): whether the save has unlocked the last penguin */
static int32_t last_penguin_unlocked(void) { return (int32_t)CALL0(0x8C019544u); }
int32_t motion_draw_l1(gaddr object, gaddr motion, gaddr frame, int32_t loop); /* core/motion.c */
/* nj_ModelL2 (Ninja, SDK): draws a model */
static inline void nj_ModelL2(gaddr model) { CALL1(0x8C0D0C22u, model); }

/* the penguins on the screen: two rows of four places, the character shown at each */
#define PENGUIN_ROWS 2
#define PENGUIN_COLUMNS 4
#define g_penguin_grid ((const int32_t (*)[PENGUIN_COLUMNS])hw_ptr(0x8C0735A4u))
/* where each character stands and how it is turned (angles in Ninja units, as floats) */
typedef struct PenguinPlace {
  Vec3 position; /* 0x00 */
  Vec3 rotation; /* 0x0C */
} PenguinPlace;
HW_SIZE(PenguinPlace, 0x18);
#define g_penguin_places ((const PenguinPlace *)hw_ptr(0x8C0734E4u))
#define CHARACTER_WITH_HELD 2 /* the penguin that holds something */
#define HELD_MOTIONS 3

/* what penguin 2 holds in each of its motions: the object (through a pointer to it) and the motion */
typedef struct HeldAction {
  gaddr object_ref; /* 0x00: the object, handed to motion_draw_l1 as it is (S15: an extra dereference here failed the difftest) */
  gaddr motion;     /* 0x04 */
} HeldAction;

#define LOCKED_CELL 0x2D
#define LOCKED_Y    F32(224.0f)
#define SPRITE_FRONT F32(-50.0f)

/* was func_8C027EF0: drawn last by the menu's screens: every penguin that can be chosen at its place (translated
 * and turned), penguin 2 with what it holds (its three motions in turn, each to its end), then the character's
 * model with the backdrop's textures; while the last penguin is locked, the backdrop's cell 0x2D at (0, 224) */
void menu_penguins_draw(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  for (int32_t row = 0; row < PENGUIN_ROWS; row++) {
    for (int32_t column = 0; column < PENGUIN_COLUMNS; column++) {
      int32_t character = g_penguin_grid[row][column];
      if (!character_available(character))
        continue;
      const PenguinPlace *place = &g_penguin_places[character];
      matrix_push();
      matrix_translate(place->position.x, place->position.y, place->position.z);
      matrix_rotate_xyz(place->rotation.x, place->rotation.y, place->rotation.z);
      motion_player_draw(m->penguins[character][0]);
      if (character == CHARACTER_WITH_HELD) {
        const HeldAction *held = (const HeldAction *)hw_ptr(m->held_actions[m->held_index]);
        if (motion_draw_l1(held->object_ref, held->motion, member_addr(menu, SetupMenu, held_frame), 1)) {
          m->held_index += 1;
          if (m->held_index >= HELD_MOTIONS) {
            m->held_frame = F32(0.0f);
            m->held_index = 0;
          }
        }
      }
      matrix_pop();
      if (character_available(character)) {
        njSetTexture(m->texlist);
        nj_ModelL2(((const gaddr *)hw_ptr(m->character_models))[character]);
      }
    }
  }
  if (last_penguin_unlocked() == 0) {
    m->sprite.tlist = m->texlist;
    m->sprite.p.x = F32(0.0f);
    m->sprite.p.y = LOCKED_Y;
    m->sprite.sx = F32(1.0f);
    m->sprite.sy = F32(1.0f);
    njDrawSprite2D(member_addr(menu, SetupMenu, sprite), LOCKED_CELL, SPRITE_FRONT, 0x20);
  }
}
HW_EXPORT(8c027ef0, menu_penguins_draw(ARG0))

/* ---- a penguin's profile ---- */

int32_t game_language(void);                                      /* game/save.c */
void text_set_scale(f32 sx, f32 sy, f32 v);                       /* core/sprite_text.c */
/* func_8C01529A (core/sprite_text.c, still translated): draws a string at (x, y) with the text style */

/* the profiles' strings: per language, one per character */
#define g_profile_strings ((const gaddr (*)[MENU_CHARACTERS])hw_ptr(0x8C095A28u))
#define PROFILE_TEXT_Y    0x51
#define PROFILE_CELL      0x32 /* the backdrop's cell of character 0's picture (one per character) */
#define PROFILE_PRIORITY  F32(-100.0f)
#define PROFILE_YELLOW    0x3E999999u /* 0.3f: the text's blue */
/* each character's string x and picture y (most keep the picture at 70) */
static const int32_t profile_text_x[MENU_CHARACTERS] = {144, 108, 128, 145, 120, 120, 138, 120};
static const f32 profile_picture_y[MENU_CHARACTERS] = {
    F32(75.0f), F32(80.0f), F32(80.0f), F32(70.0f), F32(70.0f), F32(70.0f), F32(71.0f), F32(70.0f)};

/* was func_8C028102: the profile of `character` (0 to 7): its string in the console's language (18 x 24 pixels,
 * yellow) at (x, 81), its picture (the backdrop's cell 0x32 + character) at (16, y), then the penguin itself */
void penguin_profile_draw(gaddr menu, int32_t character) {
  SetupMenu *m = setup_menu_at(menu);
  text_set_scale(F32(18.0f), F32(24.0f), F32(0.0f));
  global_color_set(F32(1.0f), F32(1.0f), F32_BITS(PROFILE_YELLOW), F32(1.0f));
  m->sprite.p.x = F32(16.0f);
  m->sprite.p.y = F32(70.0f);
  if ((uint32_t)character >= MENU_CHARACTERS)
    return;
  m->sprite.p.y = profile_picture_y[character];
  text_draw(profile_text_x[character], PROFILE_TEXT_Y, g_profile_strings[game_language()][character]);
  njDrawSprite2D(member_addr(menu, SetupMenu, sprite), PROFILE_CELL + character, PROFILE_PRIORITY, 0x20);
  motion_player_draw(m->penguins[character][0]);
}
HW_EXPORT(8c028102, penguin_profile_draw(ARG0, (int32_t)ARG1))
