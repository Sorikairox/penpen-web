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
int hwt_synth(const char *spec) { (void)spec; return 2; }
int hwt_synth_odd, hwt_synth_hw;
void hwt_synth_odd_note(const char *kind, uint32_t a) { (void)kind; (void)a; hwt_synth_odd++; }
int hwt_synth_live;
int hwt_corpus(const char *opt) { (void)opt; return -1; }
void hwt_summary(void) {}
int hwt_set(const char *opt) { (void)opt; return -1; }
#else
#include <setjmp.h>
#include <signal.h>
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
#define NST 8192 /* S17: the synthetic samples test every hand-written function in one run */
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
#define CX_N 9
#define CALLS_MAX 8192
/* S17: cx: what a draw call's HLE sees at the moment of the call besides its registers (the draw context, ctx_of) */
typedef struct { call_ent call[CALLS_MAX]; uint32_t cx[CALLS_MAX][CX_N]; int n, diverged, io_reads; uint32_t io_hash, io_n; } side_log;
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

/* S17: the draw context of an HLE draw call. The difftest compared a draw call by its registers only, so what it draws
 * from memory was seen only in the RAM at the sample's end: a sprite whose position is set, drawn, then set again for
 * the next digit, or a matrix pushed, moved, drawn with and popped, left no trace (Mull survivors in race_draw.c,
 * time_draw). For the Ninja draw functions, cx holds at the call: [0] the current matrix (njInitMatrix's stack top,
 * 48 bytes; the 3D draws only) and the render state the HLE reads (njControl3D, the constant attribute / material, the texture list),
 * [1..4] 32 bytes at r4..r7 when that register points into RAM outside the stack (the sprite, the model, the
 * vertices), [5] FR4 for the draws that read it (the priority, the motion frame). Compared per call like the
 * arguments (the pointer words only for the registers the call uses). */
static int draw_kind(uint32_t a) {
  static uint32_t ca[64], ck[64];
  static int cn;
  for (int i = 0; i < cn; i++) if (ca[i] == a) return (int)ck[i];
  const char *n = hle_name(a);
  int k = 0;
  if (n && (!strncmp(n, "njDraw", 6) || !strncmp(n, "nj_Model", 8) || !strncmp(n, "nj_Object", 9) ||
            !strncmp(n, "nj_Motion", 9) || !strncmp(n, "nj_ShapeMotion", 14) || !strcmp(n, "njCnkModDrawModel")))
    /* (njDrawPolygon takes no float: Ninja's is (points, n, trans); the HLE reads FR4 as its depth, whatever the
       caller left there: not compared) */
    k = 1 | (!strcmp(n, "njDrawSprite2D") || !strncmp(n, "nj_Motion", 9) ||
             !strncmp(n, "nj_ShapeMotion", 14) ? 2 : 0) | (strncmp(n, "njDraw", 6) || !strcmp(n, "njDrawSprite3D") ? 4 : 0);
  if (cn < 64) { ca[cn] = a; ck[cn] = (uint32_t)k; cn++; }
  return k;
}
static uint32_t mem_hash(uint32_t h, uint32_t a, uint32_t n) {
  for (uint32_t i = 0; i < n; i += 4) h = hsh(h ^ rd32(a + i));
  return h;
}
static int in_stack(uint32_t w) { uint32_t p = w & 0x1FFFFFFFu; return p >= 0x0C00C000u && p < 0x0C00F400u; }
static void ctx_of(SH4 *c, uint32_t a, uint32_t *cx) {
  memset(cx, 0, sizeof(uint32_t) * CX_N);
  int k = draw_kind(a);
  if (!k) return;
  uint32_t m = rd32(0x8C1EF2B8u); /* NJG_CURMATRIX: the 3D draws only (a 2D draw does not read it) */
  cx[0] = !(k & 4) ? 0x50u : sh4_is_ram(m) ? mem_hash(0x51u, m, 48) | 1 : 0x52u;
  cx[6] = rd32(0x8C1EF2ECu);             /* njControl3D */
  cx[7] = mem_hash(0x53u, 0x8C1EF320u, 24); /* constant attribute and-/or-masks, constant material */
  cx[8] = stack_token(rd32(0x8C1823F0u)); /* the texture list (one on the stack: a token, DECOMP.md 6) */
  for (int i = 0; i < 4; i++) {
    uint32_t p = c->r[4 + i];
    cx[1 + i] = sh4_is_ram(p) && !in_stack(p) && (p & 0xFFFFFF) + 32 <= (16u << 20) ? mem_hash(0x77u, p & ~3u, 32) | 1 : 0;
  }
  cx[5] = k & 2 ? c->fr.u[4] : 0;
}

/* an HLE call in the sandbox: logged, then answered from the live record */
static void sbx_hle(SH4 *c, uint32_t a, int arity, int fargs) {
  int k = slog->n++;
  if (k < CALLS_MAX) {
    call_ent *e = &slog->call[k];
    e->t = a; e->arity = arity < 0 ? 4 : (uint32_t)arity; e->fargs = (uint32_t)fargs;
    for (int i = 0; i < 4; i++) { e->a[i] = c->r[4 + i]; e->a[4 + i] = c->fr.u[4 + i]; }
    ctx_of(c, a, slog->cx[k]);
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
    /* S17: as rt.c's interp_call: the rest of a function Ghidra cut short, entered with the PR of its last call, returns
       to hw_call's marker when the original's caller is hand-written (side B only: side A runs no hand-written code) */
    if (c->last == SH4_BR_RET && c->pc == 0x8C000F08u) return END_RET;
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

int hwt_synth_sideb; /* S17: a synthetic sample's side B runs (a host fault there is the sample's FAIL, synth.c) */
static int run_side_b(SH4 *b) {
  rt_fn f = rt_lookup(cur->addr);
  volatile int code = sigsetjmp(sbx_jb, 1);
  if (code == 0) { f(b); hwt_synth_sideb = 0; return END_RET; }
  hwt_synth_sideb = 0;
  return code;
}

/* side B (the hand-written function, from the snapshot buf_s / snap) and the comparison with side A (a, ea, at_a,
 * log_a, buf_a). Returns 1 when the sample passes. S16: shared by the live difftest and the offline replay. */
static SH4 side_a_cpu, side_b_cpu;
static int cx_a_known = 1; /* S17: side A's draw contexts are known (live, or a sample with them) */
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
    /* S17: the draw context (ctx_of); a sample recorded before S17 has none for side A (cx_a_known 0) */
    if (cx_a_known) {
      const uint32_t *p = log_a.cx[k], *q = log_b.cx[k];
      if (p[0] != q[0]) { fail("hle draw %x matrix %x", x->t, p[0] ^ q[0], 0); return 0; }
      for (uint32_t i = 6; i < CX_N; i++)
        if (p[i] != q[i]) { fail("hle call %x (draw) render state %x: %x", (uint32_t)k, i, p[i] ^ q[i]); return 0; }
      for (uint32_t i = 0; i < 4; i++)
        if ((used >> (4 + i) & 1) && p[1 + i] != q[1 + i]) { fail("hle draw %x memory at arg %x %x", x->t, i, p[1 + i] ^ q[1 + i]); return 0; }
      if ((draw_kind(x->t) & 2) && p[5] != q[5]) { fail("hle draw %x fr4 %x", x->t, p[5] ^ q[5], 0); return 0; }
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
static int synth_keep; /* S17: the synthetic run chooses its samples itself (by new coverage of the original) */

/* side A from the snapshot (the recorded HLE results replayed), then side B and the comparison; a passing sample goes
 * to the corpus. Returns 1 pass, 0 FAIL, -1 not judged (too long). S17: shared by the live difftest and the synthetic
 * samples (hwt_synth) */
static int sample_judge(void);
void hwt_end(int sample) {
  if (sample != cur_sample || !hwt_recording) return; /* given up meanwhile */
  hwt_recording = 0;
  cur_sample = 0;
  sample_judge();
}
static int sample_judge(void) {
  if (rec_n > REC_MAX) { cur->longrun++; return -1; }
  cx_a_known = 1; /* side A runs here: its draw contexts are computed */
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
  if (ea == END_BUDGET) { cur->longrun++; cur->samples--; return -1; }
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
  return passed;
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
  if (!synth_keep && k->kept && seen && (hsh((uint32_t)sample_ids ^ 0xC0FFEEu) & 7 || (int)k->kept >= corpus_k - 1)) return;
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
                      (uint32_t)ent_n, cost, sig, 1 /* S17: the draw contexts follow */};
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
  wr(f, log_a.cx, sizeof log_a.cx[0] * (size_t)(log_a.n < CALLS_MAX ? log_a.n : CALLS_MAX));
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
  /* S17: the draw contexts (hdr[15] bit 0), after the list of entered functions */
  cx_a_known = ok && (hdr[15] & 1);
  if (cx_a_known) {
    static uint32_t skip_ent[ENT_MAX];
    ok = rd(f, skip_ent, sizeof skip_ent[0] * hdr[12]) &&
         rd(f, log_a.cx, sizeof log_a.cx[0] * (size_t)(log_a.n < CALLS_MAX ? log_a.n : CALLS_MAX));
  }
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
    if (getenv("PENPEN_REPLAY_DUMP")) { /* S17: ADDR:N words of the snapshot (debugging a sample) */
      uint32_t a0 = (uint32_t)strtoul(getenv("PENPEN_REPLAY_DUMP"), 0, 16), nw = 8;
      const char *c2 = strchr(getenv("PENPEN_REPLAY_DUMP"), ':');
      if (c2) nw = (uint32_t)strtoul(c2 + 1, 0, 10);
      for (uint32_t i = 0; i < nw; i++) { uint32_t w; memcpy(&w, buf_s + ((a0 + 4 * i) & 0xFFFFFC), 4); rt_log("hw replay dump %08x %08x", a0 + 4 * i, w); }
      rt_log("hw replay dump r4-r7 %08x %08x %08x %08x pr %08x", snap.r[4], snap.r[5], snap.r[6], snap.r[7], snap.pr);
    }
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

/* ---- S17: synthetic samples (--hwsynth SPEC with --hwcorpus DIR:K; docs/port/DECOMP.md 18) ----
 * The routes reach 1 640 of the game's 4 010 functions; a hand-written function no route runs has no live sample.
 * A synthetic sample is made the way a live one is, from a real state that the game did not give that function:
 *   1. a base: the snapshot (CPU + guest RAM) of a corpus sample (a race frame, a menu frame, ...);
 *   2. optional prelude calls on it (the ORIGINAL code, interpreted, the HLE live): e.g. the actor factory builds an
 *      object of the class under test, then its init method runs on a placement;
 *   3. the arguments: from the spec's candidates, else from the base's own objects: its registers, the pointers on its
 *      stack (the callers' live objects), the objects whose vtable holds the function (its class), small integers;
 *      the input may also poke words of an argument's object or of a global the function's literal pool names;
 *   4. the recording pass: the ORIGINAL code, interpreted from that state, with the HLE live: its results are recorded
 *      exactly as for a live sample (the HLE's r0 / stack out-parameters / RAM it filled, the hardware reads);
 *   5. then the live difftest's own judgement (sample_judge): side A (the original, interpreted, HLE replayed) and side
 *      B (the hand-written function, HLE replayed) from the snapshot, compared; a passing sample joins the corpus.
 * The inputs are chosen by a small coverage-guided search: an input is kept when its recording pass ran an instruction
 * of the function's original code that no kept input ran (an AFL-like queue of kept inputs is mutated: one argument,
 * one poke). What it proves: the hand-written function equals the original on these states, which are real objects in
 * real scenes but not necessarily states the game ever reaches with this call (a poke may make a combination the game
 * never makes). What it does not: that the game calls it this way; a frame's end (njWaitVSync) inside the call refuses
 * the input. The samples are ordinary corpus samples: --hwreplay and the Mull gate use them like live ones. */
int hwt_synth_live;
#define SY_BASES 12
#define SY_PRE 6
#define SY_CAND 64
#define SY_POOL 2048
#define SY_POKES 6
#define SY_QUEUE 64
typedef struct { int base; uint32_t a[4], f[4]; int np; uint32_t pa[SY_POKES], pv[SY_POKES]; } sy_input;
static struct {
  uint32_t fn, end, insn, iters, keep, seed, steps, stale, secs; /* S17: stale / secs: 0 = 600 / 20 */
  int nopoke;                      /* `nopoke`: the arguments only (the objects they point to stay as the base has them) */
  int nbase;
  char base[SY_BASES][512];
  int npre;
  uint32_t pre[SY_PRE][5];         /* function, 4 arguments (a value; see sy_value) */
  int ncand[4], nfcand[4];
  uint32_t cand[4][SY_CAND], fcand[4][SY_CAND];
} sj;
static uint8_t *sy_prep[SY_BASES];     /* each base after its prelude */
static SH4 sy_cpu[SY_BASES];
static uint32_t sy_preret[SY_BASES][SY_PRE];
static uint32_t sy_pool[SY_BASES][SY_POOL], sy_npool[SY_BASES];
static uint32_t sy_cls[SY_BASES][256], sy_ncls[SY_BASES]; /* objects of the function's class (by its vtable slot) */
static uint32_t sy_glob[64], sy_nglob;                      /* globals the function's literal pool names */
static uint32_t sy_offs[128], sy_noffs;   /* field offsets the function's instructions use (displacements, immediates) */
static uint32_t sy_consts[64], sy_nconsts; /* small constants it compares with or loads (cmp/eq #imm, mov #imm) */
int hwt_synth_hw;  /* the job's `hw` line: the function may write hardware registers */
void hwt_synth_odd_note(const char *kind, uint32_t a) {
  if (!hwt_synth_odd++ && getenv("PENPEN_SYNTH_DEBUG")) rt_log("hw synth odd %s %08x", kind, a);
}
int hwt_synth_odd; /* the recording pass touched memory no real pointer does (rt.c): the input is refused */
static uint8_t *sy_cov, *sy_cov_it;
static uint64_t sy_rs;
static uint32_t sy_rnd(void) { sy_rs ^= sy_rs << 13; sy_rs ^= sy_rs >> 7; sy_rs ^= sy_rs << 17; return (uint32_t)(sy_rs >> 11); }

/* a spec value: hex, or $k / $k+hex / $k-hex = the k-th prelude call's result (plus an offset) of the base */
static uint32_t sy_value(const char *t, int base);
/* [X] (S17): the word at address X of the base after its prelude, e.g. [[8c096230]+4c]: the race scene's player 0 */
static uint32_t sy_expr(const char **pp, int base) {
  const char *p = *pp;
  uint32_t v;
  char *e;
  if (*p == '[') {
    p++;
    uint32_t a = sy_expr(&p, base);
    if (*p == ']') p++;
    v = 0;
    if (sh4_is_ram(a) && sy_prep[base]) memcpy(&v, sy_prep[base] + (a & 0xFFFFFC), 4);
  } else if (*p == '$') {
    int k = (int)strtol(p + 1, &e, 10);
    v = k >= 0 && k < SY_PRE ? sy_preret[base][k] : 0;
    p = e;
  } else {
    v = (uint32_t)strtoul(p, &e, 16);
    p = e;
  }
  while (*p == '+' || *p == '-') {
    int neg = *p == '-';
    uint32_t o = (uint32_t)strtoul(p + 1, &e, 16);
    v = neg ? v - o : v + o;
    p = e;
  }
  *pp = p;
  return v;
}
static uint32_t sy_value(const char *t, int base) {
  if (t[0] == '[') return sy_expr(&t, base);
  if (t[0] == '$') {
    char *e;
    int k = (int)strtol(t + 1, &e, 10);
    uint32_t v = k >= 0 && k < SY_PRE ? sy_preret[base][k] : 0;
    if (*e == '+') v += (uint32_t)strtoul(e + 1, 0, 16);
    if (*e == '-') v -= (uint32_t)strtoul(e + 1, 0, 16);
    return v;
  }
  return (uint32_t)strtoul(t, 0, 16);
}
static char sy_ctext[4][SY_CAND][24], sy_pretext[SY_PRE][4][24];

/* the recording pass: the original code interpreted from c->pc until it returns to ret with the stack back at sp; HLE
 * calls run live (recorded when hwt_recording is set: rt_call's hooks); the function's instructions run are marked */
static int sy_run(SH4 *c, uint32_t ret, uint32_t sp, int64_t steps, uint8_t *cov) {
  while (steps-- > 0) {
    uint32_t pc = RT_NORM(c->pc);
    if (cov && pc >= sj.fn && pc < sj.end) cov[(pc - sj.fn) >> 1] = 1;
    { /* code outside the code ranges (a call through a vtable word that is no method): no game state does that */
      uint32_t k = pc & 0x1FFFFFFFu;
      if (!((k >= 0x0C010000u && k < 0x0C066900u) || (k >= 0x0C0998D4u && k < 0x0C17994Cu) || (k >= 0x0C18F112u && k < 0x0C197090u))) {
        hwt_synth_odd_note("pc", pc);
        return END_STOP;
      }
    }
    int r = sh4_step(c);
    if (r) { sbx_at = c->fault; return r == -1 ? END_ILLEGAL : r == -3 ? END_SLOT : END_UNSUP; }
    if (c->last == SH4_BR_NONE) continue;
    /* a delayed branch ran its slot in the same step (bt / bf have none: their next instruction is marked when it runs) */
    if (cov && pc + 2 >= sj.fn && pc + 2 < sj.end && RT_NORM(c->pc) != pc + 2) {
      uint16_t op; memcpy(&op, sh4_ram + (pc & 0xFFFFFF), 2);
      if ((op & 0xFD00) != 0x8900) cov[(pc + 2 - sj.fn) >> 1] = 1; /* not bt / bf (0x89xx / 0x8Bxx) */
    }
    if (c->last == SH4_BR_RET && c->pc == ret && c->r[15] >= sp) return END_RET;
    uint32_t t = RT_NORM(c->pc);
    if (c->last != SH4_BR_RET && hle_known(t)) {
      const char *nm = hle_name(t);
      if (nm && !strcmp(nm, "njWaitVSync")) return END_STOP; /* the frame would end inside the call */
      int last = (int)c->last;
      rt_call(c, t);
      if (rt_unw) { rt_unw = 0; return END_STOP; }
      c->last = (uint32_t)last;
      if (last != SH4_BR_CALL && c->pc == ret && c->r[15] >= sp) return END_RET;
    }
  }
  sbx_at = c->pc;
  return END_BUDGET;
}
static void sy_alarm(int sig) { (void)sig; if (hwt_synth_live) siglongjmp(sbx_jb, END_STOP); }
/* a host fault inside the recording pass (an HLE function indexing its own tables with a value no real call gives):
   the input is refused; outside it, the runtime's crash report (main.c) */
static struct sigaction sy_old_segv, sy_old_bus;
static int sy_faults, sy_bfaults;
static void sy_fault(int sig) {
  if (hwt_synth_live) { sy_faults++; siglongjmp(sbx_jb, END_STOP); }
  if (hwt_synth_sideb) { sy_bfaults++; siglongjmp(sbx_jb, END_ILLEGAL + 100); } /* side B's host fault (sy_job) */
  sigaction(sig, sig == SIGSEGV ? &sy_old_segv : &sy_old_bus, 0);
  raise(sig);
}
#define SY_RET 0x8C000F04u /* the return address the synthetic call gets: never executed */
/* one guarded run of the original code on ram (rt_stop and HLE trouble end it); returns the end code */
static int sy_guarded(SH4 *c, uint8_t *ram, int record, uint8_t *cov, int64_t steps) {
  uint8_t *save = sh4_ram;
  sh4_ram = ram;
  if (record) {
    rec_n = 0; stk_n = 0; iorec_n = 0; memrec_n = mempool_n = mem_call_first = 0; mem_lost = 0;
    rec_depth = rt_hle_depth;
    hwt_recording = 1;
  }
  hwt_synth_live = 1;
  int depth = rt_hle_depth;
  /* an HLE function given a state no real call gives may loop for ever (a count read through the wrong object): a
     2 s alarm ends the pass (sigsetjmp: the handler's longjmp restores the signal mask) */
  volatile int code = sigsetjmp(sbx_jb, 1);
  if (code == 0) {
    static int installed;
    if (!installed) {
      struct sigaction sa;
      memset(&sa, 0, sizeof sa);
      sa.sa_handler = sy_fault;
      sa.sa_flags = SA_NODEFER;
      sigaction(SIGSEGV, &sa, &sy_old_segv);
      sigaction(SIGBUS, &sa, &sy_old_bus);
      installed = 1;
    }
    signal(SIGALRM, sy_alarm);
    alarm(2);
    code = sy_run(c, SY_RET, c->r[15], steps, cov);
  }
  alarm(0);
  rt_hle_depth = depth;
  hwt_synth_live = 0;
  hwt_recording = 0;
  rt_sandbox = 0;
  sh4_ram = save;
  return code;
}
/* the code ranges (game, SDK, SDK code in the data area: port/recomp/diff_main.c): no object lives there, and a poke
   there would change the code side A interprets but not the code side B runs translated */
static int sy_is_code(uint32_t w) {
  uint32_t k = w & 0x00FFFFFFu;
  return (k >= 0x010000u && k < 0x066900u) || (k >= 0x0998D4u && k < 0x17994Cu) || (k >= 0x18F112u && k < 0x197090u);
}
static int sy_is_ram(uint32_t w) { return (w >> 24) == 0x8C && (w & 0xFFFFFF) >= 0x10000 && !sy_is_code(w); }
static void sy_pool_add(int b, uint32_t v) {
  if (sy_npool[b] >= SY_POOL) return;
  for (uint32_t i = 0; i < sy_npool[b]; i++) if (sy_pool[b][i] == v) return;
  sy_pool[b][sy_npool[b]++] = v;
}
/* a base: its snapshot, the prelude, the pools */
static int sy_base(int b) {
  int ea; uint32_t at;
  if (replay_load(sj.base[b], &ea, &at)) { rt_log("hw synth base unreadable %s", sj.base[b]); return -1; }
  if (!sy_prep[b] && !(sy_prep[b] = malloc(16u << 20))) return -1;
  memcpy(sy_prep[b], buf_s, 16u << 20);
  SH4 c = snap;
  for (int k = 0; k < sj.npre; k++) {
    SH4 p = c;
    p.pc = sj.pre[k][0]; p.pr = SY_RET;
    for (int i = 0; i < 4; i++) p.r[4 + i] = sy_value(sy_pretext[k][i], b);
    int e = sy_guarded(&p, sy_prep[b], 0, 0, 50000000);
    if (e != END_RET) { rt_log("hw synth prelude %d (%08x) of base %d ended %d at %08x", k, sj.pre[k][0], b, e, sbx_at); return -1; }
    sy_preret[b][k] = p.r[0];
  }
  sy_cpu[b] = c;
  sy_npool[b] = 0;
  for (int i = 0; i < 15; i++) if (sy_is_ram(c.r[i])) sy_pool_add(b, c.r[i]);
  for (int k = 0; k < sj.npre; k++) if (sy_is_ram(sy_preret[b][k])) sy_pool_add(b, sy_preret[b][k]);
  for (uint32_t a = c.r[15] & 0xFFFFFC; a < 0x00F400 && a >= 0x00C000; a += 4) {
    uint32_t w; memcpy(&w, sy_prep[b] + a, 4);
    if (sy_is_ram(w) && (w & 0xFFFFFF) >= 0x0A0000) sy_pool_add(b, w & ~3u);
  }
  /* the function's class: words of RAM holding its address are vtable slots {fn, adjust}; objects point (at their
     vtable field) to the start of such a vtable, up to 16 slots before; `this` = the field - its offset + adjust */
  sy_ncls[b] = 0;
  const uint32_t *w = (const uint32_t *)sy_prep[b];
  for (uint32_t i = 0x10000 / 4; i + 1 < (16u << 20) / 4 && sy_ncls[b] < 256; i++) {
    if (w[i] != sj.fn) continue;
    uint32_t slot = 0x8C000000u | (i * 4), adj = w[i + 1];
    if (adj > 0x1000 && adj < 0xFFFFF000u) continue;
    for (uint32_t s = 0; s < 16 && sy_ncls[b] < 256; s++) {
      uint32_t vt = slot - 8 * s;
      for (uint32_t j = 0x10000 / 4; j < (16u << 20) / 4 && sy_ncls[b] < 256; j++) {
        if (w[j] != vt) continue;
        uint32_t field = 0x8C000000u | (j * 4);
        static const uint32_t offs[] = {0x20, 0x24, 0x00, 0x04, 0x28, 0x2C, 0x30, 0x1C};
        for (int o = 0; o < 8 && sy_ncls[b] < 256; o++) sy_cls[b][sy_ncls[b]++] = field - offs[o] + adj;
      }
    }
  }
  return 0;
}
static const uint32_t sy_small[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0xFFFFFFFFu, 0x10, 0x20, 0xFF, 0x100, 9, 10, 12, 16};
static const float sy_floats[] = {0.0f, 1.0f, -1.0f, 0.5f, 2.0f, 10.0f, 100.0f, -100.0f, 1000.0f, 0.1f, 3.0f, 60.0f};
/* a value that would be a pointer into RAM only through a mirror (0x0D-0x0F, 0x8E...) or unaligned: no real object is
   there (the interpreter masks the low bits where the SH-4 would raise an address error): a small number instead */
static uint32_t sy_sane(uint32_t v) {
  if (sh4_is_ram(v) && ((v & 3) || ((v >> 24) & 0x1F) != 0x0C || sy_is_code(v))) return v & 0xFF;
  if (v >= 0xE0000000u && v < 0xFFFF0000u) return v & 0xFF; /* the SH-4's own area: no object either */
  return v;
}
static uint32_t sy_pick_arg_(int b, int i);
static uint32_t sy_pick_arg(int b, int i) { return sy_sane(sy_pick_arg_(b, i)); }
static uint32_t sy_pick_arg_(int b, int i) {
  if (sj.ncand[i]) return sy_value(sy_ctext[i][sy_rnd() % (uint32_t)sj.ncand[i]], b); /* the spec's candidates only */
  uint32_t k = sy_rnd() % 16;
  if (k < 5 && sy_ncls[b] && (i == 0 || k < 2)) return sy_cls[b][sy_rnd() % sy_ncls[b]];
  if (k < 10 && sy_npool[b]) return sy_pool[b][sy_rnd() % sy_npool[b]];
  if (k < 11) return sy_small[sy_rnd() % (sizeof sy_small / 4)];
  if (k < 12) return sy_rnd() & 1 ? sy_rnd() & 0x7FFFFFFFu : sy_rnd() & 0xFFFF; /* a number of any size (dates) */
  if (k < 13 && sy_nconsts) return sy_consts[sy_rnd() % sy_nconsts];
  return sy_cpu[b].r[4 + i];
}
static uint32_t sy_pick_farg(int b, int i) {
  if (sj.nfcand[i]) return sj.fcand[i][sy_rnd() % (uint32_t)sj.nfcand[i]];
  if (sy_rnd() & 1) return sy_cpu[b].fr.u[4 + i];
  float f = sy_floats[sy_rnd() % (sizeof sy_floats / 4)];
  uint32_t u; memcpy(&u, &f, 4);
  return u;
}
static void sy_fresh(sy_input *in, const int *ok) {
  do in->base = (int)(sy_rnd() % (uint32_t)sj.nbase); while (!ok[in->base]);
  for (int i = 0; i < 4; i++) { in->a[i] = sy_pick_arg(in->base, i); in->f[i] = sy_pick_farg(in->base, i); }
  in->np = 0;
}
static void sy_mutate(sy_input *in) {
  int n = 1 + (int)(sy_rnd() % 2);
  while (n--) {
    uint32_t k = sy_rnd() % 8;
    if (k < 2) { int i = (int)(sy_rnd() % 4); in->a[i] = sy_pick_arg(in->base, i); }
    else if (k < 3) { int i = (int)(sy_rnd() % 4); in->f[i] = sy_pick_farg(in->base, i); }
    else if (k < 7 && in->np < SY_POKES && !sj.nopoke) {
      /* a poke: a word of an argument's object, or a global the function names */
      uint32_t at;
      if (sy_nglob && (sy_rnd() & 1)) at = sy_glob[sy_rnd() % sy_nglob] + 4 * (sy_rnd() % 4);
      else {
        uint32_t p = in->a[sy_rnd() % 4];
        if (!sy_is_ram(p)) p = in->a[0];
        if (!sy_is_ram(p)) continue;
        at = (p & ~3u) + (sy_noffs && sy_rnd() % 4 ? sy_offs[sy_rnd() % sy_noffs] : 4 * (sy_rnd() % (sy_rnd() & 1 ? 32 : 128)));
      }
      if (sy_is_code(at)) continue;
      uint32_t old;
      memcpy(&old, sy_prep[in->base] + (at & 0xFFFFFC), 4);
      uint32_t v, r = sy_rnd() % 10;
      if (r < 2) v = sy_small[sy_rnd() % (sizeof sy_small / 4)];
      else if (r < 4) v = sy_nconsts ? sy_consts[sy_rnd() % sy_nconsts] : 0;
      else if (r < 5) v = old ^ (1u << (sy_rnd() % 32));
      else if (r < 6) v = old + 1;
      else if (r < 7) v = old - 1;
      else if (r < 8 && sy_npool[in->base]) v = sy_pool[in->base][sy_rnd() % sy_npool[in->base]];
      else { float f = sy_floats[sy_rnd() % (sizeof sy_floats / 4)]; memcpy(&v, &f, 4); }
      in->pa[in->np] = at; in->pv[in->np] = sy_sane(v); in->np++;
    } else if (in->np) in->np--;
  }
}
/* the snapshot of an input: the base after its prelude, its pokes; the CPU with the arguments */
static void sy_apply(const sy_input *in, uint8_t *ram, SH4 *c) {
  memcpy(ram, sy_prep[in->base], 16u << 20);
  for (int i = 0; i < in->np; i++) memcpy(ram + (in->pa[i] & 0xFFFFFC), &in->pv[i], 4);
  *c = sy_cpu[in->base];
  for (int i = 0; i < 4; i++) { c->r[4 + i] = in->a[i]; c->fr.u[4 + i] = in->f[i]; }
  c->pc = sj.fn;
  c->pr = SY_RET;
}
static void sy_describe(const sy_input *in, char *o, size_t n) {
  int k = snprintf(o, n, "base %d args %08x %08x %08x %08x f %08x %08x %08x %08x pokes", in->base, in->a[0], in->a[1],
                   in->a[2], in->a[3], in->f[0], in->f[1], in->f[2], in->f[3]);
  for (int i = 0; i < in->np && k < (int)n - 20; i++) k += snprintf(o + k, n - (size_t)k, " %08x=%08x", in->pa[i], in->pv[i]);
}
static FILE *sy_log, *sy_fail_f;
static void sy_job(void) {
  static sy_input queue[SY_QUEUE];
  int nq = 0, ok[SY_BASES] = {0}, nok = 0;
  uint32_t nh = (sj.end - sj.fn) / 2 + 1;
  sy_cov = calloc(nh, 1); sy_cov_it = calloc(nh, 1);
  sy_rs = 0x9E3779B97F4A7C15ull ^ ((uint64_t)sj.fn << 20) ^ sj.seed;
  for (int i = 0; i < 4; i++) sy_rnd();
  /* the globals the function's literal pool names (data words in its extent pointing into RAM outside the code) */
  sy_nglob = 0;
  for (uint32_t a = (sj.fn + 3) & ~3u; a + 4 <= sj.end && sy_nglob < 64; a += 4) {
    uint32_t w; memcpy(&w, sh4_ram + (a & 0xFFFFFF), 4);
    if (sy_is_ram(w) && (w & 0xFFFFFF) >= 0x057000) sy_glob[sy_nglob++] = w & ~3u;
  }
  /* the field offsets and constants of its instructions (a literal pool decodes as noise: harmless) */
  sy_noffs = sy_nconsts = 0;
#define SY_ADD(arr, n, max, v) do { uint32_t v_ = (v); int s_ = 0; for (uint32_t q_ = 0; q_ < n; q_++) s_ |= arr[q_] == v_; if (!s_ && n < max) arr[n++] = v_; } while (0)
  for (uint32_t a = sj.fn; a + 2 <= sj.end; a += 2) {
    uint16_t op; memcpy(&op, sh4_ram + (a & 0xFFFFFF), 2);
    uint32_t hi = op >> 12, d4 = op & 15, i8 = op & 0xFF;
    if (hi == 5 || hi == 1) SY_ADD(sy_offs, sy_noffs, 128, d4 * 4);                       /* mov.l @(disp,Rm) / Rm,@(disp,Rn) */
    else if ((op & 0xFE00) == 0x8400 || (op & 0xFE00) == 0x8000) SY_ADD(sy_offs, sy_noffs, 128, (d4 << ((op >> 8) & 1)) & ~3u);
    else if (hi == 0xE) { int32_t v = (int8_t)i8; if (v > 0) SY_ADD(sy_offs, sy_noffs, 128, (uint32_t)v & ~3u); SY_ADD(sy_consts, sy_nconsts, 64, (uint32_t)v); }
    else if (hi == 9) { uint16_t lw; memcpy(&lw, sh4_ram + ((a + 4 + i8 * 2) & 0xFFFFFF), 2);
      if (lw < 0x2000) SY_ADD(sy_offs, sy_noffs, 128, lw & ~3u);
      SY_ADD(sy_consts, sy_nconsts, 64, (uint32_t)(int32_t)(int16_t)lw); }
    else if ((op & 0xFF00) == 0x8800) SY_ADD(sy_consts, sy_nconsts, 64, (uint32_t)(int32_t)(int8_t)i8); /* cmp/eq #imm,r0 */
    else if ((op & 0xFF00) == 0xC800) SY_ADD(sy_consts, sy_nconsts, 64, i8);                           /* tst #imm,r0 */
  }
#undef SY_ADD
  for (int b = 0; b < sj.nbase; b++) if (!sy_base(b)) { ok[b] = 1; nok++; }
  cur = stat_of(sj.fn);
  uint32_t kept = 0, pass = 0, failn = 0, rets = 0, covered = 0, judged = 0, odd = 0, domain = 0;
  int64_t steps = sj.steps ? sj.steps : 2000000;
  char why0[128] = "";
  /* S17: a job ends after ITERS inputs, KEEP kept samples, 600 inputs in a row that ran no new code (after the first
     kept one), or 20 s */
  struct timespec tj0, tj;
  clock_gettime(CLOCK_MONOTONIC, &tj0);
  uint32_t stale = 0;
  for (uint32_t it = 0; nok && cur && it < sj.iters && kept < sj.keep; it++) {
    if (kept && ++stale > (sj.stale ? sj.stale : 600)) break;
    if ((it & 15) == 15) {
      clock_gettime(CLOCK_MONOTONIC, &tj);
      if (tj.tv_sec - tj0.tv_sec > (sj.secs ? (long)sj.secs : 20)) break;
    }
    sy_input in;
    if (nq && sy_rnd() % 4) { in = queue[sy_rnd() % (uint32_t)nq]; sy_mutate(&in); }
    else { sy_fresh(&in, ok); if (sy_rnd() & 1) sy_mutate(&in); }
    SH4 c;
    sy_apply(&in, buf_a, &c);
    memset(sy_cov_it, 0, nh);
    hwt_synth_odd = 0;
    int e = sy_guarded(&c, buf_a, 1, sy_cov_it, steps);
    if (e != END_RET) continue;
    if (hwt_synth_odd) { odd++; continue; }
    rets++;
    int fresh = 0;
    for (uint32_t i = 0; i < nh; i++) if (sy_cov_it[i] && !sy_cov[i]) fresh++;
    if (!fresh) continue;
    /* new code of the original ran: judge this input like a live sample */
    sy_apply(&in, buf_s, &snap);
    cur->why[0] = 0;
    cur_frame = 0;
    synth_keep = 1;
    hwt_synth_sideb = 1; /* cleared by run_side_b; only side B's faults are caught */
    int bf0 = sy_bfaults;
    int v = sample_judge();
    hwt_synth_sideb = 0;
    /* side B left guest RAM (a host fault): the input took an index or a pointer the hand-written code does not take
       (C indexing past the 16 MiB where the original's 32-bit address arithmetic lands somewhere): outside its domain,
       refused and counted, not a FAIL (the original would write a stray word there: no game input does) */
    if (v == 0 && sy_bfaults != bf0) { domain++; cur->fail--; continue; }
    synth_keep = 0;
    judged++;
    char d[400];
    sy_describe(&in, d, sizeof d);
    if (v == 1) {
      for (uint32_t i = 0; i < nh; i++) if (sy_cov_it[i] && !sy_cov[i]) { sy_cov[i] = 1; covered++; }
      if (nq < SY_QUEUE) queue[nq++] = in;
      else queue[sy_rnd() % SY_QUEUE] = in;
      kept++; pass++; stale = 0;
      if (sy_log) fprintf(sy_log, "%08x\tpass\t%s\t%s\n", sj.fn, sj.base[in.base], d);
    } else if (v == 0) {
      /* the FAIL sample is kept too, listed in DIR/fail.tsv (not index.tsv: the replay gate never selects it), to be
         replayed with PENPEN_REPLAY_DIFFS */
      if (!sy_fail_f) { char fp[600]; snprintf(fp, sizeof fp, "%s/fail.tsv", corpus_dir); sy_fail_f = fopen(fp, "a"); }
      if (sy_fail_f) {
        FILE *keep = index_f;
        index_f = sy_fail_f;
        synth_keep = 1;
        keep_stat *ksf = keep_of(cur->addr);
        uint32_t kept0 = ksf ? ksf->kept : 0;
        corpus_keep(END_RET, 0, 0);
        if (ksf) ksf->kept = kept0;
        synth_keep = 0;
        index_f = keep;
      }
      failn++;
      if (!why0[0]) memcpy(why0, cur->why, sizeof why0);
      rt_log("hw synth FAIL %08x it %u: %s; %s", sj.fn, it, cur->why, d);
      if (sy_log) fprintf(sy_log, "%08x\tFAIL\t%s\t%s\t%s\n", sj.fn, sj.base[in.base], d, cur->why);
      if (failn >= 3) break;
    }
  }
  /* coverage in instruction bytes of the original (the extent's halfwords that ran, of insn bytes) */
  rt_log("hw synth %08x bases %d iters %u returned %u odd %u domain %u judged %u kept %u pass %u fail %u cover %u/%u%s%s",
         sj.fn, nok, sj.iters, rets, odd, domain, judged, kept, pass, failn, covered * 2, sj.insn, failn ? ": " : "", why0);
  if (sy_log) fflush(sy_log);
  free(sy_cov); free(sy_cov_it);
}
/* SPEC: jobs, each
 *   job FN END INSN_BYTES ITERS KEEP SEED [STEPS [STALE [SECS]]]
 *   base CORPUS_SAMPLE_PATH          (1-8)
 *   pre FN A0 A1 A2 A3               (0-6: prelude calls on each base; $k = the k-th call's result)
 *   arg I V V ...                    (candidates for argument I: hex, $k, $k+hex)
 *   farg I F F ...                   (float candidates)
 *   end */
int hwt_synth(const char *spec) {
  if (hwt_alloc() < 0 || !corpus_k) { rt_log("hw synth: needs --hwcorpus DIR:K"); return 2; }
  FILE *f = fopen(spec, "r");
  if (!f) { rt_log("hw synth: no spec"); return 2; }
  char path[600];
  snprintf(path, sizeof path, "%s/synth.tsv", corpus_dir);
  sy_log = fopen(path, "a");
  char line[4096];
  int njobs = 0;
  while (fgets(line, sizeof line, f)) {
    char *tok[80];
    int n = 0;
    for (char *p = strtok(line, " \t\r\n"); p && n < 80; p = strtok(0, " \t\r\n")) tok[n++] = p;
    if (!n || tok[0][0] == '#') continue;
    if (!strcmp(tok[0], "job") && n >= 7) {
      memset(&sj, 0, sizeof sj);
      hwt_synth_hw = 0;
      sj.fn = (uint32_t)strtoul(tok[1], 0, 16); sj.end = (uint32_t)strtoul(tok[2], 0, 16);
      sj.insn = (uint32_t)strtoul(tok[3], 0, 10); sj.iters = (uint32_t)strtoul(tok[4], 0, 10);
      sj.keep = (uint32_t)strtoul(tok[5], 0, 10); sj.seed = (uint32_t)strtoul(tok[6], 0, 10);
      if (n > 7) sj.steps = (uint32_t)strtoul(tok[7], 0, 10);
      if (n > 8) sj.stale = (uint32_t)strtoul(tok[8], 0, 10);
      if (n > 9) sj.secs = (uint32_t)strtoul(tok[9], 0, 10);
      sj.fn = RT_NORM(sj.fn); sj.end = RT_NORM(sj.end);
    } else if (!strcmp(tok[0], "nopoke")) {
      sj.nopoke = 1;
    } else if (!strcmp(tok[0], "hw")) {
      hwt_synth_hw = 1;
    } else if (!strcmp(tok[0], "base") && n == 2 && sj.nbase < SY_BASES) {
      snprintf(sj.base[sj.nbase++], sizeof sj.base[0], "%s", tok[1]);
    } else if (!strcmp(tok[0], "pre") && n == 6 && sj.npre < SY_PRE) {
      sj.pre[sj.npre][0] = (uint32_t)strtoul(tok[1], 0, 16);
      for (int i = 0; i < 4; i++) snprintf(sy_pretext[sj.npre][i], 24, "%s", tok[2 + i]);
      sj.npre++;
    } else if (!strcmp(tok[0], "arg") && n >= 3) {
      int i = atoi(tok[1]) & 3;
      for (int k = 2; k < n && sj.ncand[i] < SY_CAND; k++) snprintf(sy_ctext[i][sj.ncand[i]++], 24, "%s", tok[k]);
    } else if (!strcmp(tok[0], "farg") && n >= 3) {
      int i = atoi(tok[1]) & 3;
      for (int k = 2; k < n && sj.nfcand[i] < SY_CAND; k++) { float x = strtof(tok[k], 0); memcpy(&sj.fcand[i][sj.nfcand[i]++], &x, 4); }
    } else if (!strcmp(tok[0], "end")) {
      rt_log("hw synth job %d %08x", njobs, sj.fn);
      fflush(stdout);
      if (sj.fn && sj.nbase && rt_lookup(sj.fn)) sy_job();
      else rt_log("hw synth %08x skipped (no base or not a function of this build)", sj.fn);
      njobs++;
      fflush(stdout);
    }
  }
  fclose(f);
  if (sy_log) fclose(sy_log);
  rt_log("hw synth done %d jobs (host faults inside recording passes, inputs refused: %d; host faults of side B, inputs refused: %d)", njobs, sy_faults, sy_bfaults);
  return 0;
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
