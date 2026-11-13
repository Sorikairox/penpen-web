/* Pen Pen port, S17 (b01): more of the controller as the game reads it (core/pad.h): released buttons and the
 * analog axes of a Pad. */
#include "hw.h"
#include "sdk.h"
#include "../core/pad.h"

/* the analog axes a Pad reads (pad_axis) */
enum {
  PAD_AXIS_X1 = 0, PAD_AXIS_Y1 = 1, /* main stick */
  PAD_AXIS_X2 = 2, PAD_AXIS_Y2 = 3,
  PAD_AXIS_R = 4, PAD_AXIS_L = 5,   /* triggers */
};
#define TRIGGER_FULL F32(255.0f)

/* was func_8C010A98 (a Pad method): 1 when one of `buttons` was released this frame */
int32_t pad_released(const Pad *pad, uint32_t buttons) {
  return (g_pad_released[pad->port] & buttons) != 0 ? 1 : 0;
}
HW_EXPORT(8c010a98, RET(pad_released((const Pad *)hw_ptr(ARG0), ARG1)))

/* was func_8C010AAE (a Pad method): one analog axis of the pad's controller as a float: a stick in about [-1, 1]
 * (stick_to_unit), a trigger in [0, 1]; 0 when no controller is plugged in. An axis number past 5 returns what FR0
 * held after pdGetPeripheral (the original leaves FR0 as it is). */
f32 pad_axis(const Pad *pad, int32_t axis) {
  const PdPeripheral *p = (const PdPeripheral *)hw_ptr(pdGetPeripheral(g_pad_port_id[pad->port]));
  if ((U32_AT(p->info) & 1) == 0)
    return F32(0.0f);
  switch (axis) {
  case PAD_AXIS_X1: return stick_to_unit(p->x1);
  case PAD_AXIS_Y1: return stick_to_unit(p->y1);
  case PAD_AXIS_X2: return stick_to_unit(p->x2);
  case PAD_AXIS_Y2: return stick_to_unit(p->y2);
  case PAD_AXIS_R: return fdiv(f32_from_int(p->r), TRIGGER_FULL);
  case PAD_AXIS_L: return fdiv(f32_from_int(p->l), TRIGGER_FULL);
  }
  return hw_fret();
}
HW_EXPORT(8c010aae, RETF(pad_axis((const Pad *)hw_ptr(ARG0), (int32_t)ARG1)))
