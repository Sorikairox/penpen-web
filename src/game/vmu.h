/* Pen Pen port, S17 (b02): the memory card front end's per-drive record (moved out of game/vmu.c so that
 * game/vmu_drive.c shares it; the lead removes the copy in vmu.c). */
#ifndef PENPEN_VMU_H
#define PENPEN_VMU_H
#include "hw.h"

/* S18 b01: the two block counts of BUS_DISKINFO the memory card screen shows (the rest stays bytes) */
typedef struct VmuDiskInfo {
  uint8_t unknown00[0x22];
  uint16_t blocks22;     /* 0x22: drawn second (x 320) */
  uint16_t unknown24;
  uint16_t blocks26;     /* 0x26: drawn first (x 192) */
  uint8_t unknown28[0x38 - 0x28];
} VmuDiskInfo;
HW_OFFSET(VmuDiskInfo, blocks22, 0x22);
HW_OFFSET(VmuDiskInfo, blocks26, 0x26);
HW_SIZE(VmuDiskInfo, 0x38); /* the 0x38 bytes of VmuDrive.info */

typedef struct VmuDrive {
  int16_t ready;         /* 0x00: a card was mounted */
  int16_t has_info;      /* 0x02: its disk information was read */
  int32_t status;        /* 0x04: the last operation's status */
  int32_t progress;      /* 0x08: S17 b02: the progress callback's count */
  int32_t progress_max;  /* 0x0C: S17 b02: the progress callback's total */
  int32_t busy;          /* 0x10: cleared when an operation completes (the progress callback stores its op here) */
  uint8_t info[0x4C - 0x14]; /* 0x14: BUS_DISKINFO (buGetDiskInfo; S18 b01: VmuDiskInfo, vmu_disk_info) */
  int32_t unknown4c;     /* 0x4C: set by operation 0x12, cleared by 2 */
  int32_t unknown50;     /* 0x50: S17 b02: cleared with the record */
  int32_t unknown54;     /* 0x54: (stat << 9) + 32 for operation 0x12 */
  int32_t unknown58;     /* 0x58: stat for operation 0x12 */
} VmuDrive;
HW_OFFSET(VmuDrive, progress, 0x08);
HW_OFFSET(VmuDrive, progress_max, 0x0C);
HW_OFFSET(VmuDrive, busy, 0x10);
HW_OFFSET(VmuDrive, info, 0x14);
HW_OFFSET(VmuDrive, unknown4c, 0x4C);
HW_OFFSET(VmuDrive, unknown50, 0x50);
HW_SIZE(VmuDrive, 0x5C);

#define VMU_DRIVES 0x8C1B81D0u /* a VmuDrive per drive (0-7) */
static inline VmuDrive *vmu_drive_at(int32_t drive) { return (VmuDrive *)hw_ptr(VMU_DRIVES + (uint32_t)drive * sizeof(VmuDrive)); }
/* S18 b01: a drive's disk information, typed */
static inline const VmuDiskInfo *vmu_disk_info(const VmuDrive *d) { return (const VmuDiskInfo *)(const void *)d->info; }

/* game/vmu_drive.c (S17 b02) */
void vmu_drive_clear(int32_t drive);
void vmu_install_callbacks(void);
int32_t vmu_on_progress(int32_t drive, int32_t op, int32_t count, int32_t max);
void backup_exit(void);
/* S18 b01: game/vmu_info.c */
void vmu_info_draw(int32_t drive);

#endif
