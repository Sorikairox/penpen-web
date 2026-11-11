/* Pen Pen port, S13: the hand-written C's float operations (src/include/fpu.h) against the SH-4 interpreter.
 *
 * Every operation of fpu.h is checked bit for bit against the instruction it stands for, executed by the
 * interpreter (sh4_step on the instruction word in RAM) with the game's FPSCR (0x00040001: round to zero,
 * denormals are zero, single precision): on edge operands (signed zeros, denormals, the smallest and largest
 * normals, infinities, NaNs, values one ulp around 1 and 2^24, halfway and cancelling sums) crossed with each other,
 * and on random operands (random bit patterns, and random values of nearby exponents, where rounding matters most).
 * The conversions get edge integers (0, +-1, +-2^24 +- 1..3, INT_MIN, INT_MAX) and random ones; FSCA every 16-bit
 * angle; FIPR random vectors. No game code is involved. Exit status: the number of failing checks (0 = pass), so
 * Mull (tools/s13.sh fpu-mull) counts a mutant of fpu.h killed when any check fails. */
#include <stdio.h>
#include <stdlib.h>
#include "../sh4.h"
#include "../../../src/include/fpu.h"

uint8_t *sh4_ram;
uint32_t sh4_io_r32(uint32_t a) { return a; }
uint16_t sh4_io_r16(uint32_t a) { return (uint16_t)a; }
uint8_t sh4_io_r8(uint32_t a) { return (uint8_t)a; }
void sh4_io_w32(uint32_t a, uint32_t v) { (void)a; (void)v; }
void sh4_io_w16(uint32_t a, uint16_t v) { (void)a; (void)v; }
void sh4_io_w8(uint32_t a, uint8_t v) { (void)a; (void)v; }
void sh4_io_pref(uint32_t a) { (void)a; }

#define BASE 0x8C001000u
#define GAME_FPSCR 0x00040001u
static SH4 cpu, *c = &cpu;
static long checks, fails;

/* run one instruction (then a nop) in the interpreter */
static void exec1(uint16_t op) {
  uint16_t p[2] = {op, 0x0009};
  memcpy(sh4_ram + (BASE & 0xFFFFFF), p, sizeof p);
  c->pc = BASE;
  c->fpscr = GAME_FPSCR;
  if (sh4_step(c)) { printf("fpu test: the interpreter stopped on %04x\n", op); exit(99); }
}
static void check(uint32_t got, uint32_t want, const char *what, uint32_t x, uint32_t y) {
  checks++;
  if (got != want) {
    if (fails < 20) printf("FAIL %s(%08x, %08x): fpu.h %08x, interpreter %08x\n", what, x, y, got, want);
    fails++;
  }
}

/* the interpreter's result of FRn = FRn op FRm (n = 2, m = 3) */
static uint32_t isa_bin(uint16_t base, uint32_t x, uint32_t y) {
  c->fr.u[2] = x; c->fr.u[3] = y;
  exec1((uint16_t)(base | (2 << 8) | (3 << 4)));
  return c->fr.u[2];
}
static void t_binary(uint32_t x, uint32_t y) {
  f32 a = F32_BITS(x), b = F32_BITS(y);
  check(f32_bits(fadd(a, b)), isa_bin(0xF000, x, y), "fadd", x, y);
  check(f32_bits(fsub(a, b)), isa_bin(0xF001, x, y), "fsub", x, y);
  check(f32_bits(fmul(a, b)), isa_bin(0xF002, x, y), "fmul", x, y);
  check(f32_bits(fdiv(a, b)), isa_bin(0xF003, x, y), "fdiv", x, y);
  c->fr.u[2] = x; c->fr.u[3] = y; exec1(0xF235); /* fcmp/gt fr3, fr2: fr2 > fr3 */
  check((uint32_t)fgt(a, b), c->t, "fgt", x, y);
  c->fr.u[2] = x; c->fr.u[3] = y; exec1(0xF234); /* fcmp/eq fr3, fr2 */
  check((uint32_t)feq(a, b), c->t, "feq", x, y);
  /* fmac fr0, fr3, fr2: fr2 = fr0 * fr3 + fr2, with fr0 = y as well */
  c->fr.u[0] = y; c->fr.u[2] = x; c->fr.u[3] = x; exec1(0xF23E);
  check(f32_bits(fmac(b, a, a)), c->fr.u[2], "fmac", x, y);
  c->fr.u[0] = x; c->fr.u[2] = y; c->fr.u[3] = x ^ 0x00400000u; exec1(0xF23E);
  check(f32_bits(fmac(a, F32_BITS(x ^ 0x00400000u), b)), c->fr.u[2], "fmac2", x, y);
}
static void t_unary(uint32_t x) {
  f32 a = F32_BITS(x);
  c->fr.u[2] = x; exec1(0xF26D); check(f32_bits(fsqrt(a)), c->fr.u[2], "fsqrt", x, 0);
  c->fr.u[2] = x; exec1(0xF24D); check(f32_bits(fneg(a)), c->fr.u[2], "fneg", x, 0);
  c->fr.u[2] = x; exec1(0xF25D); check(f32_bits(fabs_(a)), c->fr.u[2], "fabs", x, 0);
  c->fr.u[2] = x; exec1(0xF27D); check(f32_bits(fsrra(a)), c->fr.u[2], "fsrra", x, 0);
  c->fr.u[2] = x; exec1(0xF23D); check((uint32_t)f32_to_int(a), c->fpul, "ftrc", x, 0); /* ftrc fr2, fpul */
}
static void t_int(uint32_t i) {
  c->fpul = i; exec1(0xF22D); /* float fpul, fr2 */
  check(f32_bits(f32_from_int((int32_t)i)), c->fr.u[2], "float", i, 0);
  /* SHC's unsigned conversion: float, then fadd 2^32 when negative */
  uint32_t want = c->fr.u[2];
  if ((int32_t)i < 0) want = isa_bin(0xF000, want, 0x4F800000u);
  check(f32_bits(f32_from_uint(i)), want, "float unsigned", i, 0);
}

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng >> 16); }

int main(int argc, char **argv) {
  long nrand = argc > 1 ? atol(argv[1]) : 200000;
  sh4_ram = calloc(1, 16 << 20);
  sh4_init_tables();
  sh4_reset(c);
  static const uint32_t edge[] = {
    0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007FFFFFu, 0x807FFFFFu, 0x00400000u, /* zeros, denormals */
    0x00800000u, 0x80800000u, 0x00800001u, 0x01000000u, 0x7F7FFFFFu, 0xFF7FFFFFu, 0x7F000000u, /* normals at the ends */
    0x7F800000u, 0xFF800000u, 0x7FC00000u, 0x7FBFFFFFu, 0xFFC00001u, 0x7F800001u,              /* infinities, NaNs */
    0x3F800000u, 0xBF800000u, 0x3F800001u, 0x3F7FFFFFu, 0xBF7FFFFFu, 0x40000000u, 0x3F000000u, /* around 1 */
    0x4B800000u, 0x4B800001u, 0x4B7FFFFFu, 0x33800000u, 0x34000000u, 0xB3800000u, 0x33FFFFFFu, /* 2^24, 2^-24 */
    0x3EAAAAABu, 0x3DCCCCCDu, 0x40490FDBu, 0x4F800000u, 0xCF000000u, 0x4EFFFFFFu, 0x5F000000u, /* 1/3, 0.1, pi, 2^32 */
    0x00FFFFFFu, 0x01800000u, 0x0C800000u, 0x1F800000u, 0x60000000u, 0x7E800000u, 0x1E7FFFFFu, /* products near the ends */
  };
  const int ne = (int)(sizeof edge / sizeof edge[0]);
  for (int i = 0; i < ne; i++) {
    t_unary(edge[i]);
    for (int k = 0; k < ne; k++) t_binary(edge[i], edge[k]);
  }
  for (long n = 0; n < nrand; n++) {
    uint32_t x = rnd() | (rnd() << 16), y = rnd() | (rnd() << 16);
    t_binary(x, y);
    t_unary(x);
    /* nearby magnitudes: the sums cancel, the roundings matter */
    uint32_t y2 = (x & 0xFF800000u) + ((rnd() % 64u) << 23) - (32u << 23) + (rnd() & 0x7FFFFFu);
    t_binary(x, y2);
    t_binary(x, y2 ^ 0x80000000u);
    /* ordinary game values: |v| in [2^-8, 2^16] */
    uint32_t g = ((119u + rnd() % 24u) << 23) | (rnd() & 0x7FFFFFu) | (rnd() & 1u) << 31;
    uint32_t h = ((119u + rnd() % 24u) << 23) | (rnd() & 0x7FFFFFu) | (rnd() & 1u) << 31;
    t_binary(g, h);
    t_unary(g);
  }
  static const int32_t ints[] = {0, 1, -1, 2, -2, 16777215, 16777216, 16777217, 16777218, 16777219, -16777217,
                                 -16777219, 33554435, 2147483647, (int32_t)0x80000000u, -2147483647, 1000000007,
                                 0x7FFFFF80, 0x7FFFFFC0};
  for (int i = 0; i < (int)(sizeof ints / sizeof ints[0]); i++) t_int((uint32_t)ints[i]);
  for (long n = 0; n < nrand; n++) t_int(rnd() | (rnd() << 16));
  /* fsca: every angle (the low 16 bits only: the high bits are ignored) */
  for (uint32_t a = 0; a < 0x10000u; a++) {
    uint32_t angle = a | (rnd() << 16);
    c->fpul = angle; exec1(0xF2FD); /* fsca fpul, dr2 */
    f32 s, co;
    fsca(angle, &s, &co);
    check(f32_bits(s), c->fr.u[2], "fsca sin", angle, 0);
    check(f32_bits(co), c->fr.u[3], "fsca cos", angle, 0);
  }
  /* fipr fv4, fv8: fr11 = fv4 . fv8 */
  for (long n = 0; n < nrand / 4; n++) {
    f32 a[5], b[5]; /* a fifth element that must not count: fipr reads four */
    a[4] = b[4] = F32(1e20f);
    for (int i = 0; i < 4; i++) {
      uint32_t x = ((110u + rnd() % 40u) << 23) | (rnd() & 0x7FFFFFu) | (rnd() & 1u) << 31;
      uint32_t y = n < 64 ? edge[rnd() % (uint32_t)ne] : ((110u + rnd() % 40u) << 23) | (rnd() & 0x7FFFFFu) | (rnd() & 1u) << 31;
      a[i] = F32_BITS(x); b[i] = F32_BITS(y);
      c->fr.u[4 + i] = x; c->fr.u[8 + i] = y;
    }
    exec1(0xF9ED); /* fipr fv4, fv8: n = 8 (bits 11-10 = 2), m = 4 (bits 9-8 = 1) */
    check(f32_bits(fipr(a, b)), c->fr.u[11], "fipr", f32_bits(a[0]), f32_bits(b[0]));
  }
  printf("fpu tests: %ld checks, %ld failures\n", checks, fails);
  return fails > 99 ? 99 : (int)fails;
}
