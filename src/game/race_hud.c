/* Pen Pen port, S14 (batch b05): the race scene's screen furniture (0x8C02C798-0x8C02EF70, 0x8C030A5C): the
 * panel drawn over a split screen, the pause screen (its input and its three drawers), the sprites of the results
 * and of the ending, the item digits, and the per-player sound requests that pick the helper of the game's mode.
 *
 * The scene (RaceScene, game/race_scene.h) keeps one NJS_SPRITE of its own at 0xA5C that the results, the ending
 * and the pause screen move around and draw cell by cell; their fade is the scene's sprite_fade (results, ending)
 * or pause_fade (pause screen), and the colour goes to Ninja as a constant material. The two results / ending
 * sprites follow the scene's bodies[0..1] (position) and values1[0..1] (size); on a split screen they are placed
 * from the current view's corner and halved in height (and in width on four views).
 *
 * The pause screen belongs to the player who paused (pause_port): while it is up the scene's tree is paused; the
 * pad moves the cursor (up / down, wrapping over the menu's lines) and A or Start confirms a line: the line's sound,
 * and for the others than "continue" the scene's next state; then the tree is unpaused and the pad is ignored for
 * 30 frames. Pulling that player's controller out shows a message instead, until it is back. */
#include "hw.h"
#include "sdk.h"
#include "../core/pad.h"
#include "../core/task.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "../core/sprite_text.h"
#include "game.h"
#include "sound.h"
#include "race_scene.h"
#include "racer.h"

/* ---- calls ---- */

/* func_8C028F08: njSetConstantMaterial of {a, r, g, b} = the four floats (r, g, b, a) */
static void material_set(f32 r, f32 g, f32 b, f32 a) { FARGS4(r, g, b, a); CALL0(0x8C028F08u); }
/* sd_CmdReset (Manatee): reset the sound driver's command queue */
static inline void sd_CmdReset(void) { CALL0(0x8C09A524u); }
/* njColorBlendingMode (Ninja): the source / destination blend */
static inline void njColorBlendingMode(int32_t target, int32_t mode) { CALL2(0x8C0A23A8u, target, mode); }

/* the pause screen's drawers (still translated; the "B" pair is the other layer of the same pass) */
static void pause_draw_menu_a(int32_t cursor) { CALL1(0x8C017AC8u, cursor); }               /* func_8C017AC8 */
static void pause_draw_unplugged_a(void) { CALL0(0x8C018B82u); }                              /* func_8C018B82 */
static void pause_draw_menu_b(int32_t cursor) { CALL1(0x8C017848u, cursor); }               /* func_8C017848 */
static void pause_draw_unplugged_b(void) { CALL0(0x8C018ADCu); }                              /* func_8C018ADC */
static void pause_draw_menu_c(int32_t cursor, f32 fade) { FARGS1(fade); CALL1(0x8C0175ACu, cursor); } /* func_8C0175AC */
static void pause_draw_unplugged_c(void) { CALL0(0x8C0189E8u); }                              /* func_8C0189E8 */
/* the ending's result drawers (still translated) */
static void ending_draw_result_a(gaddr result) { CALL1(0x8C017D40u, result); }              /* func_8C017D40 */
static void ending_draw_result_b(gaddr result) { CALL1(0x8C018010u, result); }              /* func_8C018010 */
static void ending_draw_result_c(gaddr result) { CALL1(0x8C0182D4u, result); }              /* func_8C0182D4 */
/* the results' banner of a player that finished, one per kind of game (still translated) */
static void results_draw_banner_one(void) { CALL0(0x8C016E98u); }                           /* func_8C016E98 */
static void results_draw_banner_split(void) { CALL0(0x8C016FD4u); }                         /* func_8C016FD4 */
static void results_draw_banner_four(void) { CALL0(0x8C0170F8u); }                          /* func_8C0170F8 */
/* the MIDI requests of the split and four-player games (still translated) */
static void race_midi_play_split(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence) {
  CALL5(0x8C02F186u, scene, i, port, priority, sequence); /* func_8C02F186 */
}
static void race_midi_play_panned(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence) {
  CALL5(0x8C02F1E8u, scene, i, port, priority, sequence); /* func_8C02F1E8 */
}
/* func_8C041880: the course's per-frame placement of an object (still translated) */
static void course_object_update(gaddr course, gaddr object) { CALL2(0x8C041880u, course, object); }

/* ---- data ---- */

/* a line of the pause menu: the sound it plays and the scene state it goes to */
typedef struct PauseLine {
  int32_t sound;      /* 0x00: > 0: played on port 7 */
  int32_t next_state; /* 0x04 */
} PauseLine;
HW_SIZE(PauseLine, 8);
#define g_pause_last_line   GUEST_VAR(int32_t, 0x8C07FC30u)            /* the last line's index (3) */
#define PAUSE_LINES_ONE     ((const PauseLine *)hw_ptr(0x8C07FC34u))   /* modes 0 and 4 */
#define PAUSE_LINES_SPLIT   ((const PauseLine *)hw_ptr(0x8C07FC54u))   /* modes 1 to 3 */
#define PAUSE_DELAY         30
#define PAUSE_FADE_STEP     0x3D088888u /* 1/30 (0.0333333f) */
#define PAUSE_FADE_STEP_NEG 0xBD088888u /* -1/30 */

/* the panel over a split screen: a sprite template (copied to the stack), its texture list and a scrolling
 * counter */
#define PANEL_SPRITE        0x8C096314u
#define PANEL_TEXLIST       0x8C096070u
#define g_panel_scroll      GUEST_VAR(int32_t, 0x8C096334u)
#define PANEL_WIDTH         640
#define F_0_1               0x3DCCCCCCu /* 0.1f (as stored) */
#define F_0_3               0x3E999999u /* 0.3f */
#define F_0_4               0x3ECCCCCCu /* 0.4f */
#define F_0_6               0x3F199999u /* 0.6f */
#define F_0_8               0x3F4CCCCCu /* 0.8f */
#define F_0_9               0x3F666666u /* 0.9f */
#define F_0_2               0x3E4CCCCCu /* 0.2f */

/* the two cells of the results' sprites, per screen layout, and of the ending's */
#define RESULT_CELLS_FULL   ((const int32_t *)hw_ptr(0x8C096338u))
#define RESULT_CELLS_TWO    ((const int32_t *)hw_ptr(0x8C096340u))
#define RESULT_CELLS_FOUR   ((const int32_t *)hw_ptr(0x8C096350u))
#define ENDING_CELLS        ((const int32_t *)hw_ptr(0x8C096358u))

/* where the item digit goes on each view (x, y from the view's corner) */
typedef struct ScreenPoint {
  f32 x, y;
} ScreenPoint;
HW_SIZE(ScreenPoint, 8);
#define ITEM_DIGIT_AT_TWO   ((const ScreenPoint *)hw_ptr(0x8C07FCF4u))
#define ITEM_DIGIT_AT_FOUR  ((const ScreenPoint *)hw_ptr(0x8C07FD24u))

#define SPRITE_ATTR         0x22

static inline gaddr scene_sprite(gaddr scene) { return member_addr(scene, RaceScene, sprite); }
static void race_scene_set_state(RaceScene *s, int32_t state) { /* as func_8C0291E2 */
  s->state = state;
  s->substate = 0;
}

/* ---- the players ---- */

/* was func_8C02C798: player i's velocity (the address of its moving body's) */
gaddr race_player_velocity(gaddr scene, int32_t i) { return racer_velocity(race_scene_at(scene)->players[i]); }
HW_EXPORT(8c02c798, RET(race_player_velocity(ARG0, (int32_t)ARG1)))

/* was func_8C02C858: player i's state */
int32_t race_player_state(gaddr scene, int32_t i) { return racer_state(race_scene_at(scene)->players[i]); }
HW_EXPORT(8c02c858, RET(race_player_state(ARG0, (int32_t)ARG1)))

/* ---- the panel over a split screen ---- */

/* the frame of race_scene_draw_panel: the sprite at the bottom (its address goes to Ninja) */
typedef struct PanelFrame {
  NjSprite sprite;     /* 0x00 */
  uint32_t spill[2];   /* 0x20: the arguments' slots */
  uint32_t saved_pr;   /* 0x28 */
} PanelFrame;
HW_SIZE(PanelFrame, 0x2C);

static void panel_draw_cells(gaddr sprite, const int32_t *cells, int n) {
  for (int k = 0; k < n; k++)
    njDrawSprite2D(sprite, cells[k], F32(-2.0f), SPRITE_ATTR);
}

/* was func_8C02CC3E (with its tail func_8C02CC60, which Ghidra split off at the block copy's return): the panel
 * over a split screen (split = the view layout, game.h): its frame cells tinted by the kind of game, then a strip
 * that scrolls by one pixel per V-blank, drawn twice to wrap around the screen width */
void race_scene_draw_panel(gaddr scene, int32_t split) {
  static const int32_t cells_two[] = {2, 3, 4, 5, 7};
  static const int32_t cells_four[] = {2, 3, 4, 5};
  gaddr frame = hw_stack_alloc(sizeof(PanelFrame));
  gaddr sprite = member_addr(frame, PanelFrame, sprite);
  NjSprite *sp = (NjSprite *)hw_ptr(sprite);
  njColorBlendingMode(0, 8);
  njColorBlendingMode(1, 6);
  memmove(sp, hw_ptr(PANEL_SPRITE), sizeof(NjSprite));
  int32_t scroll = g_panel_scroll + g_vblanks_pending + 1;
  g_panel_scroll = scroll;
  g_panel_scroll = scroll % PANEL_WIDTH; /* the SHC runtime's signed remainder */
  if (split == 1 || split == 2) {
    njSetTexture(PANEL_TEXLIST);
    material_set(F32_BITS(F_0_9), F32(0.5f), F32(0.0f), F32(1.0f));
    panel_draw_cells(sprite, cells_two, 5);
    material_set(F32(1.0f), F32(1.0f), F32(1.0f), F32(1.0f));
  } else if (split == 3) {
    njSetTexture(PANEL_TEXLIST);
    int32_t mode = race_setup_at(race_scene_at(scene)->setup)->mode;
    if (mode == 2)
      material_set(F32_BITS(F_0_3), F32_BITS(F_0_8), F32_BITS(F_0_1), F32(1.0f));
    else
      material_set(F32(0.0f), F32_BITS(F_0_4), F32_BITS(F_0_6), F32(1.0f));
    panel_draw_cells(sprite, cells_four, 4);
    mode = race_setup_at(race_scene_at(scene)->setup)->mode;
    njDrawSprite2D(sprite, mode == 2 ? 8 : 6, F32(-2.0f), SPRITE_ATTR);
    material_set(F32(1.0f), F32(1.0f), F32(1.0f), F32(1.0f));
  } else {
    hw_stack_free(sizeof(PanelFrame));
    return;
  }
  /* the scrolling strip: at 640 - scroll, then one screen width to the left of that */
  sp->p.x = fsub(F32(640.0f), f32_from_int(g_panel_scroll));
  njDrawSprite2D(sprite, 9, F32(-1.0f), SPRITE_ATTR);
  sp->p.x = fsub(sp->p.x, F32(640.0f));
  njDrawSprite2D(sprite, 9, F32(-1.0f), SPRITE_ATTR);
  hw_stack_free(sizeof(PanelFrame));
}
HW_EXPORT(8c02cc3e, race_scene_draw_panel(ARG0, (int32_t)ARG1))

/* ---- the pause screen ---- */

/* the pausing player's port, when it is one of the four (else no controller test) */
static int pause_port_valid(int32_t port) { return port >= 0 && port <= 3; }

/* was func_8C02CE48: the pause screen's input (while the delay after a choice runs out and the drive is ready):
 * the controller check, then up / down / A or Start on the pausing player's port */
void race_pause_update(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (s->pause_delay > 0) return;
  if (gdFsGetDrvStat() == 0) return;
  int32_t port = s->pause_port;
  if (s->pad_removed != 0) {
    if (pause_port_valid(s->pause_port) && port_has_controller(s->pause_port)) s->pad_removed = 0;
    return;
  }
  if (pause_port_valid(s->pause_port) && !port_has_controller(s->pause_port)) {
    s->pad_removed = 1;
    return;
  }
  if (port_pressed(port, PAD_UP)) {
    sound_play(7, 5, 0);
    s->pause_cursor--;
    if (s->pause_cursor < 0) s->pause_cursor = g_pause_last_line;
    return;
  }
  if (port_pressed(port, PAD_DOWN)) {
    sound_play(7, 5, 0);
    s->pause_cursor++;
    if (s->pause_cursor > g_pause_last_line) s->pause_cursor = 0;
    return;
  }
  if (!port_pressed(port, PAD_START) && !port_pressed(port, PAD_A)) return;
  if (s->pause_cursor == 0) { /* continue */
    if (PAUSE_LINES_ONE[s->pause_cursor].sound > 0) sound_play(7, PAUSE_LINES_ONE[s->pause_cursor].sound, 0);
  } else if (s->pause_cursor == 1) { /* the sound is cut before the scene changes */
    sound_reset_requests();
    sd_CmdReset();
    race_scene_set_state(s, PAUSE_LINES_ONE[s->pause_cursor].next_state);
  } else {
    int32_t mode = race_setup_at(s->setup)->mode;
    if (mode == 0 || mode == 4) {
      sound_play(7, PAUSE_LINES_ONE[s->pause_cursor].sound, 0);
      race_scene_set_state(s, PAUSE_LINES_ONE[s->pause_cursor].next_state);
    } else if (mode == 1 || mode == 2 || mode == 3) {
      sound_play(7, PAUSE_LINES_SPLIT[s->pause_cursor].sound, 0);
      race_scene_set_state(s, PAUSE_LINES_SPLIT[s->pause_cursor].next_state);
    }
  }
  s->pause_delay = PAUSE_DELAY;
  tasktree_set_pause(tasktree_at(s->task.owner), 0);
}
HW_EXPORT(8c02ce48, race_pause_update(ARG0))

/* holding X and Y on the scene's pad hides the pause screen */
static int pause_hidden(RaceScene *s) { return pad_held(&s->pad, PAD_X) && pad_held(&s->pad, PAD_Y); }

/* the pause screen is drawn on the pausing player's view while the scene is paused */
static int pause_shown_here(RaceScene *s) {
  if (pause_hidden(s)) return 0;
  if (!task_is_paused(&s->task)) return 0;
  return view_current_index() == s->pause_port;
}

/* was func_8C02D4F0: the pause menu's first layer (or the unplugged-controller message) */
void race_pause_draw_a(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (!pause_shown_here(s)) return;
  if (s->pad_removed != 0)
    pause_draw_unplugged_a();
  else
    pause_draw_menu_a(s->pause_cursor);
}
HW_EXPORT(8c02d4f0, race_pause_draw_a(ARG0))

/* was func_8C02D3F4: its second layer */
void race_pause_draw_b(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (!pause_shown_here(s)) return;
  if (s->pad_removed != 0)
    pause_draw_unplugged_b();
  else
    pause_draw_menu_b(s->pause_cursor);
}
HW_EXPORT(8c02d3f4, race_pause_draw_b(ARG0))

/* was func_8C02D2E2: the full-screen pause layer: while paused, the menu (faded) and a fade that rises, else a
 * fade that falls (the update steps pause_fade by pause_fade_step); while it is above 0, four cells of the scene's
 * sprite over the whole screen. With the controller out, the message only, and the fade back to 0. */
void race_pause_draw_overlay(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (pause_hidden(s)) return;
  if (task_is_paused(&s->task)) {
    if (s->pad_removed != 0) {
      pause_draw_unplugged_c();
      s->pause_fade = F32(0.0f);
      return;
    }
    pause_draw_menu_c(s->pause_cursor, s->pause_fade);
    s->pause_fade_step = F32_BITS(PAUSE_FADE_STEP);
  } else {
    s->pause_fade_step = F32_BITS(PAUSE_FADE_STEP_NEG);
  }
  if (!fgt(s->pause_fade, F32(0.0f))) return;
  material_set(F32(1.0f), F32(1.0f), F32(1.0f), s->pause_fade);
  s->sprite.p.x = F32(0.0f);
  s->sprite.p.y = F32(0.0f);
  s->sprite.sx = F32(1.0f);
  s->sprite.sy = F32(1.0f);
  for (int32_t cell = 0xB; cell <= 0xE; cell++)
    njDrawSprite2D(scene_sprite(scene), cell, F32(-1.5f), SPRITE_ATTR);
}
HW_EXPORT(8c02d2e2, race_pause_draw_overlay(ARG0))

/* ---- the results' and the ending's sprites ---- */

enum { SPRITES_FULL, SPRITES_TWO, SPRITES_FOUR };

/* the two results sprites (after their colour is set: always): on the results screen while the fade is up */
static void results_sprites_draw(gaddr scene, const int32_t *cells, int layout) {
  RaceScene *s = race_scene_at(scene);
  material_set(F32(1.0f), F32(0.0f), F32(0.0f), s->sprite_fade);
  if (s->state != RACE_STATE_RESULTS) return;
  if (!fgt(s->sprite_fade, F32(0.0f))) return;
  f32 ox = F32(0.0f), oy = F32(0.0f);
  if (layout != SPRITES_FULL) {
    const View *v = view_at(g_cur_view);
    ox = v->origin_x;
    oy = v->origin_y;
  }
  for (int i = 0; i < 2; i++) {
    f32 x = s->bodies[i].position.x, y = s->bodies[i].position.y, size = s->values1[i].value;
    if (layout == SPRITES_FULL) {
      s->sprite.p.x = x;
      s->sprite.p.y = y;
      s->sprite.sx = size;
      s->sprite.sy = size;
    } else if (layout == SPRITES_TWO) { /* half height */
      s->sprite.p.x = fadd(x, ox);
      s->sprite.p.y = fadd(fdiv(y, F32(2.0f)), oy);
      s->sprite.sx = size;
      s->sprite.sy = size;
    } else { /* a quarter of the screen */
      s->sprite.p.x = fadd(fdiv(x, F32(2.0f)), ox);
      s->sprite.p.y = fadd(fdiv(y, F32(2.0f)), oy);
      s->sprite.sx = fdiv(size, F32(2.0f));
      s->sprite.sy = fdiv(size, F32(2.0f));
    }
    njDrawSprite2D(scene_sprite(scene), cells[i], F32(-5.0f), SPRITE_ATTR);
  }
}

/* was func_8C02D574 / func_8C02D638 / func_8C02D814: the results sprites on a full screen / a two-view split / a
 * four-view split */
void race_results_draw_full(gaddr scene) { results_sprites_draw(scene, RESULT_CELLS_FULL, SPRITES_FULL); }
void race_results_draw_two(gaddr scene) { results_sprites_draw(scene, RESULT_CELLS_TWO, SPRITES_TWO); }
void race_results_draw_four(gaddr scene) { results_sprites_draw(scene, RESULT_CELLS_FOUR, SPRITES_FOUR); }
HW_EXPORT(8c02d574, race_results_draw_full(ARG0))
HW_EXPORT(8c02d638, race_results_draw_two(ARG0))
HW_EXPORT(8c02d814, race_results_draw_four(ARG0))

/* the ending's first step: its two sprites, tinted per sprite, while the fade is up; returns whether the
 * ending is at its result step instead */
static int ending_sprites_draw(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (s->state != RACE_STATE_ENDING) return 0;
  if (s->substate == ENDING_SPRITES) {
    if (!fgt(s->sprite_fade, F32(0.0f))) return 0;
    for (int i = 0; i < 2; i++) {
      s->sprite.p.x = s->bodies[i].position.x;
      s->sprite.p.y = s->bodies[i].position.y;
      s->sprite.sx = s->values1[i].value;
      s->sprite.sy = s->values1[i].value;
      material_set(F32(1.0f), F32(0.0f), F32(0.0f), s->sprite_fade);
      njDrawSprite2D(scene_sprite(scene), ENDING_CELLS[i], F32(-1.0f), SPRITE_ATTR);
    }
    return 0;
  }
  return s->substate == ENDING_RESULT;
}

/* was func_8C02D904: the ending's sprites, then its result (one of two drawers, by ending_kind) */
void race_ending_draw_full(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (!ending_sprites_draw(scene)) return;
  if (s->ending_kind == 0)
    ending_draw_result_a(s->result);
  else
    ending_draw_result_b(s->result);
}
HW_EXPORT(8c02d904, race_ending_draw_full(ARG0))

/* was func_8C02DA24: the same with the third result drawer */
void race_ending_draw_other(gaddr scene) {
  if (!ending_sprites_draw(scene)) return;
  ending_draw_result_c(race_scene_at(scene)->result);
}
HW_EXPORT(8c02da24, race_ending_draw_other(ARG0))

/* ---- the item digits ---- */

/* red for no item, yellow for one */
static void item_color(uint32_t item) {
  if (item == 0)
    material_set(F32(1.0f), F32(0.0f), F32(0.0f), F32(1.0f));
  else
    material_set(F32(1.0f), F32(1.0f), F32(0.0f), F32(1.0f));
}

/* was func_8C02DCDC: player i's item number (1 + item) on a full screen, then a green picture and, in light green,
 * the set-up's number at 0x64 */
void race_item_draw_full(gaddr scene, int32_t i) {
  RaceScene *s = race_scene_at(scene);
  if (s->state == 2) return;
  uint32_t item = s->player_item[i];
  item_color(item);
  text_draw_digit_sized(0x1EE, 0x1E, (int32_t)item + 1, 0x52, SPRITE_ATTR);
  material_set(F32(0.0f), F32(1.0f), F32(0.0f), F32(1.0f));
  sprite_big_draw(0xE, SPRITE_ATTR, F32(547.0f), F32(50.0f), F32(0.71875f));
  material_set(F32_BITS(F_0_4), F32_BITS(F_0_9), F32_BITS(F_0_2), F32(1.0f));
  text_draw_digit_sized(0x239, 0x48, race_setup_at(s->setup)->player_count, 0x2E, SPRITE_ATTR);
}
HW_EXPORT(8c02dcdc, race_item_draw_full(ARG0, (int32_t)ARG1))

/* player i's item number on the current view, at the layout's place from the view's corner */
static void item_draw_on_view(gaddr scene, int32_t i, const ScreenPoint *places, int32_t size) {
  RaceScene *s = race_scene_at(scene);
  if (s->state == 2) return;
  int32_t view = view_current_index();
  uint32_t item = s->player_item[i];
  item_color(item);
  const View *v = view_at(g_cur_view);
  int32_t y = f32_to_int(fadd(v->origin_y, places[view].y));
  int32_t x = f32_to_int(fadd(v->origin_x, places[view].x));
  text_draw_digit_sized(x, y, (int32_t)item + 1, size, SPRITE_ATTR);
}

/* was func_8C02DDC0 / func_8C02DF04: on a two-view / four-view split */
void race_item_draw_two(gaddr scene, int32_t i) { item_draw_on_view(scene, i, ITEM_DIGIT_AT_TWO, 0x40); }
void race_item_draw_four(gaddr scene, int32_t i) { item_draw_on_view(scene, i, ITEM_DIGIT_AT_FOUR, 0x3A); }
HW_EXPORT(8c02ddc0, race_item_draw_two(ARG0, (int32_t)ARG1))
HW_EXPORT(8c02df04, race_item_draw_four(ARG0, (int32_t)ARG1))

/* was func_8C02E6F2: on the results screen (not paused), the banner of player i when it is in action 1 with its
 * word at 0x55C set, by the kind of game */
void race_results_draw_banner(gaddr scene, int32_t i) {
  RaceScene *s = race_scene_at(scene);
  if (task_is_paused(&s->task)) return;
  if (s->state != RACE_STATE_RESULTS) return;
  if (racer_action(s->players[i]) != 1) return;
  if (racer_word55c(s->players[i]) == 0) return;
  int32_t mode = race_setup_at(s->setup)->mode;
  if (mode == 0 || mode == 4)
    results_draw_banner_one();
  else if (mode == 1)
    results_draw_banner_split();
  else if (mode == 2 || mode == 3)
    results_draw_banner_four();
}
HW_EXPORT(8c02e6f2, race_results_draw_banner(ARG0, (int32_t)ARG1))

/* ---- sound by the kind of game ---- */

/* was func_8C02E948: player i's one-shot through the helper of the game's mode (race_scene.c) */
void race_sound_play_by_mode(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sound, int32_t pan,
                             int32_t pitch) {
  int32_t mode = race_setup_at(race_scene_at(scene)->setup)->mode;
  if (mode == 0 || mode == 4)
    race_sound_play(scene, i, port, priority, sound, pan, pitch);
  else if (mode == 1)
    race_sound_play_split(scene, i, port, priority, sound, pan, pitch);
  else if (mode == 2 || mode == 3)
    race_sound_play_panned(scene, i, port, priority, sound, pan, pitch);
}
HW_EXPORT(8c02e948, race_sound_play_by_mode(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4, (int32_t)ARG5, (int32_t)ARG6))

/* was func_8C02E9AA: the same for a change of the playing sound */
void race_sound_adjust_by_mode(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t volume, int32_t pan,
                               int32_t pitch) {
  int32_t mode = race_setup_at(race_scene_at(scene)->setup)->mode;
  if (mode == 0 || mode == 4)
    race_sound_adjust(scene, i, port, priority, volume, pan, pitch);
  else if (mode == 1)
    race_sound_adjust_split(scene, i, port, priority, volume, pan, pitch);
  else if (mode == 2 || mode == 3)
    race_sound_adjust_panned(scene, i, port, priority, volume, pan, pitch);
}
HW_EXPORT(8c02e9aa, race_sound_adjust_by_mode(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4, (int32_t)ARG5, (int32_t)ARG6))

/* was func_8C02EF04: the same for a MIDI sequence */
void race_midi_play_by_mode(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence) {
  int32_t mode = race_setup_at(race_scene_at(scene)->setup)->mode;
  if (mode == 0 || mode == 4)
    race_midi_play(scene, i, port, priority, sequence);
  else if (mode == 1)
    race_midi_play_split(scene, i, port, priority, sequence);
  else if (mode == 2 || mode == 3)
    race_midi_play_panned(scene, i, port, priority, sequence);
}
HW_EXPORT(8c02ef04, race_midi_play_by_mode(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* ---- the course ---- */

/* was func_8C030A5C: hand an object to the scene's course for its per-frame placement */
void race_scene_course_update(gaddr scene, gaddr object) { course_object_update(race_scene_at(scene)->course, object); }
HW_EXPORT(8c030a5c, race_scene_course_update(ARG0, ARG1))
