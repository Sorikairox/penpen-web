/* Pen Pen port, S20: the SHC C runtime's integer and memory helpers (region shc_rt of port/symbols/functions.tsv):
 * the division helper with its own register convention, the C++ array construction / destruction loops, memmove,
 * memset, memcmp, rand / srand. The float maths are in shc_math.c, the G2-bus copy in g2.c. */
#include "lib.h"

#define g_errno     GUEST_VAR(int32_t, 0x8C1BAC68u)
#define EDIVZERO    0x44E                           /* the runtime's errno for a division by zero */
#define g_rand_seed GUEST_VAR(uint32_t, 0x8C09989Cu)

/* ---- division ---- */

/* was func_8C062464: SHC's signed 32-bit division (the compiler calls it with the dividend in r1 and the divisor in
 * r0; the quotient comes back in r0 and r1, r2 / r3 kept). The quotient is truncated toward zero; a divisor of 0 sets
 * errno and gives 0 (r1 left as it came). */
int32_t rt_sdiv(int32_t dividend, int32_t divisor) {
  if (divisor == 0) {
    g_errno = EDIVZERO;
    return 0;
  }
  return (int32_t)((int64_t)dividend / divisor); /* -2^31 / -1 wraps to -2^31, as the shift-and-subtract loop gives */
}
/* the register convention: r1 / r0, the quotient in r0 and (but after a division by zero) in r1 */
static uint32_t rt_sdiv_r1_r0(uint32_t *r1, uint32_t r0) {
  int32_t q = rt_sdiv((int32_t)*r1, (int32_t)r0);
  if (r0 != 0) *r1 = (uint32_t)q;
  return (uint32_t)q;
}
HW_EXPORT(8c062464, RET(rt_sdiv_r1_r0(&REG(1), REG(0))))

/* ---- C++ arrays: the element count and size the runtime keeps in front of an array made by new[] ---- */
typedef struct ArrayHeader {
  uint32_t element_size; /* -8 */
  uint32_t count;        /* -4 */
} ArrayHeader;
HW_SIZE(ArrayHeader, 8);
static ArrayHeader *array_header(gaddr array) { return (ArrayHeader *)hw_ptr(array - sizeof(ArrayHeader)); }

/* was func_8C06270C: delete[]'s destruction: every element of an array made by new[], last first, given to the
 * destructor with the flag 0 (destroy only: the array's memory is freed by the caller) */
void rt_array_destroy(gaddr array, gaddr destructor) {
  ArrayHeader *h = array_header(array);
  uint32_t size = h->element_size;
  int32_t i = (int32_t)h->count - 1;
  gaddr element = array + (uint32_t)i * size;
  for (; i >= 0; i--, element -= size)
    CALL2(destructor, element, 0);
}
HW_EXPORT(8c06270c, rt_array_destroy(ARG0, ARG1))

/* was func_8C062754: the destruction of an array without a header (a member array): count elements of size bytes at
 * array, last first. The destructor gets only the element in r4 (r5 holds what the previous call left there, as in
 * the original, which does not set it). Returns 0. */
int32_t rt_array_destroy_n(uint32_t count, uint32_t size, gaddr array, gaddr destructor) {
  int32_t i = (int32_t)count - 1;
  gaddr element = array + (uint32_t)i * size;
  for (; i >= 0; i--, element -= size)
    CALL1(destructor, element);
  return 0;
}
HW_EXPORT(8c062754, RET(rt_array_destroy_n(ARG0, ARG1, ARG2, ARG3)))

/* was func_8C062790: new[]'s construction: memory (allocated with syMalloc when none is given), the header in front
 * of the elements, each element given to the constructor (if any), first to last. Returns the array. The constructor
 * gets the element in r4 only (r5: what was there, size or the allocator's leftover: no constructor reads it). */
gaddr rt_array_new(uint32_t count, uint32_t size, gaddr memory, gaddr constructor) {
  gaddr array = memory ? memory : sy_malloc(size);
  ArrayHeader *h = array_header(array);
  h->element_size = size;
  h->count = count;
  if (constructor) {
    gaddr element = array;
    for (uint32_t i = 0; i < count; i++, element += size)
      CALL1(constructor, element);
  }
  return array;
}
HW_EXPORT(8c062790, RET(rt_array_new(ARG0, ARG1, ARG2, ARG3)))

/* was func_8C062800: the construction of an array without a header: each of count elements of size bytes at array
 * given to the constructor, first to last; returns the array */
gaddr rt_array_construct(uint32_t count, uint32_t size, gaddr array, gaddr constructor) {
  gaddr element = array;
  for (uint32_t i = 0; i < count; i++, element += size)
    CALL1(constructor, element);
  return array;
}
HW_EXPORT(8c062800, RET(rt_array_construct(ARG0, ARG1, ARG2, ARG3)))

/* ---- memory ---- */

/* was func_8C0629D8: memmove: by 4-byte words when dst, src and n are all multiples of 4, by 2-byte halves when
 * they are even, else by bytes; forward when dst is below src, backward (from the end) otherwise; nothing when dst
 * is src or n is 0. The halves' count is taken from n's low 16 bits (the original's extu.w): only the first (forward)
 * or the last (backward) 2 * count bytes move when n is 64 KB or more. (A count of 0 there would run the original's
 * loop 2^32 times; nothing is moved here.) Returns dst. */
gaddr rt_memmove(gaddr dst, gaddr src, uint32_t n) {
  if (dst == src || n == 0) return dst;
  uint32_t any = dst | src | n;
  uint32_t bytes = (any & 1) ? n : (any & 3) ? ((n & 0xFFFF) >> 1) * 2 : n;
  if (dst < src) {
    memmove(hw_ptr(dst), hw_ptr(src), bytes);
  } else {
    uint32_t skip = n - bytes;
    memmove(hw_ptr(dst + skip), hw_ptr(src + skip), bytes);
  }
  return dst;
}
HW_EXPORT(8c0629d8, RET(rt_memmove(ARG0, ARG1, ARG2)))

/* was func_8C062A98: memset: n bytes of c's low byte at dst; returns dst */
gaddr rt_memset(gaddr dst, int32_t c, uint32_t n) {
  uint8_t *p = hw_ptr(dst);
  for (uint32_t i = 0; i < n; i++) p[i] = (uint8_t)c;
  return dst;
}
HW_EXPORT(8c062a98, RET(rt_memset(ARG0, (int32_t)ARG1, ARG2)))

/* was func_8C0638C0: memcmp: the difference of the first unequal bytes (as unsigned bytes), 0 when the n bytes are
 * equal */
int32_t rt_memcmp(gaddr a, gaddr b, uint32_t n) {
  const uint8_t *p = hw_ptr(a), *q = hw_ptr(b);
  if (n == 0) return 0;
  uint32_t i = 0;
  while (i < n && p[i] == q[i]) i++;
  if (i == n) i = n - 1; /* all equal: the last pair (0) */
  return (int32_t)p[i] - (int32_t)q[i];
}
HW_EXPORT(8c0638c0, RET(rt_memcmp(ARG0, ARG1, ARG2)))

/* ---- rand ---- */

/* was func_8C062AB0: rand: the linear congruential step seed * 1103515245 + 12345, bits 16-30 of the new seed */
int32_t rt_rand(void) {
  g_rand_seed = g_rand_seed * 0x41C64E6Du + 0x3039u;
  return (int32_t)((g_rand_seed >> 16) & 0x7FFF);
}
HW_EXPORT(8c062ab0, RET(rt_rand()))

/* was func_8C062B2C: srand */
void rt_srand(uint32_t seed) { g_rand_seed = seed; }
HW_EXPORT(8c062b2c, rt_srand(ARG0))

/* ---- S20: the block copy the compiler calls for structure assignments (reached from the C: no route runs it) ----
 * r0 bytes (a multiple of 4, at most 64) from r2 to r1, word by word from the last: an unrolled sequence entered at
 * its n-th last word through a table. Two entries: 0x8C0625E0 for an even number of words, 0x8C0625EC for an odd one
 * (they differ in the register the first word goes through; the compiler pairs them so). r3 is kept; r0 is left
 * holding the last word the sequence loaded into it: the second word for 2 words or more, else n - 4 (odd entry) or
 * the only word (even entry). */
static uint32_t block_copy(uint32_t *r0, gaddr dst, gaddr src, int even_entry) {
  uint32_t n = *r0;
  uint32_t *d = hw_ptr(dst);
  const uint32_t *s = hw_ptr(src);
  for (uint32_t i = n / 4; i-- > 0;) d[i] = s[i];
  if (n >= 8) *r0 = s[1];
  else *r0 = even_entry ? *(const uint32_t *)hw_ptr(src + n - 4) : n - 4;
  return *r0;
}
/* was func_8C0625E0: the block copy, even entry */
HW_EXPORT(8c0625e0, RET(block_copy(&REG(0), REG(1), REG(2), 1)))
/* was func_8C0625EC: the block copy, odd entry */
HW_EXPORT(8c0625ec, RET(block_copy(&REG(0), REG(1), REG(2), 0)))
