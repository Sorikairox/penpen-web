/* Pen Pen port, S6: pads (pd), the VMU (bu, pdLcd) and the Jump Pack (pdVib) — hle_io.c. */
#ifndef HLE_IO_H
#define HLE_IO_H
#include "rt.h"
#include "../render/nj.h"

extern const nj_impl_t io_impls[];
extern const int io_nimpls;
void io_init(SH4 *c);
void io_vblank(SH4 *c);        /* at each V-blank: the pads, the VMU mount */
uint32_t io_pad_on(int port);  /* the buttons held on a port (for the frame log) */
/* the host's pads (retrowser rtw_input / rtw_triggers), and the start of a host frame */
void io_host_input(int port, uint32_t buttons, int sx, int sy);
void io_host_triggers(int port, int l, int r);
void io_host_frame(void);

#endif
