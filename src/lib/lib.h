/* Pen Pen port, S20: the libraries linked after the game code (the SHC C runtime, Shinobi's heap and start-up, the
 * backup library's file image), written as C: what other hand-written files call directly.
 *
 * Library code keeps its guest-RAM state where the original keeps it (the heap's free list, rand's seed, the G2 DMA
 * table): translated SDK code that is left, the HLE and the per-frame logs read it there. */
#ifndef PENPEN_LIB_H
#define PENPEN_LIB_H

#include "hw.h"

/* ---- SR's interrupt mask (IMASK, bits 4-7), as the library's critical sections set it with stc / ldc ---- */
#define SR_IMASK_CLEAR 0xFFFFFF0Fu
#define SR_IMASK_ALL   0xF0u
/* masks every interrupt; returns the mask level it replaced (0-15) */
static inline uint32_t sr_imask_raise(void) {
  uint32_t old = (sh4_get_sr(hw_cpu) >> 4) & 0xF;
  sh4_set_sr(hw_cpu, ((sh4_get_sr(hw_cpu) & SR_IMASK_CLEAR) | SR_IMASK_ALL) & SR_MASK);
  return old;
}
/* sets the mask level (the low 4 bits of level) back */
static inline void sr_imask_set(uint32_t level) {
  sh4_set_sr(hw_cpu, (((level & 0xF) << 4) | (sh4_get_sr(hw_cpu) & SR_IMASK_CLEAR)) & SR_MASK);
}

/* ---- the C runtime (shc_rt.c) ---- */
int32_t rt_sdiv(int32_t dividend, int32_t divisor);
gaddr rt_memmove(gaddr dst, gaddr src, uint32_t n);
gaddr rt_memset(gaddr dst, int32_t c, uint32_t n);
int32_t rt_memcmp(gaddr a, gaddr b, uint32_t n);
int32_t rt_rand(void);
/* the float maths (shc_math.c) */
f32 rt_acosf_checked(f32 x);
f32 rt_frexpf(f32 x, int32_t *e);
f32 rt_modff(f32 x, f32 *whole);
void rt_srand(uint32_t seed);

/* ---- Shinobi's heap (sy/heap.c) ---- */
void sy_malloc_init(gaddr heap, uint32_t size);
gaddr sy_malloc(uint32_t size);
void sy_free(gaddr block);

/* ---- the library registry of the start-up (sy/lib_registry.c) ---- */
int32_t lib_registry_add_all(gaddr header);
int32_t lib_registry_remove_all(gaddr header);

#endif
