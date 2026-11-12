/* Pen Pen port, S15 (b01): the progress record's queries (game/save.c, game/save_progress.c), declared for their
 * callers. */
#ifndef PENPEN_SAVE_H
#define PENPEN_SAVE_H
#include "hw.h"

/* game/save.c */
int32_t save_flag(int32_t n);
int32_t save_course_flag(int32_t cup, int32_t course, int32_t item);
int32_t save_zero(void);
void save_set_flag(int32_t n);
void save_reset(gaddr record);
void save_reset_all(gaddr record);

/* game/save_progress.c (S15 b01) */
int32_t save_flags_count(void);
int32_t save_course_flags_count(void);
int32_t save_course_items_ready(int32_t course);
int32_t save_cup0_complete(void);
void save_set_byte44(int32_t i, int32_t value);
void save_backup_init(void);
void save_new_game(void);
void save_new_game_keep_video(void);
int32_t save_choice_open(int32_t n);

#endif
