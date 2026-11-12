/* Pen Pen port, S11: the Game object and its modes (game/game.h). */
#include "game.h"
#include "../core/view.h"
#include "sound.h"
#include "sdk.h"

/* still translated */
static void sb_exit_system(void) { CALL0(0x8C01022Cu); }                     /* func_8C01022C: the SDK sample's exit */

/* was func_8C019DBE: (re)start the game object: no mode yet, `first_mode` next. The root task keeps running when
 * the tree is paused (it is what un-pauses it). */
Game *game_reset(Game *game, int32_t first_mode) {
  tasktree_attach_root(&game->tree);
  game->pad.port = 0;
  game->mode = MODE_NONE;
  game->next_mode = first_mode;
  game->unknown20 = 0;
  task_at(game->tree.root)->stops_on_pause = 0;
  game->reset_exits = 1;
  return game;
}
HW_EXPORT(8c019dbe, RET(hw_addr(game_reset(game_at(ARG0), (int32_t)ARG1))))

/* was func_8C019DF4: the constructor (the Task part is the root task, inside the object) */
Game *game_construct(Game *game, int32_t first_mode) {
  game->tree.root = hw_addr(&game->root_task);
  game->root_task.vtable = GAME_VTABLE;
  task_init(&game->root_task, 0);
  return game_reset(game, first_mode);
}
HW_EXPORT(8c019df4, RET(hw_addr(game_construct(game_at(ARG0), (int32_t)ARG1))))

/* was func_8C01A0D0: Game's draw method: nothing (the scenes under the root draw themselves) */
HW_EXPORT(8c01a0d0, (void)0)


/* ---- the modes' start routines ----
 * Every one has the same shape: say whether a soft reset leaves the game from this mode (some leave the flag as
 * it is: -1 here), allocate the mode's scene, construct it (scene constructors load their data, showing frames
 * meanwhile: they can yield), and hang it under the root. */
static void game_start_mode_scene(uint32_t routine, gaddr game_addr, int32_t reset_exits, uint32_t size,
                                  gaddr construct, int nargs, uint32_t arg) {
  gaddr scene = 0;
  HW_RESUMABLE(routine, game_addr, scene, construct, nargs, arg);
  HW_RESUME_AT(0);
  if (reset_exits >= 0)
    game_at(game_addr)->reset_exits = reset_exits;
  scene = syMalloc(size);
  if (scene)
    HW_YIELDING(0, scene = hw_call(construct, nargs, scene, arg, 0, 0));
  /* (a failed allocation hangs NULL under the root: the original's bug, never reached) */
  task_add_children(task_at(game_at(game_addr)->tree.root), task_at(scene));
}
/* was func_8C01A0D4 */
HW_EXPORT_RESUMABLE(8c01a0d4, game_start_mode_scene(0x8C01A0D4u, ARG0, 1, 0x38, 0x8C01A3EEu, 1, 0))
/* was func_8C01A154 */
HW_EXPORT_RESUMABLE(8c01a154, game_start_mode_scene(0x8C01A154u, ARG0, 1, 0x1C4, 0x8C01B77Au, 2, 1))
/* was func_8C01A188 */
HW_EXPORT_RESUMABLE(8c01a188, game_start_mode_scene(0x8C01A188u, ARG0, 1, 0x28, 0x8C01DDE2u, 1, 0))
/* was func_8C01A1B8: mode 4 (GOOD NIGHT; S15) */
HW_EXPORT_RESUMABLE(8c01a1b8, game_start_mode_scene(0x8C01A1B8u, ARG0, 0, 0x88, 0x8C01D4CAu, 1, 0))
/* was func_8C01A1F2: mode 7, with the argument the previous mode left */
HW_EXPORT_RESUMABLE(8c01a1f2, game_start_mode_scene(0x8C01A1F2u, ARG0, 0, 0xA8, 0x8C01A8DEu, 2, ARG1))
/* was func_8C01A248: the demo, the same scene as mode 7 with argument 5 */
HW_EXPORT_RESUMABLE(8c01a248, game_start_mode_scene(0x8C01A248u, ARG0, 1, 0xA8, 0x8C01A8DEu, 2, 5))

/* the start routine of each mode (MODE_QUIT has none). Those of the modes the tested runs visit (0, 2, 3, 7, 8) are
 * the hand-written ones above; the others (modes 1, 4, 5, 6, 9, 11, 12: the same shape, modes 5 and 6 empty) stay
 * translated until a run reaches them. */
static const gaddr mode_start[MODE_COUNT] = {
  0x8C01A0D4u, 0x8C01A120u, 0x8C01A154u, 0x8C01A188u, 0x8C01A1B8u, 0x8C01A1EAu, 0x8C01A1EEu,
  0x8C01A1F2u, 0x8C01A248u, 0x8C01A27Cu, 0, 0x8C01A2AEu, 0x8C01A2D8u,
};

/* the soft reset: A+B+X+Y held and Start pressed, on any controller */
static int soft_reset_combo(const Game *game) {
  if (pad_held(&game->pad, PAD_A) && pad_held(&game->pad, PAD_B) && pad_held(&game->pad, PAD_X) &&
      pad_held(&game->pad, PAD_Y) && pad_pressed(&game->pad, PAD_START))
    return 1;
  for (int port = 1; port < PAD_PORTS; port++)
    if (port_held(port, PAD_A) && port_held(port, PAD_B) && port_held(port, PAD_X) && port_held(port, PAD_Y) &&
        port_pressed(port, PAD_START))
      return 1;
  return 0;
}

/* was func_8C019E8C: Game's update, the root of every frame's update pass: input, the soft reset, mode switches,
 * the demo's time-out, and the sound driver's frame. Resumable (a mode's start routine loads its scene). */
void game_update(gaddr game_addr) {
  int32_t starting = 0;
  HW_RESUMABLE(0x8C019E8Cu, game_addr, starting);
  Game *game = game_at(game_addr);
  HW_RESUME_AT(0);

  pad_read_all();
  views_reset();

  if (soft_reset_combo(game)) {
    tasktree_set_pause(&game->tree, 0);
    g_video_running = 1;
    g_soft_reset = 1;
    if (game->reset_exits) {
      sb_exit_system();
      syBtExit();
    } else {
      game->mode = MODE_NONE;
      game->next_mode = MODE_TITLE;
    }
  }

  if (game->mode != game->next_mode) {
    /* the old mode's scene goes */
    Task *root = task_at(game->tree.root);
    if (root->child) {
      task_destroy_tree(task_at(root->child));
      task_at(game->tree.root)->child = 0;
    }
    game->mode = game->next_mode;
    game->mode_frames = 0;
    if ((uint32_t)game->next_mode < MODE_COUNT) {
      starting = game->next_mode;
      if (starting == MODE_QUIT)
        g_quit_request = -1;
      else
        HW_YIELDING(0, hw_call(mode_start[starting], 2, game_addr, (uint32_t)game->mode_arg, 0, 0));
    }
  }

  game->mode_frames++;
  if (game->mode == MODE_DEMO) {
    if (pad_pressed(&game->pad, PAD_START))
      game->next_mode = MODE_TITLE;
    if (game->mode_frames >= DEMO_MODE_FRAMES)
      game->next_mode = MODE_TITLE;
  }
  /* the original hands (1, the pause state) to the sound update, which does not use them in this build */
  (void)task_is_paused(task_at(game->tree.root));
  sound_update();
}
HW_EXPORT_RESUMABLE(8c019e8c, game_update(ARG0))
