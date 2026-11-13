/* Pen Pen port, S11: the interface between the hand-written game code (src/) and the runtime.
 *
 * The hand-written C replaces translated functions one at a time (docs/port/DECOMP.md). It shares guest RAM with the
 * translated code that is left, so every game structure keeps its guest layout: structs here are declared with the
 * exact field offsets of the original (checked with HW_OFFSET), pointers inside them are 32-bit guest addresses
 * (gaddr), and a gaddr becomes a host pointer only through hw_ptr() / the typed helpers of each header.
 *
 * A hand-written function is entered by the translated code like the function it replaces: through the C symbol
 * f_<address> that the recompiler leaves out of its output (recomp.py --hand). HW_EXPORT builds that adapter from the
 * register state (ARG0..ARG3 = r4..r7, RET(v) = r0); the readable function itself takes and returns normal C values.
 * Calls into guest code that is still translated (or the SDK, or the HLE) go through hw_call(), which follows the
 * SH-4 calling convention (r4-r7, PR, r0) and the runtime's dispatch.
 *
 * Resumable functions: a frame ends inside njWaitVSync; the runtime unwinds every C function on the call chain and
 * rewinds into it on the next frame (docs/port/RECOMP.md 8). Hand-written functions on such a chain declare the
 * locals that live across a call that can yield (HW_RESUMABLE) and mark those calls (HW_YIELDING); the macros save
 * the locals in the runtime's unwind record and jump back to the call on the next frame. A hand-written function that
 * is not declared resumable stops the run if a call in it yields (a check, not a silent bug).
 */
#ifndef PENPEN_HW_H
#define PENPEN_HW_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../../port/recomp/rt_gen.h"
#include "fpu.h" /* S13: f32, the game's float arithmetic */

/* ---- guest memory ---- */
typedef uint32_t gaddr; /* a guest address (main RAM is 0x8C000000-0x8CFFFFFF; 0 = NULL) */

/* a host pointer to guest RAM (main RAM only: hardware registers go through hw_read32/hw_write32) */
static inline void *hw_ptr(gaddr a) { return sh4_ram + (a & 0x00FFFFFFu); }
/* and back: the P1 address (0x8Cxxxxxx) of a host pointer into guest RAM */
static inline gaddr hw_addr(const void *p) { return 0x8C000000u | (uint32_t)((const uint8_t *)p - sh4_ram); }
/* a variable of the original program at a fixed guest address (its .data/.bss) */
#define GUEST_VAR(type, addr) (*(type *)hw_ptr(addr))
/* S13: a word / a float of guest RAM by address (fields of objects whose class is not declared yet) */
#define U32_AT(a) (*(uint32_t *)hw_ptr(a))
#define F32_AT(a) (*(f32 *)hw_ptr(a))
/* hardware registers and other non-RAM areas: through the runtime's I/O model */
static inline uint32_t hw_read32(gaddr a) { return rd32(a); }
static inline void hw_write32(gaddr a, uint32_t v) { wr32(a, v); }
/* S16: a byte that may lie outside main RAM (a structure the BIOS HLE places at address 0, ...) */
static inline uint8_t hw_read8(gaddr a) { return rd8(a); }

/* layout checks: a struct shared with translated code must keep the original's offsets */
#define HW_OFFSET(type, field, off) _Static_assert(__builtin_offsetof(type, field) == (off), #type "." #field)
#define HW_SIZE(type, size) _Static_assert(sizeof(type) == (size), "sizeof " #type)

/* ---- the CPU the hand-written code runs on (the live one, or a difftest sandbox's) ---- */
extern SH4 *hw_cpu;

/* ---- calls into guest code (translated, SDK, HLE) ----
 * hw_call(fn, n, a0..a3): n = the number of arguments the callee takes (recorded for the difftest's comparison of
 * the SDK calls); returns r0. When the frame is being rewound it repeats the call that yielded (checked). */
uint32_t hw_call(gaddr fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
#define CALL0(fn) hw_call((fn), 0, 0, 0, 0, 0)
#define CALL1(fn, a) hw_call((fn), 1, (uint32_t)(a), 0, 0, 0)
#define CALL2(fn, a, b) hw_call((fn), 2, (uint32_t)(a), (uint32_t)(b), 0, 0)
#define CALL3(fn, a, b, c3) hw_call((fn), 3, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c3), 0)
#define CALL4(fn, a, b, c3, d) hw_call((fn), 4, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c3), (uint32_t)(d))
/* S13: float arguments go in FR4, FR5, ... (SHC's convention; integer and pointer arguments still fill r4-r7):
 * FARGS1..FARGS4 before the CALLn (the difftest compares the FR arguments of HLE calls they declare); a float
 * result is in FR0 after the call (hw_fret) */
extern int hw_call_fargs;
#define FARG_(i, v) (hw_cpu->fr.u[4 + (i)] = f32_bits(v))
#define FARGS1(a) (FARG_(0, a), hw_call_fargs = 1)
#define FARGS2(a, b) (FARG_(0, a), FARG_(1, b), hw_call_fargs = 2)
#define FARGS3(a, b, c3) (FARG_(0, a), FARG_(1, b), FARG_(2, c3), hw_call_fargs = 3)
#define FARGS4(a, b, c3, d) (FARG_(0, a), FARG_(1, b), FARG_(2, c3), FARG_(3, d), hw_call_fargs = 4)
static inline f32 hw_fret(void) { return F32_BITS(hw_cpu->fr.u[0]); }
/* a callee with more than 4 arguments: the 5th and later go on the guest stack (@r15, @(4,r15), ...), as SHC does */
uint32_t hw_call_stack(gaddr fn, int n, const uint32_t *args);
#define CALL5(fn, a, b, c3, d, e) \
  hw_call_stack((fn), 5, (const uint32_t[]){(uint32_t)(a), (uint32_t)(b), (uint32_t)(c3), (uint32_t)(d), (uint32_t)(e)})

/* a local whose address goes to guest code (an SDK function writes a handle through it) lives on the guest stack:
 * hw_stack_alloc lowers r15 (4-byte units), hw_stack_free must give the same size back before the function returns */
gaddr hw_stack_alloc(uint32_t bytes);
void hw_stack_free(uint32_t bytes);

/* ---- adapters: the translated code's view of a hand-written function ---- */
typedef struct hw_entry {
  SH4 *c, *prev_cpu;
  uint32_t addr, pr;
  int resumable, testing;
} hw_entry;
void hw_enter(hw_entry *e, SH4 *c, uint32_t addr, int resumable);
void hw_leave(hw_entry *e);
extern int hw_ret_set; /* RET was used: the difftest compares r0 */

#define ARG0 (hw_e_.c->r[4])
#define ARG1 (hw_e_.c->r[5])
#define ARG2 (hw_e_.c->r[6])
#define ARG3 (hw_e_.c->r[7])
/* the 5th and later arguments, on the guest stack at entry */
#define ARG4 rd32(hw_e_.c->r[15])
#define ARG5 rd32(hw_e_.c->r[15] + 4)
#define ARG6 rd32(hw_e_.c->r[15] + 8)
#define RET(v) do { uint32_t hw_r_ = (uint32_t)(v); if (!rt_unw) { hw_e_.c->r[0] = hw_r_; hw_ret_set |= 1; } } while (0)
/* S13: float arguments at entry (FR4, FR5, ...) and a float result (FR0: the difftest compares it) */
#define FARG0 F32_BITS(hw_e_.c->fr.u[4])
#define FARG1 F32_BITS(hw_e_.c->fr.u[5])
#define FARG2 F32_BITS(hw_e_.c->fr.u[6])
#define FARG3 F32_BITS(hw_e_.c->fr.u[7])
#define FARG4 F32_BITS(hw_e_.c->fr.u[8])
#define FARG5 F32_BITS(hw_e_.c->fr.u[9])
#define RETF(v) do { f32 hw_f_ = (v); if (!rt_unw) { hw_e_.c->fr.u[0] = f32_bits(hw_f_); hw_ret_set |= 2; } } while (0)

/* HW_EXPORT(8c0102c0, task_add_child(TASK(ARG0), TASK(ARG1))): the address in lower-case hex, without 0x, then the
 * statement that runs the readable function on the arguments. recomp.py --hand finds these lines. */
#define HW_EXPORT(addr, stmt) \
  void f_##addr(SH4 *c) { hw_entry hw_e_; hw_enter(&hw_e_, c, 0x##addr##u, 0); stmt; hw_leave(&hw_e_); }
/* the same for a function on a yielding chain (its body uses HW_RESUMABLE / HW_YIELDING) */
#define HW_EXPORT_RESUMABLE(addr, stmt) \
  void f_##addr(SH4 *c) { hw_entry hw_e_; hw_enter(&hw_e_, c, 0x##addr##u, HW_RESUMES); stmt; hw_leave(&hw_e_); }
/* main: resumable, and never sampled by the difftest (it never returns; the whole-run comparison covers it) */
#define HW_EXPORT_MAIN(addr, stmt) \
  void f_##addr(SH4 *c) { hw_entry hw_e_; hw_enter(&hw_e_, c, 0x##addr##u, HW_RESUMES | HW_NOT_SAMPLED); stmt; hw_leave(&hw_e_); }
#define HW_RESUMES 1
#define HW_NOT_SAMPLED 2

/* ---- resumable bodies ---- */
typedef struct { void *p; uint32_t n; } hw_var;
int hw_resume(uint32_t fn, hw_var *v, int nv);               /* restore the locals; the site to go back to */
void hw_suspend(uint32_t fn, int site, hw_var *v, int nv);   /* save them (the frame is unwinding) */

#define HW_VAR_(x) {(void *)&(x), sizeof(x)},
#define HW_SITE_(n) case n: goto hw_site_##n;
/* a small FOR_EACH (up to 8 items) */
#define HW_EACH_1(m, x) m(x)
#define HW_EACH_2(m, x, ...) m(x) HW_EACH_1(m, __VA_ARGS__)
#define HW_EACH_3(m, x, ...) m(x) HW_EACH_2(m, __VA_ARGS__)
#define HW_EACH_4(m, x, ...) m(x) HW_EACH_3(m, __VA_ARGS__)
#define HW_EACH_5(m, x, ...) m(x) HW_EACH_4(m, __VA_ARGS__)
#define HW_EACH_6(m, x, ...) m(x) HW_EACH_5(m, __VA_ARGS__)
#define HW_EACH_7(m, x, ...) m(x) HW_EACH_6(m, __VA_ARGS__)
#define HW_EACH_8(m, x, ...) m(x) HW_EACH_7(m, __VA_ARGS__)
#define HW_EACH_N_(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) HW_EACH_##N
#define HW_EACH(m, ...) HW_EACH_N_(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)(m, __VA_ARGS__)

/* HW_RESUMABLE(0x8C010306, task, link, paused): the function's guest address and every local that is live across a
 * yielding call; on a rewind they are restored here. HW_RESUME_AT(0, 1, 2) then jumps back to the call that yielded. */
#define HW_RESUMABLE(addr, ...) \
  hw_var hw_v_[] = {HW_EACH(HW_VAR_, __VA_ARGS__)}; \
  const uint32_t hw_fn_ = (addr); \
  const int hw_site_ = rt_rew ? hw_resume(hw_fn_, hw_v_, (int)(sizeof hw_v_ / sizeof hw_v_[0])) : -1
#define HW_RESUME_AT(...) switch (hw_site_) { HW_EACH(HW_SITE_, __VA_ARGS__) default: break; }
/* HW_RESUMABLE0(addr): the same for a function with no local to keep */
#define HW_RESUMABLE0(addr) \
  hw_var hw_v_[1] = {{0, 0}}; \
  const uint32_t hw_fn_ = (addr); \
  const int hw_site_ = rt_rew ? hw_resume(hw_fn_, hw_v_, 0) : -1
/* a call that can yield: on the way out (rt_unw) the locals are saved and the function returns. On the way back
 * in, the statement runs again (the callee rewinds too): its expressions may only use the saved locals and what is
 * computed from them before HW_RESUME_AT. One statement (safe after an if). */
#define HW_NV_ ((int)(sizeof hw_v_ / sizeof hw_v_[0]) - (hw_v_[0].p == 0))
#define HW_YIELDING(n, stmt) \
  do { hw_site_##n: stmt; if (rt_unw) { hw_suspend(hw_fn_, (n), hw_v_, HW_NV_); return; } } while (0)
#define HW_YIELDING_RET(n, stmt, dummy) \
  do { hw_site_##n: stmt; if (rt_unw) { hw_suspend(hw_fn_, (n), hw_v_, HW_NV_); return (dummy); } } while (0)

#endif
