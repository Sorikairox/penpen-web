/* Pen Pen port, S20: Ninja's matrix and vector C (src/lib/ninja/matrix.c, transform.c, vector.c, trig.c) against the
 * original SDK code run by the interpreter, function by function, on generated inputs: the current matrix (XMTRX) and
 * RAM matrices with signed zeros, ones, small and large values, random angles and vectors. Each case runs the
 * original (the disc's code, from 1ST_READ.BIN loaded at 0x8C010000) from a CPU state and a copy of RAM, and the
 * C's export adapter (f_<address>) from the same state on another copy, then compares XF0-XF15, FR0 / r0, the
 * scratch RAM the functions write, and Ninja's globals. Runs ONLY in the sandboxed container (it executes the disc's
 * code); prints counts and the failing function addresses only.
 *   test_ninja 1ST_READ.BIN [cases]   exit status: the number of failing functions (0 = pass) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../sh4.h"
#include "../../../src/include/hw.h"

uint8_t *sh4_ram;
uint32_t sh4_io_r32(uint32_t a) { return a; }
uint16_t sh4_io_r16(uint32_t a) { return (uint16_t)a; }
uint8_t sh4_io_r8(uint32_t a) { return (uint8_t)a; }
void sh4_io_w32(uint32_t a, uint32_t v) { (void)a; (void)v; }
void sh4_io_w16(uint32_t a, uint16_t v) { (void)a; (void)v; }
void sh4_io_w8(uint32_t a, uint8_t v) { (void)a; (void)v; }
void sh4_io_pref(uint32_t a) { (void)a; }

/* the runtime's side of hw.h, reduced to what the matrix code uses */
SH4 *hw_cpu;
int hw_ret_set, rt_unw, rt_rew, hw_call_fargs;
void hw_enter(hw_entry *e, SH4 *c, uint32_t addr, int resumable) {
  e->c = c; e->addr = addr; e->resumable = resumable; e->prev_cpu = hw_cpu; hw_cpu = c; hw_ret_set = 0;
}
void hw_leave(hw_entry *e) { hw_cpu = e->prev_cpu; e->c->pc = e->c->pr; }
uint32_t hw_call(gaddr fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
  (void)n; (void)a0; (void)a1; (void)a2; (void)a3;
  printf("ninja test: the C called %08x\n", fn);
  exit(99);
}
gaddr hw_stack_alloc(uint32_t bytes) { hw_cpu->r[15] -= (bytes + 3) & ~3u; return hw_cpu->r[15]; }
void hw_stack_free(uint32_t bytes) { hw_cpu->r[15] += (bytes + 3) & ~3u; }

typedef void (*hw_fn)(SH4 *);
#define F(a) void f_##a(SH4 *);
F(8c0c3168) F(8c0c31c4) F(8c0c3230) F(8c0c3308) F(8c0c3328) F(8c0c3b30) F(8c0c3d04) F(8c0c3d4c) F(8c0c3d90)
F(8c0c3e14) F(8c0c3fe8) F(8c0c407c) F(8c0c42ec) F(8c0c4118) F(8c0c4388) F(8c0c47bc) F(8c0c470c) F(8c0c487c)
F(8c0c4868) F(8c0c4890) F(8c0c4904) F(8c0c4978) F(8c0c49e0) F(8c0c4a4c) F(8c10c750) F(8c0c4bc4) F(8c0c4a70)
F(8c0c4c00) F(8c0c4bd0)
/* argument kinds: M a matrix (0 = XMTRX or a RAM scratch), V a vector scratch, O an output scratch, A an angle,
 * f a float in FR4.., n a small count */
static const struct { uint32_t addr; hw_fn fn; const char *args; char ret; } T[] = {
  {0x8C0C3168u, f_8c0c3168, "MVO", 0},  {0x8C0C31C4u, f_8c0c31c4, "MVO", 0},  {0x8C0C3230u, f_8c0c3230, "O", 0},
  {0x8C0C3308u, f_8c0c3308, "VV", 'f'},   {0x8C0C3328u, f_8c0c3328, "M", 'r'},    {0x8C0C3B30u, f_8c0c3b30, "MR", 0},
  {0x8C0C3D04u, f_8c0c3d04, "VVO", 'f'},  {0x8C0C3D4Cu, f_8c0c3d4c, "n", 'r'},    {0x8C0C3D90u, f_8c0c3d90, "M", 'r'},
  {0x8C0C3E14u, f_8c0c3e14, "MAAA", 0}, {0x8C0C3FE8u, f_8c0c3fe8, "MA", 0},   {0x8C0C407Cu, f_8c0c407c, "MA", 0},
  {0x8C0C42ECu, f_8c0c42ec, "MA", 0},   {0x8C0C4118u, f_8c0c4118, "MAAA", 0}, {0x8C0C4388u, f_8c0c4388, "MVA", 0},
  {0x8C0C47BCu, f_8c0c47bc, "Mfff", 0}, {0x8C0C470Cu, f_8c0c470c, "MV", 0},   {0x8C0C487Cu, f_8c0c487c, "V", 'f'},
  {0x8C0C4868u, f_8c0c4868, "V", 'f'},    {0x8C0C4890u, f_8c0c4890, "MR", 0},   {0x8C0C4904u, f_8c0c4904, "MV", 0},
  {0x8C0C4978u, f_8c0c4978, "Mfff", 0}, {0x8C0C49E0u, f_8c0c49e0, "M", 0},    {0x8C0C4A4Cu, f_8c0c4a4c, "V", 'f'},
  {0x8C10C750u, f_8c10c750, "M", 'f'},    {0x8C0C4BC4u, f_8c0c4bc4, "A", 'f'},    {0x8C0C4A70u, f_8c0c4a70, "A", 'f'},
  {0x8C0C4C00u, f_8c0c4c00, "A", 'f'},    {0x8C0C4BD0u, f_8c0c4bd0, "f", 'f'},
};
#define NT ((int)(sizeof T / sizeof T[0]))

#define SCR_M 0x8C200000u /* a RAM matrix */
#define SCR_R 0x8C200040u /* a second one */
#define SCR_V 0x8C200080u /* a vector (4 floats) */
#define SCR_W 0x8C200090u /* a second vector */
#define SCR_O 0x8C2000A0u /* an output (16 floats) */
#define SCR_END 0x8C2000E0u
#define STACK 0x8C00F000u
#define MAGIC 0x8C000F08u /* the return address: not code */
#define NJ_GLOBALS 0x8C1EF2A0u /* Ninja's matrix globals: 0x8C1EF2A0-0x8C1EF360, 0x8C1EFD30 */

static uint64_t rs = 0x2545F4914F6CDD1Dull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }
static uint32_t rnd_float(void) {
  static const uint32_t special[] = {0x00000000u, 0x80000000u, 0x3F800000u, 0xBF800000u, 0x3F000000u, 0x40000000u,
                                     0xC0400000u, 0x3F3504F3u, 0xBF3504F3u, 0x00000001u, 0x7F7FFFFFu};
  uint32_t k = rnd() % 8;
  if (k < 3) return special[rnd() % (sizeof special / 4)];
  if (k < 4) return ((110u + rnd() % 40u) << 23) | (rnd() & 0x7FFFFFu) | (rnd() & 1u) << 31;
  return ((120u + rnd() % 14u) << 23) | (rnd() & 0x7FFFFFu) | (rnd() & 1u) << 31; /* game-sized values */
}
static uint32_t rnd_angle(void) {
  static const uint32_t a[] = {0, 0x4000, 0x8000, 0xC000, 0x10000, 0xFFFFC000u, 0x2AAA, 0x1555};
  return rnd() & 1 ? a[rnd() % 8] : rnd() & (rnd() & 1 ? 0xFFFFu : 0xFFFFFFFFu);
}

static uint8_t *ram0, *ramA, *ramB;
static int run_original(SH4 *c, uint32_t addr) {
  c->pc = addr;
  c->pr = MAGIC;
  for (long n = 0; n < 2000000; n++) {
    if (c->pc == MAGIC) return 0;
    if (sh4_step(c)) return 1;
  }
  return 2;
}
static int differ(const SH4 *a, const SH4 *b, char ret) {
  if (memcmp(a->xf.u, b->xf.u, sizeof a->xf.u)) return 1;
  if (ret == 'f' && a->fr.u[0] != b->fr.u[0]) return 2;
  if (ret == 'r' && a->r[0] != b->r[0]) return 2;
  if (memcmp(ramA + (SCR_M & 0xFFFFFF), ramB + (SCR_M & 0xFFFFFF), SCR_END - SCR_M)) return 3;
  if (memcmp(ramA + (NJ_GLOBALS & 0xFFFFFF), ramB + (NJ_GLOBALS & 0xFFFFFF), 0xC0)) return 4;
  if (memcmp(ramA + 0x201000, ramB + 0x201000, 0x100)) return 5;
  return 0;
}

int main(int argc, char **argv) {
  long cases = argc > 2 ? atol(argv[2]) : 2000;
  ram0 = calloc(1, 16 << 20); ramA = malloc(16 << 20); ramB = malloc(16 << 20);
  FILE *f = argc > 1 ? fopen(argv[1], "rb") : 0;
  if (!f || !fread(ram0 + 0x10000, 1, (16 << 20) - 0x10000, f)) { printf("ninja test: no 1ST_READ.BIN\n"); return 99; }
  fclose(f);
  sh4_init_tables();
  int failing = 0;
  long checks = 0;
  for (int t = 0; t < NT; t++) {
    long fails = 0;
    int first = -1;
    for (long n = 0; n < cases; n++) {
      SH4 base;
      sh4_ram = ram0;
      sh4_reset(&base);
      base.fpscr = 0x00040001u;
      base.r[15] = STACK;
      for (int i = 0; i < 16; i++) base.xf.u[i] = rnd_float();
      for (uint32_t a = SCR_M; a < SCR_END; a += 4) wr32(a, rnd_float());
      /* the matrix stack: a few slots at the scratch area's end, depth 1..3 of 4 */
      wr32(0x8C1EF2BCu, 0x8C201000u); wr32(0x8C1EF2B8u, 0x8C201000u + 0x40u * (rnd() % 3));
      wr32(0x8C1EF35Cu, 1 + rnd() % 3); wr32(0x8C1EF358u, 4); wr32(0x8C1EFD30u, rnd() & 1);
      for (uint32_t a = 0x8C201000u; a < 0x8C201100u; a += 4) wr32(a, rnd_float());
      const char *s = T[t].args;
      int ri = 4, fi = 4;
      for (const char *p = s; *p; p++) {
        uint32_t v = 0;
        switch (*p) {
        case 'M': v = rnd() & 1 ? 0 : SCR_M; break;
        case 'R': v = SCR_R; break;
        case 'V': v = p > s && p[-1] == 'V' ? SCR_W : SCR_V; break;
        case 'O': v = SCR_O; break;
        case 'A': v = rnd_angle(); break;
        case 'n': v = rnd() % 4; break;
        case 'f': base.fr.u[fi++] = rnd_float(); continue;
        }
        base.r[ri++] = v;
      }
      SH4 a = base, b = base;
      memcpy(ramA, ram0, 16 << 20);
      memcpy(ramB, ram0, 16 << 20);
      sh4_ram = ramA;
      int ea = run_original(&a, T[t].addr);
      sh4_ram = ramB;
      b.pr = MAGIC;
      hw_cpu = &b;
      T[t].fn(&b);
      sh4_ram = ram0;
      checks++;
      int d = ea ? 9 : differ(&a, &b, T[t].ret);
      if (d) { fails++; if (first < 0) first = d; }
    }
    if (fails) { failing++; printf("ninja test %08x: %ld of %ld cases differ (first: kind %d)\n", T[t].addr, fails, cases, first); }
  }
  printf("ninja test: %d functions, %ld cases, %d functions differ\n", NT, checks, failing);
  return failing;
}
