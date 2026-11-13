/* Pen Pen port, S17 b04: mode 2's drawing over a view (game/race_draw.c has the other modes): three players on
 * the four views of the split screen, the fourth view (no player) showing the results' pulsing sprite. Over a
 * player's view: its item number, the results' banner, the pause screen, the results' sprites (as the four-view
 * layout: half size from the view's corner, with mode 2's own cells), the ending's item or result; then the
 * screen's polygon. */
#include <string.h>
#include "hw.h"
#include "sdk.h"
#include "../core/view.h"
#include "../core/ninja.h"
#include "../core/sprite_text.h"
#include "menu.h"
#include "race_scene.h"

/* ---- calls ---- */
/* func_8C01723E's neighbours (still translated): the ending's drawers (as game/race_scene.c names them) */
static void ending_draw_item(uint32_t item) { CALL1(0x8C0173E8u, item); }     /* func_8C0173E8 */
static void ending_draw_result2(gaddr result) { CALL1(0x8C018774u, result); } /* func_8C018774 */
/* func_8C016C44: the results' pulsing sprite (steps its counter, then draws by its sine) */
static void results_pulse_draw_b(void) { CALL0(0x8C016C44u); }

/* ---- data ---- */
#define PANEL_TEXLIST     0x8C096070u /* the scene sprite's textures */
#define MODE2_POLYGON     0x8C096294u /* four NJS_POLYGON_VTX over the screen, copied to the stack to draw */
#define RESULT_CELLS_THREE ((const int32_t *)hw_ptr(0x8C096348u)) /* the two cells of the results' sprites */
/* where the item digit goes on each view (x, y from the view's corner) */
typedef struct ScreenPoint {
  f32 x, y;
} ScreenPoint;
HW_SIZE(ScreenPoint, 8);
#define ITEM_DIGIT_AT_THREE ((const ScreenPoint *)hw_ptr(0x8C07FD04u))
#define ITEM_DIGIT_SIZE   0x3A
#define SPRITE_ATTR       0x22
#define EMPTY_VIEW        3 /* the view no player is shown on */

static inline gaddr scene_sprite(gaddr scene) { return member_addr(scene, RaceScene, sprite); }

/* was func_8C02D724: the two results sprites on mode 2's views (after their colour, always): on the results screen
 * while the fade is up, at half the position from the view's corner and half the size */
void race_results_draw_three(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  material_set_2(F32(1.0f), F32(0.0f), F32(0.0f), s->sprite_fade);
  if (s->state != RACE_STATE_RESULTS) return;
  if (!fgt(s->sprite_fade, F32(0.0f))) return;
  const View *v = view_at(g_cur_view);
  f32 ox = v->origin_x, oy = v->origin_y;
  for (int i = 0; i < 2; i++) {
    s->sprite.p.x = fadd(fdiv(s->bodies[i].position.x, F32(2.0f)), ox);
    s->sprite.p.y = fadd(fdiv(s->bodies[i].position.y, F32(2.0f)), oy);
    s->sprite.sx = fdiv(s->values1[i].value, F32(2.0f));
    s->sprite.sy = fdiv(s->values1[i].value, F32(2.0f));
    njDrawSprite2D(scene_sprite(scene), RESULT_CELLS_THREE[i], F32(-5.0f), SPRITE_ATTR);
  }
}
HW_EXPORT(8c02d724, race_results_draw_three(ARG0))

/* was func_8C02DE52: player i's item number (1 + item: red for none, yellow for one) on mode 2's view, at the
 * view's place from its corner (not in state 2, not on the fourth view; player_item indexed as given) */
void race_item_draw_three(gaddr scene, int32_t i) {
  RaceScene *s = race_scene_at(scene);
  if (s->state == 2) return;
  int32_t view = view_current_index();
  if (view == EMPTY_VIEW) return;
  uint32_t item = s->player_item[i];
  if (item == 0)
    material_set_2(F32(1.0f), F32(0.0f), F32(0.0f), F32(1.0f));
  else
    material_set_2(F32(1.0f), F32(1.0f), F32(0.0f), F32(1.0f));
  const View *v = view_at(g_cur_view);
  int32_t y = f32_to_int(fadd(v->origin_y, ITEM_DIGIT_AT_THREE[view].y));
  int32_t x = f32_to_int(fadd(v->origin_x, ITEM_DIGIT_AT_THREE[view].x));
  text_draw_digit_sized(x, y, (int32_t)item + 1, ITEM_DIGIT_SIZE, SPRITE_ATTR);
}
HW_EXPORT(8c02de52, race_item_draw_three(ARG0, (int32_t)ARG1))

/* was func_8C02DB94: in the ending, the item of the player the view shows (none on the followed player's view;
 * nothing on a view without a player), or the second result drawer on the followed player's view */
void race_ending_draw_three(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (s->state != RACE_STATE_ENDING) return;
  if (s->substate == 1) {
    int32_t player = s->view_player[view_current_index()];
    if (player < 0) return;
    if (view_current_index() == s->player_view[s->focus[0]])
      ending_draw_item(0);
    else
      ending_draw_item(s->player_item[player]);
  } else if (s->substate == 3) {
    if (view_current_index() == s->player_view[s->focus[0]]) ending_draw_result2(s->result);
  }
}
HW_EXPORT(8c02db94, race_ending_draw_three(ARG0))

/* the original's frame: the polygon copy at r15, then its saved r13, r14 and PR */
typedef struct Mode2Frame {
  uint8_t polygon[0x40]; /* 0x00 */
  uint8_t saved[0x0C];   /* 0x40 */
} Mode2Frame;
HW_SIZE(Mode2Frame, 0x4C);

/* was func_8C02BAEE: mode 2's drawing over the current view (see the top of the file) */
void race_draw_view_three(gaddr scene) {
  gaddr frame = hw_stack_alloc(sizeof(Mode2Frame));
  njSetTexture(PANEL_TEXLIST);
  control3d_push();
  control3d_set(0x10);
  int32_t player = race_scene_view_player(scene, view_current_index());
  if (player != -1) {
    race_item_draw_three(scene, player);
    race_results_draw_banner(scene, player);
    race_pause_draw_a(scene);
    race_results_draw_three(scene);
    race_ending_draw_three(scene);
  } else {
    results_pulse_draw_b();
  }
  memmove(hw_ptr(frame), hw_ptr(MODE2_POLYGON), sizeof(((Mode2Frame *)0)->polygon));
  njDrawPolygon(frame, 4, 0);
  control3d_pop();
  hw_stack_free(sizeof(Mode2Frame));
}
HW_EXPORT(8c02baee, race_draw_view_three(ARG0))
