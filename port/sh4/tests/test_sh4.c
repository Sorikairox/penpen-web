/* Pen Pen port, S3: unit tests of the SH-4 interpreter on hand-written instruction vectors.
 * The encodings below are written independently of sh4_insns.def (from the SH-4 Software Manual's
 * instruction tables), so the decoder table is checked too. No game code is involved. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../sh4.h"

int sh4_table_conflicts(void);

uint8_t *sh4_ram;
static uint32_t io_last_w;
uint32_t sh4_io_r32(uint32_t a) { return a ^ 0x5A5A5A5A; }
uint16_t sh4_io_r16(uint32_t a) { return (uint16_t)a; }
uint8_t sh4_io_r8(uint32_t a) { return (uint8_t)a; }
void sh4_io_w32(uint32_t a, uint32_t v) { (void)a; io_last_w = v; }
void sh4_io_w16(uint32_t a, uint16_t v) { (void)a; io_last_w = v; }
void sh4_io_w8(uint32_t a, uint8_t v) { (void)a; io_last_w = v; }
static uint32_t pref_addr;
void sh4_io_pref(uint32_t a) { pref_addr = a; }

static int fails, checks;
#define CHECK(cond, ...)                                              \
  do {                                                                \
    checks++;                                                         \
    if (!(cond)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
  } while (0)
#define EQ(a, b, what) CHECK((uint32_t)(a) == (uint32_t)(b), "%s: got %08x want %08x", what, (uint32_t)(a), (uint32_t)(b))

/* ---- encodings (manual tables) ---- */
#define NM(base, n, m) (uint16_t)((base) | ((n) << 8) | ((m) << 4))
#define N_(base, n) (uint16_t)((base) | ((n) << 8))
#define NI(base, n, i) (uint16_t)((base) | ((n) << 8) | ((i) & 0xFF))
#define I_(base, i) (uint16_t)((base) | ((i) & 0xFF))
#define MOV(n, m) NM(0x6003, n, m)
#define MOVI(n, i) NI(0xE000, n, i)
#define MOVWPC(n, d) NI(0x9000, n, d)
#define MOVLPC(n, d) NI(0xD000, n, d)
#define MOVA(d) I_(0xC700, d)
#define MOVBS(n, m) NM(0x2000, n, m)
#define MOVWS(n, m) NM(0x2001, n, m)
#define MOVLS(n, m) NM(0x2002, n, m)
#define MOVBL(n, m) NM(0x6000, n, m)
#define MOVWL(n, m) NM(0x6001, n, m)
#define MOVLL(n, m) NM(0x6002, n, m)
#define MOVBM(n, m) NM(0x2004, n, m)
#define MOVLM(n, m) NM(0x2006, n, m)
#define MOVBP(n, m) NM(0x6004, n, m)
#define MOVWP(n, m) NM(0x6005, n, m)
#define MOVLP(n, m) NM(0x6006, n, m)
#define MOVBS4(n, d) (uint16_t)(0x8000 | ((n) << 4) | (d))
#define MOVWS4(n, d) (uint16_t)(0x8100 | ((n) << 4) | (d))
#define MOVLS4(n, m, d) (uint16_t)(0x1000 | ((n) << 8) | ((m) << 4) | (d))
#define MOVBL4(m, d) (uint16_t)(0x8400 | ((m) << 4) | (d))
#define MOVWL4(m, d) (uint16_t)(0x8500 | ((m) << 4) | (d))
#define MOVLL4(n, m, d) (uint16_t)(0x5000 | ((n) << 8) | ((m) << 4) | (d))
#define MOVLS0(n, m) NM(0x0006, n, m)
#define MOVBL0(n, m) NM(0x000C, n, m)
#define MOVWL0(n, m) NM(0x000D, n, m)
#define MOVLSG(d) I_(0xC200, d)
#define MOVWLG(d) I_(0xC500, d)
#define MOVBLG(d) I_(0xC400, d)
#define MOVT(n) N_(0x0029, n)
#define SWAPB(n, m) NM(0x6008, n, m)
#define SWAPW(n, m) NM(0x6009, n, m)
#define XTRCT(n, m) NM(0x200D, n, m)
#define ADD(n, m) NM(0x300C, n, m)
#define ADDI(n, i) NI(0x7000, n, i)
#define ADDC(n, m) NM(0x300E, n, m)
#define ADDV(n, m) NM(0x300F, n, m)
#define SUB(n, m) NM(0x3008, n, m)
#define SUBC(n, m) NM(0x300A, n, m)
#define SUBV(n, m) NM(0x300B, n, m)
#define NEG(n, m) NM(0x600B, n, m)
#define NEGC(n, m) NM(0x600A, n, m)
#define CMPEQI(i) I_(0x8800, i)
#define CMPEQ(n, m) NM(0x3000, n, m)
#define CMPHS(n, m) NM(0x3002, n, m)
#define CMPGE(n, m) NM(0x3003, n, m)
#define CMPHI(n, m) NM(0x3006, n, m)
#define CMPGT(n, m) NM(0x3007, n, m)
#define CMPPZ(n) N_(0x4011, n)
#define CMPPL(n) N_(0x4015, n)
#define CMPSTR(n, m) NM(0x200C, n, m)
#define DIV0S(n, m) NM(0x2007, n, m)
#define DIV0U 0x0019
#define DIV1(n, m) NM(0x3004, n, m)
#define DMULS(n, m) NM(0x300D, n, m)
#define DMULU(n, m) NM(0x3005, n, m)
#define MULL(n, m) NM(0x0007, n, m)
#define MULSW(n, m) NM(0x200F, n, m)
#define MULUW(n, m) NM(0x200E, n, m)
#define MACW(n, m) NM(0x400F, n, m)
#define MACL(n, m) NM(0x000F, n, m)
#define DT(n) N_(0x4010, n)
#define EXTSB(n, m) NM(0x600E, n, m)
#define EXTSW(n, m) NM(0x600F, n, m)
#define EXTUB(n, m) NM(0x600C, n, m)
#define EXTUW(n, m) NM(0x600D, n, m)
#define AND(n, m) NM(0x2009, n, m)
#define ANDI(i) I_(0xC900, i)
#define ANDB(i) I_(0xCD00, i)
#define OR(n, m) NM(0x200B, n, m)
#define ORB(i) I_(0xCF00, i)
#define XOR(n, m) NM(0x200A, n, m)
#define XORI(i) I_(0xCA00, i)
#define NOT(n, m) NM(0x6007, n, m)
#define TST(n, m) NM(0x2008, n, m)
#define TSTI(i) I_(0xC800, i)
#define TSTB(i) I_(0xCC00, i)
#define XORB(i) I_(0xCE00, i)
#define TASB(n) N_(0x401B, n)
#define ROTL(n) N_(0x4004, n)
#define ROTR(n) N_(0x4005, n)
#define ROTCL(n) N_(0x4024, n)
#define ROTCR(n) N_(0x4025, n)
#define SHAD(n, m) NM(0x400C, n, m)
#define SHLD(n, m) NM(0x400D, n, m)
#define SHAL(n) N_(0x4020, n)
#define SHAR(n) N_(0x4021, n)
#define SHLL(n) N_(0x4000, n)
#define SHLR(n) N_(0x4001, n)
#define SHLL2(n) N_(0x4008, n)
#define SHLR2(n) N_(0x4009, n)
#define SHLL8(n) N_(0x4018, n)
#define SHLR8(n) N_(0x4019, n)
#define SHLL16(n) N_(0x4028, n)
#define SHLR16(n) N_(0x4029, n)
#define BT(d) I_(0x8900, d)
#define BF(d) I_(0x8B00, d)
#define BTS(d) I_(0x8D00, d)
#define BFS(d) I_(0x8F00, d)
#define BRA(d) (uint16_t)(0xA000 | ((d) & 0xFFF))
#define BSR(d) (uint16_t)(0xB000 | ((d) & 0xFFF))
#define BRAF(n) N_(0x0023, n)
#define BSRF(n) N_(0x0003, n)
#define JMP(n) N_(0x402B, n)
#define JSR(n) N_(0x400B, n)
#define RTS 0x000B
#define NOP 0x0009
#define CLRT 0x0008
#define SETT 0x0018
#define CLRS 0x0048
#define SETS 0x0058
#define CLRMAC 0x0028
#define LDCSR(m) N_(0x400E, m)
#define STCSR(n) N_(0x0002, n)
#define LDCGBR(m) N_(0x401E, m)
#define STCGBR(n) N_(0x0012, n)
#define LDCBANK(m, b) (uint16_t)(0x408E | ((m) << 8) | ((b) << 4))
#define STCBANK(n, b) (uint16_t)(0x0082 | ((n) << 8) | ((b) << 4))
#define LDCLSR(m) N_(0x4007, m)
#define STCLSR(n) N_(0x4003, n)
#define LDSMACH(m) N_(0x400A, m)
#define LDSMACL(m) N_(0x401A, m)
#define LDSPR(m) N_(0x402A, m)
#define STSMACH(n) N_(0x000A, n)
#define STSMACL(n) N_(0x001A, n)
#define STSPR(n) N_(0x002A, n)
#define STSLPR(n) N_(0x4022, n)
#define LDSLPR(m) N_(0x4026, m)
#define PREF(n) N_(0x0083, n)
#define OCBWB(n) N_(0x00B3, n)
#define FMOV(n, m) NM(0xF00C, n, m)
#define FMOVL(n, m) NM(0xF008, n, m)
#define FMOVS(n, m) NM(0xF00A, n, m)
#define FMOVLP(n, m) NM(0xF009, n, m)
#define FMOVSM(n, m) NM(0xF00B, n, m)
#define FMOVL0(n, m) NM(0xF006, n, m)
#define FMOVS0(n, m) NM(0xF007, n, m)
#define FLDI0(n) N_(0xF08D, n)
#define FLDI1(n) N_(0xF09D, n)
#define FLDS(m) N_(0xF01D, m)
#define FSTS(n) N_(0xF00D, n)
#define FABS(n) N_(0xF05D, n)
#define FNEG(n) N_(0xF04D, n)
#define FADD(n, m) NM(0xF000, n, m)
#define FSUB(n, m) NM(0xF001, n, m)
#define FMUL(n, m) NM(0xF002, n, m)
#define FDIV(n, m) NM(0xF003, n, m)
#define FCMPEQ(n, m) NM(0xF004, n, m)
#define FCMPGT(n, m) NM(0xF005, n, m)
#define FLOAT(n) N_(0xF02D, n)
#define FTRC(m) N_(0xF03D, m)
#define FMAC(n, m) NM(0xF00E, n, m)
#define FSQRT(n) N_(0xF06D, n)
#define FSRRA(n) N_(0xF07D, n)
#define FCNVDS(m) N_(0xF0BD, m)
#define FCNVSD(n) N_(0xF0AD, n)
#define FIPR(n, m) (uint16_t)(0xF0ED | (((n) / 4) << 10) | (((m) / 4) << 8))
#define FTRV(n) (uint16_t)(0xF1FD | (((n) / 4) << 10))
#define FSCA(n) (uint16_t)(0xF0FD | (((n) / 2) << 9))
#define FRCHG 0xFBFD
#define FSCHG 0xF3FD
#define LDSFPSCR(m) N_(0x406A, m)
#define STSFPSCR(n) N_(0x006A, n)
#define LDSFPUL(m) N_(0x405A, m)
#define STSFPUL(n) N_(0x005A, n)

#define BASE 0x8C010000u
static SH4 cpu, *c = &cpu;

static void put16(uint32_t a, uint16_t v) { memcpy(sh4_ram + (a & 0xFFFFFF), &v, 2); }
static void put32(uint32_t a, uint32_t v) { memcpy(sh4_ram + (a & 0xFFFFFF), &v, 4); }
static uint32_t get32(uint32_t a) { uint32_t v; memcpy(&v, sh4_ram + (a & 0xFFFFFF), 4); return v; }
static float f32(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t u32f(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* load a program at BASE, reset the CPU (stack at 0x8C00F000), keep the RAM */
static void prog(const uint16_t *p, int n) {
  sh4_reset(c);
  for (int i = 0; i < n; i++) put16(BASE + 2 * i, p[i]);
  put16(BASE + 2 * n, 0xFFFD); /* an illegal word as end marker */
  c->pc = BASE;
  c->r[15] = 0x8C00F000u;
}
/* step until pc reaches `end` (address) or an error; returns the last status */
static int run_to(uint32_t end, int max) {
  int r = 0;
  while (c->pc != end && max-- > 0) if ((r = sh4_step(c))) return r;
  return max < 0 ? -9 : 0;
}
#define PROG(...) do { static const uint16_t p_[] = {__VA_ARGS__}; prog(p_, sizeof p_ / 2); } while (0)
#define RUN(ninsn) run_to(BASE + 2 * (ninsn), 100000)

static void t_table(void) {
  EQ(sh4_table_conflicts(), 0, "decoder conflicts");
  int legal = 0;
  for (int op = 0; op < 65536; op++) legal += sh4_op_index((uint16_t)op) >= 0;
  /* every encoding in the manual's tables; the count is a regression guard for the table */
  CHECK(legal > 40000 && legal < 65536, "legal encodings %d", legal);
  struct { uint16_t op; const char *name; } v[] = {
    {MOV(1, 2), "MOV_RR"}, {MOVI(3, 0x80), "MOV_I"}, {MOVLPC(0, 1), "MOVL_PC"}, {MOVA(4), "MOVA"},
    {MOVBS4(3, 2), "MOVB_S4"}, {MOVLL4(1, 2, 3), "MOVL_L4"}, {DIV0U, "DIV0U"}, {DIV1(1, 0), "DIV1"},
    {BRA(-2), "BRA"}, {BSR(5), "BSR"}, {RTS, "RTS"}, {JSR(3), "JSR"}, {BRAF(2), "BRAF"},
    {LDCBANK(3, 5), "LDC_BANK"}, {STCBANK(2, 1), "STC_BANK"}, {FIPR(4, 8), "FIPR"}, {FTRV(12), "FTRV"},
    {FSCA(6), "FSCA"}, {FRCHG, "FRCHG"}, {FSCHG, "FSCHG"}, {FCNVDS(2), "FCNVDS"}, {FCNVSD(4), "FCNVSD"},
    {FSRRA(5), "FSRRA"}, {LDSFPSCR(1), "LDS_FPSCR"}, {PREF(4), "PREF"}, {TASB(3), "TASB"},
    {MACL(1, 2), "MACL"}, {MACW(1, 2), "MACW"}, {0xC310, "TRAPA"}, {0x002B, "RTE"},
  };
  for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
    const char *n = sh4_op_name(v[i].op);
    CHECK(n && !strcmp(n, v[i].name), "decode %04x: %s want %s", v[i].op, n ? n : "(illegal)", v[i].name);
  }
  CHECK(sh4_op_name(0xFFFD) == NULL, "0xFFFD must be illegal");
  CHECK(sh4_op_name(0x0000) == NULL, "0x0000 must be illegal");
}

static void t_move(void) {
  /* mov #imm sign-extends; mov Rm,Rn */
  PROG(MOVI(1, 0x80), MOVI(2, 0x7F), MOV(3, 1));
  RUN(3);
  EQ(c->r[1], 0xFFFFFF80, "mov #-128"); EQ(c->r[2], 0x7F, "mov #127"); EQ(c->r[3], 0xFFFFFF80, "mov r1,r3");
  /* PC-relative: mov.w @(d,PC) = PC+4+2d; mov.l @(d,PC) = (PC&~3)+4+4d, mova likewise */
  PROG(MOVWPC(1, 2), /* +2 */ MOVLPC(2, 1), /* +4 */ MOVA(1), NOP, 0x8001, NOP, 0x1234, 0x5678, 0x9ABC);
  RUN(4);
  /* at BASE: d=2 -> BASE+4+4 = BASE+8 holds 0x8001 */
  EQ(c->r[1], 0xFFFF8001, "mov.w @pc");
  /* at BASE+2: (BASE+2)&~3 = BASE, +4+4 = BASE+8 -> words 0x8001, NOP(0x0009) */
  EQ(c->r[2], 0x00098001, "mov.l @pc unaligned pc");
  /* at BASE+4: BASE+4+4+4 = BASE+12 */
  EQ(c->r[0], BASE + 12, "mova");
  /* loads/stores, sign extension, pre-decrement, post-increment */
  PROG(MOVI(2, -2), MOVBS(1, 2), MOVWL(3, 1), MOVLS(1, 2), MOVBL(4, 1), MOVLM(1, 2), MOVLP(5, 1), MOVBP(6, 1));
  c->r[1] = 0x8C200000u; sh4_ram[0x200001] = 0;
  RUN(8);
  EQ(c->r[3], 0x000000FE, "mov.w after mov.b: bytes FE 00");
  EQ(c->r[4], 0xFFFFFFFE, "mov.b sign-extends");
  EQ(c->r[5], 0xFFFFFFFE, "mov.l @r1+ after @-r1");
  EQ(c->r[1], 0x8C200001u, "post-increments: +4 then +1");
  EQ(c->r[6], (uint32_t)(int8_t)sh4_ram[0x200000], "mov.b @r1+");
  /* mov.l @Rm+,Rn with n == m does not increment */
  PROG(MOVLPC(1, 1), MOVLP(1, 1), NOP, NOP, 0x0000, 0x8C20);
  put32(0x8C200000u, 0x11223344u);
  RUN(2);
  EQ(c->r[1], 0x11223344u, "mov.l @r1+,r1");
  /* displacement forms */
  PROG(MOVI(0, 0x55), MOVBS4(1, 3), MOVWS4(1, 2), MOVLS4(1, 0, 2), MOVBL4(1, 3), MOV(7, 0), MOVLL4(2, 1, 2));
  c->r[1] = 0x8C200000u;
  memset(sh4_ram + 0x200000, 0, 16);
  RUN(7);
  EQ(sh4_ram[0x200003], 0x55, "mov.b r0,@(3,r1)");
  EQ(sh4_ram[0x200004], 0x55, "mov.w r0,@(4,r1)");
  EQ(get32(0x8C200008u), 0x55, "mov.l r0,@(8,r1)");
  EQ(c->r[7], 0x55, "mov.b @(3,r1),r0");
  EQ(c->r[2], 0x55, "mov.l @(8,r1),r2");
  /* @(R0,Rn), GBR forms */
  PROG(MOVI(0, 8), MOVI(2, -1), MOVLS0(1, 2), MOVBL0(3, 1), MOVWL0(4, 1), LDCGBR(1), MOVLSG(1), MOVWLG(2), STCGBR(5));
  c->r[1] = 0x8C200000u;
  RUN(9);
  EQ(get32(0x8C200008u), 0xFFFFFFFF, "mov.l r2,@(r0,r1)");
  EQ(c->r[3], 0xFFFFFFFF, "mov.b @(r0,r1)");
  EQ(c->r[4], 0xFFFFFFFF, "mov.w @(r0,r1)");
  EQ(get32(0x8C200004u), 8, "mov.l r0,@(4,gbr)");
  EQ(c->r[0], 8, "mov.w @(4,gbr),r0"); /* low half of 8 */
  EQ(c->r[5], 0x8C200000u, "stc gbr");
  /* movt, swap, xtrct */
  PROG(MOVLPC(1, 3), SETT, MOVT(2), SWAPB(3, 1), SWAPW(4, 1), MOV(5, 1), XTRCT(5, 4), NOP, 0x5678, 0x1234);
  RUN(7);
  EQ(c->r[2], 1, "movt");
  EQ(c->r[3], 0x12347856, "swap.b");
  EQ(c->r[4], 0x56781234, "swap.w");
  EQ(c->r[5], 0x12341234, "xtrct r4,r5");
}

static void t_arith(void) {
  /* 64-bit add with addc: (r1:r0) + (r3:r2) */
  PROG(CLRT, ADDC(0, 2), ADDC(1, 3));
  c->r[0] = 0xFFFFFFFF; c->r[1] = 1; c->r[2] = 2; c->r[3] = 3;
  c->pc = BASE; RUN(3);
  EQ(c->r[0], 1, "addc lo"); EQ(c->r[1], 5, "addc hi"); EQ(c->t, 0, "addc T");
  PROG(ADDV(1, 2));
  c->r[1] = 0x7FFFFFFF; c->r[2] = 1; c->pc = BASE; RUN(1);
  EQ(c->r[1], 0x80000000, "addv"); EQ(c->t, 1, "addv overflow");
  PROG(SUBV(1, 2));
  c->r[1] = 0x80000000; c->r[2] = 1; c->pc = BASE; RUN(1);
  EQ(c->r[1], 0x7FFFFFFF, "subv"); EQ(c->t, 1, "subv overflow");
  PROG(CLRT, SUBC(0, 2), SUBC(1, 3)); /* 64-bit subtract */
  c->r[0] = 0; c->r[1] = 5; c->r[2] = 1; c->r[3] = 1; c->pc = BASE; RUN(3);
  EQ(c->r[0], 0xFFFFFFFF, "subc lo"); EQ(c->r[1], 3, "subc hi"); EQ(c->t, 0, "subc T");
  PROG(CLRT, NEGC(2, 0), NEGC(3, 1)); /* 64-bit negate of r1:r0 */
  c->r[0] = 1; c->r[1] = 0; c->pc = BASE; RUN(3);
  EQ(c->r[2], 0xFFFFFFFF, "negc lo"); EQ(c->r[3], 0xFFFFFFFF, "negc hi");
  PROG(NEG(2, 1), SUB(3, 1), ADDI(3, -1), ADD(4, 3));
  c->r[1] = 5; c->r[3] = 2; c->r[4] = 10; c->pc = BASE; RUN(4);
  EQ(c->r[2], (uint32_t)-5, "neg"); EQ(c->r[3], (uint32_t)-4, "sub, add #-1"); EQ(c->r[4], 6, "add");
  /* comparisons */
  struct { uint16_t op; uint32_t n, m; uint32_t t; } cmp[] = {
    {CMPEQ(1, 2), 5, 5, 1}, {CMPEQ(1, 2), 5, 6, 0}, {CMPHS(1, 2), 1, 0xFFFFFFFF, 0},
    {CMPHS(1, 2), 7, 7, 1}, {CMPGE(1, 2), 1, 0xFFFFFFFF, 1}, {CMPGE(1, 2), 0xFFFFFFFE, 0xFFFFFFFF, 0},
    {CMPHI(1, 2), 7, 7, 0}, {CMPHI(1, 2), 0x80000000, 1, 1}, {CMPGT(1, 2), 0x80000000, 1, 0},
    {CMPGT(1, 2), 2, 1, 1}, {CMPPZ(1), 0, 0, 1}, {CMPPZ(1), 0x80000000, 0, 0}, {CMPPL(1), 0, 0, 0},
    {CMPPL(1), 1, 0, 1}, {CMPSTR(1, 2), 0x41424344, 0x00004400, 0}, {CMPSTR(1, 2), 0x41424344, 0x11114311, 1},
    {CMPSTR(1, 2), 0x41424344, 0x41FFFFFF, 1},
  };
  for (unsigned i = 0; i < sizeof cmp / sizeof cmp[0]; i++) {
    uint16_t p[1] = {cmp[i].op};
    prog(p, 1); c->r[1] = cmp[i].n; c->r[2] = cmp[i].m; RUN(1);
    CHECK(c->t == cmp[i].t, "cmp #%u op %04x n %08x m %08x: T %u", i, cmp[i].op, cmp[i].n, cmp[i].m, c->t);
  }
  PROG(MOVI(0, -3), CMPEQI(-3));
  RUN(2); EQ(c->t, 1, "cmp/eq #imm sign-extends");
  /* multiplies */
  PROG(DMULS(1, 2), STSMACH(3), STSMACL(4), DMULU(1, 2), STSMACH(5), STSMACL(6), MULL(1, 2), STSMACL(7),
       MULSW(1, 2), STSMACL(8), MULUW(1, 2), STSMACL(9));
  c->r[1] = 0xFFFFFFFE; c->r[2] = 3; c->pc = BASE; RUN(12);
  EQ(c->r[3], 0xFFFFFFFF, "dmuls hi"); EQ(c->r[4], (uint32_t)-6, "dmuls lo");
  EQ(c->r[5], 2, "dmulu hi"); EQ(c->r[6], 0xFFFFFFFA, "dmulu lo"); EQ(c->r[7], (uint32_t)-6, "mul.l");
  EQ(c->r[8], (uint32_t)-6, "muls.w"); EQ(c->r[9], 0xFFFE * 3, "mulu.w");
  /* mac.w / mac.l (S = 0), and mac.w with S = 1 saturation */
  PROG(MOVLPC(1, 4), MOV(2, 1), CLRMAC, MACW(1, 2), MACW(1, 2), STSMACL(3), STSMACH(4), NOP, NOP, NOP, 0x0000, 0x8C20);
  put16(0x8C200000u, (uint16_t)-3); put16(0x8C200002u, 5);
  RUN(7);
  /* (-3*-3) + (5*5) = 34 ; r1 and r2 both advanced twice */
  EQ(c->r[3], 34, "mac.w"); EQ(c->r[4], 0, "mac.w mach"); EQ(c->r[1], 0x8C200004u, "mac.w increments");
  PROG(CLRMAC, MACL(1, 2), STSMACH(3), STSMACL(4));
  c->r[1] = 0x8C200000u; c->r[2] = 0x8C200010u;
  put32(0x8C200000u, 0x80000000u); put32(0x8C200010u, 2);
  RUN(4);
  EQ(c->r[3], 0xFFFFFFFF, "mac.l hi"); EQ(c->r[4], 0, "mac.l lo");
  PROG(MOVLPC(1, 4), MOV(2, 1), SETS, LDSMACL(5), MACW(1, 2), STSMACL(3), STSMACH(4), NOP, NOP, NOP, 0x0000, 0x8C20);
  put16(0x8C200000u, 0x7FFF); put16(0x8C200002u, 0x7FFF);
  c->r[5] = 0x7FFFFFF0; c->pc = BASE;
  RUN(7);
  EQ(c->r[3], 0x7FFFFFFF, "mac.w saturates"); EQ(c->r[4], 1, "mac.w saturation sets MACH");
  /* dt loop: 5 iterations */
  PROG(MOVI(1, 5), MOVI(2, 0), ADDI(2, 1), DT(1), BF(-4), NOP);
  RUN(5 + 0);
  run_to(BASE + 10, 1000);
  EQ(c->r[2], 5, "dt loop count"); EQ(c->r[1], 0, "dt end"); EQ(c->t, 1, "dt T");
  /* extensions */
  PROG(EXTSB(2, 1), EXTSW(3, 1), EXTUB(4, 1), EXTUW(5, 1));
  c->r[1] = 0x12348081; c->pc = BASE; RUN(4);
  EQ(c->r[2], 0xFFFFFF81, "exts.b"); EQ(c->r[3], 0xFFFF8081, "exts.w"); EQ(c->r[4], 0x81, "extu.b");
  EQ(c->r[5], 0x8081, "extu.w");
}

/* The manual's division sequences, executed and compared with C. */
static void t_div(void) {
  uint16_t p[80];
  int n = 0;
  /* unsigned: R1:R2 (64 bits, R1 = 0) / R0 = R2 */
  p[n++] = DIV0U;
  for (int i = 0; i < 32; i++) { p[n++] = ROTCL(2); p[n++] = DIV1(1, 0); }
  p[n++] = ROTCL(2);
  srand(1);
  int bad = 0;
  for (int k = 0; k < 2000; k++) {
    uint32_t a = ((uint32_t)rand() << 16) ^ (uint32_t)rand(), b = ((uint32_t)rand() << 8) ^ (uint32_t)rand();
    if (k % 3 == 0) b &= 0xFFF;
    if (!b) b = 1;
    prog(p, n);
    c->r[0] = b; c->r[1] = 0; c->r[2] = a;
    run_to(BASE + 2 * n, 1000);
    if (c->r[2] != a / b) { if (bad++ < 3) printf("udiv %u/%u got %u\n", a, b, c->r[2]); }
  }
  EQ(bad, 0, "unsigned div1 sequence");
  /* signed: R2 / R0 = R2 (32 bits), the manual's sequence with div0s */
  n = 0;
  p[n++] = MOV(3, 2); p[n++] = ROTCL(3); p[n++] = SUBC(1, 1); p[n++] = XOR(3, 3); p[n++] = SUBC(2, 3);
  p[n++] = DIV0S(1, 0);
  for (int i = 0; i < 32; i++) { p[n++] = ROTCL(2); p[n++] = DIV1(1, 0); }
  p[n++] = ROTCL(2); p[n++] = ADDC(2, 3);
  bad = 0;
  for (int k = 0; k < 2000; k++) {
    int32_t a = (int32_t)(((uint32_t)rand() << 16) ^ (uint32_t)rand()), b = (int32_t)(((uint32_t)rand() << 4) ^ (uint32_t)rand());
    if (k & 1) b = -b;
    if (k % 5 == 0) a >>= 12;
    if (!b) b = 7;
    prog(p, n);
    c->r[0] = (uint32_t)b; c->r[2] = (uint32_t)a;
    run_to(BASE + 2 * n, 1000);
    if ((int32_t)c->r[2] != a / b) { if (bad++ < 3) printf("sdiv %d/%d got %d\n", a, b, (int32_t)c->r[2]); }
  }
  EQ(bad, 0, "signed div0s/div1 sequence");
}

static void t_logic(void) {
  PROG(AND(1, 2), OR(3, 2), XOR(4, 2), NOT(5, 2), TST(6, 2), MOVI(0, 0x3C), ANDI(0x0F), XORI(0xF0), TSTI(0x01));
  c->r[1] = 0xFF00FF00; c->r[2] = 0x0F0F0F0F; c->r[3] = 0x10000000; c->r[4] = 0xFFFFFFFF; c->r[6] = 0xF0F0F0F0;
  c->pc = BASE; RUN(9);
  EQ(c->r[1], 0x0F000F00, "and"); EQ(c->r[3], 0x1F0F0F0F, "or"); EQ(c->r[4], 0xF0F0F0F0, "xor");
  EQ(c->r[5], 0xF0F0F0F0, "not"); EQ(c->r[0], 0xFC, "and #/xor #"); EQ(c->t, 1, "tst #");
  /* byte ops at @(R0,GBR) and tas.b */
  PROG(LDCGBR(1), MOVI(0, 4), ANDB(0x0F), ORB(0x80), XORB(0x01), TSTB(0x10), TASB(1));
  c->r[1] = 0x8C200000u;
  sh4_ram[0x200004] = 0x3C; sh4_ram[0x200000] = 0;
  RUN(7);
  EQ(sh4_ram[0x200004], 0x8D, "and.b/or.b/xor.b"); EQ(c->t, 1, "tas.b of 0 sets T");
  EQ(sh4_ram[0x200000], 0x80, "tas.b sets bit 7");
}

static void t_shift(void) {
  struct { uint16_t op; uint32_t n, m, want; } v[] = {
    {SHAD(1, 2), 0x80000010, 4, 0x00000100}, {SHAD(1, 2), 0x80000010, (uint32_t)-4, 0xF8000001},
    {SHAD(1, 2), 0x80000010, (uint32_t)-32, 0xFFFFFFFF}, {SHAD(1, 2), 0x70000010, (uint32_t)-32, 0},
    {SHLD(1, 2), 0x80000010, (uint32_t)-4, 0x08000001}, {SHLD(1, 2), 0x80000010, (uint32_t)-32, 0},
    {SHLD(1, 2), 0x80000010, 31, 0}, {SHLL2(1), 0x40000001, 0, 4}, {SHLR2(1), 0x40000001, 0, 0x10000000},
    {SHLL8(1), 0x12345678, 0, 0x34567800}, {SHLR8(1), 0x12345678, 0, 0x00123456},
    {SHLL16(1), 0x12345678, 0, 0x56780000}, {SHLR16(1), 0x12345678, 0, 0x1234},
  };
  for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
    uint16_t p[1] = {v[i].op};
    prog(p, 1); c->r[1] = v[i].n; c->r[2] = v[i].m; RUN(1);
    CHECK(c->r[1] == v[i].want, "shift #%u: %08x want %08x", i, c->r[1], v[i].want);
  }
  PROG(ROTL(1), MOVT(2), ROTR(1), MOVT(3), CLRT, ROTCL(4), MOVT(5), SETT, ROTCR(6), MOVT(7), SHAR(8), SHLR(9), SHAL(10));
  c->r[1] = 0x80000001; c->r[4] = 0x80000000; c->r[6] = 2; c->r[8] = 0x80000003; c->r[9] = 0x80000003; c->r[10] = 0x40000000;
  c->pc = BASE; RUN(13);
  EQ(c->r[2], 1, "rotl T"); EQ(c->r[3], 1, "rotr T"); EQ(c->r[1], 0x80000001, "rotl+rotr");
  EQ(c->r[4], 0, "rotcl"); EQ(c->r[5], 1, "rotcl T"); EQ(c->r[6], 0x80000001, "rotcr"); EQ(c->r[7], 0, "rotcr T");
  EQ(c->r[8], 0xC0000001, "shar"); EQ(c->r[9], 0x40000001, "shlr"); EQ(c->r[10], 0x80000000, "shal");
}

static void t_branch(void) {
  int r;
  /* bt not taken / bf taken: target = PC + 4 + 2d */
  PROG(CLRT, BT(1), MOVI(1, 1), BF(0), MOVI(2, 2), MOVI(3, 3), MOVI(4, 4));
  RUN(7);
  EQ(c->r[1], 1, "bt not taken falls through"); EQ(c->r[2], 0, "bf taken skips"); EQ(c->r[3], 3, "bf target");
  /* bt/s: the slot runs whether or not the branch is taken; T is tested before the slot */
  PROG(SETT, BTS(1), CLRT, MOVI(1, 1), MOVI(2, 2));
  RUN(5);
  EQ(c->r[1], 0, "bt/s taken although the slot cleared T"); EQ(c->r[2], 2, "bt/s target");
  PROG(SETT, BFS(1), CLRT, MOVI(1, 1), MOVI(2, 2));
  RUN(5);
  EQ(c->r[1], 1, "bf/s not taken although the slot cleared T");
  /* bra: slot runs, then the target */
  PROG(BRA(1), MOVI(1, 1), MOVI(2, 2), MOVI(3, 3));
  RUN(4);
  EQ(c->r[1], 1, "bra slot"); EQ(c->r[2], 0, "bra skipped"); EQ(c->r[3], 3, "bra target");
  /* bsr sets PR = PC + 4 (after the slot); rts returns there, its own slot runs first */
  PROG(BSR(2), MOVI(1, 1), MOVI(5, 5), BRA(3), NOP, RTS, MOVI(2, 2), NOP, NOP);
  /* BASE: bsr -> BASE+4+4 = BASE+8? d=2 -> BASE+8 is "BRA"?? lay out: 0 bsr,2 slot,4 mov5,6 bra,8 nop,10 rts */
  c->pc = BASE;
  r = sh4_step(c); EQ(r, 0, "bsr step"); EQ(c->pc, BASE + 8, "bsr target"); EQ(c->pr, BASE + 4, "bsr PR");
  EQ(c->last, SH4_BR_CALL, "bsr kind"); EQ(c->r[1], 1, "bsr slot");
  c->pc = BASE + 10; r = sh4_step(c);
  EQ(c->pc, BASE + 4, "rts target"); EQ(c->r[2], 2, "rts slot"); EQ(c->last, SH4_BR_RET, "rts kind");
  /* jsr whose slot overwrites the jump register: the old value is the target */
  PROG(MOVA(2), JSR(0), MOVI(0, 0), NOP, NOP, NOP, NOP, NOP);
  RUN(1); r = sh4_step(c);
  EQ(c->pc, BASE + 12, "jsr uses the register before the slot"); EQ(c->pr, BASE + 6, "jsr PR"); EQ(c->r[0], 0, "jsr slot");
  /* braf / bsrf: PC + 4 + Rn; jmp */
  PROG(MOVI(1, 6), BRAF(1), NOP, NOP, NOP, NOP, NOP, MOVI(2, 2));
  RUN(1); sh4_step(c); EQ(c->pc, BASE + 2 + 4 + 6, "braf target");
  PROG(MOVI(1, 6), BSRF(1), NOP, NOP, NOP, NOP, NOP, MOVI(2, 2));
  RUN(1); sh4_step(c); EQ(c->pc, BASE + 12, "bsrf target"); EQ(c->pr, BASE + 6, "bsrf PR");
  PROG(JMP(3), NOP);
  c->r[3] = 0x8C123456; sh4_step(c); EQ(c->pc, 0x8C123456, "jmp target"); EQ(c->last, SH4_BR_JUMP, "jmp kind");
  /* a branch in a delay slot is an illegal slot instruction */
  PROG(BRA(4), BRA(4));
  r = sh4_step(c); EQ(r, -3, "branch in slot");
  PROG(0xFFFD);
  r = sh4_step(c); EQ(r, -1, "illegal"); EQ(c->fault, BASE, "fault address");
  PROG(0xC301);
  r = sh4_step(c); EQ(r, -2, "trapa unsupported");
  /* the slot executes with its own PC for PC-relative forms (mova in a bra slot) */
  PROG(BRA(2), MOVA(0), NOP, NOP, NOP);
  sh4_step(c); EQ(c->r[0], ((BASE + 2) & ~3u) + 4, "mova in slot");
}

static void t_system(void) {
  /* SR: T, S, Q, M bits and the register bank switch */
  PROG(LDCSR(1), STCSR(2));
  c->r[1] = 0x40000303u; /* MD=1, RB=0, M, Q, S, T */
  c->r[0] = 0xAAAA;
  RUN(2);
  EQ(c->t, 1, "sr T"); EQ(c->s, 1, "sr S"); EQ(c->q, 1, "sr Q"); EQ(c->m, 1, "sr M");
  EQ(c->r[2], 0x40000303u, "stc sr");
  EQ(c->r[0], 0, "RB 1 -> 0 switched banks (bank 0 was zero)");
  EQ(c->rbank[0], 0xAAAA, "old bank kept");
  /* ldc Rm,Rn_BANK / stc Rn_BANK,Rm */
  PROG(MOVI(3, 77), LDCBANK(3, 5), STCBANK(4, 5));
  RUN(3);
  EQ(c->rbank[5], 77, "ldc r3,r5_bank"); EQ(c->r[4], 77, "stc r5_bank,r4");
  /* sts.l / lds.l with the stack */
  PROG(MOVI(1, 0x40), LDSPR(1), STSLPR(15), LDSPR(2), LDSLPR(15), STSPR(3), LDSMACH(1), STSMACH(4));
  RUN(8);
  EQ(c->r[3], 0x40, "sts.l pr / lds.l pr"); EQ(c->r[15], 0x8C00F000u, "stack balanced"); EQ(c->r[4], 0x40, "mach");
  /* prefetch of a store-queue address flushes it; others are no-ops */
  PROG(PREF(1), PREF(2), OCBWB(2));
  c->r[1] = 0xE0000020u; c->r[2] = 0x8C100000u; pref_addr = 0;
  RUN(3);
  EQ(pref_addr, 0xE0000020u, "pref to the store queue");
  /* I/O: non-RAM addresses go to the I/O layer, mirrors of RAM do not */
  PROG(MOVLS(1, 2), MOVLL(3, 4), MOVLL(5, 6));
  c->r[1] = 0xFF000010u; c->r[2] = 0x1234; c->r[4] = 0xA05F8000u; c->r[6] = 0xAD200000u;
  put32(0x8C200000u, 0xCAFEF00D);
  RUN(3);
  EQ(io_last_w, 0x1234, "io write"); EQ(c->r[3], 0xA05F8000u ^ 0x5A5A5A5A, "io read");
  EQ(c->r[5], 0xCAFEF00D, "P2 mirror 0xAD200000 = RAM +0x200000 (mirror 0x0D)");
}

static void t_fpu(void) {
  /* single precision arithmetic */
  PROG(FLDI1(0), FLDI1(1), FADD(1, 0), FMUL(1, 1), FSUB(1, 0), FDIV(1, 2), FCMPGT(1, 0), MOVT(1), FCMPEQ(0, 0),
       FNEG(0), FABS(3), FSQRT(4), FMAC(5, 6));
  c->fr.f[2] = 0.5f; c->fr.f[3] = -7.5f; c->fr.f[4] = 2.25f; c->fr.f[5] = 1.0f; c->fr.f[6] = 3.0f;
  c->pc = BASE; RUN(13);
  CHECK(c->fr.f[1] == 6.0f, "fadd/fmul/fsub/fdiv: %g", c->fr.f[1]);
  EQ(c->r[1], 1, "fcmp/gt"); EQ(c->t, 1, "fcmp/eq");
  CHECK(c->fr.f[0] == -1.0f, "fneg %g", c->fr.f[0]); CHECK(c->fr.f[3] == 7.5f, "fabs");
  CHECK(c->fr.f[4] == 1.5f, "fsqrt"); CHECK(c->fr.f[5] == -1.0f * 3.0f + 1.0f, "fmac = fr0*frm+frn: %g", c->fr.f[5]);
  /* float / ftrc (truncation, saturation), flds/fsts */
  PROG(MOVI(1, -7), LDSFPUL(1), FLOAT(2), FTRC(3), STSFPUL(4), FTRC(5), STSFPUL(6), FTRC(7), STSFPUL(8),
       FLDS(2), STSFPUL(9), FSTS(10));
  c->fr.f[3] = -2.75f; c->fr.f[5] = 3e9f; c->fr.u[7] = 0x7FC00000; c->pc = BASE; RUN(12);
  CHECK(c->fr.f[2] == -7.0f, "float"); EQ(c->r[4], (uint32_t)-2, "ftrc truncates"); EQ(c->r[6], 0x7FFFFFFF, "ftrc saturates");
  EQ(c->r[8], 0x80000000, "ftrc NaN"); EQ(c->r[9], u32f(-7.0f), "flds"); CHECK(c->fr.f[10] == -7.0f, "fsts");
  /* fmov single forms: @Rm, @Rn, @Rm+, @-Rn, @(R0,Rm), @(R0,Rn) */
  PROG(FMOVL(0, 1), FMOVLP(2, 1), FMOVSM(1, 3), FMOV(5, 2), MOVI(0, 12), FMOVL0(4, 1), FMOVS0(1, 0));
  c->r[1] = 0x8C200000u; c->fr.f[3] = 6.0f;
  put32(0x8C200000u, u32f(1.5f)); put32(0x8C200004u, u32f(2.5f)); put32(0x8C20000Cu, u32f(9.0f));
  RUN(7);
  CHECK(c->fr.f[0] == 1.5f, "fmov.s @r1"); CHECK(c->fr.f[2] == 1.5f, "fmov.s @r1+");
  CHECK(get32(0x8C200000u) == u32f(6.0f), "fmov.s fr3,@-r1"); EQ(c->r[1], 0x8C200000u, "post/pre balance");
  CHECK(c->fr.f[5] == 1.5f, "fmov fr2,fr5"); CHECK(c->fr.f[4] == 9.0f, "fmov.s @(r0,r1)");
  CHECK(get32(0x8C20000Cu) == u32f(1.5f), "fmov.s fr0,@(r0,r1)");
  /* fschg: pair moves, XD bank; frchg: bank swap */
  PROG(FSCHG, FMOVL(2, 1), FMOVL(3, 1), FMOV(4, 3), FSCHG, FRCHG);
  c->r[1] = 0x8C200000u;
  put32(0x8C200000u, 0x11111111); put32(0x8C200004u, 0x22222222);
  RUN(6);
  EQ(c->xf.u[2], 0x11111111, "frchg swapped: fmov.d @r1,dr2 (now xf)"); EQ(c->xf.u[3], 0x22222222, "pair order");
  EQ(c->fr.u[2], 0x11111111, "fmov.d @r1,xd2 (now fr)"); EQ(c->xf.u[4], 0x11111111, "fmov xd2,dr4");
  EQ(c->xf.u[5], 0x22222222, "fmov xd2,dr4 low"); EQ(c->fpscr & FPSCR_FR, FPSCR_FR, "FR set");
  /* fipr / ftrv / fsca / fsrra */
  PROG(FIPR(4, 0), FTRV(8), FSCA(12), FSRRA(14));
  c->fpul = 0x00014000u; /* a quarter turn (only the low 16 bits count) */
  for (int i = 0; i < 4; i++) { c->fr.f[i] = (float)(i + 1); c->fr.f[4 + i] = (float)(2 * i); c->fr.f[8 + i] = (float)(i + 1); }
  for (int i = 0; i < 16; i++) c->xf.f[i] = (i % 5 == 0) ? 2.0f : 0.0f; /* 2 * identity */
  c->xf.f[12] = 10.0f; /* translation x */
  c->fr.f[14] = 4.0f;
  RUN(4);
  CHECK(c->fr.f[7] == 1 * 0 + 2 * 2 + 3 * 4 + 4 * 6, "fipr: %g", c->fr.f[7]);
  CHECK(c->fr.f[8] == 2 * 1 + 10 * 4 && c->fr.f[9] == 4 && c->fr.f[10] == 6 && c->fr.f[11] == 8, "ftrv: %g %g %g %g",
        c->fr.f[8], c->fr.f[9], c->fr.f[10], c->fr.f[11]);
  CHECK(fabsf(c->fr.f[12] - 1.0f) < 1e-6f && fabsf(c->fr.f[13]) < 1e-6f, "fsca 0x4000: sin %g cos %g", c->fr.f[12], c->fr.f[13]);
  CHECK(fabsf(c->fr.f[14] - 0.5f) < 1e-6f, "fsrra(4) %g", c->fr.f[14]);
  /* double precision: PR = 1 */
  PROG(LDSFPSCR(1), MOVI(2, 3), LDSFPUL(2), FLOAT(2), FADD(2, 2), FCNVDS(2), STSFPUL(3), FTRC(2), STSFPUL(4));
  c->r[1] = 0x00080001u;
  RUN(9);
  EQ(c->fr.u[2], 0x40180000u, "float dr2 = 3.0, fadd dr2,dr2 = 6.0 (upper word in FR2)"); EQ(c->fr.u[3], 0, "DR lower word");
  EQ(c->r[3], u32f(6.0f), "fcnvds"); EQ(c->r[4], 6, "ftrc dr");
  PROG(LDSFPSCR(1), FCNVSD(4), STSFPSCR(5));
  c->r[1] = 0x00080001u; c->fpul = u32f(0.5f);
  RUN(3);
  EQ(c->fr.u[4], 0x3FE00000u, "fcnvsd"); EQ(c->r[5], 0x00080001u, "sts fpscr");
}

int main(void) {
  sh4_ram = calloc(1, 16 << 20);
  sh4_init_tables();
  t_table();
  t_move();
  t_arith();
  t_div();
  t_logic();
  t_shift();
  t_branch();
  t_system();
  t_fpu();
  printf("sh4 tests: %d checks, %d failures\n", checks, fails);
  return fails != 0;
}
