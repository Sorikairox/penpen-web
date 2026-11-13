/* Pen Pen port, S17 b09: the rest of the real-time clock (core/rtc.c): its shutdown, its flags, the comparison of
 * two dates, and setting the clock: the AICA's seconds counter written through its write-enable register, then read
 * back the stable way to check it took. */
#include "hw.h"
#include "sdk.h"
#include "system.h"
#include "vec.h" /* member_addr */

#define g_rtc_count   GUEST_VAR(uint32_t, 0x8C1BAC50u) /* the count of seconds the V-blank handler last read */
#define g_rtc_flags   GUEST_VAR(uint32_t, 0x8C1BAC54u) /* core/rtc.c: bit 0, 1 (how the clock is read), bit 3 */
#define g_rtc_handler GUEST_VAR(gaddr, 0x8C1BAC58u)    /* the V-blank chain's handle of rtc_vblank */

#define RTC_HIGH_REGISTER   0xA0710000u /* the count's high 16 bits */
#define RTC_LOW_REGISTER    0xA0710004u /* the count's low 16 bits */
#define RTC_ENABLE_REGISTER 0xA0710008u /* 1: the next writes of the two halves are taken */

/* func_8C062E80 (still translated): writes `count` registers of `size` bytes from src to a G2-bus address (the
 * counterpart of core/rtc.c's g2_read; the last two arguments are 0 and 0 here); its result is not used */
static void g2_write(gaddr src, gaddr dst, uint32_t size, uint32_t count, uint32_t a5, uint32_t a6) {
  hw_call_stack(0x8C062E80u, 6, (const uint32_t[]){src, dst, size, count, a5, a6});
}
/* syChainDeleteHandler (Shinobi): removes a handler from its interrupt chain */
static inline uint32_t syChainDeleteHandler(gaddr handle) { return CALL1(0x8C0DCA80u, handle); }

/* was func_8C061DE2: the clock's shutdown: the count cleared, the V-blank handler taken off its chain (the chain
 * function's result is returned as it is: the original ends with a jump to it) */
uint32_t rtc_shutdown(void) {
  g_rtc_count = 0;
  return syChainDeleteHandler(g_rtc_handler);
}
HW_EXPORT(8c061de2, RET(rtc_shutdown()))

/* was func_8C061ED2 */
void rtc_set_flags(uint32_t flags) { g_rtc_flags = flags; }
HW_EXPORT(8c061ed2, rtc_set_flags(ARG0))

/* was func_8C061ED8 */
uint32_t rtc_flags(void) { return g_rtc_flags; }
HW_EXPORT(8c061ed8, RET(rtc_flags()))

/* the original's frame of rtc_date_compare (0x0C bytes under the saved PR): both counts are written there */
typedef struct RtcCompareFrame {
  uint32_t b_seconds; /* 0x00 */
  uint32_t a_seconds; /* 0x04 */
  gaddr b;            /* 0x08: the second date, kept across the first conversion */
  uint32_t saved_pr;  /* 0x0C */
} RtcCompareFrame;
HW_OFFSET(RtcCompareFrame, a_seconds, 0x04);
HW_OFFSET(RtcCompareFrame, b, 0x08);
HW_SIZE(RtcCompareFrame, 0x10);

/* was func_8C061EDE: compares two dates by their counts of seconds: 0 when equal, 1 when a is later, -1 when b is */
int32_t rtc_date_compare(gaddr a, gaddr b) {
  gaddr frame = hw_stack_alloc(sizeof(RtcCompareFrame));
  RtcCompareFrame *f = (RtcCompareFrame *)hw_ptr(frame);
  f->b = b;
  rtc_date_to_seconds(a, member_addr(frame, RtcCompareFrame, a_seconds));
  rtc_date_to_seconds(f->b, member_addr(frame, RtcCompareFrame, b_seconds));
  int32_t result;
  if (f->a_seconds == f->b_seconds)
    result = 0;
  else if (f->a_seconds > f->b_seconds) /* unsigned */
    result = 1;
  else
    result = -1;
  hw_stack_free(sizeof(RtcCompareFrame));
  return result;
}
HW_EXPORT(8c061ede, RET(rtc_date_compare(ARG0, ARG1)))

/* the original's frame of the three register writers (4 bytes under the saved PR and r14): the value written */
typedef struct RtcWriteFrame {
  union {
    uint32_t value;  /* 0x00 */
    uint16_t enable; /* 0x00: the enable writer stores only these 16 bits (the write still takes 4 bytes) */
  };
  uint32_t saved[2]; /* 0x04: PR, r14 */
} RtcWriteFrame;
HW_SIZE(RtcWriteFrame, 0x0C);

/* was func_8C062244: the clock's write enable set (a 16-bit 1 stored, its register written from the 4 bytes there);
 * returns 0 */
int32_t rtc_write_enable(void) {
  gaddr frame = hw_stack_alloc(sizeof(RtcWriteFrame));
  ((RtcWriteFrame *)hw_ptr(frame))->enable = 1;
  g2_write(member_addr(frame, RtcWriteFrame, value), RTC_ENABLE_REGISTER, 4, 1, 0, 0);
  hw_stack_free(sizeof(RtcWriteFrame));
  return 0;
}
HW_EXPORT(8c062244, RET(rtc_write_enable()))

/* writes one of the count's registers from the frame's word; returns 0 */
static int32_t rtc_write_register(gaddr reg, uint32_t value) {
  gaddr frame = hw_stack_alloc(sizeof(RtcWriteFrame));
  ((RtcWriteFrame *)hw_ptr(frame))->value = value;
  g2_write(member_addr(frame, RtcWriteFrame, value), reg, 4, 1, 0, 0);
  hw_stack_free(sizeof(RtcWriteFrame));
  return 0;
}

/* was func_8C06226C: the count's low half written */
int32_t rtc_write_low(uint32_t value) { return rtc_write_register(RTC_LOW_REGISTER, value); }
HW_EXPORT(8c06226c, RET(rtc_write_low(ARG0)))

/* was func_8C062292: the count's high half written */
int32_t rtc_write_high(uint32_t value) { return rtc_write_register(RTC_HIGH_REGISTER, value); }
HW_EXPORT(8c062292, RET(rtc_write_high(ARG0)))

/* the original's frame of rtc_set_date (4 bytes under the saved PR): the date's count */
typedef struct RtcSetFrame {
  uint32_t seconds;  /* 0x00 */
  uint32_t saved_pr; /* 0x04 */
} RtcSetFrame;
HW_SIZE(RtcSetFrame, 0x08);

/* was func_8C061E80: sets the clock to a date: the write enable, the low half, the high half (each stops it on a
 * non-zero result, which is returned); then the clock read back the stable way: when it reads the count or the
 * second after, it becomes the V-blank handler's count and 0 is returned, else -1 */
int32_t rtc_set_date(gaddr date) {
  gaddr frame = hw_stack_alloc(sizeof(RtcSetFrame));
  const RtcSetFrame *f = (const RtcSetFrame *)hw_ptr(frame);
  rtc_date_to_seconds(date, member_addr(frame, RtcSetFrame, seconds));
  int32_t result = rtc_write_enable();
  if (result == 0) {
    result = rtc_write_low(f->seconds & 0xFFFFu);
    if (result == 0) {
      result = rtc_write_high(f->seconds >> 16 & 0xFFFFu);
      if (result == 0) {
        uint32_t now = rtc_read_stable();
        if (now == f->seconds || now == f->seconds + 1) {
          g_rtc_count = now;
          result = 0;
        } else {
          result = -1;
        }
      }
    }
  }
  hw_stack_free(sizeof(RtcSetFrame));
  return result;
}
HW_EXPORT(8c061e80, RET(rtc_set_date(ARG0)))
