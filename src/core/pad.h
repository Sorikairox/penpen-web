/* Pen Pen port, S11: controller input as the game sees it.
 *
 * Once per frame pad_read_all() turns the four ports' PDS_PERIPHERAL structures (SDK) into the game's own button
 * words: held, pressed this frame, released this frame. The analog stick counts as the d-pad when it is pushed past
 * 64 (of 127). Game objects keep a `Pad`, which is just a port number. */
#ifndef PENPEN_PAD_H
#define PENPEN_PAD_H

#include "hw.h"

#define PAD_PORTS 4

/* Dreamcast controller buttons (PDD_DGT_*), as the game's words hold them */
enum {
  PAD_C = 0x0001, PAD_B = 0x0002, PAD_A = 0x0004, PAD_START = 0x0008,
  PAD_UP = 0x0010, PAD_DOWN = 0x0020, PAD_LEFT = 0x0040, PAD_RIGHT = 0x0080,
  PAD_Z = 0x0100, PAD_Y = 0x0200, PAD_X = 0x0400, PAD_D = 0x0800,
};

typedef struct Pad {
  int32_t port; /* 0-3 */
} Pad;

/* the per-port words (guest .bss) */
#define g_pad_port_id ((const uint32_t *)hw_ptr(0x8C066934u)) /* the SDK port number of each port (PDD_PORT_A0...) */
#define g_pad_type    ((uint32_t *)hw_ptr(0x8C1B30C4u))       /* the device type word (bit 0 = a controller) */
#define g_pad_held    ((uint32_t *)hw_ptr(0x8C1B30D4u))
#define g_pad_last    ((uint32_t *)hw_ptr(0x8C1B30E4u))       /* last frame's held, for the edges */
#define g_pad_pressed ((uint32_t *)hw_ptr(0x8C1B30F4u))
#define g_pad_released ((uint32_t *)hw_ptr(0x8C1B3104u))

/* the part of the SDK's PDS_PERIPHERAL the game reads */
typedef struct PdPeripheral {
  uint32_t id;        /* 0x00 */
  uint32_t support;   /* 0x04: what the device has (PDD_DEV_SUPPORT_*) */
  uint32_t on;        /* 0x08: buttons held */
  uint32_t off, press, release; /* 0x0C-0x14 (the game makes its own edges) */
  uint16_t r, l;      /* 0x18, 0x1A: analog triggers */
  int16_t x1, y1;     /* 0x1C, 0x1E: main stick */
  int16_t x2, y2;     /* 0x20, 0x22 */
  uint8_t pad24[0x30 - 0x24];
  gaddr info;         /* 0x30: PDS_PERIPHERALINFO, whose first word is the device type */
} PdPeripheral;
HW_OFFSET(PdPeripheral, x1, 0x1C);
HW_OFFSET(PdPeripheral, info, 0x30);
#define PDD_DEV_SUPPORT_AX 0x00040000u /* analog X */
#define PDD_DEV_SUPPORT_AY 0x00080000u /* analog Y */

void pad_read_all(void);
void pad_reset_all(void);
int32_t pad_held(const Pad *pad, uint32_t buttons);
int32_t pad_pressed(const Pad *pad, uint32_t buttons);
int32_t port_held(int32_t port, uint32_t buttons);
int32_t port_pressed(int32_t port, uint32_t buttons);
int32_t port_has_controller(int32_t port);

#endif
