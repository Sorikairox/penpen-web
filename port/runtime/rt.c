/* Pen Pen port, S3: boot runtime core: guest RAM, call dispatch (HLE / recompiled / interpreter),
 * hardware I/O as a logged register file, logging. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "rt.h"

static uint8_t ram[16 << 20] __attribute__((aligned(16)));
uint8_t *sh4_ram = ram;
SH4 *rt_cpu;

/* ---- logging ---- */
static char *fmt_u(char *o, uint32_t v, int base, int width, char pad) {
  char t[12]; int n = 0;
  do { t[n++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
  while (n < width) t[n++] = pad;
  while (n) *o++ = t[--n];
  return o;
}
void rt_log(const char *fmt, ...) {
  char buf[512], *o = buf, *e = buf + sizeof buf - 24;
  va_list ap;
  va_start(ap, fmt);
  for (const char *p = fmt; *p && o < e; p++) {
    if (*p != '%') { *o++ = *p; continue; }
    p++;
    char pad = ' '; int w = 0;
    if (*p == '0') { pad = '0'; p++; }
    while (*p >= '0' && *p <= '9') w = w * 10 + (*p++ - '0');
    switch (*p) {
    case 'd': { int v = va_arg(ap, int); if (v < 0) { *o++ = '-'; v = -v; } o = fmt_u(o, (uint32_t)v, 10, w, pad); break; }
    case 'u': o = fmt_u(o, va_arg(ap, uint32_t), 10, w, pad); break;
    case 'x': o = fmt_u(o, va_arg(ap, uint32_t), 16, w, pad); break;
    case 'c': *o++ = (char)va_arg(ap, int); break;
    case 's': { const char *s = va_arg(ap, const char *); while (*s && o < e) *o++ = *s++; break; }
    default: *o++ = *p;
    }
  }
  va_end(ap);
  rtw_log(buf, (int)(o - buf));
}

void rt_stop(const char *why) {
  if (rt_sandbox) { rt_log("hw sandbox stop: %s", why); hwt_sandbox_abort(7, 0); }
  if (hwt_synth_live) { rt_log("hw synth stop: %s", why); hwt_sandbox_abort(7, 0); }
  rt_log("boot stop: %s", why);
  hle_ring_dump();
  rt_summary();
  if (hle_verbose || hle_frame_log) hle_summary(); /* the test hosts; a browser host keeps only its last lines */
  rt_log("boot stopped at frame %u: %s", hle_frame(), why);
#ifdef __wasm__
  __builtin_trap();
#else
  extern void exit(int);
  exit(0);
#endif
}

/* ---- hardware I/O: a register file that remembers writes; first touches and counts are logged ---- */
#define IO_N 4096
static struct { uint32_t a, v, nr, nw; } io[IO_N];
static int io_n, io_full;
static int io_slot(uint32_t a) {
  uint32_t h = (a * 2654435761u) >> 20;
  for (int i = 0; i < IO_N; i++) {
    uint32_t j = (h + i) & (IO_N - 1);
    if (io[j].a == a && (io[j].nr | io[j].nw)) return (int)j;
    if (!(io[j].nr | io[j].nw)) { if (io_n >= IO_N * 3 / 4) { io_full = 1; return -1; } io[j].a = a; io_n++; return (int)j; }
  }
  return -1;
}
#define TMU_TCNT0 0xFFD8000Cu
/* S8: the AICA's real-time clock, seconds since 1950-01-01 00:00 local time: 0x00710000 = bits 31-16,
 * 0x00710004 = bits 15-0 (any area mirror; the game reads it through the G2 bus helper at 0xA0710000) */
static uint32_t rtc_reads;
#define RTC_HI 0x00710000u
#define RTC_LO 0x00710004u
static uint32_t io_read_live(uint32_t a) {
  if ((a & ~3u) == TMU_TCNT0) return ~hle_tmu_count(); /* the frame-driven timer (hle.c) */
  if ((a & 0x1FFFFFFCu) == RTC_HI) { rtc_reads++; return hle_rtc() >> 16; }
  if ((a & 0x1FFFFFFCu) == RTC_LO) { rtc_reads++; return hle_rtc() & 0xFFFFu; }
  int i = io_slot(a & ~3u);
  if (i < 0) return 0;
  io[i].nr++;
  return io[i].v;
}
/* S11: a difftest sample records the hardware reads of the live call; its sandbox sides replay them */
/* S17: during a synthetic sample's recording pass, an access no real pointer makes (the BIOS ROM / flash area and the
 * unassigned space, VRAM / the TA through a broken pointer: anything but the Holly / G2 / AICA / RTC registers, the
 * AICA's RAM and the SH-4's own area) refuses the input; so does any write outside RAM (game code writes no hardware
 * register but through the SDK; a job may allow it: hwt_synth_hw) */
static int io_odd(uint32_t a) {
  uint32_t p = a & 0x1FFFFFFFu;
  if (a >= 0xE0000000u && a < 0xE4000000u) return 0;                /* the store queues */
  if ((a >> 24) == 0xFF) {                                           /* the SH-4's register blocks only */
    uint32_t blk = (a >> 16) & 0xFF;
    return !(blk == 0x00 || blk == 0x20 || blk == 0x80 || blk == 0xA0 || blk == 0xC0 || blk == 0xC8 || blk == 0xD0 ||
             blk == 0xD8 || blk == 0xE0 || blk == 0xE8);
  }
  if (a >= 0xE0000000u) return 1;
  return !((p >= 0x005F0000u && p < 0x00720000u) || (p >= 0x00800000u && p < 0x00A00000u)); /* Holly, G2, AICA, RTC */
}
static uint32_t io_read(uint32_t a) {
  if (hwt_synth_live && !rt_hle_depth && !hwt_synth_hw && io_odd(a)) hwt_synth_odd_note("read", a); /* the guest code's own accesses (not an HLE's) */
  if (rt_sandbox) return hwt_io_read(a & ~3u);
  uint32_t v = io_read_live(a);
  if (hwt_recording) hwt_record_io(a & ~3u, v);
  return v;
}
static void io_write(uint32_t a, uint32_t v, int size) {
  int i = io_slot(a & ~3u);
  if (i < 0) return;
  uint32_t sh = (a & 3) * 8, m = size == 4 ? 0xFFFFFFFFu : size == 2 ? 0xFFFFu << sh : 0xFFu << sh;
  io[i].v = (io[i].v & ~m) | ((v << sh) & m);
  io[i].nw++;
}
/* S11: in a difftest sandbox (difftest.c) hardware accesses touch nothing real: reads replay the live call's (else
 * a fixed value per address), writes are logged per side and compared */
#define SBX_IO_W(a, v, n) if (hwt_synth_live && !rt_hle_depth && !hwt_synth_hw) hwt_synth_odd_note("write", a); if (rt_sandbox) { hwt_io_write(a, v, n); return; }
/* S24: the draw check's run of the original drawers (port/render/nj_model.c). While rt_low_capture is set, RT_LOW
 * (2 MB) at physical RT_LOW_AT is scratch memory: the SDK's vertex buffer, set there for the check (the HLE
 * replaces njInitVertexBuffer, which would allocate it), written directly or through the store queues (a prefetch of a
 * queue address sends it to ((QACRn & 0x1C) << 24) | (address & 0x03FFFFE0)); rt_low_write sees each write. Other
 * store-queue flushes (the TA stream: its destination is 0, the Kamui set-up being HLE) are dropped; the I/O register
 * file is left alone. */
int rt_low_capture;
void (*rt_low_write)(uint32_t a, uint32_t n);
uint8_t *rt_low;
static uint32_t rt_sq[2][8], rt_sq_mask[2];
#define RT_LOW (2u << 20)
void rt_low_begin(void) {
#ifndef __EMSCRIPTEN__
  if (!rt_low) rt_low = malloc(RT_LOW);
  rt_low_capture = rt_low != 0;
#endif
}
void rt_low_end(void) { rt_low_capture = 0; }
#define SQ_CAPTURED(a) (rt_low_capture && (a) >= 0xE0000000u && (a) < 0xE4000000u)
#define RT_LOW_AT 0x04000000u /* physical: the 64-bit texture memory's window, which nothing else touches here */
#define LOW(a) (rt_low_capture && ((a) & 0x1FFFFFFFu) - RT_LOW_AT < RT_LOW)
#define LOWP(a) (rt_low + (((a) & 0x1FFFFFFFu) - RT_LOW_AT))
#define LOWW(a, n) if (rt_low_write) rt_low_write((a) & 0x1FFFFFFFu, n)
uint32_t sh4_io_r32(uint32_t a) { if (LOW(a)) { uint32_t v; memcpy(&v, LOWP(a & ~3u), 4); return v; } return io_read(a); }
uint16_t sh4_io_r16(uint32_t a) { if (LOW(a)) { uint16_t v; memcpy(&v, LOWP(a & ~1u), 2); return v; } return (uint16_t)(io_read(a) >> ((a & 2) * 8)); }
uint8_t sh4_io_r8(uint32_t a) { if (LOW(a)) return *LOWP(a); return (uint8_t)(io_read(a) >> ((a & 3) * 8)); }
void sh4_io_w32(uint32_t a, uint32_t v) {
  if (SQ_CAPTURED(a)) { rt_sq[(a >> 5) & 1][(a >> 2) & 7] = v; rt_sq_mask[(a >> 5) & 1] |= 1u << ((a >> 2) & 7); return; }
  if (LOW(a)) { memcpy(LOWP(a & ~3u), &v, 4); LOWW(a & ~3u, 4); return; }
  SBX_IO_W(a, v, 4); io_write(a, v, 4);
}
void sh4_io_w16(uint32_t a, uint16_t v) { if (LOW(a)) { memcpy(LOWP(a & ~1u), &v, 2); LOWW(a & ~1u, 2); return; } SBX_IO_W(a, v, 2); io_write(a, v, 2); }
void sh4_io_w8(uint32_t a, uint8_t v) { if (LOW(a)) { *LOWP(a) = v; LOWW(a, 1); return; } SBX_IO_W(a, v, 1); io_write(a, v, 1); }
void sh4_io_pref(uint32_t a) {
  if (!SQ_CAPTURED(a)) return; /* store queues: the HLE replaces the TA/texture paths */
  uint32_t q = (a >> 5) & 1, ext = ((io_read_live(0xFF000038u + 4 * q) & 0x1Cu) << 24) | (a & 0x03FFFFE0u);
  if ((ext >> 26) == 3) memcpy(sh4_ram + (ext & 0xFFFFE0u), rt_sq[q], 32);
  else if (ext - RT_LOW_AT < RT_LOW) { /* the scratch: the halves the queue's writes touched since its last flush */
    memcpy(rt_low + (ext - RT_LOW_AT), rt_sq[q], 32);
    if (rt_sq_mask[q] & 0x0Fu) LOWW(ext, 16);
    if (rt_sq_mask[q] & 0xF0u) LOWW(ext + 16, 16);
  }
  /* else: the TA FIFO (its destination is 0 here, the Kamui set-up being HLE): dropped */
  rt_sq_mask[q] = 0;
}
/* S24: the I/O register file saved and restored around the draw check's run of the original drawers */
#ifndef __EMSCRIPTEN__
static struct { uint32_t a, v, nr, nw; } io_saved[IO_N];
static int io_saved_n, io_saved_full;
void rt_io_save(int restore) {
  if (restore) { memcpy(io, io_saved, sizeof io); io_n = io_saved_n; io_full = io_saved_full; }
  else { memcpy(io_saved, io, sizeof io); io_saved_n = io_n; io_saved_full = io_full; }
}
#else
void rt_io_save(int restore) { (void)restore; } /* the draw check is native only */
#endif

/* ---- dispatch ---- */
rt_fn rt_lookup(uint32_t a) {
  int lo = 0, hi = rt_fn_count - 1;
  a = RT_NORM(a);
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (rt_fn_table[mid].addr == a) return rt_fn_table[mid].fn;
    if (rt_fn_table[mid].addr < a) lo = mid + 1; else hi = mid - 1;
  }
  return 0;
}

/* ---- S6: resumable frames (rt_gen.h): the saved call sites, innermost first ---- */
int rt_unw, rt_rew;
uint32_t rt_rew_t;
#define RW_MAX 512
/* S11: a record may also hold the locals of a hand-written resumable function (src/include/hw.h) */
#define RW_BLOB 64
static struct { uint32_t fn, t; int site, nblob; uint8_t blob[RW_BLOB]; } rw[RW_MAX];
static int rw_n, rw_depth_max;
/* S11: every function that ever saved a site = the functions a frame yields through; hand-written C (src/) must
 * not be one of them unless it is written resumable (docs/port/DECOMP.md 4) */
#define YF_MAX 256
static uint32_t yf[YF_MAX];
static int yf_n;
void rt_unw_save(uint32_t fn, int site, uint32_t t) {
  if (rw_n >= RW_MAX) rt_stop("unwind: call chain too deep");
  if (site >= 0) {
    int k = 0;
    while (k < yf_n && yf[k] != fn) k++;
    if (k == yf_n && yf_n < YF_MAX) yf[yf_n++] = fn;
  }
  rw[rw_n].fn = fn; rw[rw_n].site = site; rw[rw_n].t = t; rw[rw_n].nblob = 0; rw_n++;
  if (rw_n > rw_depth_max) rw_depth_max = rw_n;
}
void rt_unw_save_blob(uint32_t fn, int site, uint32_t t, const void *blob, int n) {
  if (n > RW_BLOB) { rt_log("rt unwind: %08x saves %d bytes of locals (max %d)", fn, n, RW_BLOB); rt_stop("unwind"); }
  rt_unw_save(fn, site, t);
  memcpy(rw[rw_n - 1].blob, blob, (size_t)n);
  rw[rw_n - 1].nblob = n;
}
int rt_rew_pop_blob(uint32_t fn, uint32_t *t, void *blob, int n) {
  if (!rw_n || rw[rw_n - 1].fn != fn || rw[rw_n - 1].nblob != n) {
    rt_log("rt rewind: hand-written %08x finds %08x (%d bytes) on top", fn, rw_n ? rw[rw_n - 1].fn : 0, rw_n ? rw[rw_n - 1].nblob : -1);
    rt_stop("rewind");
  }
  rw_n--;
  *t = rw[rw_n].t;
  memcpy(blob, rw[rw_n].blob, (size_t)n);
  return rw[rw_n].site;
}
int rt_rew_pop(SH4 *c, uint32_t fn) {
  (void)c;
  if (!rw_n) { rt_log("rt rewind: no saved site for %08x", fn); rt_stop("rewind"); }
  rw_n--;
  if (rw[rw_n].fn != fn) { rt_log("rt rewind: entered %08x, saved site belongs to %08x", fn, rw[rw_n].fn); rt_stop("rewind"); }
  rt_rew_t = rw[rw_n].t;
  return rw[rw_n].site;
}
/* the HLE that yielded is reached again: the chain is rebuilt */
void rt_rew_done(void) {
  if (rw_n) { rt_log("rt rewind: %d sites left at the yield", rw_n); rt_stop("rewind"); }
  rt_rew = 0;
}
int rt_rw_depth(void) { return rw_depth_max; }

static uint32_t n_calls, n_hle, n_interp_calls, n_interp_steps;
uint32_t rt_interp_calls(void) { return n_interp_calls; }
#define SEEN_N 64
static uint32_t interp_seen[SEEN_N];
static int interp_seen_n;

/* run guest code in the interpreter until it returns to the caller's PR; calls and jumps to known
 * code (HLE or recompiled) go native. */
/* the interpreter is resumable too (S6): when a native call it made yields, it saves where it was (its
 * caller's return address and stack, the call's target, the branch kind) as three sites tagged with the entry
 * address; the rewind enters it again with the same entry, it pops them and repeats that native call. */
#define RW_INTERP_T (-1)
#define RW_INTERP_RET (-2)
#define RW_INTERP_SP (-3)
static uint32_t rw_pop_interp(uint32_t a, int tag) {
  if (!rw_n || rw[rw_n - 1].fn != a || rw[rw_n - 1].site != tag) { rt_log("rt rewind: interpreter state of %08x missing", a); rt_stop("rewind"); }
  rw_n--;
  return rw[rw_n].t;
}
int rt_no_interp; /* S20: --no-interp */
uint32_t rt_force_interp; /* test hook: run this function in the interpreter although it is translated */
static void interp_call(SH4 *c, uint32_t a) {
  uint32_t ret = c->pr, sp = c->r[15], t = 0;
  int resume = 0;
  if (rt_rew) {
    t = rw_pop_interp(a, RW_INTERP_T);
    ret = rw_pop_interp(a, RW_INTERP_RET);
    sp = rw_pop_interp(a, RW_INTERP_SP);
    resume = 1;
  } else {
    int known = 0;
    for (int i = 0; i < interp_seen_n; i++) known |= interp_seen[i] == a;
    if (!known && interp_seen_n < SEEN_N) { interp_seen[interp_seen_n++] = a; rt_log("rt interp %08x", a); }
    if (rt_no_interp && !rt_sandbox) { /* S20 (--no-interp): a build meant to run without the fallback stops here */
      rt_log("rt interp refused %08x (called from %08x)", a, c->pr);
      rt_stop("interpreter fallback entered (--no-interp)");
    }
    n_interp_calls++;
    c->pc = a;
  }
  for (;;) {
    if (!resume) {
      int r = sh4_step(c);
      n_interp_steps++;
      if (r) {
        rt_log("rt interp fault %d at %08x (entered at %08x, return address %08x, sp %08x)", r, c->fault, a, ret, sp);
        rt_stop("interpreter fault");
      }
      if (c->last == SH4_BR_NONE) continue;
      if (c->last == SH4_BR_RET && c->pc == ret && c->r[15] >= sp) return;
      /* S14: a function Ghidra cut short is translated up to the cut and the interpreter runs the rest from there,
       * entered with the PR of the last call (not its caller's): when the original's caller is hand-written, the
       * function's final rts goes to hw_call's return marker (hw.c HW_RETURN): that is the return too */
      if (c->last == SH4_BR_RET && c->pc == 0x8C000F08u) return;
      t = RT_NORM(c->pc);
      if (!((hle_known(t) || rt_lookup(t)) && t != rt_force_interp)) continue;
    }
    resume = 0;
    int last = c->last;
    rt_call(c, t); /* native; returns with c->pc = where it returned to */
    if (rt_unw) {
      rt_unw_save(a, RW_INTERP_SP, sp); rt_unw_save(a, RW_INTERP_RET, ret); rt_unw_save(a, RW_INTERP_T, t);
      return;
    }
    c->last = last;
    if (c->last != SH4_BR_CALL && c->pc == ret) return; /* a tail jump came back to our caller */
  }
}

uint32_t rt_stack_low = 0xFFFFFFFFu; /* S11: the lowest r15 seen at a call through rt_call */
int rt_sandbox;          /* S11: a difftest side is running (difftest.c): HLE calls are replayed, nothing is counted */
int rt_hle_depth;        /* HLE calls in progress (an HLE may call guest code, which may call the HLE) */
uint32_t hle_arg_mask;
void rt_call(SH4 *c, uint32_t a) {
  uint32_t n = RT_NORM(a);
  hle_fn h;
  rt_fn f;
  if (rt_sandbox) { hwt_sandbox_call(c, n); return; }
  n_calls++;
  if (c->r[15] < rt_stack_low) rt_stack_low = c->r[15];
  if ((h = hle_find(n))) {
    int rewinding = rt_rew; /* the yielding HLE entered again by the rewind: not a new call */
    n_hle++;
    if (hwt_recording && !rewinding) hwt_before_hle(c);
    rt_hle_depth++;
    hle_arg_mask = 0;
    h(c);
    rt_hle_depth--;
    c->pc = c->pr;
    if (hwt_recording && !rewinding) hwt_record_hle(c, n); /* a sampled hand-written call's HLE results */
    return;
  }
  if ((f = rt_lookup(n)) && n != rt_force_interp) { f(c); return; }
  interp_call(c, n);
}

void rt_guest_call(SH4 *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
  SH4 save = *c;
  c->r[4] = a0; c->r[5] = a1; c->r[6] = a2; c->r[7] = a3;
  c->pr = 0x8C000F00u; /* returned-to address of an HLE-made call: never executed */
  rt_call(c, fn);
  if (rt_unw) { rt_log("rt yield inside a call the HLE made to %08x", fn); rt_stop("unwind through an HLE call"); }
  uint32_t r0 = c->r[0];
  *c = save;
  c->r[0] = r0;
}

void rt_illegal(SH4 *c, uint32_t a) { (void)c; if (rt_sandbox) hwt_sandbox_abort(2, a); rt_log("rt illegal instruction at %08x", a); rt_stop("illegal"); }
void rt_illegal_slot(SH4 *c, uint32_t a) { (void)c; if (rt_sandbox) hwt_sandbox_abort(3, a); rt_log("rt illegal slot at %08x", a); rt_stop("illegal slot"); }
void rt_unsupported(SH4 *c, uint32_t a) { (void)c; if (rt_sandbox) hwt_sandbox_abort(4, a); rt_log("rt unsupported instruction at %08x", a); rt_stop("unsupported"); }
void rt_badret(SH4 *c, uint32_t want, uint32_t at) {
  if (rt_sandbox) hwt_sandbox_abort(5, at);
  rt_log("rt call at %08x returned to %08x, expected %08x", at, c->pc, want);
  rt_stop("bad return");
}
void rt_budget(SH4 *c, uint32_t at) {
  if (rt_sandbox) hwt_sandbox_abort(6, at);
  rt_log("rt watchdog: a loop at %08x ran %u back-edges (pc of last call %08x)", at, 200000000u, c->pr);
  rt_stop("watchdog");
}

void rt_summary(void) {
  hwt_summary();
  rt_log("rt calls %u hle %u interp-calls %u interp-steps %u yield-depth %d", n_calls, n_hle, n_interp_calls, n_interp_steps, rw_depth_max);
  int shown = 0;
  for (int i = 0; i < IO_N && shown < 40; i++)
    if (io[i].nr | io[i].nw) { rt_log("io %08x reads %u writes %u", io[i].a, io[i].nr, io[i].nw); shown++; }
  rt_log("io addresses %d%s; rtc reads %u", io_n, io_full ? " (table full)" : "", rtc_reads);
  rt_log("rt stack low %08x; yield-chain functions %d", rt_stack_low, yf_n);
  for (int i = 0; i < yf_n; i += 8) {
    char b[8 * 9 + 1], *o = b;
    for (int k = i; k < yf_n && k < i + 8; k++) {
      for (int d = 0; d < 8; d++) *o++ = "0123456789abcdef"[(yf[k] >> (28 - 4 * d)) & 15];
      *o++ = ' ';
    }
    *o = 0;
    rt_log("rt yield-chain %s", b);
  }
}
