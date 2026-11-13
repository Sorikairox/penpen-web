/* Pen Pen port, S18 b01: the game's shutdown after the main loop (game/main.c's game_shutdown: never reached, the
 * game leaves through syBtExit). */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/sb_system.h"
#include "game.h"
#include "vmu.h"

/* SDK (port/symbols/sdk_calls.tsv) */
static inline void sy_Nop2(void) { CALL0(0x8C09FCF2u); }
static inline void syCfgExit(void) { CALL0(0x8C0A0E96u); }
/* hand-written elsewhere, not declared in a header */
static void shared_data_release(void) { CALL0(0x8C018CECu); } /* func_8C018CEC (game/card_messages.c) */
static void sound_finish(void) { CALL0(0x8C016A54u); }        /* func_8C016A54 (game/sound_finish.c) */
static void gfx_shutdown(void) { CALL0(0x8C010D98u); }        /* func_8C010D98 (core/gfx_shutdown.c) */
/* func_8C015296 / func_8C012D18: two empty methods (src/copies/core/inline.c) */
static void empty_8c015296(void) { CALL0(0x8C015296u); }
static void empty_8c012d18(void) { CALL0(0x8C012D18u); }

/* was func_8C016792: the Game destroyed (its root task's destroy, freeing it; the original tests the pointer twice)
 * and forgotten; then every subsystem shut down in turn, the SDK sample's exit last */
void game_shutdown(void) {
  if (g_game != 0) {
    gaddr game = g_game;
    if (game != 0)
      task_call_destroy(task_at(game_at(game)->tree.root), 1);
    g_game = 0;
  }
  sy_Nop2();
  backup_exit();
  empty_8c015296();
  shared_data_release();
  sound_finish();
  gfx_shutdown();
  empty_8c012d18();
  syCfgExit();
  sb_exit_system();
}
HW_EXPORT(8c016792, game_shutdown())
