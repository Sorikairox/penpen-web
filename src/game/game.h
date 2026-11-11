/* Pen Pen port, S11: the Game object (the top of the game: its modes), and the main loop's globals.
 *
 * There is one Game, created at start-up (game_init) and kept in g_game. It is a TaskTree whose root task is part
 * of it (at 0x24: Game's vtable adjusts `this` back by 0x24, so the root task's update is game_update on the Game).
 * The game is a state machine of modes: each mode is a scene object (a Task subclass) created by the mode's start
 * routine and hung under the root; switching modes destroys the old scene's tasks and starts the next one. */
#ifndef PENPEN_GAME_H
#define PENPEN_GAME_H

#include "../core/task.h"
#include "../core/pad.h"

/* The modes, by the scene object each one creates (constructor; the scenes are still translated). Seen on the S4
 * route: 0 at power-on, then 2 (title), 3 and 4 after Start, 7 for a race. */
enum GameMode {
  MODE_0 = 0,          /* scene 0x38 bytes, func_8C01A3EE */
  MODE_1 = 1,          /* scene 0x1C4, func_8C01B77A(variant 0) */
  MODE_TITLE = 2,      /* scene 0x1C4, func_8C01B77A(variant 1); where a soft reset goes */
  MODE_3 = 3,          /* scene 0x28, func_8C01DDE2 */
  MODE_4 = 4,          /* scene 0x88, func_8C01D4CA */
  MODE_5 = 5,          /* nothing (empty start routine) */
  MODE_6 = 6,          /* nothing */
  MODE_7 = 7,          /* scene 0xA8, func_8C01A8DE(mode_arg) */
  MODE_DEMO = 8,       /* scene 0xA8, func_8C01A8DE(5); ends on Start or after 3600 frames */
  MODE_9 = 9,          /* scene 0x90, func_8C01E8DA */
  MODE_QUIT = 10,      /* sets g_quit_request */
  MODE_11 = 11,        /* scene 0x60, func_8C021A10 */
  MODE_12 = 12,        /* scene 0x8C, func_8C021E0A */
  MODE_NONE = 13,      /* before the first mode starts */
  MODE_COUNT = 13
};
#define DEMO_MODE_FRAMES 3600

typedef struct Game {
  TaskTree tree;        /* 0x00: tree.root = &root_task */
  Pad pad;              /* 0x08: player one's pad (port 0) */
  int32_t mode;         /* 0x0C: the running mode */
  int32_t next_mode;    /* 0x10: set to switch modes (by the scenes, through their owner) */
  int32_t mode_arg;     /* 0x14: handed to mode 7's scene */
  int32_t mode_frames;  /* 0x18: frames since the mode started */
  int32_t reset_exits;  /* 0x1C: the soft reset (A+B+X+Y, then Start) leaves the game instead of going to the title */
  int32_t unknown20;    /* 0x20: cleared by game_reset, not read here */
  Task root_task;       /* 0x24 */
} Game;
HW_OFFSET(Game, pad, 0x08);
HW_OFFSET(Game, mode_frames, 0x18);
HW_OFFSET(Game, root_task, 0x24);
HW_SIZE(Game, 0x48);
#define GAME_VTABLE 0x8C23D924u

static inline Game *game_at(gaddr a) { return (Game *)hw_ptr(a); }

/* ---- the main loop's globals (guest .data/.bss) ---- */
#define g_game              GUEST_VAR(gaddr, 0x8C08C488u)    /* the Game */
#define g_vblanks_pending   GUEST_VAR(int32_t, 0x8C08C48Cu)  /* V-blanks since the last update (the V-sync callback counts) */
#define g_video_running     GUEST_VAR(int32_t, 0x8C08C490u)  /* the V-sync callback sets the background and counts */
#define g_quit_request      GUEST_VAR(int32_t, 0x8C08C494u)  /* -1 = MODE_QUIT ran */
#define g_soft_reset        GUEST_VAR(int32_t, 0x8C1B3E3Cu)  /* a soft reset happened */
#define g_exit_request      GUEST_VAR(int32_t, 0x8C08AD48u)  /* leave to the boot menu at the end of the frame */
#define g_background_color  GUEST_VAR(uint32_t, 0x8C1B3154u) /* back colour and fog colour (ARGB) */
#define g_fog_dirty         GUEST_VAR(int32_t, 0x8C1B3358u)  /* the fog changed: sent at the end of the render */
#define FOG_TABLE           0x8C1B3158u                      /* the fog table handed to njSetFogTable */
#define g_panel             GUEST_VAR(gaddr, 0x8C096230u)    /* the split-screen panel object (drawn over the views) */
#define PANEL_CLIP_RECT     0x8C067264u                      /* njClip2D rectangle for the panel */
/* the drive watch */
#define g_drive_watch       GUEST_VAR(int32_t, 0x8C08AD50u)  /* 0 = ask the drive; 10..1 = wait for the answer */
#define g_drive_status      GUEST_VAR(int32_t, 0x8C08AD4Cu)  /* the last gdFsGetDrvStat */
/* video set-up */
#define g_cable_type        GUEST_VAR(int32_t, 0x8C08C324u)
#define g_video_info_b      GUEST_VAR(int32_t, 0x8C08C32Cu)
#define g_video_info_c      GUEST_VAR(int32_t, 0x8C08C328u)
#define g_video_flag        GUEST_VAR(int32_t, 0x8C08C330u)
#define g_display_mode      GUEST_VAR(int32_t, 0x8C08C334u)  /* sbInitSystem's mode: 1 on cable type 1, else 0x38 */
#define g_frame_buffer_mode GUEST_VAR(int32_t, 0x8C08C338u)
#define g_frame_count_mode  GUEST_VAR(int32_t, 0x8C08C33Cu)
#define g_frame_rate        GUEST_VAR(uint32_t, 0x8C08C320u)  /* a float: 60.0 */
#define SYCFG_WORK          0x8C1B3E40u
/* the static constructor table: {function, unused} pairs between *CTORS_BEGIN and *CTORS_END */
#define CTORS_BEGIN         0x8C23D910u
#define CTORS_END           0x8C23D914u

Game *game_construct(Game *game, int32_t first_mode);
Game *game_reset(Game *game, int32_t first_mode);
void game_update(gaddr game);

gaddr panel_get(void);
void video_resume(void);
void video_pause(void);
#endif
