/* Pen Pen port, S13 (batch b): system odds and ends at the end of the game's code (0x8C0574C0-0x8C0623EE): the
 * day of the week, the console's language from its flash, a debug word log, a global light setting. */
#include "hw.h"
#include "sdk.h"
#include "vec.h"

/* was func_8C0574C0: a Body at rest */
HW_EXPORT(8c0574c0, body_reset(ARG0))

/* ---- a global setting kept twice (two structures read it) ---- */

#define g_setting_a ((f32 *)hw_ptr(0x8C1B3B28u)) /* three floats */
#define g_setting_b ((f32 *)hw_ptr(0x8C1B3AE8u)) /* a structure with the first two at 0x0C / 0x10 */

/* was func_8C05769E: (x / 32, y / 32, z) into both places */
void setting_set(f32 x, f32 y, f32 z) {
  f32 sx = fdiv(x, F32(32.0f));
  f32 sy = fdiv(y, F32(32.0f));
  g_setting_a[0] = sx;
  g_setting_b[3] = sx;
  g_setting_a[1] = sy;
  g_setting_b[4] = sy;
  g_setting_a[2] = z;
}
HW_EXPORT(8c05769e, setting_set(FARG0, FARG1, FARG2))

/* was func_8C0622B8: the day of the week (0 = Sunday ... by the original's offset) of a date: Zeller's
 * congruence without the century terms (January and February count as months 13 and 14 of the year before) */
int32_t date_weekday(uint32_t year, uint32_t month, uint32_t day) {
  if ((uint8_t)month <= 2) {
    month += 12;
    year -= 1;
  }
  int32_t y = (uint16_t)year;
  int32_t m = (uint8_t)month;
  int32_t sum = y + (y >> 2) + (m * 13 + 8) / 5 + (uint8_t)day - 15;
  return sum % 7;
}
HW_EXPORT(8c0622b8, RET(date_weekday(ARG0, ARG1, ARG2)))

/* ---- a debug log of words (on when bit 3 of its flags is set) ---- */

#define g_log_flags GUEST_VAR(uint32_t, 0x8C1BAC54u)
#define g_log_next  GUEST_VAR(gaddr, 0x8C1BAC60u)
#define g_log_end   GUEST_VAR(gaddr, 0x8C1BAC64u)

/* was func_8C062388: append a word while there is room */
void debug_log_word(uint32_t word) {
  if ((g_log_flags & 8) == 0) return;
  if (g_log_next >= g_log_end) return;
  hw_write32(g_log_next, word);
  g_log_next += 4;
}
HW_EXPORT(8c062388, debug_log_word(ARG0))

/* ---- the console's language ---- */

/* syBtCfg_ReadFlash (Shinobi, S2 name) */
static inline void syBtCfg_ReadFlash(uint32_t a) { CALL1(0x8C0E2BCCu, a); }
/* func_8C063150 (a BIOS stub): the system configuration block read from the flash */
static gaddr bios_config_block(void) { return CALL0(0x8C063150u); }

/* the part of the configuration block read here */
typedef struct SysConfigBlock {
  uint8_t pad_00[0x0C];
  int8_t language;        /* 0x0C: an ASCII digit */
} SysConfigBlock;
HW_OFFSET(SysConfigBlock, language, 0x0C);

/* was func_8C0623B4: the language digit of the flash's configuration ('1' to '3' give 1 to 3, else 0) */
int32_t system_language(void) {
  syBtCfg_ReadFlash(0);
  gaddr block = bios_config_block();
  switch (((const SysConfigBlock *)hw_ptr(block))->language) {
  case '1': return 1;
  case '2': return 2;
  case '3': return 3;
  default: return 0;
  }
}
/* not exported (S13 difftest: its HLE calls differ from the original's (game_init samples ran too long)): HW_EXPORT(8c0623b4, RET(system_language())) */
