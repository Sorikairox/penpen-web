#include <stdio.h>
/* Pen Pen port, S11: the runtime half of src/include/hw.h: entering and leaving hand-written functions, their calls
 * into guest code, and the unwind/rewind records of the resumable ones (docs/port/DECOMP.md 3-4). */
#include "rt.h"
#include "../../src/include/hw.h"

SH4 *hw_cpu;
int hw_ret_set;
int hw_last_ret_set;      /* the RET flag of the last hand-written function that returned (difftest.c reads it) */
int hw_call_arity = -1;   /* arguments of the call hw_call is making (-1: a call made by translated code) */
int hw_call_fargs;        /* S13: float arguments (FR4...) of that call (FARGS) */
static uint32_t hw_rew_target; /* the call a rewinding resumable function must repeat */

/* the return address hand-written code gives the functions it calls: not code (as rt_guest_call's 0x8C000F00) */
#define HW_RETURN 0x8C000F08u
/* the record of a resumable adapter (its entry PR), below the body's records */
#define HW_SITE_ADAPTER (-10)

uint32_t hw_call(gaddr fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
  SH4 *c = hw_cpu;
  if (rt_unw) { rt_log("hw call to %08x while the frame unwinds (a yielding call not marked HW_YIELDING)", fn); rt_stop("hw call"); }
  if (rt_rew) {
    if (RT_NORM(fn) != RT_NORM(hw_rew_target)) {
      rt_log("hw rewind: the call that yielded went to %08x, the rewind calls %08x", hw_rew_target, fn);
      rt_stop("hw rewind");
    }
  } else {
    if (n > 0) c->r[4] = a0;
    if (n > 1) c->r[5] = a1;
    if (n > 2) c->r[6] = a2;
    if (n > 3) c->r[7] = a3;
    c->pr = HW_RETURN;
  }
  hw_rew_target = fn;
  hw_call_arity = n;
  rt_call(c, fn);
  hw_call_arity = -1;
  hw_call_fargs = 0;
  if (rt_unw) { /* the frame is unwinding: the caller saves its site (HW_YIELDING) or stops */
    hw_rew_target = fn; /* deeper hand-written calls changed it: the caller's record holds this call */
    return 0;
  }
  if (c->pc != HW_RETURN) rt_badret(c, HW_RETURN, fn);
  return c->r[0];
}

uint32_t hw_call_stack(gaddr fn, int n, const uint32_t *args) {
  SH4 *c = hw_cpu;
  int extra = n > 4 ? n - 4 : 0;
  if (rt_rew) { rt_log("hw call to %08x with stack arguments while rewinding", fn); rt_stop("hw call"); }
  c->r[15] -= 4u * (uint32_t)extra;
  for (int i = 0; i < extra; i++) wr32(c->r[15] + 4u * (uint32_t)i, args[4 + i]);
  uint32_t r = hw_call(fn, n > 4 ? 4 : n, args[0], n > 1 ? args[1] : 0, n > 2 ? args[2] : 0, n > 3 ? args[3] : 0);
  hw_call_arity = -1;
  c->r[15] += 4u * (uint32_t)extra;
  return r;
}

gaddr hw_stack_alloc(uint32_t bytes) {
  uint32_t o = hw_cpu->r[15];
  hw_cpu->r[15] -= (bytes + 3) & ~3u;
  sh4_stack_lowered(o, hw_cpu->r[15]); /* S19: a fresh frame reads as zero, as the translation's (sh4_ops.h) */
  return hw_cpu->r[15];
}
void hw_stack_free(uint32_t bytes) { hw_cpu->r[15] += (bytes + 3) & ~3u; }

/* S18: a fragment (HW_EXPORT_FRAGMENT) was entered on its own: its parent's C was supposed to cover it */
void hw_fragment_entered(uint32_t addr, uint32_t parent) {
  static char why[96];
  snprintf(why, sizeof why, "hw %08x: a fragment of %08x entered on its own", addr, parent);
  rt_log("%s", why);
  rt_stop(why);
}
void hw_enter(hw_entry *e, SH4 *c, uint32_t addr, int resumable) {
  e->c = c;
  e->addr = addr;
  e->resumable = resumable;
  e->prev_cpu = hw_cpu;
  e->testing = 0;
  hw_cpu = c;
  hw_ret_set = 0;
  if (rt_rew) {
    uint32_t t, saved[2];
    if (!resumable) { rt_log("hw %08x: rewound into, but not resumable", addr); rt_stop("hw rewind"); }
    rt_rew_pop_blob(addr, &t, saved, sizeof saved);
    e->pr = saved[0];
    e->testing = (int)saved[1]; /* a difftest sample that spans frames goes on */
    return;
  }
  e->pr = c->pr;
  if (hwt_enabled && !rt_sandbox && !(resumable & 2)) e->testing = hwt_begin(c, addr);
}

void hw_leave(hw_entry *e) {
  hw_last_ret_set = hw_ret_set;
  hw_ret_set = 0;
  hw_cpu = e->prev_cpu;
  if (rt_unw) {
    if (!e->resumable) {
      rt_log("hw %08x: a call in it yielded (the frame ended inside it) but it is not written resumable", e->addr);
      rt_stop("hw yield");
    }
    uint32_t saved[2] = {e->pr, (uint32_t)e->testing};
    rt_unw_save_blob(e->addr, HW_SITE_ADAPTER, 0, saved, sizeof saved);
    return;
  }
  /* as the original's rts: back to the caller, PR as it was on entry */
  e->c->pc = e->pr;
  e->c->pr = e->pr;
  if (e->testing) hwt_end(e->testing);
}

/* the locals of a resumable body, packed into the record */
int hw_resume(uint32_t fn, hw_var *v, int nv) {
  uint8_t blob[64];
  int n = 0;
  for (int i = 0; i < nv; i++) n += (int)v[i].n;
  if (n > (int)sizeof blob) { rt_log("hw %08x: %d bytes of locals", fn, n); rt_stop("hw resume"); }
  int site = rt_rew_pop_blob(fn, &hw_rew_target, blob, n);
  for (int i = 0, o = 0; i < nv; o += (int)v[i].n, i++) memcpy(v[i].p, blob + o, v[i].n);
  return site;
}
void hw_suspend(uint32_t fn, int site, hw_var *v, int nv) {
  uint8_t blob[64];
  int n = 0;
  for (int i = 0; i < nv; i++) {
    if (n + (int)v[i].n > (int)sizeof blob) { rt_log("hw %08x: too many locals to save", fn); rt_stop("hw suspend"); }
    memcpy(blob + n, v[i].p, v[i].n);
    n += (int)v[i].n;
  }
  rt_unw_save_blob(fn, site, hw_rew_target, blob, n);
}
