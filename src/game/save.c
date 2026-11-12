/* Pen Pen port, S13: the game's progress record (game/save.c): the block that goes to the memory card.
 *
 * One record lives at 0x8C1B7E50. Two bit sets (a flag by number; a flag per (cup, course, item)), three tables of
 * eight 16-bit scores and one of eight bytes (one entry per character, by their use), two 4x5 tables of times
 * (cups x courses: seconds, as floats) and one of ranks, a few bytes, and the video setting the record was made with.
 * Names are what the code shows; the meaning of each table is a guess from its defaults. */
#include "hw.h"
#include "../game/game.h"

#define SAVE_CUPS    4
#define SAVE_COURSES 5
#define SAVE_SLOTS   8

typedef struct SaveData {
  f32 unknown00;                            /* 0x000 */
  uint8_t flags[8];                         /* 0x004: bit n = flag n (flag 0 always reads as set) */
  uint8_t course_flags[8];                  /* 0x00C: bit cup * 20 + course * 5 + item */
  int16_t score_a[SAVE_SLOTS];              /* 0x014 */
  int16_t score_b[SAVE_SLOTS];              /* 0x024 */
  int16_t score_c[SAVE_SLOTS];              /* 0x034 */
  int8_t byte44[SAVE_SLOTS];                /* 0x044 */
  f32 time_a[SAVE_CUPS][SAVE_COURSES];      /* 0x04C: 480 by default */
  f32 time_b[SAVE_CUPS][SAVE_COURSES];      /* 0x09C: 120, 240, ... 600 by default */
  int8_t rank[SAVE_CUPS][SAVE_COURSES];     /* 0x0EC: 8 by default */
  uint8_t byte100[6];                       /* 0x100 */
  uint8_t byte106;                          /* 0x106 */
  uint8_t pad107;
  int32_t video_108;                        /* 0x108 */
  int32_t video_10c;                        /* 0x10C: g_video_flag when the record was reset */
} SaveData;
HW_OFFSET(SaveData, score_a, 0x14);
HW_OFFSET(SaveData, byte44, 0x44);
HW_OFFSET(SaveData, time_a, 0x4C);
HW_OFFSET(SaveData, time_b, 0x9C);
HW_OFFSET(SaveData, rank, 0xEC);
HW_OFFSET(SaveData, byte100, 0x100);
HW_OFFSET(SaveData, video_108, 0x108);
HW_SIZE(SaveData, 0x110);

#define SAVE_DATA 0x8C1B7E50u
#define g_save (*(SaveData *)hw_ptr(SAVE_DATA))

/* ---- bit sets ---- */

/* 1 << bit as SHAD computes it (a negative count shifts right) */
static uint32_t bit_mask(int32_t bit) { return bit >= 0 ? 1u << bit : (uint32_t)(1 >> -bit); }

/* bit n of a set (n / 8 and n % 8 as C's signed division) */
static int32_t bits_test(uint8_t *set, int32_t n) { return (set[n / 8] & bit_mask(n % 8)) != 0; }

/* was func_8C019208: flag n (flag 0 is always set) */
int32_t save_flag(int32_t n) {
  if (n == 0)
    return 1;
  return bits_test(g_save.flags, n);
}
HW_EXPORT(8c019208, RET(save_flag((int32_t)ARG0)))

/* was func_8C01924A: sets flag n */
void save_set_flag(int32_t n) {
  uint8_t *p = &g_save.flags[n / 8];
  *p = (uint8_t)(*p | bit_mask(n % 8));
}
HW_EXPORT(8c01924a, save_set_flag((int32_t)ARG0))

/* was func_8C019400: the flag of item `item` of a course */
int32_t save_course_flag(int32_t cup, int32_t course, int32_t item) {
  return bits_test(g_save.course_flags, cup * 20 + course * 5 + item);
}
HW_EXPORT(8c019400, RET(save_course_flag((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2)))

/* ---- the tables ---- */

/* was func_8C0195C0, func_8C0195CA, func_8C0195D4: entry i of a score table */
int32_t save_score_a(int32_t i) { return g_save.score_a[i]; }
int32_t save_score_b(int32_t i) { return g_save.score_b[i]; }
int32_t save_score_c(int32_t i) { return g_save.score_c[i]; }
HW_EXPORT(8c0195c0, RET(save_score_a((int32_t)ARG0)))
HW_EXPORT(8c0195ca, RET(save_score_b((int32_t)ARG0)))
HW_EXPORT(8c0195d4, RET(save_score_c((int32_t)ARG0)))

static int32_t sum8(const int16_t *t) {
  int32_t sum = 0;
  for (int i = 0; i < SAVE_SLOTS; i++)
    sum += t[i];
  return sum;
}
/* was func_8C01966E, func_8C01968A, func_8C0196A6: the sum of a score table */
int32_t save_total_a(void) { return sum8(g_save.score_a); }
int32_t save_total_b(void) { return sum8(g_save.score_b); }
int32_t save_total_c(void) { return sum8(g_save.score_c); }
HW_EXPORT(8c01966e, RET(save_total_a()))
HW_EXPORT(8c01968a, RET(save_total_b()))
HW_EXPORT(8c0196a6, RET(save_total_c()))

/* was func_8C01979C: the first index of the highest entry of score table a */
int32_t save_best_a(void) {
  int32_t best = 0;
  for (int32_t i = 1; i < SAVE_SLOTS; i++)
    if (g_save.score_a[i] > g_save.score_a[best])
      best = i;
  return best;
}
HW_EXPORT(8c01979c, RET(save_best_a()))

/* was func_8C0196C2: entry i of the byte table at 0x44 */
int32_t save_byte44(int32_t i) { return g_save.byte44[i]; }
HW_EXPORT(8c0196c2, RET(save_byte44((int32_t)ARG0)))

/* was func_8C0196E8: always 0 */
int32_t save_zero(void) { return 0; }
HW_EXPORT(8c0196e8, RET(save_zero()))

/* was func_8C0196EC: a course's time from table a */
f32 save_time_a(int32_t cup, int32_t course) { return g_save.time_a[cup][course]; }
HW_EXPORT(8c0196ec, RETF(save_time_a((int32_t)ARG0, (int32_t)ARG1)))

/* was func_8C019756: a course's rank */
int32_t save_rank(int32_t cup, int32_t course) { return g_save.rank[cup][course]; }
HW_EXPORT(8c019756, RET(save_rank((int32_t)ARG0, (int32_t)ARG1)))

/* was func_8C019860: the game's language (always 1 in this build; the title reads 0-6) */
int32_t game_language(void) { return 1; }
HW_EXPORT(8c019860, RET(game_language()))

/* ---- reset ---- */

/* was func_8C019A60: a record back to its defaults (the video words kept) */
void save_reset(gaddr record) {
  SaveData *s = (SaveData *)hw_ptr(record);
  s->unknown00 = F32(0.0f);
  for (int i = 0; i < 8; i++)
    s->flags[i] = 0;
  for (int i = 0; i < 8; i++)
    s->course_flags[i] = 0;
  for (int i = 0; i < SAVE_SLOTS; i++) {
    s->score_a[i] = 0;
    s->score_b[i] = 0;
    s->score_c[i] = 0;
    s->byte44[i] = 0;
  }
  for (int cup = 0; cup < SAVE_CUPS; cup++)
    for (int course = 0; course < SAVE_COURSES; course++) {
      s->time_a[cup][course] = F32(480.0f);
      s->time_b[cup][course] = fmul(f32_from_int(course + 1), F32(120.0f));
      s->rank[cup][course] = 8;
    }
  for (int i = 0; i < 6; i++)
    s->byte100[i] = 0;
  s->byte106 = 0;
}
HW_EXPORT(8c019a60, save_reset(ARG0))

/* was func_8C01996C: the same, and the video words from the current setting */
void save_reset_all(gaddr record) {
  save_reset(record);
  SaveData *s = (SaveData *)hw_ptr(record);
  s->video_10c = g_video_flag;
  s->video_108 = s->video_10c;
}
HW_EXPORT(8c01996c, save_reset_all(ARG0))
