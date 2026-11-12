/* Pen Pen port: core/system.c (the day of the week, a debug word log, the console's flash setting) and, S15 b09,
 * core/rtc.c (the real-time clock: the AICA's seconds counter kept up to date by a V-blank handler, and its
 * conversion to and from a calendar date). */
#ifndef PENPEN_SYSTEM_H
#define PENPEN_SYSTEM_H
#include "hw.h"

/* core/system.c */
int32_t date_weekday(uint32_t year, uint32_t month, uint32_t day);
void debug_log_word(uint32_t word);
int32_t system_language(void);

/* S15 b09: a calendar date as the clock functions fill it (the SDK's SYS_RTC_DATE layout) */
typedef struct RtcDate {
  uint16_t year;    /* 0x00: from 1950 */
  uint8_t month;    /* 0x02: 1-12 */
  uint8_t day;      /* 0x03: 1-31 */
  uint8_t hour;     /* 0x04 */
  uint8_t minute;   /* 0x05 */
  uint8_t second;   /* 0x06 */
  uint8_t weekday;  /* 0x07: date_weekday */
  uint8_t moon_age; /* 0x08: days into the lunar month (rtc_moon_age) */
} RtcDate;
HW_OFFSET(RtcDate, month, 0x02);
HW_OFFSET(RtcDate, second, 0x06);
HW_OFFSET(RtcDate, moon_age, 0x08);

static inline RtcDate *rtc_date_at(gaddr a) { return (RtcDate *)hw_ptr(a); }

/* core/rtc.c */
int32_t rtc_init(void);
void rtc_vblank(void);
uint32_t rtc_get_date(gaddr date);
uint32_t rtc_date_seconds(gaddr date);
void rtc_date_to_seconds(gaddr date, gaddr out);
void rtc_seconds_to_date(uint32_t seconds, gaddr date);
uint32_t rtc_moon_age(gaddr date);
int32_t rtc_read_seconds(gaddr out);
uint32_t rtc_read_checked(uint32_t fallback);
uint32_t rtc_read_stable(void);
int32_t video_mode(void);

#endif
