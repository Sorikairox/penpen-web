/* Pen Pen port, S20: the C runtime's G2-bus access (region shc_rt): a copy to or from a G2 device (the AICA's clock,
 * core/rtc.c) made safe by suspending the G2 DMA channels and waiting for the G2 FIFO to drain first, as the SH-4
 * must on the Dreamcast. The library's DMA state is a block in RAM (g_g2_dma) whose first word points at the G2 DMA
 * registers; the registers themselves go through the runtime's I/O model. */
#include "lib.h"

#define G2_FIFO_STATUS   0xA05F688Cu /* bit 5: the G2 FIFO holds data */
#define G2_FIFO_BUSY     0x20u
#define G2_DMA_REG_ENABLE  0x18u     /* a channel's registers (0x20 bytes each): bit 0, the channel is enabled */
#define G2_DMA_REG_SUSPEND 0x1Cu     /* 1: suspended */
#define G2_CHANNELS      4

typedef struct G2DmaChannel {
  uint32_t index;   /* 0x00: the hardware channel */
  uint8_t _04[0x14];
  uint32_t state;   /* 0x18: 2 after the copy suspended it */
  uint8_t _1c[0x18];
} G2DmaChannel;
HW_OFFSET(G2DmaChannel, state, 0x18);
HW_SIZE(G2DmaChannel, 0x34);

typedef struct G2Dma {
  gaddr regs;                          /* 0x000: the G2 DMA registers */
  G2DmaChannel channel[G2_CHANNELS];   /* 0x004 */
  uint32_t mask_interrupts;            /* 0x0D4: 1: a copy runs with every interrupt masked */
  uint8_t _d8[0x34];
  uint32_t fifo_drains;                /* 0x10C: the times a copy waited for the FIFO */
  uint32_t fifo_write_drains;          /* 0x110: the times a write waited for the FIFO's writes */
} G2Dma;
HW_OFFSET(G2Dma, mask_interrupts, 0xD4);
HW_OFFSET(G2Dma, fifo_drains, 0x10C);
HW_OFFSET(G2Dma, fifo_write_drains, 0x110);

#define g_g2_dma       GUEST_VAR(gaddr, 0x8C0998A8u)
#define g_g2_suspended GUEST_VAR(uint32_t, 0x8C1BADA0u) /* 1 between the copy's suspend and resume */

/* The block's fields go through the runtime's I/O model, as the original's loads and stores through the pointer:
 * nothing in this port sets the library up (its initialisation is not called), so the pointer is 0 and these
 * accesses land on the I/O model's address 0, not in RAM (hw_ptr would fold them into RAM at 0x8C000000). */
#define G2_FIELD(f) (g_g2_dma + offsetof(G2Dma, f))
static uint32_t dma_read(gaddr field) { return hw_read32(field); }
static void dma_write(gaddr field, uint32_t v) { hw_write32(field, v); }
/* a channel's register: its index read first, then the block's register pointer (the original's order: the reads
 * of hardware are replayed in order by the difftest) */
static gaddr channel_reg(gaddr channel, uint32_t reg) {
  uint32_t index = dma_read(channel + offsetof(G2DmaChannel, index));
  return dma_read(G2_FIELD(regs)) + index * 0x20u + reg;
}

/* was func_8C065538: whether a DMA channel is enabled (*enabled = its enable register's bit 0); returns 0 */
int32_t g2_dma_channel_enabled(gaddr channel, uint32_t *enabled) {
  *enabled = hw_read32(channel_reg(channel, G2_DMA_REG_ENABLE)) & 1u;
  return 0;
}
HW_EXPORT(8c065538, RET(g2_dma_channel_enabled(ARG0, (uint32_t *)hw_ptr(ARG1))))

/* was func_8C064BF8: lets an enabled channel run again; returns 0 */
int32_t g2_dma_channel_resume(gaddr channel) {
  uint32_t enabled;
  g2_dma_channel_enabled(channel, &enabled);
  if (enabled == 1) hw_write32(channel_reg(channel, G2_DMA_REG_SUSPEND), 0);
  return 0;
}
HW_EXPORT(8c064bf8, RET(g2_dma_channel_resume(ARG0)))

/* was func_8C064C9C: suspends an enabled channel (its state becomes 2); returns 0 */
int32_t g2_dma_channel_suspend(gaddr channel) {
  uint32_t enabled;
  g2_dma_channel_enabled(channel, &enabled);
  if (enabled == 1) {
    hw_write32(channel_reg(channel, G2_DMA_REG_SUSPEND), 1);
    dma_write(channel + offsetof(G2DmaChannel, state), 2);
  }
  return 0;
}
HW_EXPORT(8c064c9c, RET(g2_dma_channel_suspend(ARG0)))

static gaddr channel_at(int i) { return g_g2_dma + offsetof(G2Dma, channel) + (uint32_t)i * sizeof(G2DmaChannel); }

/* was func_8C063C58: resumes the four channels; returns 0 */
int32_t g2_dma_resume_all(void) {
  for (int i = 0; i < G2_CHANNELS; i++) g2_dma_channel_resume(channel_at(i));
  g_g2_suspended = 0;
  return 0;
}
HW_EXPORT(8c063c58, RET(g2_dma_resume_all()))

/* was func_8C063CD4: suspends the four channels; returns 0 */
int32_t g2_dma_suspend_all(void) {
  for (int i = 0; i < G2_CHANNELS; i++) g2_dma_channel_suspend(channel_at(i));
  g_g2_suspended = 1;
  return 0;
}
HW_EXPORT(8c063cd4, RET(g2_dma_suspend_all()))

/* was func_8C064D84: *empty = 1 when the G2 FIFO holds nothing; returns 0 */
int32_t g2_fifo_empty(uint32_t *empty) {
  *empty = (hw_read32(G2_FIFO_STATUS) & G2_FIFO_BUSY) == 0;
  return 0;
}
HW_EXPORT(8c064d84, RET(g2_fifo_empty((uint32_t *)hw_ptr(ARG0))))

/* was func_8C063D18: waits until the G2 FIFO is empty (counted); returns 0 */
int32_t g2_fifo_drain(void) {
  uint32_t empty;
  do g2_fifo_empty(&empty);
  while (empty != 1);
  dma_write(G2_FIELD(fifo_drains), dma_read(G2_FIELD(fifo_drains)) + 1);
  return 0;
}
HW_EXPORT(8c063d18, RET(g2_fifo_drain()))

/* an address step of the copy: 0 stays, 1 forward, 2 backward; another mode leaves the step the caller had in the
 * register the original uses for it (r12 for the source, r13 for the destination) */
static int32_t copy_step(uint32_t mode, uint32_t callers_register) {
  switch (mode) {
  case 0: return 0;
  case 1: return 1;
  case 2: return -1;
  default: return (int32_t)callers_register;
  }
}

/* was func_8C062B70: copies count units of size bytes (1, 2 or 4; another size copies nothing) from src to dst over
 * the G2 bus, each address stepping by its mode (copy_step, in units), with the DMA channels suspended and the FIFO
 * drained, and every interrupt masked when the library is set up so. Returns 0. */
int32_t g2_copy(gaddr src, gaddr dst, uint32_t size, int32_t count, uint32_t src_mode, uint32_t dst_mode,
                uint32_t callers_r12, uint32_t callers_r13) {
  int32_t src_step = copy_step(src_mode, callers_r12), dst_step = copy_step(dst_mode, callers_r13);
  uint32_t level = 0; /* the original's stack slot: a fresh frame's 0 when the flag is set only for the end */
  if (dma_read(G2_FIELD(mask_interrupts)) == 1) level = sr_imask_raise();
  g2_dma_suspend_all();
  g2_fifo_drain();
  switch (size) {
  case 1:
    for (int32_t i = 0; i < count; i++, src += (uint32_t)src_step, dst += (uint32_t)dst_step)
      hw_write8(dst, hw_read8(src));
    break;
  case 2:
    for (int32_t i = 0; i < count; i++, src += (uint32_t)src_step * 2u, dst += (uint32_t)dst_step * 2u)
      hw_write16(dst, hw_read16(src));
    break;
  case 4:
    for (int32_t i = 0; i < count; i++, src += (uint32_t)src_step * 4u, dst += (uint32_t)dst_step * 4u)
      hw_write32(dst, hw_read32(src));
    break;
  }
  g2_dma_resume_all();
  if (dma_read(G2_FIELD(mask_interrupts)) == 1) sr_imask_set(level);
  return 0;
}
HW_EXPORT(8c062b70, RET(g2_copy(ARG0, ARG1, ARG2, (int32_t)ARG3, ARG4, ARG5, REG(12), REG(13))))

/* ---- S20: the G2 write path (reached from the C: no route runs it) ---- */
#define G2_FIFO_WRITE_BUSY 0x01u /* bit 0 of the FIFO status: a write is still in the FIFO */

/* was func_8C064BE4: *done = 1 when the FIFO holds no write; returns 0 */
int32_t g2_fifo_write_done(uint32_t *done) {
  *done = (hw_read32(G2_FIFO_STATUS) & G2_FIFO_WRITE_BUSY) == 0;
  return 0;
}
HW_EXPORT(8c064be4, RET(g2_fifo_write_done((uint32_t *)hw_ptr(ARG0))))

/* was func_8C0639E4: waits until the FIFO's writes are done (counted); returns 0 */
int32_t g2_fifo_write_drain(void) {
  uint32_t done;
  do g2_fifo_write_done(&done);
  while (done != 1);
  dma_write(G2_FIELD(fifo_write_drains), dma_read(G2_FIELD(fifo_write_drains)) + 1);
  return 0;
}
HW_EXPORT(8c0639e4, RET(g2_fifo_write_drain()))

/* was func_8C062E80: g2_copy for writes to a G2 device: the same steps and modes (another mode keeps the caller's r10
 * for the source, r9 for the destination), and before every eighth unit the FIFO is drained and its writes waited
 * for (the FIFO holds eight). Returns 0. */
int32_t g2_write(gaddr src, gaddr dst, uint32_t size, int32_t count, uint32_t src_mode, uint32_t dst_mode,
                 uint32_t callers_r10, uint32_t callers_r9) {
  int32_t src_step = copy_step(src_mode, callers_r10), dst_step = copy_step(dst_mode, callers_r9);
  uint32_t level = 0;
  if (dma_read(G2_FIELD(mask_interrupts)) == 1) level = sr_imask_raise();
  g2_dma_suspend_all();
  g2_fifo_drain();
  if (size == 1 || size == 2 || size == 4) {
    for (int32_t i = 0; i < count; i++, src += (uint32_t)src_step * size, dst += (uint32_t)dst_step * size) {
      if ((i & 7) == 0) {
        g2_fifo_drain();
        g2_fifo_write_drain();
      }
      if (size == 1) hw_write8(dst, hw_read8(src));
      else if (size == 2) hw_write16(dst, hw_read16(src));
      else hw_write32(dst, hw_read32(src));
    }
  }
  g2_dma_resume_all();
  if (dma_read(G2_FIELD(mask_interrupts)) == 1) sr_imask_set(level);
  return 0;
}
HW_EXPORT(8c062e80, RET(g2_write(ARG0, ARG1, ARG2, (int32_t)ARG3, ARG4, ARG5, REG(10), REG(9))))
