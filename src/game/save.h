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
/* S17 b02: exported in game/save.c, declared for their callers */
int32_t save_byte44(int32_t i);
int32_t game_language(void);

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

/* game/save_records.c (S17 b02) */
int32_t save_bits100_test(int32_t n);
void save_bits100_set(int32_t n);
int32_t save_bits106_test(int32_t n);
void save_bits106_set(int32_t n);
void save_bits106_clear(int32_t n);
void save_set_course_flag(int32_t cup, int32_t course, int32_t item);
void save_set_cup0_all(void);
void save_grant_rewards(void);
int32_t save_pick_unset_flag(void);
int32_t save_set_random_flag(void);
f32 save_unknown00(void);
void save_count_a(int32_t i);
void save_count_b(int32_t i);
void save_count_c(int32_t i);
void save_set_time_a(int32_t cup, int32_t course, f32 t);
f32 save_time_b(int32_t cup, int32_t course);
void save_set_time_b(int32_t cup, int32_t course, f32 t);
void save_set_rank(int32_t cup, int32_t course, int32_t rank);
void save_set_video_108(int32_t v);

#endif
