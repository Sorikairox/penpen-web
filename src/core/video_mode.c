/* Pen Pen port, S15 b09: the video output mode the game picks at start-up from the cable and the console's flash
 * setting (core/system.c reads that setting). */
#include "hw.h"
#include "sdk.h"
#include "system.h"

/* func_8C062440 (Shinobi's cable check, still translated): the cable bits of the port register (0 = VGA) */
static int32_t cable_type(void) { return (int32_t)CALL0(0x8C062440u); }
/* func_8C0623B4 (core/system.c's system_language, which stays unexported: called here through the original, S15:
 * the C version made game_init's difftest sample differ) */
static int32_t flash_setting(void) { return (int32_t)CALL0(0x8C0623B4u); }

/* was func_8C0623EE: the video mode: 1 with a VGA cable (cable bits 0); else 2 when the flash setting is 1 or 3,
 * 0 for any other setting */
int32_t video_mode(void) {
  int32_t cable = cable_type();
  int32_t setting = flash_setting();
  if (cable == 0) return 1;
  switch (setting) {
  case 1:
  case 3:
    return 2;
  default:
    return 0;
  }
}
HW_EXPORT(8c0623ee, RET(video_mode()))
