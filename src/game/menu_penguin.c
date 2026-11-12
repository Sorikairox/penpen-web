/* Pen Pen port, S14 (b02): the penguins of the menus (game/menu_penguin.c).
 *
 * PenPen House and the good-night scene show penguins as MotionPlayers (game/motion.h) of 0xF8 bytes: the subclass's
 * constructor points the object's first word at its own 0xDC (its part after the MotionPlayer), then runs the
 * MotionPlayer's constructor (func_8C037D24: which penguin, which of its two model files, ...). The menu resets a
 * penguin to its standing or posing motion and picks four style numbers for it from a table by the save's byte for
 * that penguin. Several methods are inline copies: each is exported from one C function here. */
#include "hw.h"
#include "sdk.h"
#include "motion.h"

#define PENGUIN_PART          0xDCu        /* the subclass's part, which its first word points at */
#define MENU_MOTION_TABLE     0x8C09731Cu  /* the menus' motion table (also the constructor's, for one case) */

/* the four style numbers of each kind of penguin (the save's byte for a character picks the entry) */
typedef struct PenguinLooks {
  int8_t looks[4];
} PenguinLooks;
HW_SIZE(PenguinLooks, 4);
#define g_penguin_looks ((const PenguinLooks *)hw_ptr(0x8C085514u))
#define CHARACTER_WITH_TWO 2 /* this penguin's style 1 shows as 2 (its last two numbers) */

/* still translated */
/* func_8C037D24: the MotionPlayer's constructor (which penguin, which file, a container or 0, a fifth word) */
static void motion_player_construct(gaddr player, int32_t character, int32_t variant, gaddr container, uint32_t arg5) {
  CALL5(0x8C037D24u, player, character, variant, container, arg5);
}
/* func_8C0385AC: starts motion set `set` of the player's table at `speed` */
static void motion_player_start(gaddr player, int32_t set, f32 speed) {
  FARGS1(speed);
  CALL2(0x8C0385ACu, player, set);
}
/* func_8C037F86: a MotionPlayer's frame; func_8C0381C4: its drawing (still translated; the thunks below export
 * them under these names) */
void motion_player_update(gaddr player) { CALL1(0x8C037F86u, player); }
void motion_player_draw(gaddr player) { CALL1(0x8C0381C4u, player); }
static int32_t save_byte44(int32_t i) { return (int32_t)CALL1(0x8C0196C2u, i); } /* func_8C0196C2 (game/save.c) */

/* was func_8C01D178 and func_8C02295E (identical copies): the constructor: the first word points at the part at
 * 0xDC, then the MotionPlayer's constructor with the same arguments */
gaddr menu_penguin_construct(gaddr penguin, int32_t character, int32_t variant, gaddr container, uint32_t arg5) {
  motion_player_at(penguin)->unknown00 = penguin + PENGUIN_PART;
  motion_player_construct(penguin, character, variant, container, arg5);
  return penguin;
}
HW_EXPORT(8c01d178, RET(menu_penguin_construct(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3, ARG4)))
HW_EXPORT(8c02295e, RET(menu_penguin_construct(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3, ARG4)))

/* was func_8C01D1CC and func_8C0229B2: thunks to the MotionPlayer's update; func_8C0229B8: to its draw */
HW_EXPORT(8c01d1cc, motion_player_update(ARG0))
HW_EXPORT(8c0229b2, motion_player_update(ARG0))
HW_EXPORT(8c0229b8, motion_player_draw(ARG0))

/* was func_8C022C70: the four style numbers: the other model file has 0, 0, 1, 1; the first takes them from the
 * table entry of the save's byte for this penguin (read again for each), and one penguin shows its 1s as 2 */
void menu_penguin_set_looks(gaddr penguin) {
  MotionPlayer *p = motion_player_at(penguin);
  if (p->variant != 0) {
    p->looks[0] = 0;
    p->looks[1] = 0;
    p->looks[2] = 1;
    p->looks[3] = 1;
    return;
  }
  for (int i = 0; i < 4; i++)
    p->looks[i] = g_penguin_looks[save_byte44(p->character)].looks[i];
  if (p->character == CHARACTER_WITH_TWO) {
    if (p->looks[2] == 1)
      p->looks[2] = 2;
    if (p->looks[3] == 1)
      p->looks[3] = 2;
  }
}
/* not exported (S14 difftest: untested: since the menu code that called it is hand-written, it is entered only from C, and no passing sample of its callers covered it): HW_EXPORT(8c022c70, menu_penguin_set_looks(ARG0)) */

/* was func_8C022C2C: back to the menus' motion table: posing (set character + 1, motion 1) or standing (set 0,
 * motion 0), no request, frame 0, speed 1; that set started, then the style numbers (the original falls through
 * into func_8C022C70) */
void menu_penguin_reset(gaddr penguin, int32_t posing) {
  MotionPlayer *p = motion_player_at(penguin);
  p->motion_table = MENU_MOTION_TABLE;
  if (posing) {
    p->motion_count = p->character + 1;
    p->index = 1;
  } else {
    p->motion_count = 0;
    p->index = 0;
  }
  p->request = 0;
  p->frame = F32(0.0f);
  p->speed = F32(1.0f);
  motion_player_start(penguin, p->motion_count, F32(1.0f));
  menu_penguin_set_looks(penguin);
}
HW_EXPORT(8c022c2c, menu_penguin_reset(ARG0, (int32_t)ARG1))
