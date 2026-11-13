/* Pen Pen port, S20: the program's start-up (region sy_start: the code at the load address that the boot ROM jumps
 * to). It sets the cache controller, marks the stack area, clears the zero-initialised sections and copies the
 * initialised ones from their load images, lists the SDK libraries' information records, and runs main. On the yield
 * chain (main runs the whole game: docs/port/DECOMP.md 4), so start and its body are resumable; neither ever returns
 * (main ends the run with syBtExit). */
#include "../lib.h"

#define CCR             0xFF00001Cu /* the cache control register */
#define CCR_KEEP        0x000089AFu
#define CCR_ICACHE_INVALIDATE 0x0800u
#define STACK_AREA      0x8C00C000u /* to 0x8C00F400: filled with "SEGA" (a mark for stack-depth checks) */
#define STACK_AREA_END  0x8C00F400u
#define STACK_MARK      0x41474553u
#define g_probe         GUEST_VAR(gaddr, 0x8C179CE4u) /* a byte read at start-up (its value is not used) */
#define g_lib_table     GUEST_VAR(gaddr, 0x8C179964u) /* the program's table of library records */
#define MAIN            0x8C0164B4u

/* the start-up's section table: the bounds are words in RAM (the linker's symbols) */
#define g_bss_start     GUEST_VAR(gaddr, 0x8C0668F4u)
#define g_bss_end       GUEST_VAR(gaddr, 0x8C066900u)
#define g_data_image    GUEST_VAR(gaddr, 0x8C0668DCu)
#define g_data_start    GUEST_VAR(gaddr, 0x8C0668E4u)
#define g_data_end      GUEST_VAR(gaddr, 0x8C0668F0u)
#define g_bss2_start    GUEST_VAR(gaddr, 0x8C179990u)
#define g_bss2_end      GUEST_VAR(gaddr, 0x8C17999Cu)
#define g_data2_image   GUEST_VAR(gaddr, 0x8C179974u)
#define g_data2_start   GUEST_VAR(gaddr, 0x8C17997Cu)
#define g_data2_end     GUEST_VAR(gaddr, 0x8C179988u)

/* bytes, first to last (the bounds are read again at each step, as the original reads them) */
static void clear_bytes(gaddr p, const gaddr *end) {
  for (; p < *end; p++) *(uint8_t *)hw_ptr(p) = 0;
}
static void copy_bytes(gaddr dst, const gaddr *end, gaddr src) {
  for (; dst < *end; dst++, src++) *(uint8_t *)hw_ptr(dst) = *(const uint8_t *)hw_ptr(src);
}

/* was func_8C0100D8: the sections: the first zero-initialised one cleared, the first initialised one copied, then the
 * second pair the same way */
void sy_init_sections(void) {
  clear_bytes(g_bss_start, &g_bss_end);
  copy_bytes(g_data_start, &g_data_end, g_data_image);
  clear_bytes(g_bss2_start, &g_bss2_end);
  copy_bytes(g_data2_start, &g_data2_end, g_data2_image);
}
HW_EXPORT(8c0100d8, sy_init_sections())

/* was func_8C0100C8: the libraries' records listed */
int32_t sy_list_libraries(void) { return lib_registry_add_all(g_lib_table); }
HW_EXPORT(8c0100c8, RET(sy_list_libraries()))

/* was func_8C0100A0: the start-up's body. Its frame (the return address it pushes) lies in the stack area it then
 * fills, so the mark overwrites it: kept with hw_stack_alloc so that r15 is the original's under main. */
void sy_start_body(void) {
  HW_RESUMABLE0(0x8C0100A0u);
  HW_RESUME_AT(0);
  hw_stack_alloc(4); /* never given back: main does not return */
  (void)hw_read8(g_probe);
  for (gaddr p = STACK_AREA; p < STACK_AREA_END; p += 4) U32_AT(p) = STACK_MARK;
  sy_init_sections();
  sy_list_libraries();
  HW_YIELDING(0, CALL0(MAIN));
  lib_registry_remove_all(g_lib_table);
}
HW_EXPORT_MAIN(8c0100a0, sy_start_body())

/* was start (func_8C010000): the cache controller set (the instruction cache invalidated, the other bits kept as the
 * mask says), then the body */
void sy_start(void) {
  HW_RESUMABLE0(0x8C010000u);
  HW_RESUME_AT(0);
  hw_write32(CCR, (hw_read32(CCR) & CCR_KEEP) | CCR_ICACHE_INVALIDATE);
  HW_YIELDING(0, sy_start_body());
}
HW_EXPORT_MAIN(8c010000, sy_start())
