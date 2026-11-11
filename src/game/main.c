/* Pen Pen port, S11: start-up, the main loop and the per-frame callbacks. */
#include "game.h"
#include "../core/view.h"
#include "sound.h"
#include "sdk.h"
#include "../core/file.h"

/* still translated (the start-up of each subsystem; names say what they are where known) */
static void sb_init_system(int32_t mode, int32_t frame, int32_t count) { CALL3(0x8C010184u, mode, frame, count); } /* the SDK sample's sbInitSystem, linked into the game */
static void sb_exit_system(void) { CALL0(0x8C01022Cu); }
static void graphics_init(void) { CALL0(0x8C010C28u); }    /* heaps, texture/vertex buffers, matrices, lights */
static int32_t video_cable_type(void) { return (int32_t)CALL0(0x8C0623EEu); }
static int32_t video_info_b(void) { return (int32_t)CALL0(0x8C0623B4u); }
static int32_t video_info_c(void) { return (int32_t)CALL0(0x8C062440u); }
static void game_shutdown(void) { CALL0(0x8C016792u); }    /* never reached: the game leaves through syBtExit */
static void run_static_destructors(void) { CALL0(0x8C016510u); }
static void draw_panel(gaddr panel, int32_t split_mode) { CALL2(0x8C02CC3Eu, panel, split_mode); }
#define func_8C061DB8() CALL0(0x8C061DB8u)
#define func_8C018CBA() CALL0(0x8C018CBAu)
#define func_8C015224() CALL0(0x8C015224u)
#define func_8C018DD0() CALL0(0x8C018DD0u)
#define func_8C018C20(a, b) CALL2(0x8C018C20u, (a), (b))

void video_init(void);
void drive_watch(void);

/* was func_8C0164EC: run the C++ static constructors. The table holds pairs of words and the loop steps by
 * pairs: only the first word of each pair is called. */
void run_static_constructors(void) {
  for (gaddr entry = GUEST_VAR(gaddr, CTORS_BEGIN); entry < GUEST_VAR(gaddr, CTORS_END); entry += 8)
    CALL0(GUEST_VAR(gaddr, entry));
}
HW_EXPORT(8c0164ec, run_static_constructors())

/* was func_8C0164B4: main. Each frame: one update + draw (game_frame), then wait for the V-blank, where the
 * runtime ends the frame (resumable: the whole game runs inside this loop). Like the original (which holds their
 * addresses in registers), main enters its three stages through their entry points: the difftest samples them
 * there (main itself never returns, so it is never sampled). */
#define RUN_STATIC_CONSTRUCTORS 0x8C0164ECu
#define GAME_INIT 0x8C0166D8u
#define GAME_FRAME 0x8C0167F8u
void game_main(void) {
  int32_t status;
  HW_RESUMABLE0(0x8C0164B4u);
  HW_RESUME_AT(0, 1);

  CALL0(RUN_STATIC_CONSTRUCTORS);
  CALL0(GAME_INIT);
  for (;;) {
    HW_YIELDING(0, status = (int32_t)CALL0(GAME_FRAME));
    if (status < 0)
      break;
    HW_YIELDING(1, njWaitVSync());
  }
  game_shutdown();
  run_static_destructors();
  syBtExit();
}
HW_EXPORT_MAIN(8c0164b4, game_main())

/* was func_8C016248: the display: VGA-capable cable or TV, 60 Hz, then the SDK's system start-up */
void video_init(void) {
  g_cable_type = video_cable_type();
  g_video_info_b = video_info_b();
  g_video_info_c = video_info_c();
  g_video_flag = 1;
  g_display_mode = g_cable_type == 1 ? 1 : 0x38;
  g_frame_rate = 0x42700000u; /* 60.0f */
  sb_init_system(g_display_mode, g_frame_buffer_mode, g_frame_count_mode);
}
HW_EXPORT(8c016248, video_init())

/* was func_8C0166D8: start-up: middleware, configuration, display, every subsystem, the pads, then the Game
 * object (starting in mode 0) and the V-blank and end-of-render callbacks. */
void game_init(void) {
  mw_PreInit();
  syCfgInit(SYCFG_WORK);
  video_init();
  func_8C061DB8();
  file_init();
  graphics_init();
  sound_init(); /* (the original goes through the thunk func_8C016A4E) */
  func_8C018CBA();
  func_8C015224();
  pad_reset_all();
  func_8C018DD0();
  sy_Nop1();
  func_8C018C20(1, 0);
  func_8C018C20(7, 0);
  func_8C018C20(13, 0);
  func_8C018C20(19, 0);
  g_video_running = 1;
  g_soft_reset = 0;

  gaddr mem = syMalloc(sizeof(Game));
  g_game = mem ? hw_addr(game_construct(game_at(mem), MODE_0)) : 0;
  hw_write32(g_game + offsetof(Game, tree.updates_per_frame), 1);

  njSetVSyncFunction(0x8C01666Au); /* on_vsync */
  njSetEORFunction(0x8C016644u);   /* on_end_of_render */
  g_quit_request = 0;
}
HW_EXPORT(8c0166d8, game_init())

/* was func_8C016644: the end-of-render callback: a fog change is sent to Ninja between frames */
static void on_end_of_render(void) {
  if (g_fog_dirty) {
    njSetFogColor(g_background_color);
    njSetFogTable(FOG_TABLE);
    g_fog_dirty = 0;
  }
}
HW_EXPORT(8c016644, on_end_of_render())

/* was func_8C01666A: the V-sync callback: the background colour, the count of V-blanks the next update has to
 * catch up with, the sound driver's service */
static void on_vsync(void) {
  if (g_video_running) {
    njSetBackColor(g_background_color, g_background_color, g_background_color);
    njSetBorderColor(0);
    g_vblanks_pending++;
  }
  sdSysServer();
}
HW_EXPORT(8c01666a, on_vsync())

/* draw the whole scene tree in one view */
static void draw_view(Game *game, int32_t view) {
  view_select(view);
  task_draw_tree(task_at(game->tree.root));
  view_finish();
}

/* was func_8C0167F8: one frame: as many update passes as V-blanks went by (at most 5), then the scene tree drawn
 * in every view of the split mode, the drive watch, and the exit request. Resumable (the update pass). */
int32_t game_frame(void) {
  HW_RESUMABLE0(0x8C0167F8u);
  HW_RESUME_AT(0);

  if (g_vblanks_pending > 5)
    g_vblanks_pending = 5;
  hw_write32(g_game + offsetof(Game, tree.updates_per_frame), (uint32_t)g_vblanks_pending);
  g_vblanks_pending = 0;

  if (g_game) {
    HW_YIELDING_RET(0, tasktree_update(g_game), 0);
    Game *game = game_at(g_game);
    view_frame_begin();
    switch (g_split_mode) {
    case SPLIT_FULL:
      draw_view(game, 0);
      break;
    case SPLIT_TWO:
      draw_view(game, 0);
      draw_view(game, 1);
      break;
    case SPLIT_TWO_WITH_PANEL:
      draw_view(game, 0);
      draw_view(game, 1);
      njClip2D(2, PANEL_CLIP_RECT);
      if (g_panel)
        draw_panel(g_panel, g_split_mode);
      break;
    case SPLIT_FOUR:
      draw_view(game, 0);
      draw_view(game, 2);
      draw_view(game, 1);
      draw_view(game, 3);
      njClip2D(2, PANEL_CLIP_RECT);
      if (g_panel)
        draw_panel(g_panel, g_split_mode);
      break;
    }
    view_frame_end();
  }
  drive_watch();
  if (g_exit_request) {
    sb_exit_system();
    syBtExit();
  }
  return 0;
}
HW_EXPORT_RESUMABLE(8c0167f8, RET(game_frame()))

/* was func_8C012DC4: every frame, watch the GD-ROM drive: ask for its status, wait up to 10 frames for the answer;
 * an open lid, no disc or a drive error leaves the game (to the boot menu). */
void drive_watch(void) {
  if (g_drive_watch == 0) {
    if (gdFs_Api58() == 0)
      g_drive_watch = 10;
    return;
  }
  if (gdFsGetStat(gdFs_Api34()) != GDD_STAT_BUSY_HANDLE)
    g_drive_watch--;
  g_drive_status = gdFsGetDrvStat();
  if (g_drive_status == GDD_DRVSTAT_OPEN || g_drive_status == GDD_DRVSTAT_NODISC || g_drive_status == GDD_DRVSTAT_ERROR) {
    sb_exit_system();
    syBtExit();
  }
}
HW_EXPORT(8c012dc4, drive_watch())

/* was func_8C01B202 / func_8C01B20A: the V-sync callback's work (background colour, V-blank count) on / off
 * (scenes turn it off while they load) */
void video_resume(void) { g_video_running = 1; }
void video_pause(void) { g_video_running = 0; }
HW_EXPORT(8c01b202, video_resume())
HW_EXPORT(8c01b20a, video_pause())

/* was func_8C03C698: the split-screen panel object */
gaddr panel_get(void) { return g_panel; }
HW_EXPORT(8c03c698, RET(panel_get()))
