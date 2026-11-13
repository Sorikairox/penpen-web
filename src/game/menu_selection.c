/* Pen Pen port, S16 (w3): the SetupMenu's (game/menu.h) penguin selection (state 1), its course selection (state 2)
 * and the screen of state 2 (game/menu_selection.c).
 *
 * The penguin selection: every player of the mode steps through the penguins with left / right (skipping those
 * another player is on and those not open yet), takes one with A (or with X, a second kind, once every course item
 * is found), and gives it back with B; when every player has taken one, a second later the course selection
 * starts. Each player's slot slides to the place of the penguin it is on and lights up once taken.
 *
 * The course selection: up / down pick the course (with the course's mascot), A goes on to the variants (a
 * number of variants per course in the one-player game, five in the others), B goes back. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/pad.h"
#include "../core/ninja.h"
#include "../core/inline.h"
#include "sound.h"
#include "save.h"
#include "menu.h"
#include "race_scene.h"
#include "motion.h"
#include "../core/file.h"
#include "../core/view.h"
#include "../core/sprite_text.h"

/* functions of other files that no header declares yet */
void sound_play_sequence(int32_t port, int32_t sequence, int32_t priority); /* core/accessors.c */

#define MENU_CHARACTERS 8
#define LAST_CHARACTER  7     /* the one penguin that has no second kind */
#define SELECT_SECOND_KIND_ITEMS 0x28 /* every course item found: X takes a penguin's second kind */
#define SELECTION_WAIT  60    /* frames between the last pick and the course selection */

/* sequences each character plays: when it is taken, and when it is given back */
#define g_pick_sequences   ((const int32_t *)hw_ptr(0x8C067294u))
#define g_unpick_sequences ((const int32_t *)hw_ptr(0x8C0672B4u))
/* the penguins on the screen: two rows of four places, the character shown at each (as one list of 8) */
#define g_penguin_order ((const int32_t *)hw_ptr(0x8C0735A4u))

/* A function-local static Vec3 of the original (C++: built on its first use, a guard word after it) */
typedef struct StaticVec3 {
  Vec3 v;              /* 0x00 */
  int32_t constructed; /* 0x0C */
} StaticVec3;
HW_SIZE(StaticVec3, 0x10);
#define g_selection_eye    ((StaticVec3 *)hw_ptr(0x8C1BAAB8u))
#define g_selection_target ((StaticVec3 *)hw_ptr(0x8C1BAAC8u))
/* where a player's slot goes when it is on the penguin of each place (8 statics, their guards elsewhere) */
#define g_slot_places       ((Vec3 *)hw_ptr(0x8C095B08u))
#define g_slot_places_built ((int32_t *)hw_ptr(0x8C1BAAD8u))
static const f32 slot_place_xy[MENU_CHARACTERS][2] = {
    {F32(62.0f), F32(120.0f)}, {F32(184.0f), F32(107.0f)}, {F32(301.0f), F32(130.0f)}, {F32(420.0f), F32(128.0f)},
    {F32(42.0f), F32(244.0f)}, {F32(180.0f), F32(379.0f)}, {F32(389.0f), F32(379.0f)}, {F32(531.0f), F32(379.0f)},
};

/* the camera of the selections: from (0, 21.18, 50.25) at (0, 20.86, 49.30) */
#define EYE_Y      0x41A96A7Eu
#define EYE_Z      0x4248FEFAu
#define TARGET_Y   0x41A6E739u
#define TARGET_Z   0x424532CCu
#define CAMERA_FOV 0x462AAAAAu /* 60 degrees in Ninja angle units, as a float */

/* func_8C0229BE: a new state, from its first phase */
static void menu_set_state(gaddr menu, int32_t state) {
  SetupMenu *m = setup_menu_at(menu);
  m->state = state;
  m->phase = 0;
}

static inline RaceSetup *menu_setup(const SetupMenu *m) { return race_setup_at(m->setup); }

/* the frame of the penguin selection: the slot places are built through temporaries at these offsets from r15
 * (place k at 0x68 - 12 k) */
typedef struct SelectionFrame {
  uint8_t locals[0x14];
  Vec3 temps[MENU_CHARACTERS]; /* 0x14 */
  uint8_t rest[0x7C - 0x74];   /* the menu, PR */
} SelectionFrame;
HW_OFFSET(SelectionFrame, temps, 0x14);
HW_SIZE(SelectionFrame, 0x7C);

/* a player steps to the next (+1) or previous (-1) place whose penguin no other player is on and that is open */
static void selection_step(SetupMenu *m, int32_t i, int32_t step) {
  int32_t alone;
  do {
    if (step < 0) {
      m->order[i] -= 1;
      if (m->order[i] < 0)
        m->order[i] = MENU_CHARACTERS - 1;
    } else {
      m->order[i] += 1;
      if (m->order[i] >= MENU_CHARACTERS)
        m->order[i] = 0;
    }
    alone = 1;
    for (int32_t j = 0; j < m->player_count; j++)
      if (j != i && m->order[j] == m->order[i])
        alone = 0;
  } while (!alone || !save_choice_open(g_penguin_order[m->order[i]]));
}

/* was func_8C023D16: state 1, the penguin selection. On its first frame it starts (selection_enter). While
 * choosing (phase 1): B with player one's penguin not taken goes back (to the house in the one-player game, else
 * leaves: state 5 for mode 4, 6 for the others); each player then either gives its penguin back with B (once taken)
 * or steps with left / right, takes it with A (the first kind) or X (the second kind, once every course item is
 * found), and is on the character of its place (the last character has no second kind); once every player has
 * taken one, phase 2 waits a second and the course selection (state 2) starts. Every frame the penguins move, the
 * camera is set, the icon follows entry 0, each slot is pulled toward its place and its alpha toward 1 once taken
 * (else 0). */
void selection_update(gaddr menu) {
  gaddr frame = hw_stack_alloc(sizeof(SelectionFrame));
  SetupMenu *m = setup_menu_at(menu);
  if (m->phase == 0) {
    selection_enter(menu);
    m->phase++;
  }
  if (m->phase == 1) {
    if (m->pressed[MENU_PRESS_B] && m->picked[0] == 0) {
      sound_play_port_sound(3);
      if (menu_setup(m)->mode == 0)
        menu_set_state(menu, 0);
      else
        menu_set_state(menu, menu_setup(m)->mode == 4 ? 5 : 6);
    }
    for (int32_t i = 0; i < m->player_count; i++) {
      if (m->picked[i]) {
        if (port_pressed(i, PAD_B)) {
          sound_play_port_sound(3);
          sound_play_sequence(i, g_unpick_sequences[m->player_a[i]], 0);
          m->picked[i] = 0;
        }
        continue;
      }
      if (port_pressed(i, PAD_LEFT)) {
        sound_play_port_sound(0);
        selection_step(m, i, -1);
      } else if (port_pressed(i, PAD_RIGHT)) {
        sound_play_port_sound(0);
        selection_step(m, i, 1);
      } else if (port_pressed(i, PAD_A)) {
        sound_play_port_sound(1);
        m->player_b[i] = 0;
        m->picked[i] = 1;
        sound_play_sequence(i, g_pick_sequences[m->player_a[i]], 0);
      } else if (save_course_flags_count() == SELECT_SECOND_KIND_ITEMS && port_pressed(i, PAD_X)) {
        sound_play_port_sound(1);
        m->player_b[i] = 1;
        m->picked[i] = 1;
        sound_play_sequence(i, g_pick_sequences[m->player_a[i]], 0);
      }
      m->player_a[i] = (uint32_t)g_penguin_order[m->order[i]];
      if (m->player_a[i] == LAST_CHARACTER)
        m->player_b[i] = 0;
    }
    int32_t n = 0;
    while (n < m->player_count && m->picked[n] != 0)
      n++;
    if (n == m->player_count) {
      m->countdown = SELECTION_WAIT;
      m->phase++;
    }
  } else if (m->phase == 2) {
    m->countdown -= 1;
    if (m->countdown < 0)
      menu_set_state(menu, 2);
  }
  menu_penguins_advance(menu);

  if (g_selection_eye->constructed == 0) {
    g_selection_eye->constructed = 1;
    vec3_set(F32(0.0f), F32_BITS(EYE_Y), F32_BITS(EYE_Z), hw_addr(&g_selection_eye->v));
  }
  if (g_selection_target->constructed == 0) {
    g_selection_target->constructed = 1;
    vec3_set(F32(0.0f), F32_BITS(TARGET_Y), F32_BITS(TARGET_Z), hw_addr(&g_selection_target->v));
  }
  house_view_camera(0, hw_addr(&g_selection_eye->v), hw_addr(&g_selection_target->v), F32(0.0f),
                    F32_BITS(CAMERA_FOV));
  icon_follow(menu, 0);

  for (int32_t k = 0; k < MENU_CHARACTERS; k++) {
    if (g_slot_places_built[k] == 0) {
      g_slot_places_built[k] = 1;
      gaddr temp = member_addr(frame, SelectionFrame, temps[MENU_CHARACTERS - 1 - k]);
      vec3_set(slot_place_xy[k][0], slot_place_xy[k][1], F32(-1.0f), temp);
      vec3_copy(hw_addr(&g_slot_places[k]), temp);
    }
  }

  for (int32_t i = 0; i < m->player_count; i++) {
    body_pull(F32(0.5f), member_addr(menu, SetupMenu, slots[i]), hw_addr(&g_slot_places[m->order[i]]));
    body_step(member_addr(menu, SetupMenu, slots[i]));
    if (m->picked[i])
      body1_pull(F32(1.0f), F32(0.5f), member_addr(menu, SetupMenu, alphas[i]));
    else
      body1_pull(F32(0.0f), F32(0.5f), member_addr(menu, SetupMenu, alphas[i]));
    body1_step(member_addr(menu, SetupMenu, alphas[i]));
  }
  hw_stack_free(sizeof(SelectionFrame));
}
HW_EXPORT(8c023d16, selection_update(ARG0))

/* ---- state 2: the course selection ---- */

#define g_house_step  GUEST_VAR(f32, 0x8C0739FCu) /* 8.0: the push on place_a when the variant moves */
#define g_house_lift  GUEST_VAR(f32, 0x8C073A00u) /* 2.0: the push on place_a when the state starts */
#define MENU_COURSES  4
#define MENU_VARIANTS 5     /* the variants of the VS games */
#define PULL_MASCOT   0x3D4CCCCCu /* 0.05f */
#define FADE_RATE     F32(0.125f)
#define NJD_EVAL_HIDE 8u    /* NJS_OBJECT evalflags: not drawn */
#define BLINK_BIT     0x10  /* the lights switch every 16 frames */
#define MENU_LEAVE_TO_RACE 7

/* the phases of state 2 */
enum {
  COURSE_ENTER, COURSE_PICK,
  VARIANT_PICK_ONE,  /* the one-player game: as many variants as the save has opened */
  VARIANT_PICK_4,    /* mode 4: five variants (the mascot stands still) */
  VARIANT_PICK_VS,   /* the other modes: five variants */
};

/* which map nodes each variant shows, by kind (0: the one-player game, 1: the others) */
#define g_variant_marks  ((const int32_t (*)[MENU_VARIANTS][8])hw_ptr(0x8C073AA4u)) /* [kind][variant][8] */
#define g_variant_lights ((const int32_t (*)[MENU_VARIANTS][4])hw_ptr(0x8C073A04u)) /* [kind][variant][4] */
#define g_variant_badge  ((const int32_t (*)[MENU_VARIANTS])hw_ptr(0x8C073BE4u))    /* [kind][variant] */
#define g_course_blink   GUEST_VAR(int32_t, 0x8C095B68u) /* + 1 per frame of the variant choice */

/* the camera of each course: from `eye` at `target`, its roll and field of view */
typedef struct CourseView {
  Vec3 eye;    /* 0x00 */
  Vec3 target; /* 0x0C */
  f32 roll;    /* 0x18 */
  f32 fov;     /* 0x1C */
} CourseView;
HW_SIZE(CourseView, 0x20);
#define g_course_views ((const CourseView *)hw_ptr(0x8C0735C4u))

/* the frame of the course selection (0x34 bytes of locals, PR and r8): the out-parameters at their offsets */
typedef struct CourseFrame {
  gaddr motion;   /* 0x00: the mascot's motion (read, not used) */
  Vec3 origin;    /* 0x04 */
  uint8_t pad10[0x14 - 0x10];
  Vec3 target;    /* 0x14 */
  Vec3 eye;       /* 0x20 */
  uint8_t rest[0x3C - 0x2C];
} CourseFrame;
HW_OFFSET(CourseFrame, origin, 0x04);
HW_OFFSET(CourseFrame, target, 0x14);
HW_OFFSET(CourseFrame, eye, 0x20);
HW_SIZE(CourseFrame, 0x3C);

static inline NjObject *map_node(gaddr table, int32_t i) {
  return (NjObject *)hw_ptr(((const gaddr *)hw_ptr(table))[i]);
}
static void map_hide(gaddr table, int32_t i) { map_node(table, i)->evalflags |= NJD_EVAL_HIDE; }
static void map_show(gaddr table, int32_t i) { map_node(table, i)->evalflags &= ~NJD_EVAL_HIDE; }

/* the variant cursor up (-1) or down (+1) among `count`, with a click and a push on place_a */
static void variant_step(gaddr menu, int32_t step, int32_t count) {
  SetupMenu *m = setup_menu_at(menu);
  sound_play_port_sound(0);
  if (step < 0) {
    m->variant -= 1;
    if (m->variant < 0)
      m->variant = count - 1;
  } else {
    m->variant += 1;
    if (m->variant >= count)
      m->variant = 0;
  }
  f32_add_to(fneg(g_house_step), member_addr(menu, SetupMenu, place_a.force.x));
}

/* the variant choice of phases 3 and 4: B back to the course (phase 1), up / down among five, A to the race */
static void variant_pick_vs(gaddr menu) {
  SetupMenu *m = setup_menu_at(menu);
  if (m->pressed[MENU_PRESS_B]) {
    sound_play_port_sound(3);
    m->phase = COURSE_PICK;
  } else if (m->pressed[MENU_PRESS_UP]) {
    variant_step(menu, -1, MENU_VARIANTS);
  } else if (m->pressed[MENU_PRESS_DOWN]) {
    variant_step(menu, 1, MENU_VARIANTS);
  } else if (m->pressed[MENU_PRESS_A]) {
    sound_play_port_sound(2);
    m->state = MENU_LEAVE_TO_RACE;
  }
}

/* was func_8C02478C: state 2, the course selection. Phase 0 starts it (selection2_enter, a lift of place_a) and
 * goes on as phase 1: B goes back (to the house in the one-player game, else to the penguin selection), left /
 * right step the course (the mascot reset to a random motion), A goes on to the variants (phase 2 in the one-player
 * game, with the save's count of variants; 3 in mode 4; 4 in the others); in phase 1 the mascot loops in place,
 * pulled toward the origin. Phases 2 to 4: B back to phase 1 (from phase 2 by one step), up / down step the variant,
 * A leaves for the race (state 7); the mascot moves on (not in phase 3). Then the course map: in phase 1 every
 * course's variant marks, lights and dots hidden and the first of its badges; else they show the variant's (by
 * the kind of game), the lights blinking; the frame follows, the camera is the course's, the icon follows entry 1
 * and the course's fade goes toward 1, the others toward 0. */
void selection2_update(gaddr menu) {
  gaddr frame = hw_stack_alloc(sizeof(CourseFrame));
  CourseFrame *f = (CourseFrame *)hw_ptr(frame);
  SetupMenu *m = setup_menu_at(menu);
  switch (m->phase) {
  case COURSE_ENTER:
    selection2_enter(menu);
    m->phase++;
    f32_add_to(g_house_lift, member_addr(menu, SetupMenu, place_a.force.y));
    /* fall through */
  case COURSE_PICK:
    if (m->pressed[MENU_PRESS_B]) {
      sound_play_port_sound(3);
      menu_set_state(menu, menu_setup(m)->mode != 0);
    } else if (m->pressed[MENU_PRESS_LEFT]) {
      sound_play_port_sound(0);
      m->course -= 1;
      if (m->course < 0)
        m->course = MENU_COURSES - 1;
      mascot_reset(menu, 1);
    } else if (m->pressed[MENU_PRESS_RIGHT]) {
      sound_play_port_sound(0);
      m->course += 1;
      if (m->course >= MENU_COURSES)
        m->course = 0;
      mascot_reset(menu, 1);
    } else if (m->pressed[MENU_PRESS_A]) {
      sound_play_port_sound(1);
      m->variant = 0;
      if (menu_setup(m)->mode == 0)
        m->phase = VARIANT_PICK_ONE;
      else if (menu_setup(m)->mode == 4)
        m->phase = VARIANT_PICK_4;
      else
        m->phase = VARIANT_PICK_VS;
      m->variant_count = save_course_items_ready(m->course);
    }
    /* the mascot keeps its motion, pulled toward the origin */
    m->model_loop = 0;
    m->model_frame = fadd(m->model_frame, F32(1.0f));
    vec3_set(F32(0.0f), F32(0.0f), F32(0.0f), member_addr(frame, CourseFrame, origin));
    f->motion = ((const MotionEntry *)hw_ptr(m->motion_tables[m->course]))[m->motion].motion;
    body_pull(F32_BITS(PULL_MASCOT), member_addr(menu, SetupMenu, model_body), member_addr(frame, CourseFrame, origin));
    body_step(member_addr(menu, SetupMenu, model_body));
    break;
  case VARIANT_PICK_ONE: {
    int32_t count = save_course_items_ready(m->course);
    if (m->pressed[MENU_PRESS_B]) {
      sound_play_port_sound(3);
      m->phase -= 1;
    } else if (m->pressed[MENU_PRESS_UP]) {
      variant_step(menu, -1, count);
    } else if (m->pressed[MENU_PRESS_DOWN]) {
      variant_step(menu, 1, count);
    } else if (m->pressed[MENU_PRESS_A]) {
      sound_play_port_sound(2);
      m->state = MENU_LEAVE_TO_RACE;
    }
    mascot_update(menu);
    break;
  }
  case VARIANT_PICK_4:
    variant_pick_vs(menu);
    break;
  case VARIANT_PICK_VS:
    variant_pick_vs(menu);
    mascot_update(menu);
    break;
  default:
    break;
  }

  if (m->phase == COURSE_PICK) {
    for (int32_t course = 0; course < MENU_COURSES; course++) {
      for (int32_t k = 0; k < 4; k++) {
        map_hide(m->map_marks, course * 8 + k);
        map_hide(m->map_marks, course * 8 + 4 + k);
        map_hide(m->map_lights, course * 8 + k * 2);
        map_hide(m->map_lights, course * 8 + k * 2 + 1);
        map_hide(m->map_dots, course * 4 + k);
      }
      map_hide(m->map_badges, course * 2);
      map_show(m->map_badges, course * 2 + 1);
    }
    place_follow(menu, 1, 0);
  } else {
    int32_t kind = menu_setup(m)->mode != 0;
    g_course_blink += 1;
    for (int32_t course = 0; course < MENU_COURSES; course++) {
      for (int32_t k = 0; k < 4; k++) {
        if (g_variant_marks[kind][m->variant][k])
          map_show(m->map_marks, course * 8 + k);
        else
          map_hide(m->map_marks, course * 8 + k);
        if (g_variant_marks[kind][m->variant][4 + k])
          map_show(m->map_marks, course * 8 + 4 + k);
        else
          map_hide(m->map_marks, course * 8 + 4 + k);
        if (g_variant_lights[kind][m->variant][k]) {
          if (g_course_blink & BLINK_BIT) {
            map_hide(m->map_lights, course * 8 + k * 2);
            map_show(m->map_lights, course * 8 + k * 2 + 1);
          } else {
            map_show(m->map_lights, course * 8 + k * 2);
            map_hide(m->map_lights, course * 8 + k * 2 + 1);
          }
          map_show(m->map_dots, course * 4 + k);
        } else {
          map_hide(m->map_lights, course * 8 + k * 2);
          map_hide(m->map_lights, course * 8 + k * 2 + 1);
          map_hide(m->map_dots, course * 4 + k);
        }
      }
      if (g_variant_badge[kind][m->variant]) {
        map_show(m->map_badges, course * 2);
        map_hide(m->map_badges, course * 2 + 1);
      } else {
        map_hide(m->map_badges, course * 2);
        map_show(m->map_badges, course * 2 + 1);
      }
    }
    place_follow(menu, 2, m->variant);
  }

  const CourseView *view = &g_course_views[m->course];
  vec3_set(view->target.x, view->target.y, view->target.z, member_addr(frame, CourseFrame, target));
  vec3_set(view->eye.x, view->eye.y, view->eye.z, member_addr(frame, CourseFrame, eye));
  house_view_camera(0, member_addr(frame, CourseFrame, eye), member_addr(frame, CourseFrame, target), view->roll,
                    view->fov);
  icon_follow(menu, 1);
  for (int32_t i = 0; i < MENU_COURSES; i++) {
    if (m->course == i)
      body1_pull(F32(1.0f), FADE_RATE, member_addr(menu, SetupMenu, fades[i]));
    else
      body1_pull(F32(0.0f), FADE_RATE, member_addr(menu, SetupMenu, fades[i]));
    body1_step(member_addr(menu, SetupMenu, fades[i]));
  }
  hw_stack_free(sizeof(CourseFrame));
}
HW_EXPORT(8c02478c, selection2_update(ARG0))

/* ---- the screen of state 2 (and of state 7, leaving it) ---- */

/* functions of other files that no header declares yet */
int32_t text_item_draw(gaddr item);                                                  /* game/house.c */
void material_set(f32 r, f32 g, f32 b, f32 a);                                        /* game/house.c */
void text_set_scale(f32 sx, f32 sy, f32 v);                                           /* core/sprite_text.c */
int32_t game_language(void);                                                          /* game/save.c */
int32_t save_course_flag(int32_t cup, int32_t course, int32_t item);                  /* game/save.c */
int32_t motion_draw_l3b(gaddr object, gaddr motion, gaddr frame, int32_t loop);       /* core/motion.c */
void view_set_current_light_intensity(f32 a, f32 b, f32 c);                           /* core/view_setup.c */
void motion_model_draw(gaddr self);                                                   /* game/menu_sprites.c */

/* ---- still translated ---- */
/* func_8C01529A (core/sprite_text.c): draws a string at (x, y) with the text style */
/* func_8C028882: the variant panel of mode 4 (phase 3) */

/* the screen's text items (house.c's TextItem, 0x28 bytes): the title per language, the course's name per course */
#define TEXT_ITEM_SIZE    0x28u
#define TEXT_TITLES       0x8C073644u
#define TEXT_COURSE_NAMES 0x8C07375Cu
/* the variant's two lines of description: [language][course][one-player, VS][variant][line] */
#define g_variant_lines ((const gaddr (*)[MENU_COURSES][2][MENU_VARIANTS][2])hw_ptr(0x8C093878u))
/* the one-player game's variant names: [language][variant] */
#define g_variant_names ((const gaddr (*)[MENU_VARIANTS])hw_ptr(0x8C095868u))
/* the VS variant names: [language][mode 4, the others][course][variant] */
#define g_vs_variant_names ((const gaddr (*)[2][MENU_COURSES][MENU_VARIANTS])hw_ptr(0x8C095B6Cu))
/* the course model's frame (+ 0.25 per frame drawn) */
#define g_course_model_frame GUEST_VAR(f32, 0x8C095FCCu)

/* select.dat's entry 2: per course its model and motion */
typedef struct CourseModel {
  gaddr object; /* 0x00 */
  gaddr motion; /* 0x04 */
  uint32_t word8;
} CourseModel;
HW_SIZE(CourseModel, 0x0C);

/* the backdrop's and the map's sprite cells */
enum {
  CELL_BACKDROP = 4, CELL_ARROW_LEFT = 0, CELL_ARROW_LEFT_HELD = 1, CELL_ARROW_RIGHT = 2, CELL_ARROW_RIGHT_HELD = 3,
  CELL_COURSE_FIRST = 5, CELL_COURSE_HINT = 9, CELL_VARIANT_TITLE = 10, CELL_PANEL_A = 11, CELL_PANEL_B = 12,
  CELL_PANEL_C = 13, CELL_ITEM_CUP0 = 14, CELL_ITEM_CUP1 = 15,
};
#define BACKDROP_PRIORITY F32(-10000.0f)
#define MAP_PRIORITY      F32(-100.0f)
#define ITEM_PRIORITY     F32(-10.0f)
#define COURSE_ITEMS      5
#define GREY_03 0x3E999999u /* 0.3f */
#define GREY_01 0x3DCCCCCCu /* 0.1f */
#define GREY_08 0x3F4CCCCCu /* 0.8f */
#define GREY_06 0x3F199999u /* 0.6f */
#define GREY_02 0x3E4CCCCCu /* 0.2f */
#define GREY_07 0x3F333333u /* 0.7f */
#define LIGHT_DOWN 0xBF3504F6u /* -sqrt(1/2) */

/* the frame of the original (0x20 bytes of locals and PR): the camera's points at their offsets */
typedef struct CourseDrawFrame {
  Vec3 target; /* 0x00 */
  Vec3 eye;    /* 0x0C */
  uint8_t rest[0x24 - 0x18];
} CourseDrawFrame;
HW_OFFSET(CourseDrawFrame, eye, 0x0C);
HW_SIZE(CourseDrawFrame, 0x24);

static void map_sprite_at(SetupMenu *m, f32 x, f32 y, f32 sy) {
  m->sprite.p.x = x;
  m->sprite.p.y = y;
  m->sprite.sx = F32(1.0f);
  m->sprite.sy = sy;
}
static void map_sprite_draw(gaddr menu, int32_t cell, f32 priority, uint32_t attr) {
  njDrawSprite2D(member_addr(menu, SetupMenu, sprite), cell, priority, attr);
}

/* the text colour of a list entry: yellow when it is the cursor's, else pale blue */
static void list_color(int32_t chosen) {
  if (chosen)
    global_color_set(F32(1.0f), F32(1.0f), F32_BITS(GREY_03), F32(1.0f));
  else
    global_color_set(F32_BITS(GREY_01), F32_BITS(GREY_08), F32(1.0f), F32(1.0f));
}

/* each course's found items of a cup, a cell per found one, 30 pixels apart downward from (x, 80) */
static void course_items_draw(gaddr menu, SetupMenu *m, int32_t cup, f32 x, int32_t cell) {
  m->sprite.p.x = x;
  m->sprite.p.y = F32(80.0f);
  for (int32_t item = 0; item < COURSE_ITEMS; item++) {
    if (item > 0)
      m->sprite.p.y = fadd(m->sprite.p.y, F32(30.0f));
    if (save_course_flag(cup, m->course, item))
      map_sprite_draw(menu, cell, ITEM_PRIORITY, 0x20);
  }
}

/* was func_8C0250E4: the screen of state 2 (and of state 7). The backdrop; from the first phase on, the icon
 * (entry 1). Choosing the course (phase 1): the course's map name (tall), the left / right arrows (lit while held),
 * the frame, a hint, the title and the course's name. Choosing the variant: the course's name, the variants' title,
 * the frame, then (mode 4: its own panel) the course's tint over three panels and the variant's two lines of
 * description; in the one-player game the list of variants (the fourth and fifth once the save opens them) and the
 * course's found items of both cups; in the VS games the list of five. Then the course model turning on the map,
 * and (but in phase 3) the course's camera, its light (from the camera's direction) and the mascot. */
void selection2_draw(gaddr menu) {
  gaddr frame = hw_stack_alloc(sizeof(CourseDrawFrame));
  SetupMenu *m = setup_menu_at(menu);
  m->sprite.tlist = m->texlist;
  m->sprite.p.y = F32(0.0f);
  m->sprite.p.x = F32(0.0f);
  m->sprite.sy = F32(1.0f);
  m->sprite.sx = F32(1.0f);
  map_sprite_draw(menu, CELL_BACKDROP, BACKDROP_PRIORITY, 0);
  m->sprite.tlist = m->map_texlist;
  if (m->phase == COURSE_ENTER) {
    hw_stack_free(sizeof(CourseDrawFrame));
    return;
  }
  icon_draw(menu, 1);
  if (m->phase == COURSE_PICK) {
    map_sprite_at(m, F32(30.0f), F32(124.0f), F32(1.5f));
    map_sprite_draw(menu, m->course + CELL_COURSE_FIRST, MAP_PRIORITY, 0x20);
    map_sprite_at(m, F32(32.0f), F32(41.0f), F32(1.0f));
    map_sprite_draw(menu, pad_held(&m->pad, PAD_LEFT) ? CELL_ARROW_LEFT_HELD : CELL_ARROW_LEFT, MAP_PRIORITY, 0x20);
    map_sprite_at(m, F32(105.0f), F32(41.0f), F32(1.0f));
    map_sprite_draw(menu, pad_held(&m->pad, PAD_RIGHT) ? CELL_ARROW_RIGHT_HELD : CELL_ARROW_RIGHT, MAP_PRIORITY,
                    0x20);
    place_frame_draw(menu);
    map_sprite_at(m, F32(24.0f), F32(358.0f), F32(1.0f));
    map_sprite_draw(menu, CELL_COURSE_HINT, MAP_PRIORITY, 0x20);
    text_item_draw(TEXT_TITLES + (gaddr)game_language() * TEXT_ITEM_SIZE);
    text_item_draw(TEXT_COURSE_NAMES + (gaddr)m->course * TEXT_ITEM_SIZE);
  } else {
    map_sprite_at(m, F32(64.0f), F32(40.0f), F32(1.0f));
    map_sprite_draw(menu, m->course + CELL_COURSE_FIRST, MAP_PRIORITY, 0x20);
    map_sprite_at(m, F32(26.0f), F32(42.0f), F32(1.0f));
    map_sprite_draw(menu, CELL_VARIANT_TITLE, MAP_PRIORITY, 0x20);
    place_frame_draw(menu);
    if (m->phase == VARIANT_PICK_4) {
      variant_panel_draw_mode4(menu);
    } else {
      control3d_push();
      control3d_set(0x10);
      switch (m->course) {
      case 0: material_set(F32(1.0f), F32(0.5f), F32_BITS(GREY_07), F32_BITS(GREY_07)); break;
      case 1: material_set(F32_BITS(GREY_01), F32_BITS(GREY_06), F32_BITS(GREY_03), F32_BITS(GREY_07)); break;
      case 2: material_set(F32(0.0f), F32_BITS(GREY_06), F32(1.0f), F32_BITS(GREY_07)); break;
      case 3: material_set(F32_BITS(GREY_03), F32_BITS(GREY_02), F32_BITS(GREY_08), F32_BITS(GREY_07)); break;
      default: break;
      }
      map_sprite_at(m, F32(24.0f), F32(370.0f), F32(1.0f));
      map_sprite_draw(menu, CELL_PANEL_A, MAP_PRIORITY, 0x22);
      map_sprite_at(m, F32(72.0f), F32(370.0f), F32(1.0f));
      map_sprite_draw(menu, CELL_PANEL_B, MAP_PRIORITY, 0x22);
      map_sprite_at(m, F32(568.0f), F32(362.0f), F32(1.0f));
      map_sprite_draw(menu, CELL_PANEL_C, MAP_PRIORITY, 0x22);
      text_style_reset();
      int32_t vs = m->phase != VARIANT_PICK_ONE;
      const gaddr *lines = g_variant_lines[game_language()][m->course][vs][m->variant];
      text_draw(0x44, 0x172, lines[0]);
      text_draw(0x2C, 0x192, lines[1]);
      control3d_pop();
    }
    if (m->phase == VARIANT_PICK_ONE) {
      text_set_scale(F32(26.0f), F32(26.0f), F32(-2.0f));
      for (int32_t k = 0; k < 3; k++) {
        list_color(m->variant == k);
        text_draw(0x3E, 0x50 + 30 * k, g_variant_names[game_language()][k]);
      }
      if (save_course_items_ready(m->course) >= 4) {
        list_color(m->variant == 3);
        text_draw(0x3E, 0xAA, g_variant_names[game_language()][3]);
      }
      if (save_course_items_ready(m->course) >= 5) {
        list_color(m->variant == 4);
        text_draw(0x3E, 0xC8, g_variant_names[game_language()][4]);
      }
      m->sprite.sx = F32(1.0f);
      m->sprite.sy = F32(1.0f);
      course_items_draw(menu, m, 0, F32(15.0f), CELL_ITEM_CUP0);
      course_items_draw(menu, m, 1, F32(25.0f), CELL_ITEM_CUP1);
    } else if (m->phase == VARIANT_PICK_4 || m->phase == VARIANT_PICK_VS) {
      int32_t kind = m->phase == VARIANT_PICK_4 ? 0 : 1;
      for (int32_t k = 0; k < MENU_VARIANTS; k++) {
        text_set_scale(F32(26.0f), F32(26.0f), F32(0.0f));
        list_color(m->variant == k);
        text_draw(0x3E, 30 * k + 0x50, g_vs_variant_names[game_language()][kind][m->course][k]);
      }
    }
  }

  /* the course model, turning on the map */
  g_course_model_frame = fadd(g_course_model_frame, F32(0.25f));
  njSetTexture(m->map_texlist);
  matrix_push();
  matrix_reset();
  matrix_rotate_x(F32(32768.0f));
  matrix_translate(F32(0.0f), F32(0.0f), F32(-2000.0f));
  matrix_rotate_x(F32(16384.0f));
  matrix_scale_all(F32_BITS(0x4124CCCCu)); /* 10.3 */
  const CourseModel *model = &((const CourseModel *)hw_ptr(m->select_data2))[m->course];
  motion_draw_l3b(model->object, model->motion, hw_addr(&g_course_model_frame), 1);
  matrix_pop();
  if (m->phase == VARIANT_PICK_4) {
    hw_stack_free(sizeof(CourseDrawFrame));
    return;
  }

  const CourseView *view = &g_course_views[m->course];
  vec3_set(view->target.x, view->target.y, view->target.z, member_addr(frame, CourseDrawFrame, target));
  vec3_set(view->eye.x, view->eye.y, view->eye.z, member_addr(frame, CourseDrawFrame, eye));
  house_view_camera(0, member_addr(frame, CourseDrawFrame, eye), member_addr(frame, CourseDrawFrame, target),
                    view->roll, view->fov);
  /* the light along the camera, tipped (course 0 less steeply) */
  view_set_current_light_intensity(F32(0.25f), F32(0.5f), F32_BITS(0x3ECCCCCCu)); /* 0.4 */
  f32 dy = m->course == 0 ? F32_BITS(LIGHT_DOWN) : F32(-1.0f);
  view_current_set_light_dir(fadd(fsub(view->target.x, view->eye.x), F32(1.0f)),
                             fadd(fsub(view->target.y, view->eye.y), dy),
                             fadd(fsub(view->target.z, view->eye.z), F32(-1.0f)));
  view_current_set_light_color(F32(1.0f), F32(1.0f), F32(1.0f));
  motion_model_draw(menu);
  hw_stack_free(sizeof(CourseDrawFrame));
}
HW_EXPORT(8c0250e4, selection2_draw(ARG0))
