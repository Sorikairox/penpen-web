/* Pen Pen port, S13: the game's random numbers (core/random.c).
 *
 * Two sources: Ninja's generator, seeded from a running sum of the SH-4 timer counter, and the C runtime's rand()
 * (0..32767). Both give a float in [0, range). */
#include "hw.h"
#include "sdk.h"

#define g_random_seed GUEST_VAR(uint32_t, 0x8C08A83Cu) /* a running sum of timer readings */

static uint32_t syTmrGetCount(void) { return CALL0(0x8C09F90Eu); }
/* the Ninja generator, given a seed in FR4; a value in [0, 1) */
static f32 njRandom(f32 seed) { FARGS1(seed); CALL0(0x8C0C4B14u); return hw_fret(); }
static int32_t rt_rand(void) { return (int32_t)CALL0(0x8C062AB0u); } /* func_8C062AB0: the C runtime's rand() */

/* was func_8C0107C4: |range * njRandom(seed / 65536)|, the seed stirred with three timer readings */
f32 random_timer_scaled(f32 range) {
  uint32_t low = syTmrGetCount() & 0xFFFFu;
  uint32_t high = syTmrGetCount() >> 8;
  uint32_t third = syTmrGetCount();
  g_random_seed = low * high + g_random_seed + (third << 8);
  f32 r = njRandom(fdiv(f32_from_int((int32_t)g_random_seed), F32(65536.0f)));
  return fabs_(fmul(range, r));
}
HW_EXPORT(8c0107c4, RETF(random_timer_scaled(FARG0)))

/* was func_8C010854: range * rand() / 32768 */
f32 random_scaled(f32 range) {
  int32_t r = rt_rand();
  return fmul(range, fdiv(f32_from_int(r), F32(32768.0f)));
}
HW_EXPORT(8c010854, RETF(random_scaled(FARG0)))
