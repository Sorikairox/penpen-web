/* Pen Pen port, S3: boot. Read IP.BIN and 1ST_READ.BIN from the player's disc into guest RAM (as the
 * BIOS does: IP.BIN at 0x8C008000, the program at 0x8C010000), set the CPU up as the BIOS leaves it,
 * and call the entry point; the recompiled code then runs until the frame limit, a stop or a fault. */
#include "rt.h"

#define ENTRY 0x8C010000u

static SH4 cpu;

static int load_file(const char *name, uint32_t at) {
  uint32_t lba, size;
  if (disc_find(name, &lba, &size)) return -1;
  static uint8_t buf[256 * 2048];
  for (uint32_t done = 0; done < size;) {
    uint32_t n = (size - done + 2047) / 2048;
    if (n > 256) n = 256;
    if (disc_read_lba(lba + done / 2048, n, buf)) return -2;
    uint32_t k = n * 2048 < size - done ? n * 2048 : size - done;
    memcpy(sh4_ram + ((at + done) & 0xFFFFFF), buf, k);
    done += k;
  }
  return (int)size;
}

#ifdef __wasm__
__attribute__((export_name("rtw_start")))
#endif
void rtw_start(void) {
  SH4 *c = &cpu;
  rt_cpu = c;
  sh4_init_tables();
  if (disc_init() < 0) rt_stop("no disc");
  rt_log("boot disc files %d", disc_file_count());
  static uint8_t ip[16 * 2048];
  if (disc_read_lba(45000, 16, ip)) rt_stop("cannot read IP.BIN");
  memcpy(sh4_ram + 0x8000, ip, sizeof ip);
  int n = load_file("1ST_READ.BIN", ENTRY);
  if (n <= 0) rt_stop("1ST_READ.BIN not found");
  rt_log("boot 1ST_READ.BIN %u bytes at %08x, %d recompiled functions", (uint32_t)n, ENTRY, rt_fn_count);
  sh4_reset(c);
  hle_init(c);
  /* the state the BIOS boot leaves: privileged, bank 1, interrupts masked; stack below IP.BIN's end */
  sh4_set_sr(c, 0x600000F0u);
  c->r[15] = 0x8C00F400u;
  c->vbr = 0x8C00F400u;
  c->fpscr = 0x00040001u;
  c->pr = 0x8C000F00u;
  c->budget = 200000000;
  rt_log("boot calling the entry point");
  rt_call(c, ENTRY);
  rt_log("boot the entry point returned to %08x", c->pc);
  rt_stop("returned");
}

#ifndef __wasm__
#include <stdio.h>
#include <stdlib.h>
extern const char *disc_dir;
void rtw_log(const char *p, int len) { fwrite(p, 1, (size_t)len, stdout); fputc('\n', stdout); fflush(stdout); }
int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--disc")) disc_dir = argv[++i];
    else if (!strcmp(argv[i], "--frames")) hle_max_frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--verbose")) hle_verbose = atoi(argv[++i]);
  }
  rtw_start();
  return 0;
}
#else
__attribute__((export_name("rt_set_frames"))) void rt_set_frames(int n) { hle_max_frames = n; }
__attribute__((export_name("rt_set_verbose"))) void rt_set_verbose(int n) { hle_verbose = n; }
#endif
