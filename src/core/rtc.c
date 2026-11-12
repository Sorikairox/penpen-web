/* Pen Pen port, S15 b09: the real-time clock (the Shinobi library's clock module, linked into the game). The AICA
 * keeps a count of seconds in two 16-bit registers (0xA0710000: the high half, 0xA0710004: the low half); a V-blank
 * handler reads it every frame into g_rtc_count, and the date functions convert between that count (seconds since
 * 1950-01-01 00:00:00) and a calendar date (RtcDate, core/system.h) with its day of the week and its moon age. */
#include "hw.h"
#include "sdk.h"
#include "system.h"
#include "vec.h" /* member_addr */

#define g_rtc_count   GUEST_VAR(uint32_t, 0x8C1BAC50u) /* the count of seconds the V-blank handler last read */
#define g_rtc_flags   GUEST_VAR(uint32_t, 0x8C1BAC54u) /* bit 0: get_date reads the clock itself (the handler does
                                                        * not); bit 1: the handler reads it the stable way; bit 3: the
                                                        * debug word log (core/system.c) */
#define g_rtc_handler GUEST_VAR(gaddr, 0x8C1BAC58u)    /* the V-blank chain's handle of rtc_vblank */

#define RTC_REGISTERS     0xA0710000u /* the AICA's clock: two 32-bit registers, 16 bits each */
#define RTC_VBLANK_FN     0x8C061DF0u /* rtc_vblank, as the chain calls it */
#define RTC_CHAIN_ID      0x900       /* the V-blank interrupt's chain */
#define RTC_CHAIN_PRIO    0x5C
#define RTC_STABLE_READS  3           /* the stable read wants this many equal reads in a row */
#define RTC_CHECKED_READS 2           /* the checked read compares this many reads (the original's 2-word array) */

/* month tables (12 words each, copied to the stack by the original): the days of the year before each month */
#define g_days_before_month ((const uint32_t *)hw_ptr(0x8C08A35Cu)) /* used by the date to seconds */
#define g_month_starts      ((const uint32_t *)hw_ptr(0x8C08A38Cu)) /* used by the seconds to date */

#define EPOCH_YEAR        1950
#define DAYS_PER_YEAR     365
#define SECONDS_PER_DAY   86400u
#define LUNAR_MONTH       0x26EE7Fu   /* 2551423 seconds (29.53 days) */
#define NEW_MOON          0x5CB417D1u /* a new moon, in seconds since 1950 */

/* func_8C062B70 (still translated): reads `count` registers of `size` bytes from a G2-bus address into dst
 * (here the clock's two registers; the last two arguments are 1 and 1); 0 on success */
static int32_t g2_read(gaddr src, gaddr dst, uint32_t size, uint32_t count, uint32_t a5, uint32_t a6) {
  return (int32_t)hw_call_stack(0x8C062B70u, 6, (const uint32_t[]){src, dst, size, count, a5, a6});
}
/* syChainAddHandler (Shinobi): adds a handler to an interrupt chain; returns its handle */
static inline gaddr syChainAddHandler(uint32_t id, gaddr fn, uint32_t prio, uint32_t arg) {
  return CALL4(0x8C0DC778u, id, fn, prio, arg);
}

/* ---- reading the clock ---- */

/* the original's stack frame of rtc_read_seconds (0x14 bytes under the saved PR and r14): the two registers are
 * read into `words` */
typedef struct RtcReadFrame {
  uint8_t locals[0x0C]; /* 0x00: (the out pointer and two pointers into words) */
  uint32_t words[2];    /* 0x0C: high, low */
  uint32_t saved[2];    /* 0x14: PR, r14 */
} RtcReadFrame;
HW_OFFSET(RtcReadFrame, words, 0x0C);
HW_SIZE(RtcReadFrame, 0x1C);

/* was func_8C0621E8: reads the clock's two registers; on success *out = high << 16 | low (16 bits each), both
 * words go to the debug log, returns 0; -1 when the read fails (*out untouched) */
int32_t rtc_read_seconds(gaddr out) {
  gaddr frame = hw_stack_alloc(sizeof(RtcReadFrame));
  gaddr words = member_addr(frame, RtcReadFrame, words);
  int32_t result = -1;
  if (g2_read(RTC_REGISTERS, words, 4, 2, 1, 1) == 0) {
    const RtcReadFrame *f = (const RtcReadFrame *)hw_ptr(frame);
    U32_AT(out) = (f->words[0] << 16 & 0xFFFF0000u) | (f->words[1] & 0xFFFFu);
    debug_log_word(f->words[0]);
    debug_log_word(f->words[1]);
    result = 0;
  }
  hw_stack_free(sizeof(RtcReadFrame));
  return result;
}
HW_EXPORT(8c0621e8, RET(rtc_read_seconds(ARG0)))

/* the original's frame of rtc_read_checked (0x0C bytes under six saved registers and PR) */
typedef struct RtcCheckedFrame {
  gaddr first;                       /* 0x00: (a pointer to reads[0]) */
  uint32_t reads[RTC_CHECKED_READS]; /* 0x04 */
  uint32_t saved[7];                 /* 0x0C */
} RtcCheckedFrame;
HW_OFFSET(RtcCheckedFrame, reads, 0x04);
HW_SIZE(RtcCheckedFrame, 0x28);

/* was func_8C062106: reads the clock twice (a failed read counts as `fallback`); the count when both reads agree,
 * else `fallback` */
uint32_t rtc_read_checked(uint32_t fallback) {
  gaddr frame = hw_stack_alloc(sizeof(RtcCheckedFrame));
  uint32_t *r = ((RtcCheckedFrame *)hw_ptr(frame))->reads;
  for (int i = 0; i < RTC_CHECKED_READS; i++)
    if (rtc_read_seconds(member_addr(frame, RtcCheckedFrame, reads[i])) != 0) r[i] = fallback;
  int differ = 0;
  for (int i = 0; i < RTC_CHECKED_READS - 1; i++)
    if (r[i] != r[i + 1]) {
      differ = 1;
      break;
    }
  uint32_t result = differ ? fallback : r[0];
  hw_stack_free(sizeof(RtcCheckedFrame));
  return result;
}
HW_EXPORT(8c062106, RET(rtc_read_checked(ARG0)))

/* the original's frame of rtc_read_stable (8 bytes under five saved registers and PR) */
typedef struct RtcStableFrame {
  uint32_t current;  /* 0x00 */
  uint32_t previous; /* 0x04 */
  uint32_t saved[6]; /* 0x08 */
} RtcStableFrame;
HW_OFFSET(RtcStableFrame, previous, 0x04);
HW_SIZE(RtcStableFrame, 0x20);

/* was func_8C06217C: reads the clock until a read succeeds, then until three reads in a row succeed and agree with
 * the one before; returns the count */
uint32_t rtc_read_stable(void) {
  gaddr frame = hw_stack_alloc(sizeof(RtcStableFrame));
  RtcStableFrame *f = (RtcStableFrame *)hw_ptr(frame);
  gaddr current = member_addr(frame, RtcStableFrame, current);
  gaddr previous = member_addr(frame, RtcStableFrame, previous);
  uint32_t result;
  int ok = 0;
  do {
    if (rtc_read_seconds(previous) == 0) ok = 1;
  } while (!ok);
  if (!ok) {
    result = (uint32_t)-1; /* (never: the loop above ends only on a good read) */
  } else {
    int stable = 0;
    do {
      int same = 0;
      while (rtc_read_seconds(current) == 0 && f->previous == f->current) {
        same++;
        f->previous = f->current;
        if (same >= RTC_STABLE_READS) break;
      }
      if (same == RTC_STABLE_READS) stable = 1;
    } while (!stable);
    result = f->current;
  }
  hw_stack_free(sizeof(RtcStableFrame));
  return result;
}
HW_EXPORT(8c06217c, RET(rtc_read_stable()))

/* was func_8C061DF0: the V-blank handler: unless get_date reads the clock itself (flag bit 0), the checked read
 * (keeping the last count when the reads disagree); with flag bit 1, the stable read */
void rtc_vblank(void) {
  if ((g_rtc_flags & 1) == 0) g_rtc_count = rtc_read_checked(g_rtc_count);
  if ((g_rtc_flags & 2) != 0) g_rtc_count = rtc_read_stable();
}
HW_EXPORT(8c061df0, rtc_vblank())

/* was func_8C061DB8: the clock's start-up: the count read the stable way, the flags cleared, one handler pass, then
 * the handler added to the V-blank chain; returns 0 */
int32_t rtc_init(void) {
  g_rtc_count = rtc_read_stable();
  g_rtc_flags = 0;
  rtc_vblank();
  g_rtc_handler = syChainAddHandler(RTC_CHAIN_ID, RTC_VBLANK_FN, RTC_CHAIN_PRIO, 0);
  return 0;
}
HW_EXPORT(8c061db8, RET(rtc_init()))

/* ---- dates ---- */

/* was func_8C061F10 (with its body at func_8C061F1E): the seconds since 1950-01-01 00:00:00 of a date (a leap
 * year every fourth year from 1952) */
uint32_t rtc_date_seconds(gaddr date) {
  const RtcDate *d = rtc_date_at(date);
  uint32_t years = (uint32_t)d->year - EPOCH_YEAR;
  uint32_t days = (years + 2) / 4 + years * DAYS_PER_YEAR; /* with a day for each leap year up to this one's end */
  if (((years + 2) & 3) == 0 && !(d->month > 2)) days -= 1; /* this leap year's day not passed yet */
  days += g_days_before_month[d->month - 1] + (uint32_t)(d->day - 1);
  uint32_t hours = days * 24 + d->hour;
  uint32_t minutes = hours * 60 + d->minute;
  return minutes * 60 + d->second;
}
void rtc_date_to_seconds(gaddr date, gaddr out) { U32_AT(out) = rtc_date_seconds(date); }
HW_EXPORT(8c061f10, rtc_date_to_seconds(ARG0, ARG1))

/* was func_8C0622F4: the moon's age of a date: whole days since the last new moon */
uint32_t rtc_moon_age(gaddr date) {
  uint32_t seconds = rtc_date_seconds(date);
  uint32_t into;
  if (seconds >= NEW_MOON)
    into = (seconds - NEW_MOON) % LUNAR_MONTH;
  else
    into = LUNAR_MONTH - (NEW_MOON - seconds) % LUNAR_MONTH;
  return into / SECONDS_PER_DAY;
}
HW_EXPORT(8c0622f4, RET(rtc_moon_age(ARG0)))

/* was func_8C061FA4 (with its body at func_8C061FB8): the date of a count of seconds since 1950, with its day of
 * the week and its moon age */
void rtc_seconds_to_date(uint32_t seconds, gaddr date) {
  RtcDate *d = rtc_date_at(date);
  const uint32_t *starts = g_month_starts;
  d->second = (uint8_t)(seconds % 60);
  uint32_t minutes = seconds / 60;
  d->minute = (uint8_t)(minutes % 60);
  uint32_t hours = minutes / 60;
  d->hour = (uint8_t)(hours % 24);
  uint32_t days = hours / 24;
  uint32_t years = days / DAYS_PER_YEAR;
  uint32_t day = days % DAYS_PER_YEAR;
  uint32_t leap_days = (years + 1) / 4; /* the leap days of the years passed */
  if (leap_days > day) {
    /* the leap days take it back into the year before */
    d->year = (uint16_t)(years + EPOCH_YEAR - 1);
    day += DAYS_PER_YEAR - leap_days;
    if ((d->year & 3) == 0) day += 1;
  } else {
    day -= leap_days;
    d->year = (uint16_t)(years + EPOCH_YEAR);
  }
  uint32_t month;
  if ((d->year & 3) == 0) {
    /* a leap year: February has a day more */
    month = 1;
    if (day >= starts[1]) {
      for (month = 2; month < 12; month++)
        if (day < starts[month] + 1) break;
    }
  } else {
    for (month = 1; month < 12; month++)
      if (day < starts[month]) break;
  }
  d->month = (uint8_t)month;
  d->day = (uint8_t)(day - starts[month - 1] + 1);
  if ((d->year & 3) == 0 && d->month > 2) d->day -= 1;
  d->weekday = (uint8_t)date_weekday((uint32_t)(int32_t)(int16_t)d->year, (uint32_t)(int32_t)(int8_t)d->month,
                                     (uint32_t)(int32_t)(int8_t)d->day);
  d->moon_age = (uint8_t)rtc_moon_age(date);
}
HW_EXPORT(8c061fa4, rtc_seconds_to_date(ARG0, ARG1))

/* was func_8C061E1A: today's date: with flag bit 0 the clock is read now (the stable way) and kept as the count,
 * else the V-blank handler's last count; returns the flags */
uint32_t rtc_get_date(gaddr date) {
  RtcDate *d = rtc_date_at(date);
  if ((g_rtc_flags & 1) == 1) {
    uint32_t now = rtc_read_stable();
    rtc_seconds_to_date(now, date);
    g_rtc_count = now;
  } else {
    rtc_seconds_to_date(g_rtc_count, date);
  }
  /* (the day of the week and the moon age again, as the original does) */
  d->weekday = (uint8_t)date_weekday((uint32_t)(int32_t)(int16_t)d->year, (uint32_t)(int32_t)(int8_t)d->month,
                                     (uint32_t)(int32_t)(int8_t)d->day);
  d->moon_age = (uint8_t)rtc_moon_age(date);
  return g_rtc_flags;
}
HW_EXPORT(8c061e1a, RET(rtc_get_date(ARG0)))
