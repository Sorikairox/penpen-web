/* Pen Pen port, S3/S4: HLE boundary. The SDK functions the game calls (port/symbols/sdk_calls.tsv, plus
 * hle_extra.tsv) are reached through rt_call, which asks this table first. Each entry is either
 *   - HLE: a host function here (files, pads, frame clock, timers, interrupt chains, and the logging
 *     stubs of the libraries later sessions implement: Ninja drawing S5, pd/bu S6, sd S7), or
 *   - guest: the SDK's own code, recompiled with the game (tools/s3.sh gen --ranges boot) and run as is:
 *     the pure functions (heap, matrix/vector maths, camera, chunk-file parser) whose results the game
 *     uses; checked by the differential harness like the game code.
 * Every call is counted (per run and per frame). The frame loop of the console (end of render, V-blank,
 * timer, pads, drive) is driven from njWaitVSync: one V-blank per call, no wall time anywhere, so a run is
 * a deterministic function of the disc and the pad script. Semantics: docs/port/SDK.md 2, RECOMP.md 7. */
#include "rt.h"
#include "../render/nj.h"

typedef struct { uint32_t a; const char *name, *lib; hle_fn fn; uint32_t calls, fcalls, first; } hle_ent;
static void h_default(SH4 *c);

static hle_ent tab[] = { /* sorted by address */
    /* BIOS vectors (0x8C0000B0...): hle_init points each vector word at its fake entry  */
    {0x8C000F10u, "bios_sysinfo", "bios", h_default, 0, 0, 0},
    {0x8C000F18u, "bios_font", "bios", h_default, 0, 0, 0},
    {0x8C000F20u, "bios_flash", "bios", h_default, 0, 0, 0},
    {0x8C000F28u, "bios_gd", "bios", h_default, 0, 0, 0},
    {0x8C000F30u, "bios_misc", "bios", h_default, 0, 0, 0},
    {0x8C000F38u, "bios_menu", "bios", h_default, 0, 0, 0},
    /* the SDK entry points */
#define HLE_NAME(a, n, lib) {a, #n, #lib, h_default, 0, 0, 0},
#include "hle_names.h"
#undef HLE_NAME
};
#define NTAB ((int)(sizeof tab / sizeof tab[0]))
static hle_ent *cur;
int hle_max_frames = 3;
int hle_verbose = 600;
int hle_digest_every = 1;
int hle_frame_log = 1;
static uint32_t seq, frame;
static uint32_t f_hle, f_guest; /* calls in the current frame */

static hle_ent *lookup(uint32_t a) {
  int lo = 0, hi = NTAB - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    if (tab[m].a == a) return &tab[m];
    if (tab[m].a < a) lo = m + 1; else hi = m - 1;
  }
  return 0;
}
static void first_seen(hle_ent *e) {
  if (!e->calls) {
    e->first = frame;
    if (hle_verbose) rt_log("hle new %s %s at frame %u", e->fn ? "hle" : "guest", e->name, frame);
  }
}
/* the dispatcher's question: HLE function for a, or 0 (guest SDK code, counted here too, or unknown) */
hle_fn hle_find(uint32_t a) {
  hle_ent *e = lookup(a);
  if (!e) return 0;
  if (e->fn) { cur = e; return e->fn; }
  first_seen(e);
  e->calls++; e->fcalls++; f_guest++;
  return 0;
}
int hle_known(uint32_t a) { hle_ent *e = lookup(a); return e && e->fn; }

#define R(i) c->r[i]
/* the last calls, dumped when the run stops (to see what led to a fault) */
#define RING 24
static struct { uint32_t seq, pr, a[4]; const char *name; } ring[RING];
void hle_ring_dump(void) {
  for (uint32_t k = seq > RING ? seq - RING : 0; k < seq; k++) {
    int i = (int)(k % RING);
    rt_log("hle last %u %s(%x, %x, %x, %x) from %08x", ring[i].seq, ring[i].name, ring[i].a[0], ring[i].a[1],
           ring[i].a[2], ring[i].a[3], ring[i].pr);
  }
}
static void logcall(SH4 *c, const char *extra) {
  first_seen(cur);
  cur->calls++; cur->fcalls++; f_hle++;
  seq++;
  { int i = (int)((seq - 1) % RING); ring[i].seq = seq; ring[i].pr = c->pr; ring[i].name = cur->name;
    for (int k = 0; k < 4; k++) ring[i].a[k] = R(4 + k); }
  /* the first calls of each function in full, then only counts */
  if (seq <= (uint32_t)hle_verbose && cur->calls <= 8)
    rt_log("hle %u %s(%x, %x, %x, %x)%s from %08x", seq, cur->name, R(4), R(5), R(6), R(7), extra, c->pr);
}
static void h_default(SH4 *c) { logcall(c, ""); R(0) = 0; }
void hle_logcall(SH4 *c, const char *extra) { logcall(c, extra); }
uint32_t hle_frame(void) { return frame; }
static void h_true(SH4 *c) { logcall(c, ""); R(0) = 1; }

/* ---- guest SDK functions: pure code, run as recompiled SDK code (not HLE) ----
 * Heap: the SDK allocator itself (32-byte units, a 32-byte header in front of each block, blocks cut from
 * the end of the first free block that fits, NULL when none fits, coalescing free, free(NULL) a no-op;
 * syMallocInit aligns the arena start up and its size down to 32 bytes). Maths, camera and view: Ninja's
 * own code on its globals (current matrix, matrix stack of njInitMatrix), so the Ninja HLE of S5 reads
 * the real matrices. */
static const char *const guest[] = {
  "syMallocInit", "syMallocFinish", "syMalloc", "syFree", "sy_Nop1", "sy_Nop2",
  "njInitMatrix", "njPushMatrix", "njPopMatrix", "njUnitMatrix", "njGetMatrix", "njSetMatrix", "njMultiMatrix",
  "njInvertMatrix", "njTranslate", "njTranslateV", "njScale", "njScaleV", "njRotateX", "njRotateY", "njRotateZ",
  "njRotateXYZ", "njRotateZXY", "njRotate", "njCalcPoint", "njCalcVector", "njInnerProduct", "njOuterProduct",
  "njScalor", "njScalor2", "njUnitVector", "njSin", "njCos", "njTan", "njSqrt", "njRandom", "njProjectScreen",
  "njCalcScreen", "njInitView", "njSetView", "njSetAspect", "njSetScreen", "nj_InitCamera", "nj_SetCamera",
  "nj_ReadChunkFile", "nj_MemCopy4",
  /* S5: pure setters of the Ninja render state; the draw HLE reads their globals (render/nj.h) */
  "njSetTexture", "njControl3D", "njSetConstantAttr", "njSetConstantMaterial", "njInitTextureBuffer",
};
/* the heap's free list (SDK globals of syMalloc, for the per-frame log): rover -> circular list of
 * {next, size in 32-byte units} */
#define HEAP_ROVER 0x8C1C6754u
static uint32_t heap_free(uint32_t *blocks) {
  uint32_t p = rd32(HEAP_ROVER), start = p, sum = 0, n = 0;
  *blocks = 0;
  if (!sh4_is_ram(p)) return 0;
  do {
    sum += rd32(p + 4) * 32; n++;
    p = rd32(p);
  } while (p != start && sh4_is_ram(p) && n < 100000);
  *blocks = n;
  return sum;
}

/* ---- clock: TMU0 as syHwInit sets it (TCR0 = 2: P-clock 50 MHz / 64 = 781 250 Hz, counting down from
 * 0xFFFFFFFF; syTmrGetCount = ~TCNT0), advanced by the frame clock: NTSC 59.94 Hz V-blanks, so
 * 781 250 * 1001 / 60 000 ticks per frame; within a frame each read advances one tick (time passes
 * between two reads), never reaching the next frame's first tick. */
#define TMU_HZ 781250u
static uint32_t tmr_intra;
static uint32_t tmr_at(uint32_t f) { return (uint32_t)(((uint64_t)f * TMU_HZ * 1001u) / 60000u); }
uint32_t hle_tmu_count(void) {
  uint32_t base = tmr_at(frame), span = tmr_at(frame + 1) - base;
  if (tmr_intra + 1 < span) tmr_intra++;
  return base + tmr_intra;
}
static void h_syTmrGetCount(SH4 *c) { logcall(c, ""); R(0) = hle_tmu_count(); }

/* ---- syChain: interrupt event chains. The SDK keys them by event code: 0x900 = Kamui's V-blank callback
 * (type 1), 0x920 = Kamui's type-2 callback (end of render), 0xAxx/0xBxx Holly IRQ groups, below 0x900
 * VBR exception vectors. Handlers run in priority order (lower value first), each with its argument. */
#define NCHAIN 32
static struct { uint32_t id, event, fn, prio, arg; } chain[NCHAIN];
static int nchain;
static uint32_t chain_next = 1;
static void h_syChainAddHandler(SH4 *c) {
  logcall(c, "");
  uint32_t ev = R(4) & 0xFFFF;
  if (nchain >= NCHAIN) { R(0) = 0; return; }
  int i = nchain;
  while (i > 0 && (chain[i - 1].event > ev || (chain[i - 1].event == ev && chain[i - 1].prio > R(6)))) i--;
  memmove(&chain[i + 1], &chain[i], (size_t)(nchain - i) * sizeof chain[0]);
  chain[i].id = chain_next++; chain[i].event = ev; chain[i].fn = R(5); chain[i].prio = R(6); chain[i].arg = R(7);
  nchain++;
  if (ev != 0x900 && ev != 0x920) rt_log("hle chain event %x is not fired by the frame loop", ev);
  R(0) = chain[i].id;
}
static void h_syChainDeleteHandler(SH4 *c) {
  logcall(c, "");
  for (int i = 0; i < nchain; i++)
    if (chain[i].id == R(4)) { memmove(&chain[i], &chain[i + 1], (size_t)(nchain - i - 1) * sizeof chain[0]); nchain--; break; }
  R(0) = 0;
}
static void chain_fire(SH4 *c, uint32_t event) {
  for (int i = 0; i < nchain; i++)
    if (chain[i].event == event) rt_guest_call(c, chain[i].fn, chain[i].arg, 0, 0, 0);
}

/* ---- pads: the PDS_PERIPHERAL structures of pdGetPeripheral, in the BIOS work area (no BIOS runs).
 * Layout: id 0, support 4, on 8, off 12, press 16, release 20, r 24, l 26, x1 28, y1 30, x2 32, y2 34,
 * name 36, extend 40, old 44, info 48 (pointer to {type, ...}: 1 = controller, 0 = nothing).
 * Port A (0) holds a standard controller (support 0x000F06FE: buttons B A Start U D L R Y X, analog
 * R/L triggers, analog X/Y); the others are empty. Updated once per V-blank from the pad script. */
#define PAD_BASE (GUEST_SCRATCH + 0x000u)  /* 4 ports x 6 units x 0x40 */
#define PAD_INFO (GUEST_SCRATCH + 0x600u)  /* 0x600: controller info, 0x640: empty info */
#define PAD_STD_SUPPORT 0x000F06FEu
static uint32_t pad_prev[4];
typedef struct { uint32_t from, to, buttons; } pad_ev;
#define NPAD 256
static pad_ev pad_script[NPAD];
static int npad;
static const struct { const char *n; uint32_t bit; } pad_names[] = {
  {"C", 1u << 0}, {"B", 1u << 1}, {"A", 1u << 2}, {"START", 1u << 3}, {"UP", 1u << 4}, {"DOWN", 1u << 5},
  {"LEFT", 1u << 6}, {"RIGHT", 1u << 7}, {"Z", 1u << 8}, {"Y", 1u << 9}, {"X", 1u << 10}, {"D", 1u << 11},
  {"LT", 1u << 16}, {"RT", 1u << 17}, /* analog triggers pulled fully (digital bits + r/l = 255) */
};
/* "F[+N]:BTN[|BTN...],...": hold the buttons on port A during frames F..F+N-1 (N default 4) */
int hle_set_pad_script(const char *s) {
  npad = 0;
  while (*s && npad < NPAD) {
    uint32_t f = 0, n = 4, b = 0;
    while (*s >= '0' && *s <= '9') f = f * 10 + (uint32_t)(*s++ - '0');
    if (*s == '+') { s++; n = 0; while (*s >= '0' && *s <= '9') n = n * 10 + (uint32_t)(*s++ - '0'); }
    if (*s++ != ':') { rt_log("hle pad script: syntax error"); npad = 0; return -1; }
    while (*s && *s != ',') {
      char nm[8]; int k = 0;
      while (*s && *s != '|' && *s != ',' && k < 7) nm[k++] = *s++;
      nm[k] = 0;
      if (*s == '|') s++;
      int ok = 0;
      for (unsigned j = 0; j < sizeof pad_names / sizeof pad_names[0]; j++)
        if (!strcmp(nm, pad_names[j].n)) { b |= pad_names[j].bit; ok = 1; }
      if (!ok && k) { rt_log("hle pad script: unknown button"); npad = 0; return -2; }
    }
    if (*s == ',') s++;
    pad_script[npad].from = f; pad_script[npad].to = f + n; pad_script[npad].buttons = b; npad++;
  }
  rt_log("hle pad script %d events", npad);
  return npad;
}
static uint32_t pad_buttons(uint32_t f) {
  uint32_t b = 0;
  for (int i = 0; i < npad; i++) if (f >= pad_script[i].from && f < pad_script[i].to) b |= pad_script[i].buttons;
  return b;
}
static void pads_update(void) {
  wr32(PAD_INFO, 1); wr32(PAD_INFO + 0x40, 0);
  for (int port = 0; port < 4; port++) {
    uint32_t on = port == 0 ? pad_buttons(frame) : 0, prev = pad_prev[port];
    for (int unit = 0; unit < 6; unit++) {
      uint32_t p = PAD_BASE + (uint32_t)(port * 6 + unit) * 0x40;
      int present = port == 0 && unit == 0;
      wr32(p + 0, (uint32_t)(port * 6 + unit));
      wr32(p + 4, present ? PAD_STD_SUPPORT : 0);
      wr32(p + 8, present ? on : 0);
      wr32(p + 12, present ? ~on : 0);
      wr32(p + 16, present ? on & ~prev : 0);
      wr32(p + 20, present ? ~on & prev : 0);
      wr16(p + 24, present && (on & (1u << 17)) ? 255 : 0);
      wr16(p + 26, present && (on & (1u << 16)) ? 255 : 0);
      for (uint32_t k = 28; k < 36; k += 2) wr16(p + k, 0); /* sticks centred */
      wr32(p + 36, 0); wr32(p + 40, 0);
      wr32(p + 44, present ? prev : 0);
      wr32(p + 48, present ? PAD_INFO : PAD_INFO + 0x40);
    }
    pad_prev[port] = on;
  }
}
static void h_pdGetPeripheral(SH4 *c) {
  logcall(c, "");
  uint32_t port = R(4);
  R(0) = port < 24 ? PAD_BASE + port * 0x40 : 0;
}

/* ---- gdFs on the disc reader. Handles are guest structures in the work area the game gives gdFsInit,
 * with the SDK's field layout (what the SDK's own gdFsTell and CRI read): +0 drive, +8 first LBA,
 * +0x10 size in sectors, +0x14 position (sectors), +0x18 transferred bytes, +0x1C requested sectors,
 * +0x4C open (u16), +0x50 status (u16: 0 idle, 1 complete, 2 reading, 4 busy, 5 error), +0x52 error.
 * gdFsRead is synchronous; gdFsReqRd32 is asynchronous and completes deterministically at the next
 * V-blank or at the 8th status poll in the same frame, whichever is first (the SDK's own wait loops poll
 * within a frame). Drive status: PAUSE (1) idle, PLAY (3) while CDDA plays. */
#define NFH 16
#define FH_STRIDE 0x80u
#define GD_SYS (GUEST_SCRATCH + 0x800u) /* the drive handle (gdFs_Api34) */
#define GD_DRIVE (GUEST_SCRATCH + 0x900u)
enum { ST_IDLE = 0, ST_COMPLETE = 1, ST_READ = 2, ST_BUSY = 4, ST_ERR = 5 };
static uint32_t gd_work, gd_max = 8;
static struct { uint32_t lba, size, nsct; int open, index; uint32_t req_n, req_buf, polls; int pending; } fh[NFH];
static int gd_files_loaded, gd_sys_busy_until;
static int cdda_playing;
static uint32_t fh_addr(int i) { return gd_work + (uint32_t)i * FH_STRIDE; }
static int fh_of(uint32_t h) {
  if (!gd_work || h < gd_work || (h - gd_work) % FH_STRIDE) return -1;
  uint32_t i = (h - gd_work) / FH_STRIDE;
  return i < gd_max && fh[i].open ? (int)i : -1;
}
static void fh_stat(int i, int st) { wr16(fh_addr(i) + 0x50, (uint16_t)st); }
static uint32_t fh_pos(int i) { return rd32(fh_addr(i) + 0x14); }
static void guest_str(uint32_t a, char *o, int n) {
  int i = 0;
  for (; i < n - 1; i++) { char ch = (char)rd8(a + (uint32_t)i); if (!ch) break; o[i] = (ch >= 32 && ch < 127) ? ch : '?'; }
  o[i] = 0;
}
static int read_sectors(uint32_t lba, uint32_t n, uint32_t dst) {
  static uint8_t sec[256 * 2048];
  while (n) {
    uint32_t k = n > 256 ? 256 : n;
    if (disc_read_lba(lba, k, sec)) return -1;
    for (uint32_t j = 0; j < k * 2048; j++) wr8(dst + j, sec[j]);
    lba += k; dst += k * 2048; n -= k;
  }
  return 0;
}
static void h_gdFsInit(SH4 *c) {
  logcall(c, "");
  gd_max = R(4) > NFH ? NFH : R(4);
  gd_work = (R(5) + 31) & ~31u;
  for (uint32_t i = 0; i < gd_max * FH_STRIDE; i += 4) wr32(gd_work + i, 0);
  wr16(GD_SYS + 0x4C, 1); wr16(GD_SYS + 0x50, ST_IDLE); wr32(GD_SYS, GD_DRIVE);
  R(0) = 0;
}
static void h_gdFsOpen(SH4 *c) {
  char name[40], extra[48];
  guest_str(R(4), name, sizeof name);
  uint32_t lba = 0, size = 0;
  int index = -1;
  R(0) = 0;
  if (gd_work && disc_find_ex(name, &lba, &size, &index) == 0)
    for (int i = 0; i < (int)gd_max; i++)
      if (!fh[i].open) {
        uint32_t h = fh_addr(i);
        fh[i].lba = lba; fh[i].size = size; fh[i].nsct = (size + 2047) / 2048; fh[i].open = 1; fh[i].index = index;
        fh[i].pending = 0;
        for (uint32_t k = 0; k < FH_STRIDE; k += 4) wr32(h + k, 0);
        wr32(h + 0, GD_DRIVE); wr32(h + 8, lba); wr32(h + 0x10, fh[i].nsct);
        wr16(h + 0x4C, 1); wr16(h + 0x50, ST_IDLE);
        R(0) = h;
        break;
      }
  /* file names are the disc's own: logged as directory index + the 4-digit group (PLAN 2) + size */
  int k = 0;
  const char *t = R(0) ? " -> open" : " -> not found";
  while (*t) extra[k++] = *t++;
  extra[k] = 0;
  logcall(c, extra);
  if (R(0)) {
    int g = 0, digits = 0;
    while (digits < 4 && name[digits] >= '0' && name[digits] <= '9') g = g * 10 + (name[digits++] - '0');
    gd_files_loaded++;
    rt_log("hle file open #%d group %d size %u at frame %u", index, digits == 4 ? g : -1, size, frame);
  }
}
static void h_gdFsClose(SH4 *c) {
  logcall(c, "");
  int i = fh_of(R(4));
  if (i >= 0) { fh[i].open = 0; fh[i].pending = 0; wr16(R(4) + 0x4C, 0); }
  R(0) = 0;
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
  wr32(R(5), fh[i].nsct);
  R(0) = 1;
}
static void gd_complete(int i) {
  uint32_t h = fh_addr(i), pos = fh_pos(i);
  if (read_sectors(fh[i].lba + pos, fh[i].req_n, fh[i].req_buf)) { fh_stat(i, ST_ERR); wr16(h + 0x52, 0xFFFF); }
  else {
    wr32(h + 0x14, pos + fh[i].req_n);
    wr32(h + 0x18, fh[i].req_n * 2048);
    fh_stat(i, ST_COMPLETE);
  }
  fh[i].pending = 0;
}
/* gdFsRead(h, nsct, buf): synchronous, clamped to the file; 0 = done, -10 bad handle */
static void h_gdFsRead(SH4 *c) {
  logcall(c, "");
  int i = fh_of(R(4));
  if (i < 0) { R(0) = (uint32_t)-10; return; }
  uint32_t n = R(5), pos = fh_pos(i);
  if (pos + n > fh[i].nsct) n = pos < fh[i].nsct ? fh[i].nsct - pos : 0;
  fh[i].req_n = n; fh[i].req_buf = R(6);
  wr32(fh_addr(i) + 0x1C, n);
  gd_complete(i);
  R(0) = rd16(fh_addr(i) + 0x50) == ST_COMPLETE ? 0 : (uint32_t)-1;
}
/* gdFsReqRd32(h, nsct, buf): asynchronous; returns the sectors requested, -14 if buf is not 32-byte
 * aligned; the status reads 2 (reading) until the request completes */
static void h_gdFsReqRd32(SH4 *c) {
  logcall(c, "");
  int i = fh_of(R(4));
  if (i < 0) { R(0) = (uint32_t)-10; return; }
  if (R(6) & 31) { R(0) = (uint32_t)-14; return; }
  if (fh[i].pending) { R(0) = 0; return; }
  uint32_t n = R(5), pos = fh_pos(i);
  if (pos >= fh[i].nsct) { wr32(fh_addr(i) + 0x18, 0); fh_stat(i, ST_COMPLETE); R(0) = 0; return; }
  if (pos + n > fh[i].nsct) n = fh[i].nsct - pos;
  if (!n) { wr32(fh_addr(i) + 0x18, 0); fh_stat(i, ST_COMPLETE); R(0) = 0; return; }
  fh[i].req_n = n; fh[i].req_buf = R(6); fh[i].pending = 1; fh[i].polls = 0;
  wr32(fh_addr(i) + 0x1C, n); wr32(fh_addr(i) + 0x18, 0);
  fh_stat(i, ST_READ);
  R(0) = n;
}
static void h_gdFsCancel(SH4 *c) {
  logcall(c, "");
  int i = fh_of(R(4));
  if (i < 0) { R(0) = (uint32_t)-10; return; }
  fh[i].pending = 0;
  fh_stat(i, ST_IDLE);
  R(0) = 0;
}
/* gdFsGetStat(h): the status field; -10 for a bad handle */
static void h_gdFsGetStat(SH4 *c) {
  logcall(c, "");
  uint32_t h = R(4);
  int i = fh_of(h);
  if (i >= 0 && fh[i].pending && ++fh[i].polls >= 8) gd_complete(i);
  if ((gd_work && h == GD_SYS) || i >= 0) { R(0) = (uint32_t)(int32_t)(int16_t)rd16(h + 0x50); return; }
  R(0) = (uint32_t)-10;
}
/* gdFs_Api34 (driver slot 0x34): the drive's own handle, used for drive-status requests */
static void h_gdFs_Api34(SH4 *c) { logcall(c, ""); R(0) = gd_work ? GD_SYS : 0; }
/* gdFs_Api58 (driver slot 0x58): request the drive status; the drive handle is busy until the next frame */
static void h_gdFs_Api58(SH4 *c) {
  logcall(c, "");
  if (gd_work) { wr16(GD_SYS + 0x50, ST_BUSY); gd_sys_busy_until = (int)frame + 1; }
  R(0) = 0;
}
static void h_gdFsGetDrvStat(SH4 *c) { logcall(c, ""); R(0) = cdda_playing ? 3 : 1; }
static void h_gdFsChangeDir(SH4 *c) {
  logcall(c, "");
  char name[8];
  guest_str(R(4), name, sizeof name);
  /* one flat root directory on this disc: the root (or "." / "") is the only directory */
  R(0) = (!name[0] || !strcmp(name, "\\") || !strcmp(name, "/") || !strcmp(name, ".")) ? 1 : 0;
  if (!R(0)) rt_log("hle gdFsChangeDir to a subdirectory: refused (flat disc)");
}
static void gd_vblank(void) {
  for (int i = 0; i < (int)gd_max; i++) if (fh[i].open && fh[i].pending) gd_complete(i);
  if (gd_work && (int)frame >= gd_sys_busy_until && rd16(GD_SYS + 0x50) == ST_BUSY) wr16(GD_SYS + 0x50, ST_COMPLETE);
}
/* CDDA (S7 plays it): drive state and a log line */
static void h_gdFsDaPlay(SH4 *c) {
  logcall(c, "");
  cdda_playing = 1;
  rt_log("hle cdda play %d %d repeat %d at frame %u", (int)R(4), (int)R(5), (int)R(6), frame);
  R(0) = 0;
}
static void h_gdFsDaPause(SH4 *c) { logcall(c, ""); if (cdda_playing) rt_log("hle cdda pause at frame %u", frame); cdda_playing = 0; R(0) = 0; }
static void h_gdFsDaStop(SH4 *c) { logcall(c, ""); if (cdda_playing) rt_log("hle cdda stop at frame %u", frame); cdda_playing = 0; R(0) = 0; }
static void h_gdFsDaRelease(SH4 *c) { logcall(c, ""); rt_log("hle cdda release (resume) at frame %u", frame); cdda_playing = 1; R(0) = 0; }

/* ---- V-sync, callbacks, the frame loop ---- */
static uint32_t vsync_fn, eor_fn;
static void h_njSetVSyncFunction(SH4 *c) { logcall(c, ""); vsync_fn = R(4); R(0) = 0; }
static void h_njSetEORFunction(SH4 *c) { logcall(c, ""); eor_fn = R(4); R(0) = 0; }
static uint32_t f_interp0;
/* --watch: guest words appended to the frame line (debugging aid) */
static uint32_t watch[8];
static int nwatch;
int hle_set_watch(const char *s) {
  nwatch = 0;
  while (*s && nwatch < 8) {
    uint32_t v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    for (; *s && *s != ','; s++) {
      int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 : *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
      if (d < 0) return -1;
      v = v << 4 | (uint32_t)d;
    }
    if (*s == ',') s++;
    watch[nwatch++] = v;
  }
  return nwatch;
}
static uint64_t ram_digest(void) {
  uint64_t h = 0x9E3779B97F4A7C15ull, v;
  for (uint32_t i = 0; i < (16u << 20); i += 8) { memcpy(&v, sh4_ram + i, 8); h ^= v; h *= 0x100000001B3ull; h ^= h >> 29; }
  return h;
}
/* screens, inferred from the call pattern: the set of SDK functions called in a frame (HLE and guest).
 * When a new set holds for 8 frames in a row it is logged once, with its busiest functions. */
static uint32_t scr_cur, scr_cand, scr_run, scr_n;
static void screen_track(void) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < NTAB; i++) if (tab[i].fcalls) { h ^= (uint32_t)i; h *= 16777619u; }
  if (h == scr_cur) { scr_run = 0; return; }
  if (h != scr_cand) { scr_cand = h; scr_run = 0; }
  if (++scr_run < 8) return;
  scr_cur = h; scr_run = 0; scr_n++;
  char buf[400], *o = buf, *e = buf + sizeof buf - 40;
  int nf = 0, shown[8], ns = 0;
  for (int i = 0; i < NTAB; i++) nf += tab[i].fcalls != 0;
  for (; ns < 8; ns++) { /* the 8 busiest */
    int best = -1;
    for (int i = 0; i < NTAB; i++) {
      int dup = 0;
      for (int k = 0; k < ns; k++) dup |= shown[k] == i;
      if (!dup && tab[i].fcalls && (best < 0 || tab[i].fcalls > tab[best].fcalls)) best = i;
    }
    if (best < 0) break;
    shown[ns] = best;
    for (const char *t = tab[best].name; *t && o < e; t++) *o++ = *t;
    *o++ = ' '; *o++ = 'x';
    char num[12]; int k = 0; uint32_t v = tab[best].fcalls;
    do { num[k++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (k) *o++ = num[--k];
    *o++ = ' ';
  }
  *o = 0;
  rt_log("hle screen %u from frame %u: %d functions/frame, files %d: %s", scr_n, frame - 7, nf, gd_files_loaded, buf);
}
static void frame_log(void) {
  screen_track();
  if (!hle_frame_log) return;
  uint32_t blocks, fr = heap_free(&blocks);
  char d[20] = "-";
  if (hle_digest_every > 0 && frame % (uint32_t)hle_digest_every == 0) {
    uint64_t h = ram_digest();
    for (int i = 0; i < 16; i++) d[i] = "0123456789abcdef"[(h >> (60 - 4 * i)) & 15];
    d[16] = 0;
  }
  char w[8 * 10 + 1], *o = w;
  for (int i = 0; i < nwatch; i++) {
    uint32_t v = rd32(watch[i]);
    *o++ = ' ';
    for (int k = 0; k < 8; k++) *o++ = "0123456789abcdef"[(v >> (28 - 4 * k)) & 15];
  }
  *o = 0;
  rt_log("frame %u hle %u guest %u interp %u files %d heap %u/%u pad %x ram %s%s%s", frame, f_hle, f_guest,
         rt_interp_calls() - f_interp0, gd_files_loaded, fr, blocks, pad_prev[0], d, nwatch ? " watch" : "", w);
}
static void h_njWaitVSync(SH4 *c) {
  logcall(c, "");
  /* end of render: the EOR callback, then the type-2 chain */
  if (eor_fn) rt_guest_call(c, eor_fn, 0, 0, 0, 0);
  chain_fire(c, 0x920);
  nj_frame_end();
  frame_log();
  for (int i = 0; i < NTAB; i++) tab[i].fcalls = 0;
  f_hle = f_guest = 0; f_interp0 = rt_interp_calls();
  frame++;
  if (frame >= (uint32_t)hle_max_frames) rt_stop("frame limit");
  nj_frame_begin(frame);
  /* V-blank: time, drive, pads, the V-blank chain, Ninja's V-sync function */
  tmr_intra = 0;
  gd_vblank();
  pads_update();
  chain_fire(c, 0x900);
  if (vsync_fn) rt_guest_call(c, vsync_fn, 0, 0, 0, 0);
  R(0) = 0;
}
static void h_syBtExit(SH4 *c) { logcall(c, ""); rt_stop("the game asked to exit to the BIOS"); }
static void h_syCfgGetSoundMode(SH4 *c) { logcall(c, ""); if (R(4)) wr32(R(4), 0); R(0) = 0; }

static const struct { const char *name; hle_fn fn; } impl[] = {
  {"gdFsInit", h_gdFsInit}, {"gdFsOpen", h_gdFsOpen}, {"gdFsGetFileSize", h_gdFsGetFileSize},
  {"gdFsGetFileSctSize", h_gdFsGetFileSctSize}, {"gdFsRead", h_gdFsRead}, {"gdFsClose", h_gdFsClose},
  {"gdFsReqRd32", h_gdFsReqRd32}, {"gdFsCancel", h_gdFsCancel}, {"gdFsGetStat", h_gdFsGetStat},
  {"gdFs_Api34", h_gdFs_Api34}, {"gdFs_Api58", h_gdFs_Api58}, {"gdFsChangeDir", h_gdFsChangeDir},
  {"gdFsGetDrvStat", h_gdFsGetDrvStat}, {"gdFsDaPlay", h_gdFsDaPlay}, {"gdFsDaPause", h_gdFsDaPause},
  {"gdFsDaStop", h_gdFsDaStop}, {"gdFsDaRelease", h_gdFsDaRelease},
  {"pdGetPeripheral", h_pdGetPeripheral},
  {"njSetVSyncFunction", h_njSetVSyncFunction}, {"njSetEORFunction", h_njSetEORFunction},
  {"njWaitVSync", h_njWaitVSync}, {"syTmrGetCount", h_syTmrGetCount},
  {"syChainAddHandler", h_syChainAddHandler}, {"syChainDeleteHandler", h_syChainDeleteHandler},
  {"syBtExit", h_syBtExit}, {"syCfgGetSoundMode", h_syCfgGetSoundMode},
  {"pdInitPeripheral", h_true}, /* pads are refreshed by the frame loop */
};

void hle_init(SH4 *c) {
  (void)c;
  for (int j = 1; j < NTAB; j++)
    if (tab[j].a <= tab[j - 1].a) rt_stop("hle table not sorted");
  for (unsigned i = 0; i < sizeof impl / sizeof impl[0]; i++) {
    int hit = 0;
    for (int j = 0; j < NTAB; j++)
      if (!strcmp(tab[j].name, impl[i].name)) { tab[j].fn = impl[i].fn; hit = 1; }
    if (!hit) rt_log("hle no table entry for %s", impl[i].name);
  }
  int ng = 0;
  for (unsigned i = 0; i < sizeof guest / sizeof guest[0]; i++)
    for (int j = 0; j < NTAB; j++)
      if (!strcmp(tab[j].name, guest[i])) {
        if (!rt_lookup(tab[j].a)) { rt_log("hle %s: no translated code, stays a stub", guest[i]); continue; }
        tab[j].fn = 0; ng++;
      }
  /* S5: the Ninja drawing HLE (port/render) */
  const nj_impl_t *lists[2] = {nj_impls, nj_model_impls};
  const int counts[2] = {nj_nimpls, nj_model_nimpls};
  for (int l = 0; l < 2; l++)
    for (int i = 0; i < counts[l]; i++) {
      int hit = 0;
      for (int j = 0; j < NTAB; j++)
        if (!strcmp(tab[j].name, lists[l][i].name)) { tab[j].fn = lists[l][i].fn; hit = 1; }
      if (!hit) rt_log("hle no table entry for %s", lists[l][i].name);
    }
  nj_frame_begin(0);
  rt_log("hle %d entries, %d run as guest SDK code", NTAB, ng);
  /* the BIOS syscall vectors read by the game's syscall stubs */
  static const uint32_t vec[][2] = {{0x8C0000B0u, 0x8C000F10u}, {0x8C0000B4u, 0x8C000F18u}, {0x8C0000B8u, 0x8C000F20u},
                                    {0x8C0000BCu, 0x8C000F28u}, {0x8C0000C0u, 0x8C000F30u}, {0x8C0000E0u, 0x8C000F38u}};
  for (unsigned i = 0; i < sizeof vec / sizeof vec[0]; i++) wr32(vec[i][0], vec[i][1]);
  pads_update();
}

void hle_summary(void) {
  uint32_t total = 0;
  int used = 0;
  for (int i = 0; i < NTAB; i++) if (tab[i].calls) { total += tab[i].calls; used++; }
  rt_log("hle summary: %u calls to %d functions, %u frames, %d files opened", total, used, frame, gd_files_loaded);
  for (int i = 0; i < NTAB; i++)
    if (tab[i].calls)
      rt_log("hle   %s %s %u first frame %u", tab[i].fn ? "hle" : "guest", tab[i].name, tab[i].calls, tab[i].first);
}
