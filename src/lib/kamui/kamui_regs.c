/* Pen Pen port, S20: Kamui's register writes used by Ninja's fog: a word into the Holly (PowerVR) register block at
 * 0xA05F8000 (uncached), and the fog density register (FOG_DENSITY, offset 0xB8) through it. */
#include "hw.h"
#include "kamui_lib.h"

#define HOLLY_REGS        0xA05F8000u /* the Holly register block (P2, uncached) */
#define HOLLY_FOG_DENSITY 0xB8u       /* FOG_DENSITY: the fog table's depth scale (mantissa and exponent) */

/* was func_8C141AC4: writes value into the Holly register at offset; returns 1 */
int32_t km_write_holly_reg(uint32_t offset, uint32_t value) {
  hw_write32(HOLLY_REGS + offset, value);
  return 1;
}
HW_EXPORT(8c141ac4, RET(km_write_holly_reg(ARG0, ARG1)))

/* was func_8C11D138: sets the fog density register; returns 0 */
int32_t km_set_fog_density(uint32_t density) {
  km_write_holly_reg(HOLLY_FOG_DENSITY, density);
  return 0;
}
HW_EXPORT(8c11d138, RET(km_set_fog_density(ARG0)))
