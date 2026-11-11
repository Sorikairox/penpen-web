/* Pen Pen port, S3: boot runtime core: guest RAM, call dispatch (HLE / recompiled / interpreter),
 * hardware I/O as a logged register file, logging. */
#include <stdarg.h>
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
static uint32_t io_read(uint32_t a) {
  if ((a & ~3u) == TMU_TCNT0) return ~hle_tmu_count(); /* the frame-driven timer (hle.c) */
  int i = io_slot(a & ~3u);
  if (i < 0) return 0;
  io[i].nr++;
  return io[i].v;
}
static void io_write(uint32_t a, uint32_t v, int size) {
  int i = io_slot(a & ~3u);
  if (i < 0) return;
  uint32_t sh = (a & 3) * 8, m = size == 4 ? 0xFFFFFFFFu : size == 2 ? 0xFFFFu << sh : 0xFFu << sh;
  io[i].v = (io[i].v & ~m) | ((v << sh) & m);
  io[i].nw++;
}
uint32_t sh4_io_r32(uint32_t a) { return io_read(a); }
uint16_t sh4_io_r16(uint32_t a) { return (uint16_t)(io_read(a) >> ((a & 2) * 8)); }
uint8_t sh4_io_r8(uint32_t a) { return (uint8_t)(io_read(a) >> ((a & 3) * 8)); }
void sh4_io_w32(uint32_t a, uint32_t v) { io_write(a, v, 4); }
void sh4_io_w16(uint32_t a, uint16_t v) { io_write(a, v, 2); }
void sh4_io_w8(uint32_t a, uint8_t v) { io_write(a, v, 1); }
void sh4_io_pref(uint32_t a) { (void)a; } /* store queues: the HLE replaces the TA/texture paths */

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

static uint32_t n_calls, n_hle, n_interp_calls, n_interp_steps;
uint32_t rt_interp_calls(void) { return n_interp_calls; }
#define SEEN_N 64
static uint32_t interp_seen[SEEN_N];
static int interp_seen_n;

/* run guest code in the interpreter until it returns to the caller's PR; calls and jumps to known
 * code (HLE or recompiled) go native. */
static void interp_call(SH4 *c, uint32_t a) {
  uint32_t ret = c->pr, sp = c->r[15];
  int known = 0;
  for (int i = 0; i < interp_seen_n; i++) known |= interp_seen[i] == a;
  if (!known && interp_seen_n < SEEN_N) { interp_seen[interp_seen_n++] = a; rt_log("rt interp %08x", a); }
  n_interp_calls++;
  c->pc = a;
  for (;;) {
    int r = sh4_step(c);
    n_interp_steps++;
    if (r) {
      rt_log("rt interp fault %d at %08x (entered at %08x, return address %08x, sp %08x)", r, c->fault, a, ret, sp);
      rt_stop("interpreter fault");
    }
    if (c->last == SH4_BR_NONE) continue;
    if (c->last == SH4_BR_RET && c->pc == ret && c->r[15] >= sp) return;
    uint32_t t = RT_NORM(c->pc);
    if (hle_known(t) || rt_lookup(t)) {
      rt_call(c, t); /* native; returns with c->pc = where it returned to */
      if (rt_unw) { rt_log("rt yield inside interpreted code entered at %08x", a); rt_stop("unwind through the interpreter"); }
      if (c->last != SH4_BR_CALL && c->pc == ret) return; /* a tail jump came back to our caller */
    }
  }
}

/* ---- S6: resumable frames (rt_gen.h): the saved call sites, innermost first ---- */
int rt_unw, rt_rew;
uint32_t rt_rew_t;
#define RW_MAX 512
static struct { uint32_t fn, t; int site; } rw[RW_MAX];
static int rw_n, rw_depth_max;
void rt_unw_save(uint32_t fn, int site, uint32_t t) {
  if (rw_n >= RW_MAX) rt_stop("unwind: call chain too deep");
  rw[rw_n].fn = fn; rw[rw_n].site = site; rw[rw_n].t = t; rw_n++;
  if (rw_n > rw_depth_max) rw_depth_max = rw_n;
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

void rt_call(SH4 *c, uint32_t a) {
  uint32_t n = RT_NORM(a);
  hle_fn h;
  rt_fn f;
  n_calls++;
  if ((h = hle_find(n))) { n_hle++; h(c); c->pc = c->pr; return; }
  if ((f = rt_lookup(n))) { f(c); return; }
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

void rt_illegal(SH4 *c, uint32_t a) { (void)c; rt_log("rt illegal instruction at %08x", a); rt_stop("illegal"); }
void rt_illegal_slot(SH4 *c, uint32_t a) { (void)c; rt_log("rt illegal slot at %08x", a); rt_stop("illegal slot"); }
void rt_unsupported(SH4 *c, uint32_t a) { (void)c; rt_log("rt unsupported instruction at %08x", a); rt_stop("unsupported"); }
void rt_badret(SH4 *c, uint32_t want, uint32_t at) {
  rt_log("rt call at %08x returned to %08x, expected %08x", at, c->pc, want);
  rt_stop("bad return");
}
void rt_budget(SH4 *c, uint32_t at) {
  rt_log("rt watchdog: a loop at %08x ran %u back-edges (pc of last call %08x)", at, 200000000u, c->pr);
  rt_stop("watchdog");
}

void rt_summary(void) {
  rt_log("rt calls %u hle %u interp-calls %u interp-steps %u yield-depth %d", n_calls, n_hle, n_interp_calls, n_interp_steps, rw_depth_max);
  int shown = 0;
  for (int i = 0; i < IO_N && shown < 40; i++)
    if (io[i].nr | io[i].nw) { rt_log("io %08x reads %u writes %u", io[i].a, io[i].nr, io[i].nw); shown++; }
  rt_log("io addresses %d%s", io_n, io_full ? " (table full)" : "");
}
