/* Pen Pen port, S13: the memory card (VMU) front end (game/vmu.c): the backup library's completion callback, which
 * keeps a small record per drive, and the picture on the VMU's screen. */
#include "hw.h"
#include "sdk.h"
#include "vmu.h"

/* VmuDrive, VMU_DRIVES: game/vmu.h (S17 b02) */
#define VMU_LCD_PICTURES  0x8C08C564u /* a bitmap address per picture */

/* the backup library and the VMU screen (SDK; 0x8C0A00DA and 0x8C0A00E0 have no S2 name: they take a callback) */
static int32_t buGetDiskInfo(int32_t drive, gaddr info) { return (int32_t)CALL2(0x8C0A025Cu, drive, info); }
static int32_t pdVmsLcdIsReady(int32_t port) { return (int32_t)CALL1(0x8C09FDB4u, port); }
static int32_t pdVmsLcdWrite(int32_t port, gaddr bitmap, uint32_t flag) { return (int32_t)CALL3(0x8C09FD04u, port, bitmap, flag); }

enum { VMU_OP_MOUNT = 1, VMU_OP_2 = 2, VMU_OP_12 = 0x12 };

/* was func_8C018C20: shows picture n on the VMU of a port (when its screen is ready) */
int32_t vmu_show_picture(int32_t port, int32_t n) {
  int32_t ready = pdVmsLcdIsReady(port);
  if (ready == 0)
    return ready;
  return pdVmsLcdWrite(port, U32_AT(VMU_LCD_PICTURES + 4u * (uint32_t)n), 2);
}
HW_EXPORT(8c018c20, RET(vmu_show_picture((int32_t)ARG0, (int32_t)ARG1)))

/* func_8C01989A (installs the two callbacks): game/vmu_drive.c (S17 b02) */

/* was func_8C0198AC: an operation on a drive completed with stat */
int32_t vmu_on_complete(int32_t drive, int32_t op, int32_t stat) {
  gaddr record = VMU_DRIVES + (uint32_t)drive * sizeof(VmuDrive);
  VmuDrive *d = (VmuDrive *)hw_ptr(record);
  gaddr info = record + (gaddr)__builtin_offsetof(VmuDrive, info);
  switch (op) {
  case VMU_OP_12:
    d->unknown4c = 1;
    d->unknown54 = (int32_t)(((uint32_t)stat << 9) + 0x20);
    d->unknown58 = stat;
    break;
  case VMU_OP_MOUNT:
    if (stat == 0) {
      d->ready = 1;
      if (buGetDiskInfo(drive, info) == 0)
        d->has_info = 1;
      d->status = 0;
    }
    break;
  case VMU_OP_2:
    vmu_drive_clear(drive);
    d->unknown4c = 0;
    break;
  default:
    d->status = stat;
    buGetDiskInfo(drive, info);
    break;
  }
  d->busy = 0;
  return 0;
}
HW_EXPORT(8c0198ac, RET(vmu_on_complete((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2)))
