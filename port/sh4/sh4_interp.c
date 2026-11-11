/* Pen Pen port, S3: SH-4 interpreter (the test oracle, and the run-time fallback for code the recompiler
 * did not translate). Decoding comes from sh4_insns.def, the non-branch semantics from sh4_ops.h; this file
 * adds the control transfers with their delay slots, as the SH-4 Software Manual describes them:
 * a delayed branch computes its target (and sets PR for calls) before the slot instruction runs, the
 * conditional delayed branches test T before the slot. */
#include "sh4_ops.h"

enum {
#define I(name, pat, kind) IDX_##name,
#include "sh4_insns.def"
#undef I
  IDX_COUNT
};

static const struct {
  const char *name, *pat;
  int br;
} insns[] = {
#define OP 0
#define BR 1
#define I(name, pat, kind) {#name, pat, kind},
#include "sh4_insns.def"
#undef I
#undef OP
#undef BR
};

static int16_t optab[65536];
static int inited;

void sh4_init_tables(void) {
  if (inited) return;
  inited = 1;
  for (int op = 0; op < 65536; op++) optab[op] = -1;
  for (int i = 0; i < IDX_COUNT; i++) {
    uint16_t mask = 0, val = 0;
    for (int b = 0; b < 16; b++) {
      char ch = insns[i].pat[b];
      if (ch == '0' || ch == '1') { mask |= 1u << (15 - b); if (ch == '1') val |= 1u << (15 - b); }
    }
    for (int op = 0; op < 65536; op++)
      if ((op & mask) == val) {
        if (optab[op] >= 0) optab[op] = -2; /* conflict: reported by sh4_table_conflicts */
        else optab[op] = (int16_t)i;
      }
  }
}

int sh4_table_conflicts(void) {
  sh4_init_tables();
  int n = 0;
  for (int op = 0; op < 65536; op++) n += optab[op] == -2;
  return n;
}

int sh4_op_index(uint16_t op) {
  sh4_init_tables();
  return optab[op] >= 0 ? optab[op] : -1;
}

const char *sh4_op_name(uint16_t op) {
  int i = sh4_op_index(op);
  return i >= 0 ? insns[i].name : NULL;
}

static void swap_rbank(SH4 *c) {
  for (int i = 0; i < 8; i++) { uint32_t t = c->r[i]; c->r[i] = c->rbank[i]; c->rbank[i] = t; }
}

void sh4_set_sr(SH4 *c, uint32_t v) {
  int oldb = (c->sr & SR_MD) && (c->sr & SR_RB);
  int newb = (v & SR_MD) && (v & SR_RB);
  if (oldb != newb) swap_rbank(c);
  c->t = v & 1;
  c->s = (v >> 1) & 1;
  c->q = (v >> 8) & 1;
  c->m = (v >> 9) & 1;
  c->sr = v & (SR_MASK & ~(SR_T | SR_S | SR_Q | SR_M));
}

void sh4_set_fpscr(SH4 *c, uint32_t v) {
  if ((v ^ c->fpscr) & FPSCR_FR) { sh4_fbank t = c->fr; c->fr = c->xf; c->xf = t; }
  c->fpscr = v;
}

void sh4_reset(SH4 *c) {
  sh4_init_tables();
  memset(c, 0, sizeof *c);
  c->sr = SR_MD | SR_RB | SR_BL | SR_IMASK;
  c->fpscr = 0x00040001u;
  c->pc = 0xA0000000u;
}

/* Execute one non-branch instruction (also used for delay slots). 0 ok, -1 illegal, -3 branch in slot. */
static int exec_op(SH4 *c, uint32_t pc, uint16_t op, int slot) {
  int i = optab[op];
  if (i < 0) { c->fault = pc; return -1; }
  switch (i) {
#define OP(name) case IDX_##name: op_##name(c, op, pc); return 0;
#define BR(name) case IDX_##name: c->fault = pc; return slot ? -3 : -2;
#define I(name, pat, kind) kind(name)
#include "sh4_insns.def"
#undef I
#undef OP
#undef BR
  }
  return -1;
}

#define D8 ((uint32_t)(int32_t)(int8_t)(op & 0xFF) * 2)
#define D12 ((uint32_t)(((int32_t)((op & 0xFFF) << 20)) >> 19))

int sh4_step(SH4 *c) {
  uint32_t pc = c->pc, tgt;
  uint16_t op = rd16(pc);
  int i = optab[op], r;
  c->last = SH4_BR_NONE;
  if (i < 0) { c->fault = pc; return -1; }
  switch (i) {
  case IDX_BT:
  case IDX_BF:
    if (c->t == (i == IDX_BT)) { c->pc = pc + 4 + D8; c->last = SH4_BR_JUMP; }
    else c->pc = pc + 2;
    return 0;
  case IDX_BTS:
  case IDX_BFS: {
    int take = c->t == (i == IDX_BTS);
    tgt = pc + 4 + D8;
    if ((r = exec_op(c, pc + 2, rd16(pc + 2), 1))) return r;
    if (take) { c->pc = tgt; c->last = SH4_BR_JUMP; }
    else c->pc = pc + 4;
    return 0;
  }
  case IDX_BRA: tgt = pc + 4 + D12; c->last = SH4_BR_JUMP; break;
  case IDX_BSR: tgt = pc + 4 + D12; c->pr = pc + 4; c->last = SH4_BR_CALL; break;
  case IDX_BRAF: tgt = pc + 4 + c->r[(op >> 8) & 15]; c->last = SH4_BR_JUMP; break;
  case IDX_BSRF: tgt = pc + 4 + c->r[(op >> 8) & 15]; c->pr = pc + 4; c->last = SH4_BR_CALL; break;
  case IDX_JMP: tgt = c->r[(op >> 8) & 15]; c->last = SH4_BR_JUMP; break;
  case IDX_JSR: tgt = c->r[(op >> 8) & 15]; c->pr = pc + 4; c->last = SH4_BR_CALL; break;
  case IDX_RTS: tgt = c->pr; c->last = SH4_BR_RET; break;
  default:
    if ((r = exec_op(c, pc, op, 0))) return r;
    c->pc = pc + 2;
    return 0;
  }
  /* delayed unconditional transfer: the slot runs with the target already fixed */
  if ((r = exec_op(c, pc + 2, rd16(pc + 2), 1))) { c->last = SH4_BR_NONE; return r; }
  c->pc = tgt;
  return 0;
}
