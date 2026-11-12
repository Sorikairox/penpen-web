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
void hwt_hle_wrote(uint32_t addr, uint32_t len) { (void)addr; (void)len; }
uint32_t hwt_io_read(uint32_t a) { (void)a; return 0; }
void hwt_record_io(uint32_t a, uint32_t v) { (void)a; (void)v; }
void hwt_io_write(uint32_t a, uint32_t v, int size) { (void)a; (void)v; (void)size; }
int hwt_begin(SH4 *c, uint32_t addr) { (void)c; (void)addr; return 0; }
void hwt_end(int sample) { (void)sample; }
void hwt_abandon(void) {}
int hwt_replay(const char *list) { (void)list; return 2; }
int hwt_corpus(const char *opt) { (void)opt; return -1; }
void hwt_summary(void) {}
int hwt_set(const char *opt) { (void)opt; return -1; }
#else
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int hwt_enabled, hwt_recording;
int hwt_alloc(void);
static int hwt_budget = 160;   /* samples per function */
static int hwt_spread = 50;    /* after the first calls: at most one sample per function every this many frames */
/* S14: test only these functions (--hwtest B:S:A1,A2,...; the Mull gate's first pass samples only the exports of the
   mutated file) */
#define ONLY_MAX 4096
static uint32_t hwt_only[ONLY_MAX];
static int hwt_only_n;
static int only_has(uint32_t a) {
  for (int i = 0; i < hwt_only_n; i++) if (hwt_only[i] == a) return 1;
  return 0;
}
extern int hw_last_ret_set, hw_call_arity, hw_call_fargs;
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
typedef struct { uint32_t t, r0, f0, f1, args, stk_first, stk_n, mem_first, mem_n; } hle_rec;
#define REC_MAX 200000
static hle_rec *rec;
static int rec_n, rec_depth;
#define STK_AREA_LO 0x00C000u
#define STK_AREA_HI 0x00F400u
#define STK_MAX 400000
static uint32_t (*stk)[2]; /* offset from r15, value */
static uint32_t stk_n, stk_r15;
static uint8_t stk_before[STK_AREA_HI - STK_AREA_LO];
/* S12: guest RAM an HLE call filled outside the stack (gdFsRead's sectors: the file a loader then works on). The HLE
 * says what it wrote (hwt_hle_wrote); the bytes are kept and both sandbox sides get them when the call is replayed,
 * so code that reads what it loaded (the container fix-ups) is tested on the real data. */
typedef struct { uint32_t addr, len, pool; } mem_rec;
#define MEMREC_MAX 8192
#define MEMPOOL_MAX (12u << 20)
static mem_rec *memrec;
static uint8_t *mempool;
static uint32_t memrec_n, mempool_n, mem_call_first, mem_lost;
void hwt_hle_wrote(uint32_t addr, uint32_t len) {
  if (!hwt_recording || rt_hle_depth != rec_depth + 1) return; /* only the sampled call's own HLE calls */
  uint32_t off = addr & 0xFFFFFFu;
  if (((addr >> 24) & 0x1Fu) != 0x0Cu) return; /* not main RAM (the sandboxes compare main RAM only) */
  if (off + len > (16u << 20) || memrec_n >= MEMREC_MAX || mempool_n + len > MEMPOOL_MAX) { mem_lost++; return; }
  memrec[memrec_n].addr = off; memrec[memrec_n].len = len; memrec[memrec_n].pool = mempool_n;
  memcpy(mempool + mempool_n, sh4_ram + off, len);
  mempool_n += len; memrec_n++;
}
void hwt_before_hle(SH4 *c) {
  if (rt_hle_depth != rec_depth) return;
  mem_call_first = memrec_n;
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
    r->mem_first = mem_call_first; r->mem_n = memrec_n - mem_call_first;
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
typedef struct { uint32_t t, arity, fargs, a[8]; } call_ent; /* a[4..7]: FR4-FR7 */
#define CALLS_MAX 8192
typedef struct { call_ent call[CALLS_MAX]; int n, diverged, io_reads; uint32_t io_hash, io_n; } side_log;
static side_log log_a, log_b, *slog;
static int replay_diffs; /* S16: --hwreplay's debug output (PENPEN_REPLAY_DIFFS=N: N differing words; above 100: HLE calls too) */
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
  if (replay_diffs) rt_log("hw replay io-write side %s %08x %08x %d", slog == &log_a ? "a" : "b", a, v, size);
  slog->io_hash = hsh(slog->io_hash ^ hsh(a + (uint32_t)size) ^ v);
  slog->io_n++;
}

/* an HLE call in the sandbox: logged, then answered from the live record */
static void sbx_hle(SH4 *c, uint32_t a, int arity, int fargs) {
  int k = slog->n++;
  if (k < CALLS_MAX) {
    call_ent *e = &slog->call[k];
    e->t = a; e->arity = arity < 0 ? 4 : (uint32_t)arity; e->fargs = (uint32_t)fargs;
    for (int i = 0; i < 4; i++) { e->a[i] = c->r[4 + i]; e->a[4 + i] = c->fr.u[4 + i]; }
  }
  uint32_t h = hsh(a ^ hsh((uint32_t)k + 0x1234u));
  if (replay_diffs > 100) rt_log("hw replay hle %d %08x (recorded %08x)", k, a, k < rec_n && k < REC_MAX ? rec[k].t : 0);
  if (k < rec_n && k < REC_MAX && rec[k].t == a) {
    c->r[0] = rec[k].r0; c->fr.u[0] = rec[k].f0; c->fr.u[1] = rec[k].f1;
    for (uint32_t i = 0; i < rec[k].stk_n; i++) /* its out-parameters in the caller's frame */
      wr32(c->r[15] + stk[rec[k].stk_first + i][0], stk[rec[k].stk_first + i][1]);
    for (uint32_t i = 0; i < rec[k].mem_n; i++) { /* what it read into guest RAM (S12) */
      const mem_rec *m = &memrec[rec[k].mem_first + i];
      memcpy(sh4_ram + m->addr, mempool + m->pool, m->len);
    }
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
/* S16: every function side A entered (translated or hand-written), kept with a corpus sample: the offline replay of a
   mutant picks the samples that ran the mutated function's original code */
#define ENT_MAX 4096
static uint32_t ent[ENT_MAX], ent_set[2 * ENT_MAX];
static int ent_n, ent_on;
static void ent_add(uint32_t t) {
  if (!ent_on || ent_n >= ENT_MAX) return;
  uint32_t h = hsh(t) & (2 * ENT_MAX - 1);
  while (ent_set[h]) { if (ent_set[h] == t) return; h = (h + 1) & (2 * ENT_MAX - 1); }
  ent_set[h] = t;
  ent[ent_n++] = t;
}
static void ent_clear(void) { memset(ent_set, 0, sizeof ent_set); ent_n = 0; }
static void note_entry(uint32_t t) {
  if (!side_a) return;
  int lo = 0, hi = rt_fn_count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (rt_fn_table[mid].addr == t) {
      ent_add(t);
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
      sbx_hle(c, t, -1, 0);
      if (last != SH4_BR_CALL && c->pc == ret && c->r[15] >= sp) return END_RET; /* a tail jump to the HLE */
    }
  }
  sbx_at = c->pc;
  return END_BUDGET;
}

/* rt_call inside the sandbox (side B: the hand-written function and the translated code it calls) */
void hwt_sandbox_call(SH4 *c, uint32_t a) {
  int arity = hw_call_arity, fargs = hw_call_fargs;
  hw_call_arity = -1;
  hw_call_fargs = 0;
  if (hle_known(a)) { sbx_hle(c, a, arity, fargs); return; }
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
  if (hwt_recording || (hwt_only_n && !only_has(addr)) || (int)s->samples >= hwt_budget) return 0;
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
  memrec_n = mempool_n = mem_call_first = 0;
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

/* side B (the hand-written function, from the snapshot buf_s / snap) and the comparison with side A (a, ea, at_a,
 * log_a, buf_a). Returns 1 when the sample passes. S16: shared by the live difftest and the offline replay. */
static SH4 side_a_cpu, side_b_cpu;
static int side_b_compare(int ea, uint32_t at_a) {
  SH4 *a = &side_a_cpu, *b = &side_b_cpu;
  uint8_t *ram = sh4_ram;
  SH4 *save_cpu = hw_cpu;
  rt_sandbox = 1;
  memcpy(buf_b, buf_s, 16u << 20);
  sh4_ram = buf_b;
  slog = &log_b; log_b.n = log_b.diverged = log_b.io_reads = 0; log_b.io_hash = log_b.io_n = 0;
  *b = snap;
  b->budget = 200000000;
  hw_last_ret_set = 0;
  int eb = run_side_b(b);
  uint32_t at_b = sbx_at;
  int ret = hw_last_ret_set;
  rt_sandbox = 0;
  sh4_ram = ram;
  hw_cpu = save_cpu;
  if (ret) cur->retv = 1;
  if (log_a.diverged || log_b.diverged) cur->diverged++;
  int f0 = (int)cur->fail;
  if (ea != eb) { fail("end %x/%x at %x", (uint32_t)ea, (uint32_t)eb, eb == END_RET ? at_a : at_b); return 0; }
  if (ea != END_RET) { if (at_a != at_b) { fail("fault %x/%x", at_a, at_b, 0); return 0; } return 1; }
  /* registers the caller may rely on (MACH/MACL are caller-saved in this compiler: func_8C010DDE uses mul.l
   * without saving them) */
  for (int i = 8; i < 16; i++) if (a->r[i] != b->r[i]) { fail("r%x %x/%x", (uint32_t)i, a->r[i], b->r[i]); return 0; }
  if (RT_NORM(a->pc) != RT_NORM(b->pc)) { fail("pc %x/%x", a->pc, b->pc, 0); return 0; }
  if (a->pr != b->pr) { fail("pr %x/%x", a->pr, b->pr, 0); return 0; }
  if (a->gbr != b->gbr || a->fpscr != b->fpscr) { fail("gbr/fpscr %x/%x", a->gbr ^ b->gbr, a->fpscr ^ b->fpscr, 0); return 0; }
  for (int i = 12; i < 16; i++) if (a->fr.u[i] != b->fr.u[i]) { fail("fr%x %x/%x", (uint32_t)i, a->fr.u[i], b->fr.u[i]); return 0; }
  if ((ret & 1) && a->r[0] != b->r[0]) { fail("r0 %x/%x", a->r[0], b->r[0], 0); return 0; }
  if ((ret & 2) && a->fr.u[0] != b->fr.u[0]) { fail("fr0 %x/%x", a->fr.u[0], b->fr.u[0], 0); return 0; } /* S13: RETF */
  /* the calls into the HLE, in order */
  if (log_a.n != log_b.n) { fail("hle calls %x/%x", (uint32_t)log_a.n, (uint32_t)log_b.n, 0); return 0; }
  for (int k = 0; k < log_a.n && k < CALLS_MAX; k++) {
    call_ent *x = &log_a.call[k], *y = &log_b.call[k];
    if (x->t != y->t) { fail("hle call %x to %x/%x", (uint32_t)k, x->t, y->t); return 0; }
    /* the arguments: as many as hand-written code passed, else the registers the live HLE read (the other
     * argument registers hold whatever the caller left there) */
    uint32_t used = y->arity < 4 ? (0xFu >> (4 - y->arity)) << 4
                    : k < rec_n && k < REC_MAX && rec[k].t == x->t ? rec[k].args : 0x10u;
    for (uint32_t i = 0; i < 4; i++)
      if ((used >> (4 + i) & 1) && stack_token(x->a[i]) != stack_token(y->a[i])) {
        fail("hle call to %x arg %x %x", x->t, i, x->a[i] ^ y->a[i]);
        return 0;
      }
    /* S13: the float arguments hand-written code passed (FARGS: FR4 ...) */
    for (uint32_t i = 0; i < y->fargs && i < 4; i++)
      if (x->a[4 + i] != y->a[4 + i]) { fail("hle call to %x fr%x %x", x->t, 4 + i, x->a[4 + i] ^ y->a[4 + i]); return 0; }
  }
  if (log_a.io_n != log_b.io_n || log_a.io_hash != log_b.io_hash) { fail("hardware writes %x/%x", log_a.io_n, log_b.io_n, 0); return 0; }
  /* guest RAM, but the dead stack; a word pointing into the stack counts as one token */
  uint32_t lo = STACK_LO & 0xFFFFFF, hi = snap.r[15] & 0xFFFFFF;
  if (hi < lo || hi > 0x00F400) hi = lo;
  if (memcmp(buf_a, buf_b, lo) || memcmp(buf_a + hi, buf_b + hi, (16u << 20) - hi)) {
    for (uint32_t i = 0; i < (16u << 20); i += 4) {
      if (i >= lo && i < hi) continue;
      uint32_t x, y;
      memcpy(&x, buf_a + i, 4); memcpy(&y, buf_b + i, 4);
      if (x != y && stack_token(x) != stack_token(y)) {
        if (replay_diffs) { /* S16: --hwreplay with PENPEN_REPLAY_DIFFS=N: the first N differing words */
          int shown = 0;
          for (uint32_t k = i; k < (16u << 20) && shown < replay_diffs; k += 4) {
            if (k >= lo && k < hi) continue;
            memcpy(&x, buf_a + k, 4); memcpy(&y, buf_b + k, 4);
            if (x != y && stack_token(x) != stack_token(y)) { rt_log("hw replay diff %08x %08x/%08x", 0x8C000000u | k, x, y); shown++; }
          }
          memcpy(&x, buf_a + i, 4); memcpy(&y, buf_b + i, 4);
        }
        fail("ram %x %x/%x", 0x8C000000u | i, x, y);
        return 0;
      }
    }
  }
  return (int)cur->fail == f0;
}

static void corpus_keep(int ea, uint32_t at_a, uint32_t cost);

void hwt_end(int sample) {
  if (sample != cur_sample || !hwt_recording) return; /* given up meanwhile */
  hwt_recording = 0;
  cur_sample = 0;
  if (rec_n > REC_MAX) { cur->longrun++; return; }
  uint8_t *ram = sh4_ram;
  SH4 *save_cpu = hw_cpu;
  SH4 *a = &side_a_cpu;
  cur->samples++;
  rt_sandbox = 1;
  /* side A: the original code */
  memcpy(buf_a, buf_s, 16u << 20);
  sh4_ram = buf_a;
  slog = &log_a; log_a.n = log_a.diverged = log_a.io_reads = 0; log_a.io_hash = log_a.io_n = 0;
  *a = snap;
  a->pc = cur->addr;
  inside_n = 0; side_a = 1;
  ent_clear();
  int ea = sbx_steps(a, snap.pr, snap.r[15], STEPS_A);
  side_a = 0;
  uint32_t at_a = sbx_at;
  rt_sandbox = 0;
  sh4_ram = ram;
  hw_cpu = save_cpu;
  if (ea == END_BUDGET) { cur->longrun++; cur->samples--; return; }
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  int passed = side_b_compare(ea, at_a);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  if (passed) {
    cur->pass++;
    for (int k = 0; k < inside_n; k++) { /* their original code ran inside a passing sample */
      hwt_stat *s = stat_of(inside[k]);
      if (s && s != cur) s->inside++;
    }
    corpus_keep(ea, at_a, (uint32_t)((t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000));
  }
}

/* ---- S16: the sample corpus (--hwcorpus DIR:K) and its offline replay (--hwreplay LIST) ----
 * A Mull mutant used to be judged by whole routes from boot (DECOMP.md 16.2: ~59 s per mutant, most of it the
 * survivors' routes). The live difftest already has everything a sample needs to be judged again without the run:
 * the snapshot, the HLE results (with their stack out-parameters and the RAM they filled), the hardware reads, and
 * side A's outcome (the original code, interpreted). With --hwcorpus a route's run keeps up to K passing samples per
 * function (the first, then ones whose side A entered a new set of functions, then a few at random) in DIR: guest
 * RAM as 4 KiB pages in one content-addressed store (DIR/pages.bin: the samples of a run share most pages), one file
 * per sample, and DIR/index.tsv (id, function, frame, side B's cost, the functions side A entered). --hwreplay LIST
 * runs side B of each listed sample with this build (a mutant switched on) and compares it with the recorded side A:
 * a mutant is judged in seconds, without a route. The corpus holds guest RAM: it stays in the Docker volume
 * penpen-corpus, never in git. */
#define PAGE 4096u
#define NPAGE ((16u << 20) / PAGE)
#define KEEP_MAX 16
static char corpus_dir[512];
static int corpus_k;
static FILE *pages_f, *index_f;
static uint32_t pages_n, corpus_n;
static uint64_t *ph_key; /* open addressing: page hash -> page number + 1 */
static uint32_t *ph_val, ph_cap;
typedef struct { uint32_t addr, kept, sig[KEEP_MAX]; } keep_stat;
static keep_stat *ks;
static int ks_n;
static uint64_t page_hash(const uint8_t *p) {
  uint64_t h = 0x9E3779B97F4A7C15ull;
  for (uint32_t i = 0; i < PAGE; i += 8) {
    uint64_t w;
    memcpy(&w, p + i, 8);
    h ^= w * 0xC2B2AE3D27D4EB4Full;
    h = (h << 31 | h >> 33) * 0x9E3779B185EBCA87ull;
  }
  h ^= h >> 33; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33;
  return h ? h : 1;
}
static uint32_t page_put(const uint8_t *p) {
  uint64_t h = page_hash(p);
  uint32_t i = (uint32_t)h & (ph_cap - 1);
  while (ph_key[i]) {
    if (ph_key[i] == h) return ph_val[i] - 1;
    i = (i + 1) & (ph_cap - 1);
  }
  if (pages_n + 1 > ph_cap / 2) { rt_log("hw corpus page store full"); return 0xFFFFFFFFu; }
  ph_key[i] = h;
  ph_val[i] = ++pages_n;
  fwrite(p, 1, PAGE, pages_f);
  return pages_n - 1;
}
static keep_stat *keep_of(uint32_t addr) {
  for (int i = 0; i < ks_n; i++) if (ks[i].addr == addr) return &ks[i];
  if (ks_n == NST) return 0;
  memset(&ks[ks_n], 0, sizeof ks[ks_n]);
  ks[ks_n].addr = addr;
  return &ks[ks_n++];
}
static int u32cmp(const void *x, const void *y) {
  uint32_t a = *(const uint32_t *)x, b = *(const uint32_t *)y;
  return a < b ? -1 : a > b;
}
/* the sample's path signature: the functions side A entered, the number of HLE calls (log2), how it ended */
static uint32_t sample_sig(void) {
  qsort(ent, (size_t)ent_n, sizeof ent[0], u32cmp);
  uint32_t h = 0x811C9DC5u;
  for (int i = 0; i < ent_n; i++) h = hsh(h ^ ent[i]);
  uint32_t lg = 0;
  for (uint32_t n = (uint32_t)log_a.n; n; n >>= 1) lg++;
  return hsh(h ^ lg * 0x9E37u) | 1;
}
static void wr(FILE *f, const void *p, size_t n) { fwrite(p, 1, n, f); }
static void corpus_keep(int ea, uint32_t at_a, uint32_t cost) {
  if (!corpus_k || rec_n > REC_MAX || iorec_n > IOREC_MAX || stk_n >= STK_MAX || mem_lost) return;
  keep_stat *k = keep_of(cur->addr);
  if (!k || (int)k->kept >= corpus_k) return;
  uint32_t sig = sample_sig();
  int seen = 0;
  for (uint32_t i = 0; i < k->kept; i++) seen |= k->sig[i] == sig;
  /* the first; a new path; else one in 8 at random (by the sample id), the last slot kept for a new path */
  if (k->kept && seen && (hsh((uint32_t)sample_ids ^ 0xC0FFEEu) & 7 || (int)k->kept >= corpus_k - 1)) return;
  char path[600];
  uint32_t id = corpus_n++;
  snprintf(path, sizeof path, "%s/s%06u.smp", corpus_dir, id);
  FILE *f = fopen(path, "wb");
  if (!f) { rt_log("hw corpus cannot write a sample"); corpus_k = 0; return; }
  static uint32_t snap_pages[NPAGE], a_pages[NPAGE][2];
  uint32_t na = 0;
  for (uint32_t i = 0; i < NPAGE; i++) {
    snap_pages[i] = page_put(buf_s + i * PAGE);
    if (memcmp(buf_s + i * PAGE, buf_a + i * PAGE, PAGE)) { a_pages[na][0] = i; a_pages[na][1] = page_put(buf_a + i * PAGE); na++; }
  }
  uint32_t hdr[16] = {0x31535050u /* PPS1 */, (uint32_t)sizeof(SH4), cur->addr, cur_frame, cur->calls,
                      (uint32_t)rec_n, stk_n, (uint32_t)iorec_n, memrec_n, mempool_n, (uint32_t)log_a.n, na,
                      (uint32_t)ent_n, cost, sig, 0};
  wr(f, hdr, sizeof hdr);
  wr(f, &snap, sizeof snap);
  wr(f, rec, sizeof *rec * (size_t)rec_n);
  wr(f, stk, sizeof *stk * stk_n);
  wr(f, iorec, sizeof *iorec * (size_t)iorec_n);
  wr(f, memrec, sizeof *memrec * memrec_n);
  wr(f, mempool, mempool_n);
  /* side A's outcome */
  int32_t ea_hdr[4] = {ea, (int32_t)at_a, log_a.diverged, 0};
  wr(f, ea_hdr, sizeof ea_hdr);
  wr(f, &side_a_cpu, sizeof side_a_cpu);
  uint32_t io[2] = {log_a.io_hash, log_a.io_n};
  wr(f, io, sizeof io);
  wr(f, log_a.call, sizeof log_a.call[0] * (size_t)(log_a.n < CALLS_MAX ? log_a.n : CALLS_MAX));
  wr(f, snap_pages, sizeof snap_pages);
  wr(f, a_pages, sizeof a_pages[0] * na);
  wr(f, ent, sizeof ent[0] * (size_t)ent_n);
  int bad = ferror(f);
  bad |= fclose(f);
  if (bad) { rt_log("hw corpus write failed"); corpus_k = 0; return; }
  fflush(pages_f);
  k->sig[k->kept++] = sig;
  fprintf(index_f, "s%06u\t%08x\t%u\t%u\t%08x\t", id, cur->addr, cur_frame, cost, sig);
  for (int i = 0; i < ent_n; i++) fprintf(index_f, i ? ",%x" : "%x", ent[i]);
  fprintf(index_f, "\n");
  fflush(index_f);
}

/* --hwreplay LIST: each line a sample file of a corpus (its pages.bin beside it); side B with this build against the
 * recorded side A. Prints "hw replay pass|FAIL ..." per sample and stops at the first FAIL; "hw replay done". */
static struct { char dir[512]; const uint8_t *p; size_t n; } pmap[64];
static int pmap_n;
static const uint8_t *pages_of(const char *dir) {
  for (int i = 0; i < pmap_n; i++) if (!strcmp(pmap[i].dir, dir)) return pmap[i].p;
  if (pmap_n == 64) return 0;
  char path[600];
  snprintf(path, sizeof path, "%s/pages.bin", dir);
  int fd = open(path, O_RDONLY);
  if (fd < 0) return 0;
  struct stat sb;
  if (fstat(fd, &sb)) { close(fd); return 0; }
  void *p = mmap(0, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (p == MAP_FAILED) return 0;
  snprintf(pmap[pmap_n].dir, sizeof pmap[pmap_n].dir, "%s", dir);
  pmap[pmap_n].p = p; pmap[pmap_n].n = (size_t)sb.st_size;
  return pmap[pmap_n++].p;
}
static int rd(FILE *f, void *p, size_t n) { return fread(p, 1, n, f) == n; }
/* loads one sample into the difftest's state; 0 on success */
static int replay_load(const char *path, int *ea, uint32_t *at_a) {
  char dir[512];
  snprintf(dir, sizeof dir, "%s", path);
  char *sl = strrchr(dir, '/');
  if (sl) *sl = 0; else snprintf(dir, sizeof dir, ".");
  const uint8_t *pg = pages_of(dir);
  FILE *f = fopen(path, "rb");
  if (!f || !pg) { if (f) fclose(f); return 1; }
  uint32_t hdr[16];
  int ok = rd(f, hdr, sizeof hdr) && hdr[0] == 0x31535050u && hdr[1] == sizeof(SH4) && hdr[5] <= REC_MAX &&
           hdr[6] <= STK_MAX && hdr[7] <= IOREC_MAX && hdr[8] <= MEMREC_MAX && hdr[9] <= MEMPOOL_MAX && hdr[11] <= NPAGE &&
           hdr[12] <= ENT_MAX;
  if (ok) {
    cur = stat_of(hdr[2]);
    if (!cur) { fclose(f); return 1; }
    cur->why[0] = 0;
    cur_frame = hdr[3];
    rec_n = (int)hdr[5]; stk_n = hdr[6]; iorec_n = (int)hdr[7]; memrec_n = hdr[8]; mempool_n = hdr[9];
    log_a.n = (int)hdr[10];
    ok = rd(f, &snap, sizeof snap) && rd(f, rec, sizeof *rec * (size_t)rec_n) && rd(f, stk, sizeof *stk * stk_n) &&
         rd(f, iorec, sizeof *iorec * (size_t)iorec_n) && rd(f, memrec, sizeof *memrec * memrec_n) &&
         rd(f, mempool, mempool_n);
  }
  int32_t ea_hdr[4];
  uint32_t io[2];
  static uint32_t snap_pages[NPAGE], a_pages[NPAGE][2];
  ok = ok && rd(f, ea_hdr, sizeof ea_hdr) && rd(f, &side_a_cpu, sizeof side_a_cpu) && rd(f, io, sizeof io) &&
       rd(f, log_a.call, sizeof log_a.call[0] * (size_t)(log_a.n < CALLS_MAX ? log_a.n : CALLS_MAX)) &&
       rd(f, snap_pages, sizeof snap_pages) && rd(f, a_pages, sizeof a_pages[0] * hdr[11]);
  fclose(f);
  if (!ok) return 1;
  *ea = ea_hdr[0]; *at_a = (uint32_t)ea_hdr[1];
  log_a.diverged = ea_hdr[2]; log_a.io_hash = io[0]; log_a.io_n = io[1];
  for (uint32_t i = 0; i < NPAGE; i++) memcpy(buf_s + i * PAGE, pg + (size_t)snap_pages[i] * PAGE, PAGE);
  memcpy(buf_a, buf_s, 16u << 20);
  for (uint32_t i = 0; i < hdr[11]; i++) memcpy(buf_a + a_pages[i][0] * PAGE, pg + (size_t)a_pages[i][1] * PAGE, PAGE);
  return 0;
}
int hwt_replay(const char *list) {
  if (hwt_alloc() < 0) return 2;
  const char *d = getenv("PENPEN_REPLAY_DIFFS");
  replay_diffs = d ? atoi(d) : 0;
  FILE *l = fopen(list, "r");
  if (!l) { rt_log("hw replay: no list"); return 2; }
  char path[600];
  int n = 0, bad = 0, failed = 0, all = 0;
  while (fgets(path, sizeof path, l)) {
    path[strcspn(path, "\r\n")] = 0;
    if (!strcmp(path, "#all")) { all = 1; continue; } /* the control: every sample's verdict, no stop at a FAIL */
    if (!path[0] || path[0] == '#') continue;
    int ea;
    uint32_t at_a;
    if (replay_load(path, &ea, &at_a)) { rt_log("hw replay unreadable %s", path); bad++; continue; }
    n++;
    if (!side_b_compare(ea, at_a)) {
      rt_log("hw replay FAIL %s %08x: %s", path, cur->addr, cur->why);
      failed++;
      if (!all) { fclose(l); return 1; }
    } else if (hle_verbose || all) rt_log("hw replay pass %s %08x", path, cur->addr);
  }
  fclose(l);
  rt_log("hw replay done %d failed %d unreadable %d", n, failed, bad);
  return failed ? 1 : 0;
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
  if (*e == ':')
    do hwt_only[hwt_only_n < ONLY_MAX ? hwt_only_n++ : ONLY_MAX - 1] = (uint32_t)strtoul(e + 1, &e, 16);
    while (*e == ',');
  if (hwt_budget <= 0) return 0;
  return hwt_alloc();
}
int hwt_alloc(void) {
  if (buf_s) return 1;
  buf_s = malloc(16u << 20); buf_a = malloc(16u << 20); buf_b = malloc(16u << 20);
  rec = malloc(sizeof *rec * REC_MAX);
  iorec = malloc(sizeof *iorec * IOREC_MAX);
  stk = malloc(sizeof *stk * STK_MAX);
  memrec = malloc(sizeof *memrec * MEMREC_MAX);
  mempool = malloc(MEMPOOL_MAX);
  if (!buf_s || !buf_a || !buf_b || !rec || !iorec || !stk) return -1;
  hwt_enabled = 1;
  return 1;
}
/* --hwcorpus DIR:K (with --hwtest): keep up to K passing samples per function in DIR (a new, empty directory) */
int hwt_corpus(const char *opt) {
  const char *c = strrchr(opt, ':');
  if (!c || (size_t)(c - opt) >= sizeof corpus_dir) return -1;
  memcpy(corpus_dir, opt, (size_t)(c - opt));
  corpus_dir[c - opt] = 0;
  corpus_k = atoi(c + 1);
  if (corpus_k < 1 || corpus_k > KEEP_MAX) return -1;
  char path[600];
  for (char *q = corpus_dir + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(corpus_dir, 0755); *q = '/'; }
  mkdir(corpus_dir, 0755);
  snprintf(path, sizeof path, "%s/pages.bin", corpus_dir);
  pages_f = fopen(path, "wb");
  snprintf(path, sizeof path, "%s/index.tsv", corpus_dir);
  index_f = fopen(path, "w");
  ph_cap = 1u << 22; /* 2 M pages (8 GiB) at most per run */
  ph_key = calloc(ph_cap, sizeof *ph_key);
  ph_val = calloc(ph_cap, sizeof *ph_val);
  ks = calloc(NST, sizeof *ks);
  if (!pages_f || !index_f || !ph_key || !ph_val || !ks) return -1;
  ent_on = 1;
  return 1;
}
#endif
