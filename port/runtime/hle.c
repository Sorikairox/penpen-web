/* Pen Pen port, S3: HLE boundary. The SDK functions the game calls (port/symbols/sdk_calls.tsv) are not
 * translated: the dispatcher sends their addresses here. Every call is counted and logged (name + r4-r7);
 * a few have real behaviour so the boot can go on: the heap, files on the disc, pads, V-sync, timer.
 * Everything else returns 0. Semantics from docs/port/SDK.md 2; S4+ replaces these with full HLE. */
#include "rt.h"

typedef struct { uint32_t a; const char *name, *lib; hle_fn fn; uint32_t calls; } hle_ent;
static void h_default(SH4 *c);

static hle_ent tab[] = { /* sorted by address */
    /* BIOS vectors (0x8C0000B0...): hle_init points each vector word at its fake entry  */
    {0x8C000F10u, "bios_sysinfo", "bios", h_default, 0},
    {0x8C000F18u, "bios_font", "bios", h_default, 0},
    {0x8C000F20u, "bios_flash", "bios", h_default, 0},
    {0x8C000F28u, "bios_gd", "bios", h_default, 0},
    {0x8C000F30u, "bios_misc", "bios", h_default, 0},
    {0x8C000F38u, "bios_menu", "bios", h_default, 0},
    /* the SDK entry points */
#define HLE_NAME(a, n, lib) {a, #n, #lib, h_default, 0},
#include "hle_names.h"
#undef HLE_NAME
};
#define NTAB ((int)(sizeof tab / sizeof tab[0]))
static hle_ent *cur;
int hle_max_frames = 3;
int hle_verbose = 600;
static uint32_t seq;

hle_fn hle_find(uint32_t a) {
  int lo = 0, hi = NTAB - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    if (tab[m].a == a) { cur = &tab[m]; return tab[m].fn; }
    if (tab[m].a < a) lo = m + 1; else hi = m - 1;
  }
  return 0;
}

#define R(i) c->r[i]
static void logcall(SH4 *c, const char *extra) {
  cur->calls++;
  seq++;
  /* the first calls of each function in full, then only counts (syMalloc/syFree and maths are frequent) */
  if (seq <= (uint32_t)hle_verbose && cur->calls <= 8)
    rt_log("hle %u %s(%x, %x, %x, %x)%s from %08x", seq, cur->name, R(4), R(5), R(6), R(7), extra, c->pr);
}
static void h_default(SH4 *c) { logcall(c, ""); R(0) = 0; }

/* ---- syMalloc: first-fit over the heap the game hands to syMallocInit, 32-byte aligned ---- */
#define NBLK 8192
static struct { uint32_t a, size; int used; } blk[NBLK];
static int nblk;
static void h_syMallocInit(SH4 *c) {
  logcall(c, "");
  uint32_t a = (R(4) + 31) & ~31u, e = (R(4) + R(5)) & ~31u;
  blk[0].a = a; blk[0].size = e - a; blk[0].used = 0; nblk = 1;
  R(0) = 0;
}
static void h_syMalloc(SH4 *c) {
  logcall(c, "");
  uint32_t n = (R(4) + 31) & ~31u;
  if (!n) n = 32;
  for (int i = 0; i < nblk; i++)
    if (!blk[i].used && blk[i].size >= n) {
      if (blk[i].size > n && nblk < NBLK) {
        memmove(&blk[i + 2], &blk[i + 1], (size_t)(nblk - i - 1) * sizeof blk[0]);
        blk[i + 1].a = blk[i].a + n; blk[i + 1].size = blk[i].size - n; blk[i + 1].used = 0;
        blk[i].size = n; nblk++;
      }
      blk[i].used = 1;
      R(0) = blk[i].a;
      return;
    }
  rt_log("hle syMalloc(%x) failed", R(4));
  R(0) = 0;
}
static void h_syFree(SH4 *c) {
  logcall(c, "");
  for (int i = 0; i < nblk; i++)
    if (blk[i].a == R(4) && blk[i].used) {
      blk[i].used = 0;
      if (i + 1 < nblk && !blk[i + 1].used) {
        blk[i].size += blk[i + 1].size;
        memmove(&blk[i + 1], &blk[i + 2], (size_t)(nblk - i - 2) * sizeof blk[0]); nblk--;
      }
      if (i > 0 && !blk[i - 1].used) {
        blk[i - 1].size += blk[i].size;
        memmove(&blk[i], &blk[i + 1], (size_t)(nblk - i - 1) * sizeof blk[0]); nblk--;
      }
      break;
    }
  R(0) = 0;
}

/* ---- gdFs on the disc reader ---- */
#define NFH 16
static struct { uint32_t lba, size, pos; int open; } fh[NFH];
#define FH_BASE 0x00F00000u /* handles are tokens, not guest pointers */
static int fh_of(uint32_t h) { uint32_t i = h - FH_BASE; return i < NFH && fh[i].open ? (int)i : -1; }
static void guest_str(uint32_t a, char *o, int n) {
  int i = 0;
  for (; i < n - 1; i++) { char ch = (char)rd8(a + (uint32_t)i); if (!ch) break; o[i] = (ch >= 32 && ch < 127) ? ch : '?'; }
  o[i] = 0;
}
static void h_gdFsOpen(SH4 *c) {
  char name[40], extra[64];
  guest_str(R(4), name, sizeof name);
  uint32_t lba, size;
  R(0) = 0;
  if (disc_find(name, &lba, &size) == 0)
    for (int i = 0; i < NFH; i++)
      if (!fh[i].open) { fh[i].lba = lba; fh[i].size = size; fh[i].pos = 0; fh[i].open = 1; R(0) = FH_BASE + (uint32_t)i; break; }
  /* file names are the disc's own; log them only as size + found/missing */
  int k = 0;
  const char *t = R(0) ? " -> open" : " -> not found";
  while (*t) extra[k++] = *t++;
  extra[k] = 0;
  logcall(c, extra);
  if (R(0)) rt_log("hle   file size %u", size);
}
static void h_gdFsGetFileSize(SH4 *c) {
  logcall(c, "");
  int i = fh_of(R(4));
  if (i < 0) { R(0) = 0; return; }
  wr32(R(5), fh[i].size);
  R(0) = 1;
}
static void h_gdFsGetFileSctSize(SH4 *c) {
  logcall(c, "");
  int i = fh_of(R(4));
  if (i < 0) { R(0) = 0; return; }
  wr32(R(5), (fh[i].size + 2047) / 2048);
  R(0) = 1;
}
static void h_gdFsRead(SH4 *c) {
  logcall(c, "");
  int i = fh_of(R(4));
  uint32_t n = R(5), dst = R(6);
  if (i < 0) { R(0) = (uint32_t)-1; return; }
  static uint8_t sec[256 * 2048];
  while (n) {
    uint32_t k = n > 256 ? 256 : n;
    if (disc_read_lba(fh[i].lba + fh[i].pos, k, sec)) { R(0) = (uint32_t)-1; return; }
    for (uint32_t j = 0; j < k * 2048; j++) wr8(dst + j, sec[j]);
    fh[i].pos += k; dst += k * 2048; n -= k;
  }
  R(0) = 0;
}
static void h_gdFsClose(SH4 *c) { logcall(c, ""); int i = fh_of(R(4)); if (i >= 0) fh[i].open = 0; R(0) = 0; }
static void h_true(SH4 *c) { logcall(c, ""); R(0) = 1; }
static void h_gdFsGetDrvStat(SH4 *c) { logcall(c, ""); R(0) = 1; /* GDD_DRVSTAT_PAUSE: disc in, idle */ }

/* ---- pads: zeroed per-port structures in the BIOS work area ---- */
static void h_pdGetPeripheral(SH4 *c) { logcall(c, ""); R(0) = GUEST_SCRATCH + (R(4) & 31) * 0x40; }

/* ---- V-sync, callbacks, timer ---- */
static uint32_t vsync_fn, eor_fn, frame;
static void h_njSetVSyncFunction(SH4 *c) { logcall(c, ""); vsync_fn = R(4); R(0) = 0; }
static void h_njSetEORFunction(SH4 *c) { logcall(c, ""); eor_fn = R(4); R(0) = 0; }
static void h_njWaitVSync(SH4 *c) {
  logcall(c, "");
  frame++;
  rt_log("hle frame %u (vsync callback %08x)", frame, vsync_fn);
  if (frame >= (uint32_t)hle_max_frames) rt_stop("frame limit");
  if (vsync_fn) rt_guest_call(c, vsync_fn, 0, 0, 0, 0);
  R(0) = 0;
}
static uint32_t tmr;
static void h_syTmrGetCount(SH4 *c) { logcall(c, ""); tmr += 3125; R(0) = tmr; /* ~1/16 ms per call at P0/4 */ }
static uint32_t chain_n;
static void h_syChainAddHandler(SH4 *c) { logcall(c, ""); R(0) = ++chain_n; }
static void h_syBtExit(SH4 *c) { logcall(c, ""); rt_stop("the game asked to exit to the BIOS"); }
static void h_syCfgGetSoundMode(SH4 *c) { logcall(c, ""); if (R(4)) wr32(R(4), 0); R(0) = 0; }

static const struct { const char *name; hle_fn fn; } impl[] = {
  {"syMallocInit", h_syMallocInit}, {"syMalloc", h_syMalloc}, {"syFree", h_syFree},
  {"gdFsOpen", h_gdFsOpen}, {"gdFsGetFileSize", h_gdFsGetFileSize}, {"gdFsGetFileSctSize", h_gdFsGetFileSctSize},
  {"gdFsRead", h_gdFsRead}, {"gdFsClose", h_gdFsClose}, {"gdFsChangeDir", h_true},
  {"gdFsGetDrvStat", h_gdFsGetDrvStat}, {"pdGetPeripheral", h_pdGetPeripheral},
  {"njSetVSyncFunction", h_njSetVSyncFunction}, {"njSetEORFunction", h_njSetEORFunction},
  {"njWaitVSync", h_njWaitVSync}, {"syTmrGetCount", h_syTmrGetCount}, {"syChainAddHandler", h_syChainAddHandler},
  {"syBtExit", h_syBtExit}, {"syCfgGetSoundMode", h_syCfgGetSoundMode},
};

void hle_init(SH4 *c) {
  (void)c;
  for (int j = 1; j < NTAB; j++)
    if (tab[j].a <= tab[j - 1].a) rt_stop("hle table not sorted");
  for (unsigned i = 0; i < sizeof impl / sizeof impl[0]; i++)
    for (int j = 0; j < NTAB; j++)
      if (!strcmp(tab[j].name, impl[i].name)) tab[j].fn = impl[i].fn;
  /* the BIOS syscall vectors read by the game's syscall stubs */
  static const uint32_t vec[][2] = {{0x8C0000B0u, 0x8C000F10u}, {0x8C0000B4u, 0x8C000F18u}, {0x8C0000B8u, 0x8C000F20u},
                                    {0x8C0000BCu, 0x8C000F28u}, {0x8C0000C0u, 0x8C000F30u}, {0x8C0000E0u, 0x8C000F38u}};
  for (unsigned i = 0; i < sizeof vec / sizeof vec[0]; i++) wr32(vec[i][0], vec[i][1]);
}

void hle_summary(void) {
  uint32_t total = 0;
  int used = 0;
  for (int i = 0; i < NTAB; i++) if (tab[i].calls) { total += tab[i].calls; used++; }
  rt_log("hle summary: %u calls to %d functions, %u frames", total, used, frame);
  for (int i = 0; i < NTAB; i++)
    if (tab[i].calls) rt_log("hle   %s %u", tab[i].name, tab[i].calls);
}
