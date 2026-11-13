/* Pen Pen port, S17 (b02): the memory card front end (game/vmu.c's neighbours): a drive's record cleared, the
 * backup library's two callbacks installed (completion: vmu_on_complete; progress: vmu_on_progress), and the
 * library's shutdown. */
#include "hw.h"
#include "sdk.h"
#include "vmu.h"

#define VMU_ON_COMPLETE 0x8C0198ACu /* game/vmu.c's vmu_on_complete */
#define VMU_ON_PROGRESS 0x8C019928u /* vmu_on_progress below */

/* the backup library (SDK; 0x8C0A00DA and 0x8C0A00E0 have no S2 name: each takes a callback, the completion and
 * the progress one by the way they are used) */
static void bu_8C0A00DA(gaddr callback) { CALL1(0x8C0A00DAu, callback); }
static void bu_8C0A00E0(gaddr callback) { CALL1(0x8C0A00E0u, callback); }
static int32_t buExit(void) { return (int32_t)CALL0(0x8C09FF4Au); }

/* was func_8C01986A: a drive's record back to empty (operation 2's handling in vmu_on_complete) */
void vmu_drive_clear(int32_t drive) {
  VmuDrive *d = vmu_drive_at(drive);
  d->progress = 0;
  d->progress_max = 0;
  d->busy = 0;
  d->ready = 0;
  d->has_info = 0;
  d->unknown50 = 0;
  for (unsigned i = 0; i < sizeof d->info; i++)
    d->info[i] = 0;
}
HW_EXPORT(8c01986a, vmu_drive_clear((int32_t)ARG0))

/* was func_8C01989A: installs the backup library's two callbacks (buInit's callback) */
void vmu_install_callbacks(void) {
  bu_8C0A00DA(VMU_ON_COMPLETE);
  bu_8C0A00E0(VMU_ON_PROGRESS);
}
HW_EXPORT(8c01989a, vmu_install_callbacks())

/* was func_8C019928: the progress callback: an operation on a drive is at count of max */
int32_t vmu_on_progress(int32_t drive, int32_t op, int32_t count, int32_t max) {
  VmuDrive *d = vmu_drive_at(drive);
  d->progress = count;
  d->progress_max = max;
  d->busy = op;
  return 0;
}
HW_EXPORT(8c019928, RET(vmu_on_progress((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3)))

/* was func_8C018E2E: the backup library shut down (buExit retried until it succeeds) */
void backup_exit(void) {
  while (buExit() != 0) {
  }
}
HW_EXPORT(8c018e2e, backup_exit())
