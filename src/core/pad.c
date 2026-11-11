/* Pen Pen port, S11: controller input (core/pad.h). */
#include "pad.h"
#include "sdk.h"

#define STICK_THRESHOLD 64 /* the original compares the stick, as a float, with +-64.0 */

static void pad_clear(int port) {
  g_pad_held[port] = 0;
  g_pad_pressed[port] = 0;
  g_pad_released[port] = 0;
  g_pad_last[port] = 0;
}

/* was func_8C01087C: read the four ports. A port whose device is not a controller reads as nothing; a controller
 * without both analog axes stops the scan (the ports after it keep last frame's words: the original returns). */
void pad_read_all(void) {
  for (int port = 0; port < PAD_PORTS; port++) {
    const PdPeripheral *per = (const PdPeripheral *)hw_ptr(pdGetPeripheral(g_pad_port_id[port]));
    g_pad_type[port] = *(const uint32_t *)hw_ptr(per->info);
    if (g_pad_type[port] & 0x80) { /* not a usable device */
      pad_clear(port);
      continue;
    }
    if (!(g_pad_type[port] & 1)) { /* no controller */
      pad_clear(port);
      continue;
    }
    int axes = ((per->support & PDD_DEV_SUPPORT_AX) != 0) + ((per->support & PDD_DEV_SUPPORT_AY) != 0);
    if (axes < 2) {
      pad_clear(port);
      return;
    }
    /* buttons, plus the stick as a d-pad */
    g_pad_held[port] = per->on;
    if (per->x1 > STICK_THRESHOLD) g_pad_held[port] |= PAD_RIGHT;
    if (per->x1 < -STICK_THRESHOLD) g_pad_held[port] |= PAD_LEFT;
    if (per->y1 < -STICK_THRESHOLD) g_pad_held[port] |= PAD_UP;
    if (per->y1 > STICK_THRESHOLD) g_pad_held[port] |= PAD_DOWN;
    g_pad_pressed[port] = g_pad_held[port] & ~g_pad_last[port];
    g_pad_released[port] = ~g_pad_held[port] & g_pad_last[port];
    g_pad_last[port] = g_pad_held[port];
  }
}
HW_EXPORT(8c01087c, pad_read_all())

/* was func_8C010A0A: at start-up: read each port once (the SDK starts its polling) and clear the edges. */
void pad_reset_all(void) {
  for (int port = 0; port < PAD_PORTS; port++) {
    pdGetPeripheral(g_pad_port_id[port]);
    g_pad_last[port] = 0;
    g_pad_released[port] = 0;
    g_pad_pressed[port] = 0;
  }
}
HW_EXPORT(8c010a0a, pad_reset_all())

/* was func_8C010A52 */
int32_t pad_held(const Pad *pad, uint32_t buttons) { return (g_pad_held[pad->port] & buttons) != 0; }
HW_EXPORT(8c010a52, RET(pad_held((const Pad *)hw_ptr(ARG0), ARG1)))

/* was func_8C010A68 */
int32_t pad_pressed(const Pad *pad, uint32_t buttons) { return (g_pad_pressed[pad->port] & buttons) != 0; }
HW_EXPORT(8c010a68, RET(pad_pressed((const Pad *)hw_ptr(ARG0), ARG1)))

/* was func_8C019D14: the same by port number (the game object's copy) */
int32_t port_held(int32_t port, uint32_t buttons) { return (g_pad_held[port] & buttons) != 0; }
HW_EXPORT(8c019d14, RET(port_held((int32_t)ARG0, ARG1)))

/* was func_8C019D2A (S13: exported with its copies; S11 left it translated: only a soft reset from ports 2-4 reaches
 * this copy) */
int32_t port_pressed(int32_t port, uint32_t buttons) { return (g_pad_pressed[port] & buttons) != 0; }
HW_EXPORT(8c019d2a, RET(port_pressed((int32_t)ARG0, ARG1)))

/* was func_8C01AFE0: a standard controller is plugged into the port (device type word 1); a method: the port is
 * the second argument */
int32_t port_has_controller(int32_t port) { return g_pad_type[port] == 1; }
HW_EXPORT(8c01afe0, RET(port_has_controller((int32_t)ARG1)))
