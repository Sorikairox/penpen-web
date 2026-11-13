/* Pen Pen port, S18 b01: the memory card screen's line for one drive (game/vmu.h's VmuDrive). */
#include "hw.h"
#include "sdk.h"
#include "../core/sprite_text.h"
#include "../core/sprite_number.h"
#include "vmu.h"

#define VMU_INFO_Y        0x60
#define VMU_INFO_X        192
#define VMU_INFO_X2       320
#define VMU_INFO_X3       448
#define VMU_DIGIT_SIZE    32
#define VMU_TITLE_LOCATION 0x00060003u /* NJM_LOCATION(3, 6) */
#define VMU_TITLE_FORMAT  0x8C0706ACu  /* the debug line's format (the drive number) */
#define VMU_TEXT_BLOCKS   0x8C0706BCu  /* after the two counts */
#define VMU_TEXT_NO_INFO  0x8C0706CCu  /* a card without its information yet */
#define VMU_TEXT_BUSY     0x8C0706E0u  /* no card, an operation 1 under way */
#define VMU_TEXT_NO_CARD  0x8C0706F8u

/* SDK: Ninja's debug print */
static inline void njPrintSize(int32_t size) { CALL1(0x8C0D1C5Eu, size); }
/* njPrint(location, format, ...): SHC passes the format and the values on the stack (the format at @r15, the
 * drive at @(4,r15)); the location in r4 */
static void njPrint_drive(uint32_t location, gaddr format, int32_t drive) {
  gaddr args = hw_stack_alloc(8);
  *(uint32_t *)hw_ptr(args) = format;
  *(uint32_t *)hw_ptr(args + 4) = (uint32_t)drive;
  CALL1(0x8C0D2614u, location);
  hw_stack_free(8);
}

/* was func_8C019BB4: the drive's debug line, then: with a card, its two block counts and their word (or a word
 * while its information is not read); without, a word saying whether operation 1 is under way */
void vmu_info_draw(int32_t drive) {
  VmuDrive *d = vmu_drive_at(drive);
  njPrintSize(12);
  njPrint_drive(VMU_TITLE_LOCATION, VMU_TITLE_FORMAT, drive);
  if (d->ready != 0) {
    if (d->has_info != 0) {
      number_draw(VMU_INFO_X, VMU_INFO_Y, vmu_disk_info(d)->blocks26, VMU_DIGIT_SIZE);
      number_draw(VMU_INFO_X2, VMU_INFO_Y, vmu_disk_info(d)->blocks22, VMU_DIGIT_SIZE);
      text_draw(VMU_INFO_X3, VMU_INFO_Y, VMU_TEXT_BLOCKS);
    } else {
      text_draw(VMU_INFO_X, VMU_INFO_Y, VMU_TEXT_NO_INFO);
    }
  } else {
    text_draw(VMU_INFO_X, VMU_INFO_Y, d->busy == 1 ? VMU_TEXT_BUSY : VMU_TEXT_NO_CARD);
  }
}
HW_EXPORT(8c019bb4, vmu_info_draw((int32_t)ARG0))
