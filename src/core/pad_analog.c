/* Pen Pen port, S13: the analog stick as the game reads it (core/pad_analog.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/pad.h"

/* was func_8C010B30: a signed 8-bit stick value as a float in about [-1, 1]: the low bit dropped, positive values
 * pushed up by 2 (so 126 reaches 1.0), divided by 128 */
f32 stick_to_unit(int32_t value) {
  int32_t v = (int8_t)value & ~1;
  if (v > 0)
    v += 2;
  return fdiv(f32_from_int(v), F32(128.0f));
}
HW_EXPORT(8c010b30, RETF(stick_to_unit((int32_t)ARG0)))

/* was func_8C010B4C (a method: `this` unused): the stick of a port packed in a word: x (low bit = B) in bits
 * 15-8, y (low bit = A) in bits 7-0; 0 when no controller is plugged in */
uint32_t pad_stick_word(gaddr self, int32_t port) {
  const PdPeripheral *p = (const PdPeripheral *)hw_ptr(pdGetPeripheral(g_pad_port_id[port]));
  if ((U32_AT(p->info) & 1) == 0)
    return 0;
  uint32_t b = (p->on & PAD_B) != 0, a = (p->on & PAD_A) != 0;
  uint32_t x = ((uint32_t)(int32_t)p->x1 & 0xFEu) | b;
  uint32_t y = ((uint32_t)(int32_t)p->y1 & 0xFEu) | a;
  return x << 8 | y;
}
HW_EXPORT(8c010b4c, RET(pad_stick_word(ARG0, (int32_t)ARG1)))
