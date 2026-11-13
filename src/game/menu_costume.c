/* Pen Pen port, S16 (w3): the SetupMenu's (game/menu.h) state 3, the costumes (game/menu_costume.c).
 *
 * Player one picks a penguin (left / right among those open; A goes to the costumes, B back to the house), then
 * moves a cursor in a grid of 4 rows of 12 cells, each a costume item (a save flag number; -1 kind = no cell). Some
 * items come as a pair (kind 2) or three (kind 3) cells: once every item of the group is found, the group is one
 * cell (the cursor on its first cell, stepping over it whole) and shows the group's own item. A puts the cell's
 * item on the penguin when it is found (save_set_byte44, the penguin's looks redone), B goes back to the penguin.
 * The penguin turns half round now and then.
 *
 * Also the screen of state 4 (the penguin's records: its scores, its costume, the totals; its update is
 * penguin_look_update in game/menu_screens.c). */
#include "hw.h"
#include "../core/sprite_text.h" /* text_draw (S16: hand-written) */
#include "sdk.h"
#include "../core/vec.h"
#include "../core/pad.h"
#include "../core/inline.h"
#include "sound.h"
#include "save.h"
#include "motion.h"
#include "menu.h"
#include "../core/ninja.h"
#include "../core/file.h"

/* functions of other files that no header declares yet */
void setup_menu_grid_reset(gaddr menu); /* game/scene_misc.c */
f32 random_timer_scaled(f32 range);     /* core/random.c */

/* ---- still translated ---- */
/* func_8C022C70 (game/menu_penguin.c's menu_penguin_set_looks, not exported): the penguin's four style numbers */
static void penguin_set_looks(gaddr penguin) { CALL1(0x8C022C70u, penguin); }

/* ---- data of the original ---- */
#define GRID_ROWS    4
#define GRID_COLUMNS 12
/* each cell's kind: -1 no cell, 2 one of a pair, 3 one of three, else a single item */
#define g_grid_kinds  ((const int8_t (*)[GRID_COLUMNS])hw_ptr(0x8C073C0Cu))
/* each cell's item (a save flag number) */
#define g_grid_items  ((const int32_t (*)[GRID_COLUMNS])hw_ptr(0x8C07383Cu))
/* the item of a whole pair (by column / 2) and of a whole three (by column / 3) */
#define g_pair_items  ((const int32_t (*)[GRID_COLUMNS / 2])hw_ptr(0x8C0738FCu))
#define g_three_items ((const int32_t (*)[GRID_COLUMNS / 3])hw_ptr(0x8C07395Cu))
enum { CELL_NONE = -1, CELL_PAIR = 2, CELL_THREE = 3 };

#define MENU_CHARACTERS 8
#define PAD_UNGROUPED 0x10000u /* while player one holds this bit, the pairs and threes stay single cells */
#define SPIN_CHANCE   F32(180.0f)      /* a half turn when random(180) < 1 */
#define SPIN_HALF     F32(32768.0f)    /* half a turn in Ninja angle units */
#define SPIN_DAMPING  F32(0.25f)
#define SPIN_RATE     F32_BITS(0x3DCCCCCCu) /* 0.1f */

/* the camera of the penguin screens: from just in front of the penguin */
#define VIEW_TARGET_Y  0xBB8FFB8Bu /* -0.0043942f */
#define VIEW_TARGET_Z  0x450E6C46u /* 2278.77f */
#define VIEW_EYE_Z     0x450E7C46u /* 2279.77f */
#define VIEW_FOV       0x4372AA4Cu /* 242.665f */

/* the phases of state 3 */
enum { COSTUME_ENTER, COSTUME_PENGUIN, COSTUME_GRID };

/* func_8C0229BE: a new state, from its first phase */
static void menu_set_state(gaddr menu, int32_t state) {
  SetupMenu *m = setup_menu_at(menu);
  m->state = state;
  m->phase = 0;
}

/* the frame of the original (0x30 bytes of locals, PR and r8): the camera's points at their offsets */
typedef struct CostumeFrame {
  uint8_t locals[0x0C];
  Vec3 target; /* 0x0C */
  Vec3 eye;    /* 0x18 */
  uint8_t rest[0x38 - 0x24];
} CostumeFrame;
HW_OFFSET(CostumeFrame, target, 0x0C);
HW_OFFSET(CostumeFrame, eye, 0x18);
HW_SIZE(CostumeFrame, 0x38);

static inline int32_t cell_kind(const SetupMenu *m) { return g_grid_kinds[m->grid_row][m->grid_column]; }

/* whether both items of the pair the cursor is in are found */
static int32_t pair_found(const SetupMenu *m) {
  const int32_t *items = g_grid_items[m->grid_row];
  int32_t first = m->grid_column / 2 * 2;
  int32_t a = items[first], b = items[first + 1];
  return save_flag(a) && save_flag(b);
}

/* whether the three items of the three the cursor is in are found */
static int32_t three_found(const SetupMenu *m) {
  const int32_t *items = g_grid_items[m->grid_row];
  int32_t first = m->grid_column / 3 * 3;
  int32_t a = items[first], b = items[first + 1], c = items[first + 2];
  return save_flag(a) && save_flag(b) && save_flag(c);
}

/* the cursor right by one cell or by its group (2 or 3), round the row; past the cells that are not there */
static void grid_right(SetupMenu *m, int32_t group) {
  do {
    if (group == 0) {
      m->grid_column += 1;
      if (m->grid_column >= GRID_COLUMNS)
        m->grid_column = 0;
    } else if (group == CELL_PAIR) {
      m->grid_column += 2;
      if (m->grid_column >= GRID_COLUMNS)
        m->grid_column = 0;
    } else if (group == CELL_THREE) {
      m->grid_column += 3;
      if (m->grid_column >= GRID_COLUMNS)
        m->grid_column = 0;
    }
  } while (cell_kind(m) == CELL_NONE);
}

/* the cursor left: one cell; from a pair two back from column 4, else one; from a three two back from column 6,
 * else three; round the row, past the cells that are not there */
static void grid_left(SetupMenu *m, int32_t group) {
  do {
    if (group == 0) {
      m->grid_column -= 1;
      if (m->grid_column < 0)
        m->grid_column = GRID_COLUMNS - 1;
    } else if (group == CELL_PAIR) {
      if (m->grid_column == 4)
        m->grid_column -= 2;
      else
        m->grid_column -= 1;
      if (m->grid_column < 0)
        m->grid_column = GRID_COLUMNS - 1;
    } else if (group == CELL_THREE) {
      if (m->grid_column == 6)
        m->grid_column -= 2;
      else
        m->grid_column -= 3;
      if (m->grid_column < 0)
        m->grid_column = GRID_COLUMNS - 1;
    }
  } while (cell_kind(m) == CELL_NONE);
}

/* was func_8C025CEC: state 3's update. Phase 0 starts it (the grid cursor reset) and goes on as phase 1: right /
 * left step player one's penguin among those open (with a click), A goes to the grid (phase 2), B back to the house
 * (the penguins posing); the cursor stays on the first cell. Phase 2: unless player one holds the ungrouping bit,
 * a found pair or three puts the cursor on its first cell and moves it whole; right / left / up / down move the
 * cursor (up and down keep it in a cell that is there by moving left), A puts the item on (when found; else a
 * buzz), B goes back to the penguin. Then the cell's item (a found group's own), the icon and the frame follow
 * their entries (2, 3), the penguin turns half round at random (both ways), its spin pulled toward that, the
 * penguin moves and the camera looks at it. */
void costume_update(gaddr menu) {
  gaddr frame = hw_stack_alloc(sizeof(CostumeFrame));
  SetupMenu *m = setup_menu_at(menu);
  int32_t group = 0;
  int32_t ungrouped = 0;
  if (port_held(0, PAD_UNGROUPED))
    ungrouped = 1;

  switch (m->phase) {
  case COSTUME_ENTER:
    setup_menu_grid_reset(menu);
    m->phase++;
    /* fall through */
  case COSTUME_PENGUIN:
    if (m->pressed[MENU_PRESS_RIGHT]) {
      do {
        m->player_a[0] += 1;
        if ((int32_t)m->player_a[0] >= MENU_CHARACTERS)
          m->player_a[0] = 0;
      } while (!save_choice_open((int32_t)m->player_a[0]));
      sound_play_port_sound(0);
    } else if (m->pressed[MENU_PRESS_LEFT]) {
      do {
        m->player_a[0] -= 1;
        if ((int32_t)m->player_a[0] < 0)
          m->player_a[0] = MENU_CHARACTERS - 1;
      } while (!save_choice_open((int32_t)m->player_a[0]));
      sound_play_port_sound(0);
    } else if (m->pressed[MENU_PRESS_A]) {
      m->phase = COSTUME_GRID;
      sound_play_port_sound(1);
    } else if (m->pressed[MENU_PRESS_B]) {
      sound_play_port_sound(3);
      menu_penguins_reset(menu, 1);
      menu_set_state(menu, 0);
    }
    m->grid_column = 0;
    m->grid_row = 0;
    m->grid_item = 0;
    break;
  case COSTUME_GRID:
    if (cell_kind(m) == CELL_PAIR && !ungrouped && pair_found(m)) {
      m->grid_column = m->grid_column / 2 * 2;
      group = CELL_PAIR;
    }
    if (cell_kind(m) == CELL_THREE && !ungrouped && three_found(m)) {
      m->grid_column = m->grid_column / 3 * 3;
      group = CELL_THREE;
    }
    if (m->pressed[MENU_PRESS_RIGHT]) {
      sound_play_port_sound(0);
      grid_right(m, group);
    } else if (m->pressed[MENU_PRESS_LEFT]) {
      sound_play_port_sound(0);
      grid_left(m, group);
    } else if (m->pressed[MENU_PRESS_UP]) {
      sound_play_port_sound(0);
      m->grid_row -= 1;
      if (m->grid_row < 0)
        m->grid_row = GRID_ROWS - 1;
      while (cell_kind(m) == CELL_NONE)
        m->grid_column -= 1;
    } else if (m->pressed[MENU_PRESS_DOWN]) {
      sound_play_port_sound(0);
      m->grid_row += 1;
      if (m->grid_row >= GRID_ROWS)
        m->grid_row = 0;
      while (cell_kind(m) == CELL_NONE)
        m->grid_column -= 1;
    } else if (m->pressed[MENU_PRESS_A]) {
      if (save_flag(m->grid_item)) {
        save_set_byte44((int32_t)m->player_a[0], m->grid_item);
        penguin_set_looks(m->penguins[(int32_t)m->player_a[0]][0]);
        sound_play_port_sound(1);
      } else {
        sound_play_port_sound(3);
      }
    } else if (m->pressed[MENU_PRESS_B]) {
      m->phase = COSTUME_PENGUIN;
      sound_play_port_sound(3);
    }
    break;
  default:
    break;
  }

  /* the cell's item, or its found group's */
  m->grid_item = g_grid_items[m->grid_row][m->grid_column];
  if (!ungrouped) {
    if (cell_kind(m) == CELL_PAIR && pair_found(m))
      m->grid_item = g_pair_items[m->grid_row][m->grid_column / 2];
    if (cell_kind(m) == CELL_THREE && three_found(m))
      m->grid_item = g_three_items[m->grid_row][m->grid_column / 3];
  }
  icon_follow(menu, 2);
  place_follow(menu, 3, 0);

  /* now and then half a turn one way, now and then the other */
  f32 turn = fgt(F32(1.0f), random_timer_scaled(SPIN_CHANCE)) ? SPIN_HALF : F32(0.0f);
  m->spin_target = fadd(m->spin_target, turn);
  turn = fgt(F32(1.0f), random_timer_scaled(SPIN_CHANCE)) ? SPIN_HALF : F32(0.0f);
  m->spin_target = fsub(m->spin_target, turn);
  m->spin.damping = SPIN_DAMPING;
  body1_pull(m->spin_target, SPIN_RATE, member_addr(menu, SetupMenu, spin));
  body1_step(member_addr(menu, SetupMenu, spin));
  motion_player_update(m->penguins[(int32_t)m->player_a[0]][0]);

  vec3_set(F32(0.0f), F32_BITS(VIEW_TARGET_Y), F32_BITS(VIEW_TARGET_Z), member_addr(frame, CostumeFrame, target));
  vec3_set(F32(0.0f), F32(0.0f), F32_BITS(VIEW_EYE_Z), member_addr(frame, CostumeFrame, eye));
  house_view_camera(0, member_addr(frame, CostumeFrame, eye), member_addr(frame, CostumeFrame, target), F32(0.0f),
                    F32_BITS(VIEW_FOV));
  hw_stack_free(sizeof(CostumeFrame));
}
HW_EXPORT(8c025cec, costume_update(ARG0))

/* ---- the screen of state 3 ---- */

/* functions of other files that no header declares yet */
int32_t game_language(void);                     /* game/save.c */
void text_set_scale(f32 sx, f32 sy, f32 v);      /* core/sprite_text.c */
void material_set(f32 r, f32 g, f32 b, f32 a);   /* game/house.c */

/* ---- still translated / SDK ---- */
/* func_8C01529A (core/sprite_text.c): draws a string at (x, y) with the text style; its width */
/* njColorBlendingMode (Ninja, SDK) */
static inline void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* each penguin's place on the costume screen (translated to before its profile is drawn) */
#define g_costume_places ((const Vec3 *)hw_ptr(0x8C07399Cu))
/* each costume item's four strings (name, found: two lines and a third): [language][item][4] */
#define COSTUME_ITEMS_STRINGS 53 /* items per language in the table (0x350 bytes) */
#define g_costume_strings ((const gaddr (*)[COSTUME_ITEMS_STRINGS][4])hw_ptr(0x8C094138u))
/* the screen's words: [language][11] (0: the count's label, 9: the second's) */
#define g_costume_words ((const gaddr (*)[11])hw_ptr(0x8C0958F4u))
#define g_costume_blink  GUEST_VAR(int32_t, 0x8C095FD0u) /* + 1 per frame drawn: the cursor's cell blinks */
#define g_costume_angle  GUEST_VAR(int32_t, 0x8C095FD4u) /* + 910.22 (5 degrees) per frame drawn */
#define g_common_texlist GUEST_VAR(gaddr, 0x8C1B7E40u)   /* (game/model.c's word) */
#define COSTUME_ITEMS    44
#define NJD_EVAL_UNIT_SCL 4u
#define NJD_EVAL_BREAK    0x10u

enum {
  CELL_SCREEN = 4, CELL_LEFT = 0, CELL_LEFT_HELD = 1, CELL_RIGHT = 2, CELL_RIGHT_HELD = 3, CELL_TITLE = 0x3A,
  CELL_ITEM_1 = 0x3B, CELL_ITEM_0 = 0x3C, CELL_ITEM_GROUP = 0x3D,
};
#define SCREEN_PRIORITY F32(-10000.0f)
#define CELL_PRIORITY   F32(-5000.0f)
#define GREY_03 0x3E999999u /* 0.3f */

static void costume_sprite_draw(gaddr menu, int32_t cell, f32 priority, uint32_t attr) {
  njDrawSprite2D(member_addr(menu, SetupMenu, sprite), cell, priority, attr);
}

/* the cursor's cell (or group, by `same`) lights up every other frame while choosing */
static void cell_highlight(const SetupMenu *m, int32_t same, int32_t row) {
  if (m->phase == COSTUME_GRID && (g_costume_blink & 1) && same && m->grid_row == row) {
    njColorBlendingMode(0, 8);
    njColorBlendingMode(1, 10);
  }
}

static f32 cell_x(int32_t column) { return fmac(F32(48.0f), f32_from_int(column), F32(32.0f)); }

/* was func_8C0265EC: state 3's screen (from its first phase): the backdrop, the icon (entry 2), the arrows (lit
 * while held), the frame, the penguin's profile at its place (turned by its spin), the title; the grid of costume
 * items (48 pixels apart from (32, 256)): a single cell by its kind, a found pair or three drawn once from its
 * first cell (the cursor's lit every other frame while choosing); while choosing, the cell's item name, and once
 * found its description; the count of items found (and that count scaled to 1005 / 440); the costume model with the
 * found items shown */
void costume_draw(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  if (m->phase == COSTUME_ENTER)
    return;
  int32_t ungrouped = 0;
  if (port_held(0, PAD_UNGROUPED))
    ungrouped = 1;
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  m->sprite.tlist = m->texlist;
  m->sprite.p.y = F32(0.0f);
  m->sprite.p.x = F32(0.0f);
  costume_sprite_draw(menu, CELL_SCREEN, SCREEN_PRIORITY, 0);
  m->sprite.tlist = m->map_texlist;
  icon_draw(menu, 2);
  m->sprite.p.x = F32(52.0f);
  m->sprite.p.y = F32(41.0f);
  m->sprite.sx = F32(1.0f);
  m->sprite.sy = F32(1.0f);
  costume_sprite_draw(menu, pad_held(&m->pad, PAD_LEFT) ? CELL_LEFT_HELD : CELL_LEFT, F32(-10.0f), 0x20);
  m->sprite.p.x = F32(142.0f);
  m->sprite.p.y = F32(41.0f);
  m->sprite.sx = F32(1.0f);
  m->sprite.sy = F32(1.0f);
  costume_sprite_draw(menu, pad_held(&m->pad, PAD_RIGHT) ? CELL_RIGHT_HELD : CELL_RIGHT, F32(-100.0f), 0x20);
  place_frame_draw(menu);

  matrix_push();
  const Vec3 *place = &g_costume_places[(int32_t)m->player_a[0]];
  matrix_translate(place->x, place->y, place->z);
  matrix_rotate_y(m->spin.value);
  penguin_profile_draw(menu, (int32_t)m->player_a[0]);
  matrix_pop();
  m->sprite.p.x = F32(16.0f);
  m->sprite.p.y = F32(100.0f);
  costume_sprite_draw(menu, CELL_TITLE, CELL_PRIORITY, 0x20);

  g_costume_blink += 1;
  for (int32_t row = 0; row < GRID_ROWS; row++) {
    for (int32_t column = 0; column < GRID_COLUMNS; column++) {
      m->sprite.p.x = cell_x(column);
      m->sprite.p.y = fmac(F32(48.0f), f32_from_int(row), F32(256.0f));
      const int32_t *items = g_grid_items[row];
      int32_t on_cell = m->grid_column == column;
      switch (g_grid_kinds[row][column]) {
      case 0:
        cell_highlight(m, on_cell, row);
        costume_sprite_draw(menu, CELL_ITEM_0, CELL_PRIORITY, 0x20);
        break;
      case 1:
        cell_highlight(m, on_cell, row);
        costume_sprite_draw(menu, CELL_ITEM_1, CELL_PRIORITY, 0x20);
        break;
      case CELL_PAIR: {
        int32_t first = column / 2 * 2;
        int32_t a = items[first], b = items[first + 1];
        if (ungrouped) {
          cell_highlight(m, on_cell, row);
          costume_sprite_draw(menu, CELL_ITEM_GROUP, CELL_PRIORITY, 0x20);
        } else if (save_flag(a) && save_flag(b)) {
          if (column % 2 == 0) { /* the found pair, from its first cell */
            cell_highlight(m, m->grid_column / 2 == column / 2, row);
            m->sprite.p.x = cell_x(column / 2 * 2);
            costume_sprite_draw(menu, CELL_ITEM_GROUP, CELL_PRIORITY, 0x20);
            m->sprite.p.x = cell_x(column / 2 * 2 + 1);
            costume_sprite_draw(menu, CELL_ITEM_GROUP, CELL_PRIORITY, 0x20);
          }
        } else {
          cell_highlight(m, on_cell, row);
          costume_sprite_draw(menu, CELL_ITEM_GROUP, CELL_PRIORITY, 0x20);
        }
        break;
      }
      case CELL_THREE: {
        int32_t first = column / 3 * 3;
        int32_t a = items[first], b = items[first + 1], c = items[first + 2];
        if (ungrouped) {
          cell_highlight(m, on_cell, row);
          costume_sprite_draw(menu, CELL_ITEM_GROUP, CELL_PRIORITY, 0x20);
        } else if (save_flag(a) && save_flag(b) && save_flag(c)) {
          if (column % 3 == 0) { /* the found three, from its first cell */
            cell_highlight(m, m->grid_column / 3 == column / 3, row);
            for (int32_t k = 0; k < 3; k++) {
              m->sprite.p.x = cell_x(column / 3 * 3 + k);
              costume_sprite_draw(menu, CELL_ITEM_GROUP, CELL_PRIORITY, 0x20);
            }
          }
        } else {
          cell_highlight(m, on_cell, row);
          costume_sprite_draw(menu, CELL_ITEM_GROUP, CELL_PRIORITY, 0x20);
        }
        break;
      }
      default:
        break;
      }
      njColorBlendingMode(0, 8);
      njColorBlendingMode(1, 6);
    }
  }

  if (m->phase == COSTUME_GRID) {
    text_set_scale(F32(32.0f), F32(32.0f), F32(-4.0f));
    global_color_set(F32_BITS(0x3F199999u), F32(1.0f), F32_BITS(0x3EC28F5Cu), F32(1.0f)); /* 0.6, 1, 0.38 */
    text_draw(0xB8, 0x68, g_costume_strings[game_language()][m->grid_item][0]);
    if (save_flag(m->grid_item)) {
      text_set_scale(F32(24.0f), F32(32.0f), F32(0.0f));
      global_color_set(F32(1.0f), F32(1.0f), F32_BITS(GREY_03), F32(1.0f));
      text_draw(0x118, 0x68, g_costume_strings[game_language()][m->grid_item][1]);
      global_color_set(F32(1.0f), F32(1.0f), F32(1.0f), F32(1.0f));
      text_draw(0xE8, 0x8C, g_costume_strings[game_language()][m->grid_item][2]);
      text_draw(0xD0, 0xAC, g_costume_strings[game_language()][m->grid_item][3]);
    }
  }
  int32_t language = game_language();
  text_set_scale(F32(32.0f), F32(32.0f), F32(0.0f));
  global_color_set(F32(1.0f), F32_BITS(0x3F23D70Au), F32_BITS(0x3D75C28Fu), F32(1.0f)); /* 1, 0.64, 0.06 */
  int32_t width = text_draw(0xC8, 0xD0, g_costume_words[language][0]);
  text_draw(0x1EE, 0xD0, g_costume_words[language][9]);
  material_set(F32(1.0f), F32(1.0f), F32_BITS(GREY_03), F32(1.0f));
  menu_number_draw(menu, width + 0xC8, 0xD0, save_flags_count());
  menu_number_draw(menu, 0x206, 0xD0, save_flags_count() * 1005 / 440);
  g_costume_angle = f32_to_int(fadd(f32_from_int(g_costume_angle), F32_BITS(0x44638E38u))); /* + 910.22 */

  /* the costume model: the found items shown (unit scale), the others cut off */
  for (int32_t i = 0; i < COSTUME_ITEMS; i++) {
    NjObject *node = (NjObject *)hw_ptr(((const gaddr *)hw_ptr(m->costume_nodes))[i]);
    if (save_flag(i + 1)) {
      node->evalflags &= ~NJD_EVAL_BREAK;
      node = (NjObject *)hw_ptr(((const gaddr *)hw_ptr(m->costume_nodes))[i]);
      node->evalflags |= NJD_EVAL_UNIT_SCL;
    } else {
      node->evalflags |= NJD_EVAL_BREAK;
    }
  }
  matrix_push();
  njSetTexture(g_common_texlist);
  nj_ModelL1(m->costume_model);
  matrix_pop();
}
HW_EXPORT(8c0265ec, costume_draw(ARG0))

/* ---- the screen of state 4: a penguin's records ---- */

/* functions of other files that no header declares yet */
int32_t save_score_a(int32_t i);  /* game/save.c */
int32_t save_score_b(int32_t i);
int32_t save_score_c(int32_t i);
int32_t save_total_a(void);
int32_t save_total_b(void);
int32_t save_total_c(void);
int32_t save_best_a(void);
int32_t save_byte44(int32_t i);

/* the SHC runtime's signed division (func_8C062464: r1 / r0), called itself here because the divisor can be 0
 * (no record yet), where it sets the runtime's errno and gives 0 */
static int32_t shc_divide(int32_t dividend, int32_t divisor) {
  hw_cpu->r[1] = (uint32_t)dividend;
  hw_cpu->r[0] = (uint32_t)divisor;
  return (int32_t)CALL0(0x8C062464u);
}

/* the records screen's dark panel: four NJS_POLYGON_VTX (x, y, z, colour), copied to the stack and drawn */
#define RECORDS_PANEL      0x8C095FD8u
#define RECORDS_PANEL_SIZE 0x40u
enum {
  CELL_BEST = 0x3E, CELL_CORNER_TL = 0x3F, CELL_CORNER_TR = 0x40, CELL_CORNER_BL = 0x41, CELL_CORNER_BR = 0x42,
  CELL_EDGE_TOP = 0x43, CELL_EDGE_BOTTOM = 0x44, CELL_EDGE_SIDE = 0x45,
};
#define ANGLE_QUARTER 0x4000 /* a quarter turn in Ninja angle units */

/* the frame of the original (0x54 bytes of locals, PR and r8): the panel's copy at its offset */
typedef struct RecordsFrame {
  uint8_t locals[0x10];
  uint8_t panel[RECORDS_PANEL_SIZE]; /* 0x10 */
  uint8_t rest[0x5C - 0x50];
} RecordsFrame;
HW_OFFSET(RecordsFrame, panel, 0x10);
HW_SIZE(RecordsFrame, 0x5C);

static void records_sprite_at(SetupMenu *m, f32 x, f32 y) {
  m->sprite.p.x = x;
  m->sprite.p.y = y;
}

/* was func_8C027234 (with its tail, func_8C027628, which the original reaches by falling through from its block
 * copy): state 4's screen (from its first phase): the backdrop, the icon (entry 3), the frame, the arrows (lit while
 * held), the penguin's profile at its place, the title, a mark when it holds the best score a; a panel's corners and
 * edges and the dark panel; the words and the penguin's scores (a, its share of all a's in 1005 / 10 of the total, b,
 * c), its costume's name, then the totals and the count of items found (also scaled to 1005 / 440) */
void records_draw(gaddr menu) {
  gaddr frame = hw_stack_alloc(sizeof(RecordsFrame));
  SetupMenu *m = setup_menu_at(menu);
  if (m->phase == 0) {
    hw_stack_free(sizeof(RecordsFrame));
    return;
  }
  m->sprite.tlist = m->texlist;
  m->sprite.p.x = F32(0.0f);
  m->sprite.p.y = F32(0.0f);
  m->sprite.sx = F32(1.0f);
  m->sprite.sy = F32(1.0f);
  costume_sprite_draw(menu, CELL_SCREEN, SCREEN_PRIORITY, 0);
  m->sprite.tlist = m->map_texlist;
  icon_draw(menu, 3);
  place_frame_draw(menu);
  m->sprite.sx = F32(1.0f);
  m->sprite.sy = F32(1.0f);
  records_sprite_at(m, F32(52.0f), F32(41.0f));
  costume_sprite_draw(menu, pad_held(&m->pad, PAD_LEFT) ? CELL_LEFT_HELD : CELL_LEFT, F32(-10.0f), 0x20);
  records_sprite_at(m, F32(142.0f), F32(41.0f));
  costume_sprite_draw(menu, pad_held(&m->pad, PAD_RIGHT) ? CELL_RIGHT_HELD : CELL_RIGHT, F32(-100.0f), 0x20);

  matrix_push();
  const Vec3 *place = &g_costume_places[(int32_t)m->player_a[0]];
  matrix_translate(place->x, place->y, place->z);
  penguin_profile_draw(menu, (int32_t)m->player_a[0]);
  matrix_pop();
  records_sprite_at(m, F32(16.0f), F32(100.0f));
  costume_sprite_draw(menu, CELL_TITLE, CELL_PRIORITY, 0x20);
  if (save_best_a() == (int32_t)m->player_a[0]) {
    records_sprite_at(m, F32(76.0f), F32(240.0f));
    costume_sprite_draw(menu, CELL_BEST, CELL_PRIORITY, 0x20);
  }
  /* the panel's corners and edges */
  records_sprite_at(m, F32(32.0f), F32(272.0f));
  costume_sprite_draw(menu, CELL_CORNER_TL, CELL_PRIORITY, 0x20);
  records_sprite_at(m, F32(32.0f), F32(416.0f));
  costume_sprite_draw(menu, CELL_CORNER_BL, CELL_PRIORITY, 0x20);
  records_sprite_at(m, F32(576.0f), F32(272.0f));
  costume_sprite_draw(menu, CELL_CORNER_TR, CELL_PRIORITY, 0x20);
  records_sprite_at(m, F32(576.0f), F32(416.0f));
  costume_sprite_draw(menu, CELL_CORNER_BR, CELL_PRIORITY, 0x20);
  records_sprite_at(m, F32(64.0f), F32(272.0f));
  costume_sprite_draw(menu, CELL_EDGE_TOP, CELL_PRIORITY, 0x20);
  records_sprite_at(m, F32(64.0f), F32(432.0f));
  costume_sprite_draw(menu, CELL_EDGE_BOTTOM, CELL_PRIORITY, 0x20);
  records_sprite_at(m, F32(32.0f), F32(416.0f));
  m->sprite.ang = -ANGLE_QUARTER;
  costume_sprite_draw(menu, CELL_EDGE_SIDE, CELL_PRIORITY, 0x21);
  records_sprite_at(m, F32(608.0f), F32(304.0f));
  m->sprite.ang = ANGLE_QUARTER;
  costume_sprite_draw(menu, CELL_EDGE_SIDE, CELL_PRIORITY, 0x21);
  gaddr panel = member_addr(frame, RecordsFrame, panel);
  memmove(hw_ptr(panel), hw_ptr(RECORDS_PANEL), RECORDS_PANEL_SIZE);
  njDrawPolygon(panel, 4, 1);

  /* the penguin's scores */
  int32_t language = game_language();
  int32_t character = (int32_t)m->player_a[0];
  global_color_set(F32_BITS(0x3F199999u), F32(1.0f), F32_BITS(0x3EC28F5Cu), F32(1.0f)); /* 0.6, 1, 0.38 */
  text_set_scale(F32(32.0f), F32(32.0f), F32(0.0f));
  int32_t width = text_draw(0xD0, 0x78, g_costume_words[language][1]);
  text_draw(width + 0xD0, 0x78, g_costume_words[language][10]);
  text_draw(0x1EE, 0x78, g_costume_words[language][9]);
  text_draw(0xE8, 0x9B, g_costume_words[language][2]);
  text_draw(0x1EA, 0x9B, g_costume_words[language][10]);
  text_draw(0xD8, 0xBE, g_costume_words[language][3]);
  text_draw(0x1DA, 0xBE, g_costume_words[language][10]);
  material_set(F32(1.0f), F32(1.0f), F32_BITS(GREY_03), F32(1.0f));
  menu_number_draw(menu, width + 0x88, 0x78, save_score_a(character));
  int32_t share = save_score_a(character) * 1005;
  menu_number_draw(menu, 0x206, 0x78, shc_divide(share, save_total_a() * 10));
  menu_number_draw(menu, 0x1A2, 0x9B, save_score_b(character));
  menu_number_draw(menu, 0x192, 0xBE, save_score_c(character));
  /* its costume's name */
  global_color_set(F32(1.0f), F32(1.0f), F32_BITS(GREY_03), F32(1.0f));
  int32_t language2 = game_language();
  text_draw(0xC8, 0xE1, g_costume_strings[language2][save_byte44(character)][1]);
  /* the totals */
  text_set_scale(F32(32.0f), F32(32.0f), F32(0.0f));
  global_color_set(F32(1.0f), F32_BITS(0x3EF5C28Fu), F32_BITS(0x3F547AE1u), F32(1.0f)); /* 1, 0.48, 0.83 */
  text_draw(0x5E, 0x125, g_costume_words[language][5]);
  text_draw(0x3E, 0x16B, g_costume_words[language][6]);
  text_draw(0x1DE, 0x125, g_costume_words[language][10]);
  text_draw(0x1BE, 0x16B, g_costume_words[language][10]);
  global_color_set(F32(1.0f), F32_BITS(0x3F23D70Au), F32_BITS(0x3D75C28Fu), F32(1.0f)); /* 1, 0.64, 0.06 */
  text_draw(0x4E, 0x148, g_costume_words[language][7]);
  text_draw(0x2E, 0x18E, g_costume_words[language][8]);
  text_draw(0x1CE, 0x148, g_costume_words[language][10]);
  text_draw(0x1C3, 0x18E, g_costume_words[language][9]);
  material_set(F32(1.0f), F32(1.0f), F32_BITS(GREY_03), F32(1.0f));
  menu_number_draw(menu, 0x196, 0x125, save_total_a());
  menu_number_draw(menu, 0x186, 0x148, save_total_b());
  menu_number_draw(menu, 0x176, 0x16B, save_total_c());
  menu_number_draw(menu, 0x183, 0x18E, save_flags_count());
  menu_number_draw(menu, 0x1DB, 0x18E, save_flags_count() * 1005 / 440);
  hw_stack_free(sizeof(RecordsFrame));
}
HW_EXPORT(8c027234, records_draw(ARG0))
