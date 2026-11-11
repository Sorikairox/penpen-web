/* Pen Pen port, S11: the in-run differential test of the hand-written functions (docs/port/DECOMP.md 5).
 *
 * With --hwtest N, a sampled call of a hand-written function (hw.c: hw_enter) is tested on the real state the game
 * gives it at that moment, during a real run (boot, route):
 *   1. the CPU state and the 16 MiB of guest RAM are copied (snapshot S);
 *   2. the call runs live (the run goes on with its result) while the results of its HLE calls are recorded (and
 *      which argument registers each HLE function read, and what it wrote into its caller's stack frame: the
 *      out-parameters); a call that ends the frame (yields) is followed into the next frames until it returns;
 *   3. side A: the ORIGINAL code, in the SH-4 interpreter, from S;  side B: the hand-written function, from S;
 *      both in a sandbox: HLE calls are not executed but replayed (the k-th HLE call returns what the k-th returned
 *      live), so are hardware reads, hardware writes are logged, nothing is counted;
 *   4. compared: how each side ended, the callee-saved registers (r8-r15, GBR, FPSCR, FR12-FR15), PC and PR on
 *      return, r0 when the hand-written function returns a value, every HLE call (target, and the arguments it
 *      reads), the hardware writes, and all of guest RAM except the stack below the entry r15 (dead after the
 *      return); a word pointing into the stack counts as one token (hand-written code keeps no guest frames).
 * Guest code that is still translated runs as itself on side B and is interpreted on side A, so a sample tests the
 * function together with everything it calls, on a path the game really takes. The hand-written functions whose
 * original code side A ran inside a passing sample are counted as tested inside it ("inside").
 * Runs only in the sandboxed containers (it executes the disc's code), native builds only (3 x 16 MiB buffers). */
#include "rt.h"
#ifdef __wasm__
/* the browser module has no difftest (and no setjmp): the hooks are inert */
int hwt_enabled, hwt_recording;
void hwt_sandbox_call(SH4 *c, uint32_t a) { (void)c; (void)a; __builtin_trap(); }
void hwt_sandbox_abort(int code, uint32_t at) { (void)code; (void)at; __builtin_trap(); }
void hwt_before_hle(SH4 *c) { (void)c; }
void hwt_record_hle(SH4 *c, uint32_t a) { (void)c; (void)a; }
uint32_t hwt_io_read(uint32_t a) { (void)a; return 0; }
void hwt_record_io(uint32_t a, uint32_t v) { (void)a; (void)v; }
void hwt_io_write(uint32_t a, uint32_t v, int size) { (void)a; (void)v; (void)size; }
int hwt_begin(SH4 *c, uint32_t addr) { (void)c; (void)addr; return 0; }
void hwt_end(int sample) { (void)sample; }
void hwt_abandon(void) {}
void hwt_summary(void) {}
int hwt_set(const char *opt) { (void)opt; return -1; }
#else
#include <setjmp.h>
#include <stdlib.h>

int hwt_enabled, hwt_recording;
static int hwt_budget = 160;   /* samples per function */
static int hwt_spread = 50;    /* after the first calls: at most one sample per function every this many frames */
static uint32_t hwt_only;      /* test only this function */
extern int hw_last_ret_set, hw_call_arity;
extern SH4 *hw_cpu;
extern uint8_t *sh4_ram;

#define STACK_LO 0x8C00C000u   /* the stack grows down from 0x8C00F400 (the BIOS's r15); a few KiB are used */
#define STEPS_A 400000000      /* interpreter steps for side A (a scene constructor showing a loading screen runs long) */
#define PENDING_FRAMES_MAX 600 /* a sampled call still running after this many frames is given up (main never returns) */
enum { END_RET = 1, END_ILLEGAL, END_SLOT, END_UNSUP, END_BADRET, END_BUDGET, END_STOP };

static uint32_t hsh(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
/* a word pointing into the stack area (DECOMP.md 6) */
static uint32_t stack_token(uint32_t w) {
  uint32_t p = w & 0x1FFFFFFFu;
  return p >= 0x0C00C000u && p < 0x0C00F400u && (w >> 29) != 7 ? 0x57AC4000u : w;
}

/* ---- per-function results ---- */
typedef struct {
  uint32_t addr, calls, samples, pass, fail, longrun, abandoned, diverged, retv, next_frame;
  uint32_t inside; /* passing samples of other hand-written functions that ran this one's original code */
  char why[128];
} hwt_stat;
#define NST 1024
static hwt_stat st[NST];
static int nst;
static hwt_stat *stat_of(uint32_t a) {
  for (int i = 0; i < nst; i++) if (st[i].addr == a) return &st[i];
  if (nst == NST) return 0;
  memset(&st[nst], 0, sizeof st[nst]);
  st[nst].addr = a;
  return &st[nst++];
}

/* ---- the live call's HLE results ---- */
/* args: the argument registers the HLE read (bits 4-7); stk..: the words it wrote into its caller's stack frames
 * (out-parameters such as gdFsGetFileSize's size), as offsets from r15 at the call: each sandbox side has its own
 * stack depth, and the code still translated keeps the same frame layout, so a side gets them at its own r15 */
typedef struct { uint32_t t, r0, f0, f1, args, stk_first, stk_n; } hle_rec;
#define REC_MAX 200000
static hle_rec *rec;
static int rec_n, rec_depth;
#define STK_AREA_LO 0x00C000u
#define STK_AREA_HI 0x00F400u
#define STK_MAX 400000
static uint32_t (*stk)[2]; /* offset from r15, value */
static uint32_t stk_n, stk_r15;
static uint8_t stk_before[STK_AREA_HI - STK_AREA_LO];
void hwt_before_hle(SH4 *c) {
  if (rt_hle_depth != rec_depth) return;
  stk_r15 = c->r[15];
  memcpy(stk_before, sh4_ram + STK_AREA_LO, sizeof stk_before);
}
void hwt_record_hle(SH4 *c, uint32_t a) {
  if (rt_hle_depth != rec_depth) return; /* an HLE call made inside another HLE call (a callback): not the sample's */
  if (rec_n < REC_MAX) {
    hle_rec *r = &rec[rec_n];
    r->t = a; r->r0 = c->r[0]; r->f0 = c->fr.u[0]; r->f1 = c->fr.u[1];
    r->args = hle_arg_mask;
    r->stk_first = stk_n; r->stk_n = 0;
    uint32_t live_lo = (stk_r15 & 0xFFFFFFu) > STK_AREA_LO ? (stk_r15 & 0xFFFFFCu) : STK_AREA_LO;
    if (memcmp(stk_before + (live_lo - STK_AREA_LO), sh4_ram + live_lo, STK_AREA_HI - live_lo))
      for (uint32_t o = live_lo; o < STK_AREA_HI; o += 4) {
        uint32_t x, y;
        memcpy(&x, stk_before + (o - STK_AREA_LO), 4); memcpy(&y, sh4_ram + o, 4);
        if (x != y && stk_n < STK_MAX) { stk[stk_n][0] = o - (stk_r15 & 0xFFFFFFu); stk[stk_n][1] = y; stk_n++; r->stk_n++; }
      }
  }
  rec_n++;
}

/* ---- the sample being taken ---- */
static uint8_t *buf_s, *buf_a, *buf_b;
static SH4 snap;
static hwt_stat *cur;
static uint32_t cur_frame;
static int cur_sample, sample_ids; /* a sample may span frames (the call yields): its id travels in the unwind record */

/* ---- one side's log ---- */
typedef struct { uint32_t t, arity, a[6]; } call_ent;
#define CALLS_MAX 8192
typedef struct { call_ent call[CALLS_MAX]; int n, diverged, io_reads; uint32_t io_hash, io_n; } side_log;
static side_log log_a, log_b, *slog;
static jmp_buf sbx_jb;
static uint32_t sbx_at;

/* the live call's hardware reads, replayed in order by each side (a polling loop ends as it did live) */
#define IOREC_MAX 400000
static uint32_t (*iorec)[2];
static int iorec_n;
void hwt_record_io(uint32_t a, uint32_t v) {
  if (rt_hle_depth != rec_depth) return;
  if (iorec_n < IOREC_MAX) { iorec[iorec_n][0] = a; iorec[iorec_n][1] = v; }
  iorec_n++;
}
uint32_t hwt_io_read(uint32_t a) {
  int k = slog->io_reads++;
  if (k < iorec_n && k < IOREC_MAX && iorec[k][0] == a) return iorec[k][1];
  slog->diverged = 1;
  return hsh(a ^ 0x5A5A0F0Fu);
}
void hwt_io_write(uint32_t a, uint32_t v, int size) {
  slog->io_hash = hsh(slog->io_hash ^ hsh(a + (uint32_t)size) ^ v);
  slog->io_n++;
}

/* an HLE call in the sandbox: logged, then answered from the live record */
static void sbx_hle(SH4 *c, uint32_t a, int arity) {
  int k = slog->n++;
  if (k < CALLS_MAX) {
    call_ent *e = &slog->call[k];
    e->t = a; e->arity = arity < 0 ? 4 : (uint32_t)arity;
    e->a[0] = c->r[4]; e->a[1] = c->r[5]; e->a[2] = c->r[6]; e->a[3] = c->r[7]; e->a[4] = c->fr.u[4]; e->a[5] = c->fr.u[5];
  }
  uint32_t h = hsh(a ^ hsh((uint32_t)k + 0x1234u));
  if (k < rec_n && k < REC_MAX && rec[k].t == a) {
    c->r[0] = rec[k].r0; c->fr.u[0] = rec[k].f0; c->fr.u[1] = rec[k].f1;
    for (uint32_t i = 0; i < rec[k].stk_n; i++) /* its out-parameters in the caller's frame */
      wr32(c->r[15] + stk[rec[k].stk_first + i][0], stk[rec[k].stk_first + i][1]);
  } else {
    slog->diverged = 1;
    c->r[0] = h; c->fr.u[0] = 0; c->fr.u[1] = 0;
  }
  /* the rest of what the calling convention lets a callee change */
  for (int i = 1; i < 8; i++) c->r[i] = hsh(h + (uint32_t)i);
  c->t = 0;
  c->pc = c->pr;
}

/* the hand-written functions whose original code side A ran (calls and jumps into them) in this sample */
#define INSIDE_MAX 256
static uint32_t inside[INSIDE_MAX];
static int inside_n, side_a;
static void note_entry(uint32_t t) {
  if (!side_a) return;
  int lo = 0, hi = rt_fn_count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (rt_fn_table[mid].addr == t) {
      if (!rt_fn_table[mid].hand) return;
      for (int k = 0; k < inside_n; k++) if (inside[k] == t) return;
      if (inside_n < INSIDE_MAX) inside[inside_n++] = t;
      return;
    }
    if (rt_fn_table[mid].addr < t) lo = mid + 1; else hi = mid - 1;
  }
}

/* interpret from c->pc until the return to ret with the stack back at sp; HLE targets are replayed */
static int sbx_steps(SH4 *c, uint32_t ret, uint32_t sp, int64_t steps) {
  while (steps-- > 0) {
    int r = sh4_step(c);
    if (r) { sbx_at = c->fault; return r == -1 ? END_ILLEGAL : r == -3 ? END_SLOT : END_UNSUP; }
    if (c->last == SH4_BR_NONE) continue;
    if (c->last == SH4_BR_RET && c->pc == ret && c->r[15] >= sp) return END_RET;
    uint32_t t = RT_NORM(c->pc);
    if (c->last != SH4_BR_RET) note_entry(t);
    if (hle_known(t)) {
      int last = (int)c->last;
      sbx_hle(c, t, -1);
      if (last != SH4_BR_CALL && c->pc == ret && c->r[15] >= sp) return END_RET; /* a tail jump to the HLE */
    }
  }
  sbx_at = c->pc;
  return END_BUDGET;
}

/* rt_call inside the sandbox (side B: the hand-written function and the translated code it calls) */
void hwt_sandbox_call(SH4 *c, uint32_t a) {
  int arity = hw_call_arity;
  hw_call_arity = -1;
  if (hle_known(a)) { sbx_hle(c, a, arity); return; }
  rt_fn f = rt_lookup(a);
  if (f) { f(c); return; }
  uint32_t ret = c->pr, sp = c->r[15];
  c->pc = a;
  int r = sbx_steps(c, ret, sp, STEPS_A);
  if (r != END_RET) hwt_sandbox_abort(r, sbx_at);
}
void hwt_sandbox_abort(int code, uint32_t at) {
  sbx_at = at;
  longjmp(sbx_jb, code == 2 ? END_ILLEGAL : code == 3 ? END_SLOT : code == 4 ? END_UNSUP : code == 5 ? END_BADRET
                  : code == 6 ? END_BUDGET : code == 7 ? END_STOP : code);
}

/* ---- sampling ---- */
int hwt_begin(SH4 *c, uint32_t addr) {
  hwt_stat *s = stat_of(addr);
  if (!s) return 0;
  s->calls++;
  uint32_t fr = hle_frame();
  if (hwt_recording && fr - cur_frame > PENDING_FRAMES_MAX) { cur->abandoned++; hwt_recording = 0; cur_sample = 0; }
  if (hwt_recording || (hwt_only && addr != hwt_only) || (int)s->samples >= hwt_budget) return 0;
  uint32_t n = s->calls;
  int want = n <= 8 || (n & (n - 1)) == 0 || fr >= s->next_frame;
  if (!want) return 0;
  if (fr >= s->next_frame) s->next_frame = fr + (uint32_t)hwt_spread;
  memcpy(buf_s, sh4_ram, 16u << 20);
  snap = *c;
  cur = s;
  cur_frame = fr;
  rec_n = 0;
  stk_n = 0;
  iorec_n = 0;
  rec_depth = rt_hle_depth;
  hwt_recording = 1;
  cur_sample = ++sample_ids;
  return cur_sample;
}
void hwt_abandon(void) {
  hwt_recording = 0;
  cur->abandoned++;
  cur_sample = 0;
}

static void fail(const char *fmt, uint32_t x, uint32_t y, uint32_t z) {
  cur->fail++;
  if (!cur->why[0]) {
    char b[128];
    /* a tiny formatter for "%x" fields only (rt_log takes no buffer) */
    int o = 0;
    uint32_t v[3] = {x, y, z};
    int vi = 0;
    for (const char *p = fmt; *p && o < 100; p++) {
      if (p[0] == '%' && p[1] == 'x' && vi < 3) {
        for (int d = 0; d < 8; d++) b[o++] = "0123456789abcdef"[(v[vi] >> (28 - 4 * d)) & 15];
        vi++; p++;
      } else b[o++] = *p;
    }
    b[o] = 0;
    memcpy(cur->why, b, (size_t)o + 1);
    rt_log("hw FAIL %08x frame %u call %u: %s", cur->addr, cur_frame, cur->calls, b);
  }
}

static int run_side_b(SH4 *b) {
  rt_fn f = rt_lookup(cur->addr);
  volatile int code = setjmp(sbx_jb);
  if (code == 0) { f(b); return END_RET; }
  return code;
}

void hwt_end(int sample) {
  if (sample != cur_sample || !hwt_recording) return; /* given up meanwhile */
  hwt_recording = 0;
  cur_sample = 0;
  if (rec_n > REC_MAX) { cur->longrun++; return; }
  uint8_t *ram = sh4_ram;
  SH4 *save_cpu = hw_cpu;
  static SH4 a, b;
  cur->samples++;
  rt_sandbox = 1;
  /* side A: the original code */
  memcpy(buf_a, buf_s, 16u << 20);
  sh4_ram = buf_a;
  slog = &log_a; log_a.n = log_a.diverged = log_a.io_reads = 0; log_a.io_hash = log_a.io_n = 0;
  a = snap;
  a.pc = cur->addr;
  inside_n = 0; side_a = 1;
  int ea = sbx_steps(&a, snap.pr, snap.r[15], STEPS_A);
  side_a = 0;
  uint32_t at_a = sbx_at;
  /* side B: the hand-written function */
  memcpy(buf_b, buf_s, 16u << 20);
  sh4_ram = buf_b;
  slog = &log_b; log_b.n = log_b.diverged = log_b.io_reads = 0; log_b.io_hash = log_b.io_n = 0;
  b = snap;
  b.budget = 200000000;
  int eb = run_side_b(&b);
  uint32_t at_b = sbx_at;
  int ret = hw_last_ret_set;
  rt_sandbox = 0;
  sh4_ram = ram;
  hw_cpu = save_cpu;
  if (ret) cur->retv = 1;
  if (log_a.diverged || log_b.diverged) cur->diverged++;
  int f0 = (int)cur->fail;
  if (ea == END_BUDGET) { cur->longrun++; cur->samples--; return; }
  if (ea != eb) { fail("end %x/%x at %x", (uint32_t)ea, (uint32_t)eb, eb == END_RET ? at_a : at_b); return; }
  if (ea != END_RET) { if (at_a != at_b) fail("fault %x/%x", at_a, at_b, 0); else cur->pass++; return; }
  /* registers the caller may rely on (MACH/MACL are caller-saved in this compiler: func_8C010DDE uses mul.l
   * without saving them) */
  for (int i = 8; i < 16; i++) if (a.r[i] != b.r[i]) { fail("r%x %x/%x", (uint32_t)i, a.r[i], b.r[i]); return; }
  if (RT_NORM(a.pc) != RT_NORM(b.pc)) { fail("pc %x/%x", a.pc, b.pc, 0); return; }
  if (a.pr != b.pr) { fail("pr %x/%x", a.pr, b.pr, 0); return; }
  if (a.gbr != b.gbr || a.fpscr != b.fpscr) { fail("gbr/fpscr %x/%x", a.gbr ^ b.gbr, a.fpscr ^ b.fpscr, 0); return; }
  for (int i = 12; i < 16; i++) if (a.fr.u[i] != b.fr.u[i]) { fail("fr%x %x/%x", (uint32_t)i, a.fr.u[i], b.fr.u[i]); return; }
  if (ret && a.r[0] != b.r[0]) { fail("r0 %x/%x", a.r[0], b.r[0], 0); return; }
  /* the calls into the HLE, in order */
  if (log_a.n != log_b.n) { fail("hle calls %x/%x", (uint32_t)log_a.n, (uint32_t)log_b.n, 0); return; }
  for (int k = 0; k < log_a.n && k < CALLS_MAX; k++) {
    call_ent *x = &log_a.call[k], *y = &log_b.call[k];
    if (x->t != y->t) { fail("hle call %x to %x/%x", (uint32_t)k, x->t, y->t); return; }
    /* the arguments: as many as hand-written code passed, else the registers the live HLE read (the other
     * argument registers hold whatever the caller left there) */
    uint32_t used = y->arity < 4 ? (0xFu >> (4 - y->arity)) << 4
                    : k < rec_n && k < REC_MAX && rec[k].t == x->t ? rec[k].args : 0x10u;
    for (uint32_t i = 0; i < 4; i++)
      if ((used >> (4 + i) & 1) && stack_token(x->a[i]) != stack_token(y->a[i])) {
        fail("hle call to %x arg %x %x", x->t, i, x->a[i] ^ y->a[i]);
        return;
      }
  }
  if (log_a.io_n != log_b.io_n || log_a.io_hash != log_b.io_hash) { fail("hardware writes %x/%x", log_a.io_n, log_b.io_n, 0); return; }
  /* guest RAM, but the dead stack; a word pointing into the stack counts as one token */
  uint32_t lo = STACK_LO & 0xFFFFFF, hi = snap.r[15] & 0xFFFFFF;
  if (hi < lo || hi > 0x00F400) hi = lo;
  if (memcmp(buf_a, buf_b, lo) || memcmp(buf_a + hi, buf_b + hi, (16u << 20) - hi)) {
    for (uint32_t i = 0; i < (16u << 20); i += 4) {
      if (i >= lo && i < hi) continue;
      uint32_t x, y;
      memcpy(&x, buf_a + i, 4); memcpy(&y, buf_b + i, 4);
      if (x != y && stack_token(x) != stack_token(y)) { fail("ram %x %x/%x", 0x8C000000u | i, x, y); return; }
    }
  }
  if ((int)cur->fail == f0) {
    cur->pass++;
    for (int k = 0; k < inside_n; k++) { /* their original code ran inside a passing sample */
      hwt_stat *s = stat_of(inside[k]);
      if (s && s != cur) s->inside++;
    }
  }
}

void hwt_summary(void) {
  if (!hwt_enabled) return;
  int own = 0, inside_only = 0, failed = 0, untested = 0, hand = 0;
  uint32_t samples = 0;
  static const hwt_stat none;
  for (int i = 0; i < rt_fn_count; i++) {
    if (!rt_fn_table[i].hand) continue;
    hand++;
    const hwt_stat *s = &none;
    for (int k = 0; k < nst; k++) if (st[k].addr == rt_fn_table[i].addr) s = &st[k];
    samples += s->samples;
    const char *verdict;
    if (s->fail) { failed++; verdict = "FAILED"; }
    else if (s->pass) { own++; verdict = "pass"; }
    else if (s->inside) { inside_only++; verdict = "pass (inside other functions' samples)"; }
    else {
      untested++;
      verdict = !s->calls ? "UNTESTED (not entered from translated code, not run inside a passing sample)"
                : s->abandoned ? "UNTESTED (every sample given up)" : "UNTESTED (every sample too long)";
    }
    rt_log("hw test %08x calls %u samples %u pass %u fail %u long %u given-up %u replay-diverged %u inside %u%s: %s",
           rt_fn_table[i].addr, s->calls, s->samples, s->pass, s->fail, s->longrun, s->abandoned, s->diverged, s->inside,
           s->retv ? " ret" : "", verdict);
  }
  rt_log("hw test hand-written %d pass %d (own samples %d, inside others %d) fail %d untested %d samples %u", hand,
         own + inside_only, own, inside_only, failed, untested, samples);
}

/* --hwtest BUDGET[:SPREAD[:ONLY]] */
int hwt_set(const char *opt) {
  char *e;
  hwt_budget = (int)strtol(opt, &e, 10);
  if (*e == ':') hwt_spread = (int)strtol(e + 1, &e, 10);
  if (*e == ':') hwt_only = (uint32_t)strtoul(e + 1, &e, 16);
  if (hwt_budget <= 0) return 0;
  buf_s = malloc(16u << 20); buf_a = malloc(16u << 20); buf_b = malloc(16u << 20);
  rec = malloc(sizeof *rec * REC_MAX);
  iorec = malloc(sizeof *iorec * IOREC_MAX);
  stk = malloc(sizeof *stk * STK_MAX);
  if (!buf_s || !buf_a || !buf_b || !rec || !iorec || !stk) return -1;
  hwt_enabled = 1;
  return 1;
}
#endif
