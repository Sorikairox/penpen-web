/* Pen Pen port, S20: Ninja's fog table. The PowerVR fog table has 128 entries; entry i stands for the depth
 * 2^(i/16) * (1 + (i%16)/16) (a float-like scale: 16 steps per power of two), and the fog density register sets the
 * scale of those depths. njGenerateFogTable3 picks the density from the far plane and fills the table with a
 * linear fade from the near plane to the far one. */
#include "hw.h"
#include "ninja_lib.h"
#include "../lib.h"
#include "../kamui/kamui_lib.h"

#define FOG_TABLE_N      128
#define FOG_TABLE_BYTES  0x200u
#define FOG_NEAR_MIN     F32_BITS(0xC77FFF00u) /* -65535: a near plane below it gets the fixed density */
#define FOG_DENSITY_FAR  0x8010u               /* the density used then */
#define FOG_MANTISSA_ONE 0x8000u               /* the density's mantissa bits: 1, then 1/2 ... 1/128 */
#define FOG_MANTISSA_1_2 0x4000u
#define FOG_MANTISSA_BITS 7


/* the depth of fog table entry i, as the generator computes it */
static f32 fog_depth(int32_t i) {
  f32 scale = f32_from_int(1 << (i >> 4));
  f32 step = fadd(f32_from_int(i & 15), F32(16.0f));
  return fdiv(fmul(scale, step), F32(16.0f));
}

/* was func_8C0C6CF0 (njGenerateFogTable3): builds the fog table at `table` (128 floats) for the planes n and f
 * (negative, view space looks down -z) and sets the fog density register from f */
void nj_generate_fog_table3(gaddr table, f32 n, f32 f) {
  f32 *t = (f32 *)hw_ptr(table);
  f32 neg_f = fneg(f);
  uint32_t density;

  if (fgt(FOG_NEAR_MIN, n)) {
    density = FOG_DENSITY_FAR;
  } else {
    /* the density: the far distance as 1.mmmmmmm * 2^e (the original's whole frame: frexpf writes the exponent
     * into its lowest word, of which the low byte is used) */
    gaddr frame = hw_stack_alloc(36);
    f32 m = fmul(rt_frexpf(neg_f, (int32_t *)hw_ptr(frame)), F32(2.0f));
    int8_t *exponent = (int8_t *)hw_ptr(frame);
    *exponent = (int8_t)(*exponent - 1);
    density = 0;
    if (!fgt(F32(0.0f), fadd(m, F32(-1.0f)))) {
      m = fsub(m, F32(1.0f));
      density = FOG_MANTISSA_ONE;
    }
    f32 bit = F32(0.5f);
    for (int k = 0; k < FOG_MANTISSA_BITS; k++) {
      if (!fgt(F32(0.0f), fadd(m, fneg(bit)))) {
        density |= FOG_MANTISSA_1_2 >> k;
        m = fsub(m, bit);
      }
      bit = fmul(bit, F32(0.5f));
    }
    density |= (uint8_t)*exponent;
    hw_stack_free(36);
  }
  km_set_fog_density(density);

  f32 neg_n = fneg(n);
  if (!fgt(neg_n, fdiv(neg_f, F32(256.0f)))) {
    /* the near plane is within the first 1/256 of the far one: each entry is 1 / its depth */
    for (int32_t i = 0; i < FOG_TABLE_N; i++) {
      int32_t depth16 = (16 + (i & 15)) * (1 << (i >> 4)); /* 16 * the depth, an integer */
      t[i] = fdiv(F32(1.0f), fdiv(f32_from_int(depth16), F32(16.0f)));
    }
    return;
  }

  /* the entries whose depth (scaled to f) lies beyond the near plane fade linearly to 0 at the far one ... */
  int32_t count = 0;
  for (int32_t i = 0; i < FOG_TABLE_N; i++)
    if (fgt(fdiv(neg_f, fog_depth(i)), neg_n)) count++;
  f32 span = fsub(fneg(fdiv(neg_f, fog_depth(count))), f);
  for (int32_t i = 0; i < count; i++) {
    f32 z = fdiv(neg_f, fog_depth(i));
    t[i] = fsub(F32(1.0f), fdiv(fsub(neg_f, z), span));
  }
  /* ... and the rest are 0 */
  for (gaddr p = table + (uint32_t)count * 4u; p < table + FOG_TABLE_BYTES; p += 4) F32_AT(p) = F32(0.0f);
}
HW_EXPORT(8c0c6cf0, nj_generate_fog_table3(ARG0, FARG0, FARG1))

/* was func_8C0C6F84 (a thunk): njSetFogTable (Kamui, 0x8C11D0D0: loads a fog table into the PowerVR) with the
 * registers as they came; its r0 comes back unchanged */
static inline uint32_t njSetFogTable_km(gaddr table) { return CALL1(0x8C11D0D0u, table); }
HW_EXPORT(8c0c6f84, RET(njSetFogTable_km(ARG0)))
