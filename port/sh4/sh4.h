/* Pen Pen port, S3: SH-4 CPU state and guest memory access.
 *
 * Our own code, written from the public SH-4 documentation (Renesas SH-4 Software Manual). Shared by the
 * interpreter (sh4_interp.c, the test oracle) and the C emitted by the recompiler (port/recomp/recomp.py):
 * both execute the non-branch instructions through the same op_* functions of sh4_ops.h, and each handles
 * control flow (delay slots, branches, calls) in its own way. See docs/port/RECOMP.md.
 */
#ifndef SH4_H
#define SH4_H

#include <stdint.h>
#include <string.h>

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "host must be little-endian (wasm32, x86-64, arm64)"
#endif

/* FPSCR bits */
#define FPSCR_RM 0x00000003u
#define FPSCR_DN 0x00040000u
#define FPSCR_PR 0x00080000u
#define FPSCR_SZ 0x00100000u
#define FPSCR_FR 0x00200000u
#define FPSCR_MASK 0x003FFFFFu

/* SR bits (T, S, Q, M are kept in their own fields) */
#define SR_T 0x00000001u
#define SR_S 0x00000002u
#define SR_IMASK 0x000000F0u
#define SR_Q 0x00000100u
#define SR_M 0x00000200u
#define SR_FD 0x00008000u
#define SR_BL 0x10000000u
#define SR_RB 0x20000000u
#define SR_MD 0x40000000u
#define SR_MASK 0x700083F3u

typedef union {
  float f[16];
  uint32_t u[16];
} sh4_fbank;

typedef struct SH4 {
  uint32_t r[16];
  uint32_t rbank[8]; /* the inactive bank of R0-R7 */
  uint32_t t, s, q, m;
  uint32_t sr;       /* the other SR bits (MD, RB, BL, FD, IMASK) */
  uint32_t gbr, vbr, ssr, spc, sgr, dbr;
  uint32_t mach, macl, pr, pc;
  sh4_fbank fr;      /* the current FPU bank (FR0-FR15) */
  sh4_fbank xf;      /* the other bank (XF0-XF15) */
  uint32_t fpscr, fpul;
  /* run-time bookkeeping, not architectural */
  int64_t budget;    /* back-edge / step budget (diff harness) */
  uint32_t last;     /* interpreter: kind of the last control transfer (SH4_BR_*) */
  uint32_t fault;    /* interpreter: address of an illegal instruction */
} SH4;

#define SH4_BR_NONE 0
#define SH4_BR_JUMP 1 /* bra, braf, jmp, bt, bf */
#define SH4_BR_CALL 2 /* bsr, bsrf, jsr */
#define SH4_BR_RET 3  /* rts */

/* ---- guest memory ----
 * Main RAM: 16 MiB at physical 0x0C000000, seen through P0/P1/P2/P3 (0x0C/0x8C/0xAC/0xCC...) and its
 * mirrors 0x0D-0x0F. Everything else goes to sh4_io_* (hardware registers, store queues, VRAM, ...).
 * With SH4_MEM_HOOK defined (the differential harness), every access goes to the hook functions. */
extern uint8_t *sh4_ram;

uint32_t sh4_io_r32(uint32_t a);
uint16_t sh4_io_r16(uint32_t a);
uint8_t sh4_io_r8(uint32_t a);
void sh4_io_w32(uint32_t a, uint32_t v);
void sh4_io_w16(uint32_t a, uint16_t v);
void sh4_io_w8(uint32_t a, uint8_t v);

#define SH4_INLINE static inline __attribute__((always_inline))

SH4_INLINE int sh4_is_ram(uint32_t a) { return ((a >> 26) & 7) == 3 && (a >> 29) != 7; }

#ifdef SH4_MEM_HOOK
#define rd32 sh4_io_r32
#define rd16 sh4_io_r16
#define rd8 sh4_io_r8
#define wr32 sh4_io_w32
#define wr16 sh4_io_w16
#define wr8 sh4_io_w8
#else
#ifdef PENPEN_DBG /* S19: writes into a watched RAM range are reported (debug builds only) */
extern uint32_t dbg_w_lo, dbg_w_hi;
void dbg_write(uint32_t a, uint32_t v, int size);
#define DBG_W(a, v, n) if (__builtin_expect(((a) & 0xFFFFFF) >= dbg_w_lo && ((a) & 0xFFFFFF) < dbg_w_hi, 0)) dbg_write(a, v, n)
#else
#define DBG_W(a, v, n)
#endif
SH4_INLINE uint32_t rd32(uint32_t a) {
  if (sh4_is_ram(a)) { uint32_t v; memcpy(&v, sh4_ram + (a & 0xFFFFFC), 4); return v; }
  return sh4_io_r32(a);
}
SH4_INLINE uint16_t rd16(uint32_t a) {
  if (sh4_is_ram(a)) { uint16_t v; memcpy(&v, sh4_ram + (a & 0xFFFFFE), 2); return v; }
  return sh4_io_r16(a);
}
SH4_INLINE uint8_t rd8(uint32_t a) {
  if (sh4_is_ram(a)) return sh4_ram[a & 0xFFFFFF];
  return sh4_io_r8(a);
}
SH4_INLINE void wr32(uint32_t a, uint32_t v) {
  if (sh4_is_ram(a)) { DBG_W(a, v, 4); memcpy(sh4_ram + (a & 0xFFFFFC), &v, 4); return; }
  sh4_io_w32(a, v);
}
SH4_INLINE void wr16(uint32_t a, uint16_t v) {
  if (sh4_is_ram(a)) { DBG_W(a, v, 2); memcpy(sh4_ram + (a & 0xFFFFFE), &v, 2); return; }
  sh4_io_w16(a, v);
}
SH4_INLINE void wr8(uint32_t a, uint8_t v) {
  if (sh4_is_ram(a)) { DBG_W(a, v, 1); sh4_ram[a & 0xFFFFFF] = v; return; }
  sh4_io_w8(a, v);
}
#endif

/* SR as a whole */
SH4_INLINE uint32_t sh4_get_sr(const SH4 *c) {
  return c->sr | c->t | (c->s << 1) | (c->q << 8) | (c->m << 9);
}
void sh4_set_sr(SH4 *c, uint32_t v);    /* switches register banks when RB changes */
void sh4_set_fpscr(SH4 *c, uint32_t v); /* switches FPU banks when FR changes */

/* ---- interpreter (sh4_interp.c) ---- */
void sh4_init_tables(void);
const char *sh4_op_name(uint16_t op); /* "MOV_RR", ..., or NULL for an illegal encoding */
int sh4_op_index(uint16_t op);        /* index in sh4_insns.def, or -1 */
/* Execute the instruction at c->pc (and its delay slot). 0 = ok, -1 = illegal instruction (c->fault),
 * -2 = unsupported (rte/sleep/trapa/ldtlb), -3 = illegal slot instruction. */
int sh4_step(SH4 *c);
void sh4_reset(SH4 *c);

#endif
