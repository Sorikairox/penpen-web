/* Pen Pen port, S13: the memory card (VMU) front end (game/vmu.c): the backup library's completion callback, which
 * keeps a small record per drive, and the picture on the VMU's screen. */
#include "hw.h"
#include "sdk.h"

typedef struct VmuDrive {
  int16_t ready;         /* 0x00: a card was mounted */
  int16_t has_info;      /* 0x02: its disk information was read */
  int32_t status;        /* 0x04: the last operation's status */
  uint8_t unknown08[0x10 - 0x08];
  int32_t busy;          /* 0x10: cleared when an operation completes */
  uint8_t info[0x4C - 0x14]; /* 0x14: BUS_DISKINFO (buGetDiskInfo) */
  int32_t unknown4c;     /* 0x4C: set by operation 0x12, cleared by 2 */
  uint8_t unknown50[4];
  int32_t unknown54;     /* 0x54: (stat << 9) + 32 for operation 0x12 */
  int32_t unknown58;     /* 0x58: stat for operation 0x12 */
} VmuDrive;
HW_OFFSET(VmuDrive, busy, 0x10);
HW_OFFSET(VmuDrive, info, 0x14);
HW_OFFSET(VmuDrive, unknown4c, 0x4C);
HW_SIZE(VmuDrive, 0x5C);

#define VMU_DRIVES        0x8C1B81D0u /* VmuDrive per drive */
#define VMU_LCD_PICTURES  0x8C08C564u /* a bitmap address per picture */
#define VMU_ON_COMPLETE   0x8C0198ACu /* vmu_on_complete */
#define VMU_CALLBACK_2    0x8C019928u /* the second callback (still translated) */

/* the backup library and the VMU screen (SDK; 0x8C0A00DA and 0x8C0A00E0 have no S2 name: they take a callback) */
static int32_t buGetDiskInfo(int32_t drive, gaddr info) { return (int32_t)CALL2(0x8C0A025Cu, drive, info); }
static void bu_8C0A00DA(gaddr callback) { CALL1(0x8C0A00DAu, callback); }
static void bu_8C0A00E0(gaddr callback) { CALL1(0x8C0A00E0u, callback); }
static int32_t pdVmsLcdIsReady(int32_t port) { return (int32_t)CALL1(0x8C09FDB4u, port); }
static int32_t pdVmsLcdWrite(int32_t port, gaddr bitmap, uint32_t flag) { return (int32_t)CALL3(0x8C09FD04u, port, bitmap, flag); }
/* func_8C01986A: the game's handling of operation 2 (still translated; it receives the callback's arguments) */
static void vmu_on_op2(int32_t drive, int32_t op, int32_t stat) { CALL3(0x8C01986Au, drive, op, stat); }

enum { VMU_OP_MOUNT = 1, VMU_OP_2 = 2, VMU_OP_12 = 0x12 };

/* was func_8C018C20: shows picture n on the VMU of a port (when its screen is ready) */
int32_t vmu_show_picture(int32_t port, int32_t n) {
  int32_t ready = pdVmsLcdIsReady(port);
  if (ready == 0)
    return ready;
  return pdVmsLcdWrite(port, U32_AT(VMU_LCD_PICTURES + 4u * (uint32_t)n), 2);
}
HW_EXPORT(8c018c20, RET(vmu_show_picture((int32_t)ARG0, (int32_t)ARG1)))

/* was func_8C01989A: installs the backup library's two callbacks */
void vmu_install_callbacks(void) {
  bu_8C0A00DA(VMU_ON_COMPLETE);
  bu_8C0A00E0(VMU_CALLBACK_2);
}
/* not exported (S13 difftest: no route reaches it): HW_EXPORT(8c01989a, vmu_install_callbacks()) */

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
    vmu_on_op2(drive, op, stat);
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
