/* Pen Pen port, S18 b01: the SDK sample's sbInitSystem / sbExitSystem as linked into the game: the systems started
 * (hardware, heap, Ninja, peripherals, GD-ROM file system) with the interrupts masked, and shut down in reverse.
 * The interrupt mask is SR's IMASK field: written through the CPU's SR as the original's stc / ldc do. */
#include "hw.h"
#include "sdk.h"
#include "sb_system.h"

#define SR_IMASK_CLEAR 0xFFFFFF0Fu /* SR without its IMASK bits */
#define SR_IMASK_ALL   0xF0u       /* IMASK 15: every interrupt masked */

#define HEAP_START_PTR GUEST_VAR(gaddr, 0x8C23D920u) /* the end of the program's data: the heap starts after it */
#define HEAP_END       0x8CE3FFE0u
#define NINJA_WORK_SIZE 0x120000u
#define PAD_RECV_BUF   0x8C1970C0u
#define PAD_SEND_BUF   0x8C1A30E0u
#define GDFS_WORK      0x8C1AF100u /* aligned to 32 bytes, then 0x20 on */
#define GDFS_DIRENT    0x8C1B0494u
#define GDFS_MAX_OPEN  8
#define GDFS_DIRENT_COUNT 0x100
#define GDFS_ERR_NO_DISC (-0x17)   /* the error that leaves for the boot menu */
#define ALIGN32_DOWN(a) ((a) & ~0x1Fu)

/* SDK (port/symbols/sdk_calls.tsv) */
static inline void syHwInit(void) { CALL0(0x8C09F7D0u); }
static inline void syHwInit2(void) { CALL0(0x8C09F7FAu); }
static inline void syHwFinish(void) { CALL0(0x8C09F80Au); }
static inline void syMallocInit(gaddr heap, uint32_t size) { CALL2(0x8C09F428u, heap, size); }
static inline void syMallocFinish(void) { CALL0(0x8C09F45Eu); }
static inline void nj_SetWorkSize(uint32_t size) { CALL1(0x8C0A1294u, size); }
static inline void njInitSystem(int32_t mode, int32_t frame, int32_t count) { CALL3(0x8C0A1486u, mode, frame, count); }
static inline void njExitSystem(void) { CALL0(0x8C0A2060u); }
static inline void pdInitPeripheral(int32_t logic, gaddr recv, gaddr send) { CALL3(0x8C09ED34u, logic, recv, send); }
static inline void pdExitPeripheral(void) { CALL0(0x8C09F284u); }
static inline void gdFsFinish(void) { CALL0(0x8C09EB0Au); }
/* gdFsInit through its thunk func_8C09D4F2 (as the original calls it) */
static inline int32_t gdFsInit(int32_t max_open, gaddr work, int32_t dirents, gaddr dirent_buf) {
  return (int32_t)CALL4(0x8C09D4F2u, max_open, work, dirents, dirent_buf);
}
/* func_8C061DE2 (core/rtc_set.c, not declared in a header): the clock's shutdown */
static void rtc_shutdown(void) { CALL0(0x8C061DE2u); }

static void interrupts_mask_all(void) {
  sh4_set_sr(hw_cpu, ((sh4_get_sr(hw_cpu) & SR_IMASK_CLEAR) | SR_IMASK_ALL) & SR_MASK);
}
static void interrupts_unmask(void) { sh4_set_sr(hw_cpu, (sh4_get_sr(hw_cpu) & SR_IMASK_CLEAR) & SR_MASK); }

/* was func_8C010184: sbInitSystem(mode, frame, count): with the interrupts masked the hardware, the heap (from the
 * 32-byte aligned end of the program's data, P1, plus 0x20, up to 0x8CE3FFE0), Ninja's work size and system, the
 * second hardware stage and the peripherals; then the interrupts on and the GD-ROM file system started, retried
 * until it succeeds (no disc: back to the boot menu) */
void sb_init_system(int32_t mode, int32_t frame, int32_t count) {
  interrupts_mask_all();
  syHwInit();
  gaddr heap = ALIGN32_DOWN(HEAP_START_PTR | 0x80000000u);
  syMallocInit(heap + 0x20, HEAP_END - heap);
  nj_SetWorkSize(NINJA_WORK_SIZE);
  njInitSystem(mode, frame, count);
  syHwInit2();
  pdInitPeripheral(0, PAD_RECV_BUF, PAD_SEND_BUF);
  interrupts_unmask();
  int32_t err;
  do {
    err = gdFsInit(GDFS_MAX_OPEN, ALIGN32_DOWN(GDFS_WORK) + 0x20, GDFS_DIRENT_COUNT, ALIGN32_DOWN(GDFS_DIRENT) + 0x20);
    if (err == GDFS_ERR_NO_DISC)
      syBtExit();
  } while (err != 0);
}
HW_EXPORT(8c010184, sb_init_system((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C01022C: sbExitSystem: the file system, the clock, the peripherals, Ninja, the heap and the hardware
 * shut down; then every interrupt masked */
void sb_exit_system(void) {
  gdFsFinish();
  rtc_shutdown();
  pdExitPeripheral();
  njExitSystem();
  syMallocFinish();
  syHwFinish();
  interrupts_mask_all();
}
HW_EXPORT(8c01022c, sb_exit_system())
