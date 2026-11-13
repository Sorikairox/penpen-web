/* Pen Pen port, S17 (b02): the rest of the progress record's accessors (game/save.c's SaveData at 0x8C1B7E50):
 * the score counters, the time and rank tables' setters, the two small bit sets at 0x100 and 0x106, the flags
 * given for groups of flags (rewards), a random flag not yet set, and the course flags' setter. */
#include "hw.h"
#include "save.h"
#include "../game/scenery.h" /* random_timer_scaled (core/random.c) */

#define SAVE_CUPS    4
#define SAVE_COURSES 5
#define SAVE_SLOTS   8

/* the record's layout (game/save.c's SaveData; the lead may move it into save.h) */
typedef struct SaveRecord {
  f32 unknown00;                            /* 0x000 */
  uint8_t flags[8];                         /* 0x004: bit n = flag n */
  uint8_t course_flags[8];                  /* 0x00C: bit cup * 20 + course * 5 + item */
  int16_t score_a[SAVE_SLOTS];              /* 0x014 */
  int16_t score_b[SAVE_SLOTS];              /* 0x024 */
  int16_t score_c[SAVE_SLOTS];              /* 0x034 */
  int8_t byte44[SAVE_SLOTS];                /* 0x044 */
  f32 time_a[SAVE_CUPS][SAVE_COURSES];      /* 0x04C */
  f32 time_b[SAVE_CUPS][SAVE_COURSES];      /* 0x09C */
  int8_t rank[SAVE_CUPS][SAVE_COURSES];     /* 0x0EC */
  uint8_t bits100[6];                       /* 0x100: a bit set (save.c's byte100) */
  uint8_t bits106;                          /* 0x106: eight bits (save.c's byte106) */
  uint8_t pad107;
  int32_t video_108;                        /* 0x108 */
  int32_t video_10c;                        /* 0x10C */
} SaveRecord;
HW_OFFSET(SaveRecord, course_flags, 0x0C);
HW_OFFSET(SaveRecord, score_a, 0x14);
HW_OFFSET(SaveRecord, score_b, 0x24);
HW_OFFSET(SaveRecord, score_c, 0x34);
HW_OFFSET(SaveRecord, time_a, 0x4C);
HW_OFFSET(SaveRecord, time_b, 0x9C);
HW_OFFSET(SaveRecord, rank, 0xEC);
HW_OFFSET(SaveRecord, bits100, 0x100);
HW_OFFSET(SaveRecord, bits106, 0x106);
HW_OFFSET(SaveRecord, video_108, 0x108);
HW_SIZE(SaveRecord, 0x110);

#define g_record (*(SaveRecord *)hw_ptr(0x8C1B7E50u)) /* the progress record (save.c's SAVE_DATA) */

#define SCORE_LIMIT 1000 /* a score that reaches it is held at 999 */
#define SCORE_MAX   999
#define FLAG_LAST   44   /* the numbered flags 1 to 44 (save_flags_count) */
#define FLAG_PICK_RANGE F32_BITS(0x422FF5C2u) /* 43.99: a random flag is 1 + (0 .. 43.99) */

/* 1 << n as SHAD computes it: a negative count shifts the 1 out to the right */
static uint32_t shad_bit(int32_t n) { return n >= 0 ? 1u << (n & 31) : 0u; }

/* ---- bit sets ---- */

/* bit n of a set (n / 8 and n % 8 as C's signed division; the byte is read sign-extended, as the original) */
static int32_t bits_test(gaddr set, int32_t n) {
  int8_t byte = ((int8_t *)hw_ptr(set))[n / 8];
  return ((uint32_t)(int32_t)byte & shad_bit(n % 8)) != 0;
}
static void bits_set(gaddr set, int32_t n) {
  uint8_t *p = (uint8_t *)hw_ptr(set) + n / 8;
  *p = (uint8_t)(*p | shad_bit(n % 8));
}

#define BITS100 0x8C1B7F50u /* &g_record.bits100 */
#define BITS106 0x8C1B7F56u /* &g_record.bits106 */
#define COURSE_FLAGS 0x8C1B7E5Cu /* &g_record.course_flags */

/* was func_8C0197C4: bit n of the set at 0x100 (n 0..47) */
int32_t save_bits100_test(int32_t n) { return bits_test(BITS100, n); }
HW_EXPORT(8c0197c4, RET(save_bits100_test((int32_t)ARG0)))

/* was func_8C0197FC: sets bit n of the set at 0x100 */
void save_bits100_set(int32_t n) { bits_set(BITS100, n); }
HW_EXPORT(8c0197fc, save_bits100_set((int32_t)ARG0))

/* was func_8C019830: bit n (0..7) of the byte at 0x106 */
int32_t save_bits106_test(int32_t n) {
  return ((uint32_t)(int32_t)(int8_t)g_record.bits106 & shad_bit(n)) != 0;
}
HW_EXPORT(8c019830, RET(save_bits106_test((int32_t)ARG0)))

/* was func_8C019842: sets bit n of the byte at 0x106 */
void save_bits106_set(int32_t n) { g_record.bits106 = (uint8_t)(g_record.bits106 | shad_bit(n)); }
HW_EXPORT(8c019842, save_bits106_set((int32_t)ARG0))

/* was func_8C019850: clears bit n of the byte at 0x106 */
void save_bits106_clear(int32_t n) { g_record.bits106 = (uint8_t)(g_record.bits106 & ~shad_bit(n)); }
HW_EXPORT(8c019850, save_bits106_clear((int32_t)ARG0))

/* was func_8C01944A: sets item `item` of a course; cup -1 means cup 1 once cup 0 is complete, else cup 0 */
void save_set_course_flag(int32_t cup, int32_t course, int32_t item) {
  if (cup == -1)
    cup = save_cup0_complete() != 0 ? 1 : 0;
  bits_set(COURSE_FLAGS, cup * 20 + course * 5 + item);
}
HW_EXPORT(8c01944a, save_set_course_flag((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C019588: every item of courses 0-3 of cup 0 set */
void save_set_cup0_all(void) {
  for (int32_t course = 0; course < 4; course++)
    for (int32_t item = 0; item < 5; item++)
      save_set_course_flag(0, course, item);
}
HW_EXPORT(8c019588, save_set_cup0_all())

/* ---- flags ---- */

/* the reward flags: each is set when every flag of its group is set (groups of two, then of three) */
static const struct { int8_t first, count, reward; } k_rewards[] = {
  {0x17, 2, 0x2D}, {0x19, 2, 0x2E}, {0x1B, 3, 0x2F}, {0x1E, 3, 0x30},
  {0x21, 3, 0x31}, {0x24, 3, 0x32}, {0x27, 3, 0x33}, {0x2A, 3, 0x34},
};

/* was func_8C01927E: sets the reward flags 45-52 of the groups (flags 23-44) that are complete */
void save_grant_rewards(void) {
  for (unsigned g = 0; g < sizeof k_rewards / sizeof k_rewards[0]; g++) {
    int32_t k = 0;
    while (k < k_rewards[g].count && save_flag(k_rewards[g].first + k) != 0)
      k++;
    if (k == k_rewards[g].count)
      save_set_flag(k_rewards[g].reward);
  }
}
HW_EXPORT(8c01927e, save_grant_rewards())

/* was func_8C01937E: a flag of 1-44 that is not set yet, picked at random (-1 when all are set) */
int32_t save_pick_unset_flag(void) {
  if (save_flags_count() == FLAG_LAST)
    return -1;
  int32_t n;
  do
    n = f32_to_int(fadd(random_timer_scaled(FLAG_PICK_RANGE), F32(1.0f)));
  while (save_flag(n) != 0);
  return n;
}
HW_EXPORT(8c01937e, RET(save_pick_unset_flag()))

/* was func_8C019358: sets a random flag not yet set and returns it (-1 when all are set) */
int32_t save_set_random_flag(void) {
  if (save_flags_count() == FLAG_LAST)
    return -1;
  int32_t n = save_pick_unset_flag();
  save_set_flag(n);
  return n;
}
HW_EXPORT(8c019358, RET(save_set_random_flag()))

/* ---- tables ---- */

/* was func_8C0193E8: the record's first word (a float) */
f32 save_unknown00(void) { return g_record.unknown00; }
HW_EXPORT(8c0193e8, RETF(save_unknown00()))

/* entry i of a score table plus one, held at 999 */
static void score_count(int16_t *table, int32_t i) {
  int16_t v = (int16_t)(table[i] + 1);
  table[i] = v;
  if (v >= SCORE_LIMIT)
    table[i] = SCORE_MAX;
}
/* was func_8C0195F0, func_8C01961A, func_8C019644: entry i (0..7) of score table a / b / c plus one */
void save_count_a(int32_t i) { score_count(g_record.score_a, i); }
void save_count_b(int32_t i) { score_count(g_record.score_b, i); }
void save_count_c(int32_t i) { score_count(g_record.score_c, i); }
HW_EXPORT(8c0195f0, save_count_a((int32_t)ARG0))
HW_EXPORT(8c01961a, save_count_b((int32_t)ARG0))
HW_EXPORT(8c019644, save_count_c((int32_t)ARG0))

/* was func_8C019700: a course's time in table a */
void save_set_time_a(int32_t cup, int32_t course, f32 t) { g_record.time_a[cup][course] = t; }
HW_EXPORT(8c019700, save_set_time_a((int32_t)ARG0, (int32_t)ARG1, FARG0))

/* was func_8C019716: a course's time from table b */
f32 save_time_b(int32_t cup, int32_t course) { return g_record.time_b[cup][course]; }
HW_EXPORT(8c019716, RETF(save_time_b((int32_t)ARG0, (int32_t)ARG1)))

/* was func_8C019740: a course's time in table b */
void save_set_time_b(int32_t cup, int32_t course, f32 t) { g_record.time_b[cup][course] = t; }
HW_EXPORT(8c019740, save_set_time_b((int32_t)ARG0, (int32_t)ARG1, FARG0))

/* was func_8C019766: a course's rank */
void save_set_rank(int32_t cup, int32_t course, int32_t rank) { g_record.rank[cup][course] = (int8_t)rank; }
HW_EXPORT(8c019766, save_set_rank((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C019864: the record's video word at 0x108 */
void save_set_video_108(int32_t v) { g_record.video_108 = v; }
HW_EXPORT(8c019864, save_set_video_108((int32_t)ARG0))
