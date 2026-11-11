/* Pen Pen port, S3: differential harness, interpreter vs recompiled C.
 *
 * For every translated function and several random seeds: build one random CPU state, run the function
 * once in the interpreter (port/sh4) and once as recompiled C, both against the same memory model, and
 * compare the final CPU state, every byte written, the calls made (target + arguments, in order) and the
 * way the run ended. Calls leave the function under test: both sides hand them to the same deterministic
 * stub, so a function is tested alone. Runs inside the sandbox only (it executes translated disc code).
 *
 * Memory model (SH4_MEM_HOOK): main RAM reads come from the loaded image (literal pools and data are real),
 * RAM outside the image and every other address read as pseudo-random bytes fixed per address; writes go
 * to a per-run overlay (read back by later reads) and are compared at the end; writes into the code
 * ranges are dropped (both sides then execute the same code).
 * Usage: diff --bin 1ST_READ.BIN [--trials N] [--steps N] [--only ADDR] [--quiet]
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "rt_gen.h"

#define LOAD 0x8C010000u
#define MAGIC_RET 0x8C00F0F0u /* returned-to address: not code */

uint8_t *sh4_ram; /* unused with SH4_MEM_HOOK, but sh4.h declares it */
static uint8_t *image;     /* 16 MiB: the binary at +0x10000, random bytes elsewhere */
static uint32_t img_end;

/* ---- write overlay: open addressing on physical byte addresses ---- */
#define OV_BITS 20
#define OV_SIZE (1u << OV_BITS)
typedef struct { uint32_t key[OV_SIZE]; uint8_t val[OV_SIZE]; uint32_t gen[OV_SIZE]; uint32_t cur; int n, full; uint32_t order[OV_SIZE]; } overlay;
static overlay ov_a, ov_b, *ov;

static inline uint32_t mkey(uint32_t a) { return sh4_is_ram(a) ? (0x0C000000u | (a & 0xFFFFFF)) : a; }
static inline uint32_t hsh(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
static uint8_t base_byte(uint32_t k) {
  if ((k & 0xFF000000u) == 0x0C000000u) return image[k & 0xFFFFFF];
  return (uint8_t)hsh(k ^ 0xA5A5A5A5u);
}
static int ov_find(uint32_t k, int insert) {
  uint32_t i = hsh(k) & (OV_SIZE - 1);
  for (;;) {
    if (ov->gen[i] != ov->cur) {
      if (!insert) return -1;
      if (ov->n >= (int)(OV_SIZE / 2)) { ov->full = 1; return -1; }
      ov->gen[i] = ov->cur; ov->key[i] = k; ov->order[ov->n++] = i;
      return (int)i;
    }
    if (ov->key[i] == k) return (int)i;
    i = (i + 1) & (OV_SIZE - 1);
  }
}
static uint8_t mr8(uint32_t a) { uint32_t k = mkey(a); int i = ov_find(k, 0); return i >= 0 ? ov->val[i] : base_byte(k); }
/* The code ranges are read-only in the model: a random pointer must not rewrite the code that the
 * interpreter fetches from memory (the recompiled side has it compiled in). The game has no
 * self-modifying code (SDK.md 6). */
static int is_code(uint32_t k) {
  return (k >= 0x0C010000u && k < 0x0C066900u) || (k >= 0x0C0998D4u && k < 0x0C17994Cu) ||
         (k >= 0x0C18F112u && k < 0x0C197090u);
}
static void mw8(uint32_t a, uint8_t v) { uint32_t k = mkey(a); if (is_code(k)) return; int i = ov_find(k, 1); if (i >= 0) ov->val[i] = v; }
uint8_t sh4_io_r8(uint32_t a) { return mr8(a); }
uint16_t sh4_io_r16(uint32_t a) { a &= ~1u; return (uint16_t)(mr8(a) | mr8(a + 1) << 8); }
uint32_t sh4_io_r32(uint32_t a) { a &= ~3u; return mr8(a) | mr8(a + 1) << 8 | mr8(a + 2) << 16 | (uint32_t)mr8(a + 3) << 24; }
void sh4_io_w8(uint32_t a, uint8_t v) { mw8(a, v); }
void sh4_io_w16(uint32_t a, uint16_t v) { a &= ~1u; mw8(a, (uint8_t)v); mw8(a + 1, (uint8_t)(v >> 8)); }
void sh4_io_w32(uint32_t a, uint32_t v) { a &= ~3u; for (int i = 0; i < 4; i++) mw8(a + i, (uint8_t)(v >> (8 * i))); }
void sh4_io_pref(uint32_t a) { (void)a; }

/* ---- calls out of the function under test ---- */
#define MAXCALLS 64
typedef struct { uint32_t n; uint32_t tgt[MAXCALLS]; uint32_t args[MAXCALLS]; uint32_t raw[MAXCALLS][6]; } calllog;
static calllog log_a, log_b, *clog;

static int trace;
static void stub(SH4 *c, uint32_t t) {
  uint32_t h = hsh(RT_NORM(t) ^ hsh(c->r[4] ^ hsh(c->r[5] ^ hsh(c->r[6] ^ hsh(c->r[7] ^ hsh(c->fr.u[4] ^ c->r[15]))))));
  if (trace) printf("diff stub %08x pr %08x\n", t, c->pr);
  if (clog->n < MAXCALLS) {
    clog->tgt[clog->n] = RT_NORM(t); clog->args[clog->n] = h;
    uint32_t *w = clog->raw[clog->n];
    w[0] = c->r[4]; w[1] = c->r[5]; w[2] = c->r[6]; w[3] = c->r[7]; w[4] = c->fr.u[4]; w[5] = c->r[15];
  }
  clog->n++;
  /* the SH-4 calling convention clobbers r0-r7, MACH/MACL, T, FR0-FR11; returns in r0 / fr0 */
  for (int i = 0; i < 8; i++) c->r[i] = hsh(h + i);
  c->r[0] = (h & 3) == 0 ? 0 : hsh(h + 99);
  c->fr.f[0] = (float)(int32_t)(h & 0xFFFF) / 256.0f;
  c->t = h >> 31;
  c->macl = hsh(h + 7);
  c->pc = c->pr;
}

/* ---- run-time hooks for the generated code (RT_DIFF) ---- */
static jmp_buf jb;
static uint32_t jb_addr;
enum { END_RET = 1, END_ILLEGAL, END_SLOT, END_UNSUP, END_BADRET, END_BUDGET };
void rt_call(SH4 *c, uint32_t a) { stub(c, a); }
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
void rt_illegal(SH4 *c, uint32_t a) { (void)c; jb_addr = a; longjmp(jb, END_ILLEGAL); }
void rt_illegal_slot(SH4 *c, uint32_t a) { (void)c; jb_addr = a; longjmp(jb, END_SLOT); }
void rt_unsupported(SH4 *c, uint32_t a) { (void)c; jb_addr = a; longjmp(jb, END_UNSUP); }
void rt_badret(SH4 *c, uint32_t want, uint32_t at) { (void)c; (void)want; jb_addr = at; longjmp(jb, END_BADRET); }
void rt_budget(SH4 *c, uint32_t at) { (void)c; jb_addr = at; longjmp(jb, END_BUDGET); }

/* ---- random state ---- */
static uint64_t rs;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
static uint32_t rval(void) {
  switch (rnd() % 8) {
  case 0: case 1: case 2: return 0x8C300000u + (rnd() & 0xFFFFC); /* a pointer into free RAM */
  case 3: return LOAD + 0x57000 + (rnd() & 0x3FFFC);                 /* a pointer into the image data */
  case 4: return rnd() & 0xFF;
  case 5: return (uint32_t)(int32_t)(int8_t)rnd();
  case 6: return rnd() & 1;
  default: return rnd();
  }
}
static void random_state(SH4 *c, uint64_t seed) {
  rs = seed * 0x9E3779B97F4A7C15ull + 1;
  for (int i = 0; i < 8; i++) rnd();
  sh4_reset(c);
  for (int i = 0; i < 15; i++) c->r[i] = rval();
  for (int i = 0; i < 8; i++) c->rbank[i] = rnd();
  c->r[15] = 0x8CF00000u - (rnd() & 0x3FC);
  c->t = rnd() & 1; c->s = 0; c->q = rnd() & 1; c->m = rnd() & 1;
  c->gbr = rval(); c->mach = rnd(); c->macl = rnd();
  for (int i = 0; i < 16; i++) {
    c->fr.f[i] = (float)((int32_t)rnd() % 20000) / 64.0f;
    c->xf.f[i] = (float)((int32_t)rnd() % 2000) / 128.0f;
  }
  c->fpul = rval();
  c->fpscr = 0x00040001u; /* the game's FPSCR: single precision, SZ = 0, FR = 0 */
  c->pr = MAGIC_RET;
}

typedef struct { int end; uint32_t addr; SH4 st; } outcome;

static int run_interp(SH4 *c, uint32_t start, uint32_t end, int64_t steps, outcome *o) {
  c->pc = start;
  while (steps-- > 0) {
    uint32_t at = c->pc;
    int r = sh4_step(c);
    if (trace) printf("diff trace %08x -> %08x kind %u\n", at, c->pc, c->last);
    if (r) {
      o->end = r == -1 ? END_ILLEGAL : r == -3 ? END_SLOT : END_UNSUP;
      o->addr = c->fault;
      if (r == -3) o->addr = c->fault; /* the slot's address */
      return 1;
    }
    if (c->last == SH4_BR_RET) { o->end = END_RET; return 1; }
    if (c->last == SH4_BR_CALL) stub(c, c->pc); /* returns to PR; may be past the function's end */
    if (RT_NORM(c->pc) < start || RT_NORM(c->pc) >= end) { stub(c, c->pc); o->end = END_RET; return 1; }
  }
  return 0; /* out of budget */
}

static int same_state(const SH4 *a, const SH4 *b, char *why) {
#define F(x) if (a->x != b->x) { sprintf(why, #x " %08x/%08x", (uint32_t)a->x, (uint32_t)b->x); return 0; }
  for (int i = 0; i < 16; i++) if (a->r[i] != b->r[i]) { sprintf(why, "r%d %08x/%08x", i, a->r[i], b->r[i]); return 0; }
  for (int i = 0; i < 8; i++) if (a->rbank[i] != b->rbank[i]) { sprintf(why, "rbank%d", i); return 0; }
  for (int i = 0; i < 16; i++) if (a->fr.u[i] != b->fr.u[i]) { sprintf(why, "fr%d %08x/%08x", i, a->fr.u[i], b->fr.u[i]); return 0; }
  for (int i = 0; i < 16; i++) if (a->xf.u[i] != b->xf.u[i]) { sprintf(why, "xf%d", i); return 0; }
  F(t) F(s) F(q) F(m) F(sr) F(gbr) F(vbr) F(ssr) F(spc) F(sgr) F(dbr) F(mach) F(macl) F(pr) F(fpscr) F(fpul)
#undef F
  return 1;
}

static int same_mem(char *why) {
  if (ov_a.full || ov_b.full) { sprintf(why, "overlay full"); return 0; }
  /* every byte written by one run must have the same value in the other run's view */
  overlay *save = ov;
  for (int pass = 0; pass < 2; pass++) {
    overlay *x = pass ? &ov_b : &ov_a, *y = pass ? &ov_a : &ov_b;
    for (int j = 0; j < x->n; j++) {
      uint32_t i = x->order[j], k = x->key[i];
      ov = y;
      int yi = ov_find(k, 0);
      uint8_t yv = yi >= 0 ? y->val[yi] : base_byte(k);
      if (yv != x->val[i]) { sprintf(why, "mem %08x %02x/%02x", k, x->val[i], yv); ov = save; return 0; }
    }
  }
  ov = save;
  return 1;
}

int main(int argc, char **argv) {
  const char *bin = 0;
  int trials = 12, quiet = 0;
  int64_t steps = 20000;
  uint32_t only = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--bin")) bin = argv[++i];
    else if (!strcmp(argv[i], "--trials")) trials = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--steps")) steps = atoll(argv[++i]);
    else if (!strcmp(argv[i], "--only")) only = (uint32_t)strtoul(argv[++i], 0, 16);
    else if (!strcmp(argv[i], "--quiet")) quiet = 1;
    else if (!strcmp(argv[i], "--trace")) trace = 1;
  }
  if (!bin) { fprintf(stderr, "usage\n"); return 2; }
  sh4_init_tables();
  image = malloc(16 << 20);
  for (uint32_t i = 0; i < (16u << 20); i++) image[i] = (uint8_t)hsh(i * 2654435761u);
  FILE *f = fopen(bin, "rb");
  if (!f) { perror(bin); return 2; }
  size_t n = fread(image + 0x10000, 1, (16 << 20) - 0x10000, f);
  fclose(f);
  img_end = LOAD + (uint32_t)n;
  /* function extents from meta.tsv next to the generated code: passed implicitly via the table order */
  FILE *m = fopen(getenv("DIFF_META") ? getenv("DIFF_META") : "/build/gen/meta.tsv", "r");
  if (!m) { perror("meta.tsv"); return 2; }
  int nf = 0, pass = 0, failn = 0, untested = 0, cmp_runs = 0, timeouts = 0, err_runs = 0;
  char line[256];
  static SH4 init, a, b;
  while (fgets(line, sizeof line, m)) {
    uint32_t start, end;
    int nl, ind, aff;
    if (sscanf(line, "%x %x %d %d %d", &start, &end, &nl, &ind, &aff) != 5) continue;
    if (only && start != only) continue;
    rt_fn fn = rt_lookup(start);
    if (!fn) continue;
    nf++;
    int ok = 0, bad = 0;
    char why[96] = "";
    for (int k = 0; k < trials && !bad; k++) {
      random_state(&init, ((uint64_t)start << 16) ^ (uint64_t)k);
      outcome oa = {0}, ob = {0};
      /* interpreter */
      a = init; ov = &ov_a; ov->cur++; ov->n = 0; ov->full = 0; clog = &log_a; clog->n = 0;
      if (!run_interp(&a, start, end, steps, &oa)) { timeouts++; continue; }
      /* recompiled */
      b = init; ov = &ov_b; ov->cur++; ov->n = 0; ov->full = 0; clog = &log_b; clog->n = 0;
      b.budget = steps;
      int code = setjmp(jb);
      if (code == 0) { fn(&b); ob.end = END_RET; }
      else { ob.end = code; ob.addr = jb_addr; }
      cmp_runs++;
      if (oa.end != END_RET) err_runs++;
      /* compare */
      if (oa.end != ob.end) { sprintf(why, "end %d/%d at %08x/%08x", oa.end, ob.end, oa.addr, ob.addr); bad = 1; }
      else if (oa.end != END_RET && oa.addr != ob.addr) { sprintf(why, "fault %08x/%08x", oa.addr, ob.addr); bad = 1; }
      else if (oa.end == END_RET && RT_NORM(a.pc) != RT_NORM(b.pc)) { sprintf(why, "pc %08x/%08x", a.pc, b.pc); bad = 1; }
      else if (log_a.n != log_b.n) { sprintf(why, "calls %u/%u", log_a.n, log_b.n); bad = 1; }
      else {
        for (uint32_t j = 0; j < log_a.n && j < MAXCALLS; j++)
          if (log_a.tgt[j] != log_b.tgt[j] || log_a.args[j] != log_b.args[j]) {
            int w = 0;
            while (w < 5 && log_a.raw[j][w] == log_b.raw[j][w]) w++;
            sprintf(why, "call %u %08x/%08x arg %d %08x/%08x", j, log_a.tgt[j], log_b.tgt[j], w, log_a.raw[j][w],
                    log_b.raw[j][w]); bad = 1; break;
          }
        if (!bad && !same_state(&a, &b, why)) bad = 1;
        if (!bad && !same_mem(why)) bad = 1;
      }
      if (!bad) ok++;
      else if (!quiet) printf("diff FAIL %08x trial %d: %s\n", start, k, why);
    }
    if (bad) failn++;
    else if (ok) pass++;
    else { untested++; if (!quiet) printf("diff untested %08x\n", start); }
  }
  fclose(m);
  printf("diff functions %d pass %d fail %d untested %d runs %d (error-ending %d) interp-timeouts %d\n", nf, pass,
         failn, untested, cmp_runs, err_runs, timeouts);
  return 0;
}
