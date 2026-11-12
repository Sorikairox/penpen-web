/* Pen Pen port, S15 (b01): counts over the progress record's flags (game/save.c's bit sets), and a setter of its
 * byte table at 0x44. */
#include "hw.h"
#include "save.h"

#define SAVE_FLAG_LAST   44 /* the numbered flags counted: 1 to 44 (flag 0 always reads as set) */
#define SAVE_FLAG_CUPS   2  /* the course flags counted: cups 0 and 1 */
#define SAVE_FLAG_COURSES 4 /* courses 0-3 of a cup */
#define SAVE_FLAG_ITEMS  5  /* items 0-4 of a course */

#define g_save_byte44 ((int8_t *)hw_ptr(0x8C1B7E94u)) /* the record's (0x8C1B7E50) byte table at 0x44 */

/* was func_8C0193BE: how many of the flags 1 to 44 are set */
int32_t save_flags_count(void) {
  int32_t count = 0;
  for (int32_t n = 1; n <= SAVE_FLAG_LAST; n++)
    if (save_flag(n))
      count++;
  return count;
}
HW_EXPORT(8c0193be, RET(save_flags_count()))

/* was func_8C0194B6: how many course flags are set over cups 0-1, courses 0-3, items 0-4 */
int32_t save_course_flags_count(void) {
  int32_t count = 0;
  for (int32_t cup = 0; cup < SAVE_FLAG_CUPS; cup++)
    for (int32_t course = 0; course < SAVE_FLAG_COURSES; course++)
      for (int32_t item = 0; item < SAVE_FLAG_ITEMS; item++)
        if (save_course_flag(cup, course, item))
          count++;
  return count;
}
HW_EXPORT(8c0194b6, RET(save_course_flags_count()))

/* was func_8C01950A: 5 when items 0, 1 and 2 of a course of cup 0 are all set, else 3 */
int32_t save_course_items_ready(int32_t course) {
  if (save_course_flag(0, course, 0) == 0)
    return 3;
  if (save_course_flag(0, course, 1) == 0)
    return 3;
  if (save_course_flag(0, course, 2) == 0)
    return 3;
  return 5;
}
HW_EXPORT(8c01950a, RET(save_course_items_ready((int32_t)ARG0)))

/* was func_8C019544: 1 when every item of every course (0-3) of cup 0 is set, else 0 */
int32_t save_cup0_complete(void) {
  for (int32_t course = 0; course < SAVE_FLAG_COURSES; course++)
    for (int32_t item = 0; item < SAVE_FLAG_ITEMS; item++)
      if (save_course_flag(0, course, item) == 0)
        return 0;
  return 1;
}
HW_EXPORT(8c019544, RET(save_cup0_complete()))

/* was func_8C0196C8: entry i of the byte table at 0x44 = value, unless save_zero (which the original asks with value)
 * says otherwise (it is always 0 in this build) */
void save_set_byte44(int32_t i, int32_t value) {
  if (save_zero() == 0)
    g_save_byte44[i] = (int8_t)value;
}
HW_EXPORT(8c0196c8, save_set_byte44((int32_t)ARG0, (int32_t)ARG1))

/* ---- a new record ---- */

#define SAVE_RECORD     0x8C1B7E50u /* the progress record (game/save.c's SAVE_DATA) */
#define BU_WORK         0x8C1B8C90u /* the backup library's work area */
#define BU_WORK_SIZE    0x1E20u
#define BU_DRIVES       15          /* buInit's drive mask: all four ports */
#define VMU_INSTALL_CALLBACKS 0x8C01989Au /* game/vmu.c's vmu_install_callbacks: buInit's completion callback */

/* func_8C062A98: the C runtime's memset (by its arguments: area, byte, size) */
static void rt_memset(gaddr area, int32_t byte, uint32_t size) { CALL3(0x8C062A98u, area, byte, size); }
/* the backup library (SDK) */
static int32_t buInit(int32_t drives, int32_t mode, gaddr work, gaddr callback) {
  return (int32_t)CALL4(0x8C09FE68u, drives, mode, work, callback);
}

/* flags 2 and 1 set, byte 0 = 2 and byte 3 = 1 (what a new record starts with after its reset) */
static void save_new_defaults(void) {
  save_set_flag(2);
  save_set_flag(1);
  save_set_byte44(0, 2);
  save_set_byte44(3, 1);
}

/* the body of func_8C018DF2 (not exported here: it is not in this batch): the record reset with the video words
 * from the current setting, then the defaults */
void save_new_game(void) {
  save_reset_all(SAVE_RECORD);
  save_new_defaults();
}

/* was func_8C018DD0: the backup library started (its work area cleared, vmu_install_callbacks as its callback; its
 * result is not used), then a new record (the original falls through into func_8C018DF2) */
void save_backup_init(void) {
  rt_memset(BU_WORK, 0, BU_WORK_SIZE);
  buInit(BU_DRIVES, 1, BU_WORK, VMU_INSTALL_CALLBACKS);
  save_new_game();
}
HW_EXPORT(8c018dd0, save_backup_init())

/* was func_8C018E10: the record reset (its video words kept), then the defaults */
void save_new_game_keep_video(void) {
  save_reset(SAVE_RECORD);
  save_new_defaults();
}
HW_EXPORT(8c018e10, save_new_game_keep_video())

/* was func_8C019778: 0 for choice 7 while cup 0 is not complete (save_cup0_complete), else 1 */
int32_t save_choice_open(int32_t n) {
  if (save_cup0_complete() == 0 && n == 7)
    return 0;
  return 1;
}
HW_EXPORT(8c019778, RET(save_choice_open((int32_t)ARG0)))
