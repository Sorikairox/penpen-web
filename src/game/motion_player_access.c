/* Pen Pen port, S17 (b02): small methods of the penguins' MotionPlayer (game/motion.h) that the model viewer
 * (func_8C0200E8 and its neighbours) calls on its penguins (0x8C01F714-0x8C01F74B). */
#include "hw.h"
#include "motion.h"
#include "save.h"

/* the style numbers of each look (menu_penguin.c's table), by the save's byte for the penguin */
typedef struct PenguinLooks {
  int8_t looks[4];
} PenguinLooks;
HW_SIZE(PenguinLooks, 4);
#define g_penguin_looks ((const PenguinLooks *)hw_ptr(0x8C085514u))

/* was func_8C01D23C: the four style numbers from the table entry of the save's byte for this penguin (read again
 * for each) */
void motion_player_looks_from_save(gaddr player) {
  MotionPlayer *p = motion_player_at(player);
  for (int i = 0; i < 4; i++)
    p->looks[i] = g_penguin_looks[save_byte44(p->character)].looks[i];
}
HW_EXPORT(8c01d23c, motion_player_looks_from_save(ARG0))

/* was func_8C01F714: play motion set `set` at speed 1 */
void motion_player_play_set_1(gaddr player, int32_t set) { motion_player_play_set(player, set, F32(1.0f)); }
HW_EXPORT(8c01f714, motion_player_play_set_1(ARG0, (int32_t)ARG1))

/* was func_8C01F71A: the playing speed */
void motion_player_set_speed(gaddr player, f32 speed) { motion_player_at(player)->speed = speed; }
HW_EXPORT(8c01f71a, motion_player_set_speed(ARG0, FARG0))

/* was func_8C01F720: style number i (0..3) */
void motion_player_set_look(gaddr player, int32_t i, int32_t look) { motion_player_at(player)->looks[i] = look; }
HW_EXPORT(8c01f720, motion_player_set_look(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C01F72C: style number i (0..3) */
int32_t motion_player_look(gaddr player, int32_t i) { return motion_player_at(player)->looks[i]; }
HW_EXPORT(8c01f72c, RET(motion_player_look(ARG0, (int32_t)ARG1)))

/* was func_8C01F738: the word at 0x14 (1 at construction) toggled between 0 and 1; returns the new value */
int32_t motion_player_toggle_word14(gaddr player) {
  MotionPlayer *p = motion_player_at(player);
  int32_t v = p->word14 == 0;
  p->word14 = v;
  return v;
}
HW_EXPORT(8c01f738, RET(motion_player_toggle_word14(ARG0)))

/* was func_8C01F742: the current request */
gaddr motion_player_request(gaddr player) { return motion_player_at(player)->request; }
HW_EXPORT(8c01f742, RET(motion_player_request(ARG0)))

/* was func_8C01F746: the current frame */
f32 motion_player_frame_now(gaddr player) { return motion_player_at(player)->frame; }
HW_EXPORT(8c01f746, RETF(motion_player_frame_now(ARG0)))
