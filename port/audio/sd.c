/* Pen Pen port, S7: the Manatee sd library as HLE (docs/port/AUDIO.md 2, SDK.md 2.5). The SDK's wrappers
 * write a host command record for the AICA's ARM7 driver; here each one acts on our mixer directly. What the
 * wrappers do was read in the SDK's own code (sandbox, terminal only); the return codes are the SDK's
 * (0x0D000001 library not initialised, 0x07000103 null handle/pointer, 0x03000001 argument out of range).
 *
 *  - Memory blocks: sdMemBlkCreate(&h) / sdMemBlkSetPrm(h, ptr, size, attr, cb) / sdMemBlkDestroy(h): guest
 *    handles in the HLE scratch area. sdMultiUnitDownload(h): the SMLT file at the block's pointer: header
 *    'SMLT', version 0x101, unit count, then 32-byte unit records {fourcc, bank, sound-RAM address, sound-RAM
 *    size, file offset, file size, 0, 0} (SMSB MIDI sequences, SMPB MIDI programs, SFPB/SFOB/SFPW DSP program,
 *    output, work, SOSB one-shots); each unit is copied to its sound-RAM address, as the driver would; the
 *    voices still playing from those addresses are cut first.
 *  - One-shot bank (SOSB): 'SOSB', version, size, count, offsets[count] (from the unit start) to 0x38-byte
 *    'SOSP' records: 24 halfwords of AICA slot registers (sample address relative to the unit), then 'ENDP'.
 *  - Ports: sdShotOpenPort(&h) / sdMidiOpenPort(&h) allocate one of 8 ports per kind and write the handle (a
 *    guest port record {kind, port number, -, -, in use}); the game opens all 16 at start-up.
 *    sdShotPlay(h, bank, no, prio 0..31) starts SOSP `no` of the one-shot bank on the port (a new play on a
 *    port releases the port's previous sound); sdShotStop releases it; sdShotSetVol(h, v, time) (v -128..127,
 *    sent + 0x80; taken as a linear level 0..255: the game plays menu sounds at 127, the race's loops at -1 down
 *    to -85), sdShotSetPan(h, p, time) (-128..127), sdShotSetPitch(h, cents, time)
 *    (-6399..6399, sent + 0x8000) are port state, applied to the port's sound and kept for the next one. The
 *    game always passes time 0.
 *  - MIDI sequences (SMSB + SMPB, S8): sdMidiPlay(h, bank, seq, prio) plays the sequence on the port
 *    (seq.c), sdMidiStop ends it (key off), sdMidiSetVol/SetPan(h, v, time) set the port's level / pan
 *    (-128..127, as the shot ports).
 *  - Globals: sd_SetMasterVol(0..15) (the AICA master volume; the wrapper sends v << 4), sd_SetStereoMono
 *    ('STRO'/'MONO'), sd_Cmd53(l, r) = host command 0x35, the CDDA volume per channel 0..127 (negative values
 *    are refused by the wrapper: the game "mutes" with -127, which the SDK rejects), sd_Cmd54(l, r) = 0x36,
 *    the CDDA pan per channel (the game sends -127, 127: left to the left, right to the right). sd_CmdBytes
 *    (0x88), sd_CmdU8U8 (0x84, 0x82) and sd_CmdReset (0x80) are logged only. */
#include "audio.h"
#include "seq.h"

#define R(i) c->r[i]
#define SD_BASE (GUEST_SCRATCH + 0xC00u) /* 16 ports x 8 bytes, then 8 memory blocks x 16 bytes */
#define PORTS SD_BASE
#define MEMBLK (SD_BASE + 0x80u)
#define E_INIT 0x0D000001u
#define E_NULL 0x07000103u
#define E_RANGE 0x03000001u
#define E_MLT 0x09000001u

static int inited;
static struct { uint32_t ptr, size; int used; } mb[8];
static struct { int used, kind, vol, pan, cents; uint32_t voice; } port[16]; /* 0-7 MIDI, 8-15 shot */
static struct { uint32_t addr, size; int present; } sosb[4];
static int midi_logged;

static void sd_log(SH4 *c) { hle_logcall(c, ""); }

static void h_sdLibInit(SH4 *c) { sd_log(c); inited = 1; R(0) = 0; }
static void h_sdDrvInit(SH4 *c) { sd_log(c); R(0) = inited ? 0 : E_INIT; }
static void h_sdSysFinish(SH4 *c) { sd_log(c); inited = 0; R(0) = 0; }
static void h_sdSysServer(SH4 *c) { sd_log(c); R(0) = 0; }

static void h_sdMemBlkCreate(SH4 *c) {
  sd_log(c);
  if (!inited) { R(0) = E_INIT; return; }
  if (!R(4)) { R(0) = E_NULL; return; }
  for (int i = 0; i < 8; i++)
    if (!mb[i].used) {
      mb[i].used = 1; mb[i].ptr = mb[i].size = 0;
      uint32_t h = MEMBLK + 16u * (uint32_t)i;
      wr32(h, 1); wr32(h + 4, (uint32_t)i); wr32(h + 8, 0); wr32(h + 12, 0);
      wr32(R(4), h);
      R(0) = 0;
      return;
    }
  wr32(R(4), 0);
  R(0) = 0x07000101u;
}
static int mb_of(uint32_t h) {
  if (h < MEMBLK || h >= MEMBLK + 8 * 16 || (h - MEMBLK) % 16) return -1;
  int i = (int)((h - MEMBLK) / 16);
  return mb[i].used ? i : -1;
}
static void h_sdMemBlkSetPrm(SH4 *c) {
  sd_log(c);
  int i = mb_of(R(4));
  if (i < 0) { R(0) = E_NULL; return; }
  mb[i].ptr = R(5); mb[i].size = R(6);
  wr32(R(4) + 8, R(5)); wr32(R(4) + 12, R(6));
  R(0) = 0;
}
static void h_sdMemBlkDestroy(SH4 *c) {
  sd_log(c);
  int i = mb_of(R(4));
  if (i < 0) { R(0) = 0x07000102u; return; }
  mb[i].used = 0; wr32(R(4), 0);
  R(0) = 0;
}

static void h_sdMultiUnitDownload(SH4 *c) {
  sd_log(c);
  int i = mb_of(R(4));
  if (!inited) { R(0) = E_INIT; return; }
  if (i < 0) { R(0) = E_NULL; return; }
  uint32_t p = mb[i].ptr;
  if (rd32(p) != 0x544C4D53u) { R(0) = E_MLT; rt_log("hle sd download: not an SMLT bank"); return; }
  uint32_t ver = rd32(p + 4), n = rd32(p + 8), u = (int8_t)ver >= 1 ? p + 0x20 : p + 0x10;
  int copied = 0;
  uint32_t bytes = 0;
  for (uint32_t k = 0; k < n && k < 16; k++, u += 32) {
    uint32_t tag = rd32(u), bank = rd32(u + 4), addr = rd32(u + 8), size = rd32(u + 12), off = rd32(u + 16), fsz = rd32(u + 20);
    if ((int32_t)size < 0 || (int32_t)fsz <= 0) continue;
    if (addr >= ARAM_SIZE || fsz > ARAM_SIZE - addr) { rt_log("hle sd download: unit %u outside sound RAM", k); continue; }
    aica_stop_range(addr, fsz);
    for (uint32_t j = 0; j < fsz; j++) aica_ram[addr + j] = rd8(p + off + j);
    copied++; bytes += fsz;
    seq_bank(tag, (int)bank, addr, fsz); /* SMSB / SMPB: MIDI (seq.c) */
    if (tag == 0x42534F53u && bank < 4) { /* SOSB */
      sosb[bank].addr = addr; sosb[bank].size = fsz; sosb[bank].present = 1;
      uint32_t cnt = addr + 12 < ARAM_SIZE ? (uint32_t)aica_ram[addr + 12] | (uint32_t)aica_ram[addr + 13] << 8 : 0;
      rt_log("hle sd download: one-shot bank %u, %u sounds, %u bytes at %x", bank, cnt, fsz, addr);
    }
  }
  rt_log("hle sd download: %d units, %u bytes into sound RAM at frame %u", copied, bytes, hle_frame());
  R(0) = 0;
}

/* ports */
static int port_of(uint32_t h) {
  if (h < PORTS || h >= PORTS + 16 * 8 || (h - PORTS) % 8) return -1;
  int i = (int)((h - PORTS) / 8);
  return port[i].used ? i : -1;
}
static void open_port(SH4 *c, int kind) {
  sd_log(c);
  if (!inited) { R(0) = E_INIT; return; }
  if (!R(4)) { R(0) = E_NULL; return; }
  int base = kind == 1 ? 0 : 8;
  for (int i = base; i < base + 8; i++)
    if (!port[i].used) {
      port[i].used = 1; port[i].kind = kind; port[i].vol = 127; port[i].pan = 0; port[i].cents = 0; port[i].voice = 0;
      uint32_t h = PORTS + 8u * (uint32_t)i;
      wr8(h, (uint8_t)kind); wr8(h + 1, (uint8_t)(i - base)); wr16(h + 2, 0); wr32(h + 4, 1);
      wr32(R(4), h);
      R(0) = 0;
      return;
    }
  wr32(R(4), 0);
  R(0) = 0;
}
static void close_port(SH4 *c) {
  sd_log(c);
  int i = port_of(R(4));
  if (i >= 0) { aica_key_off(port[i].voice); port[i].used = 0; wr32(R(4) + 4, 0); }
  R(0) = 0;
}
static void h_sdShotOpenPort(SH4 *c) { open_port(c, 2); }
static void h_sdMidiOpenPort(SH4 *c) { open_port(c, 1); }
static void h_sdShotClosePort(SH4 *c) { close_port(c); }
static void h_sdMidiClosePort(SH4 *c) { close_port(c); }

static uint32_t shots, shot_log;
static void h_sdShotPlay(SH4 *c) {
  sd_log(c);
  if (!inited) { R(0) = E_INIT; return; }
  int i = port_of(R(4)), bank = (int)(R(5) & 0xFF), no = (int)(R(6) & 0xFF), prio = (int8_t)R(7);
  if (!R(4)) { R(0) = E_NULL; return; }
  if (prio < 0 || prio > 31) { R(0) = E_RANGE; return; }
  R(0) = 0;
  if (i < 0 || bank > 3 || !sosb[bank].present) return;
  uint32_t a = sosb[bank].addr, cnt = aica_ram[a + 12] | (uint32_t)aica_ram[a + 13] << 8;
  if ((uint32_t)no >= cnt) { rt_log("hle sd shot %d: bank %d has %u", no, bank, cnt); return; }
  uint32_t off = 0;
  for (int k = 0; k < 4; k++) off |= (uint32_t)aica_ram[a + 16 + 4 * (uint32_t)no + (uint32_t)k] << (8 * k);
  if (off + 0x38 > sosb[bank].size) return;
  uint16_t regs[24];
  for (int k = 0; k < 24; k++) regs[k] = (uint16_t)(aica_ram[a + off + 4 + 2 * (uint32_t)k] | aica_ram[a + off + 5 + 2 * (uint32_t)k] << 8);
  aica_key_off(port[i].voice);
  port[i].voice = aica_key_on(regs, a, prio);
  aica_set_port(port[i].voice, port[i].vol, port[i].pan, port[i].cents);
  shots++;
  if (shot_log < 64) { shot_log++; rt_log("hle sd shot port %d sound %d prio %d at frame %u", i - 8, no, prio, hle_frame()); }
}
static void h_sdShotStop(SH4 *c) {
  sd_log(c);
  int i = port_of(R(4));
  if (!R(4)) { R(0) = E_NULL; return; }
  if (i >= 0) { aica_key_off(port[i].voice); port[i].voice = 0; }
  R(0) = 0;
}
static void port_param(SH4 *c, int which) {
  sd_log(c);
  int i = port_of(R(4));
  if (!R(4)) { R(0) = E_NULL; return; }
  if (which == 2) {
    int v = (int16_t)R(5);
    if (v < -6399 || v > 6399) { R(0) = E_RANGE; return; }
  }
  R(0) = 0;
  if (i < 0) return;
  static uint32_t nlog;
  if (which < 2 && nlog < 64) { nlog++; rt_log("hle sd shot port %d %s %d at frame %u", i - 8, which ? "pan" : "vol", (int8_t)R(5), hle_frame()); }
  if (which == 0) port[i].vol = (int8_t)R(5);
  else if (which == 1) port[i].pan = (int8_t)R(5);
  else port[i].cents = (int16_t)R(5);
  aica_set_port(port[i].voice, port[i].vol, port[i].pan, port[i].cents);
}
static void h_sdShotSetVol(SH4 *c) { port_param(c, 0); }
static void h_sdShotSetPan(SH4 *c) { port_param(c, 1); }
static void h_sdShotSetPitch(SH4 *c) { port_param(c, 2); }

static void h_sdMidiPlay(SH4 *c) {
  sd_log(c);
  if (!inited) { R(0) = E_INIT; return; }
  if (!R(4)) { R(0) = E_NULL; return; }
  int prio = (int8_t)R(7);
  if (prio < 0 || prio > 31) { R(0) = E_RANGE; return; }
  R(0) = 0;
  int i = port_of(R(4));
  if (i < 0 || i >= 8) return;
  if (midi_logged < 64) { midi_logged++; rt_log("hle sd midi port %d sequence %u of bank %u at frame %u", i, R(6) & 0xFF, R(5) & 0xFF, hle_frame()); }
  seq_play(i, (int)(R(5) & 0xFF), (int)(R(6) & 0xFF));
}
static void h_sdMidiStop(SH4 *c) { sd_log(c); if (!R(4)) { R(0) = E_NULL; return; } int i = port_of(R(4)); if (i >= 0 && i < 8) seq_stop(i); R(0) = 0; }
static void midi_param(SH4 *c, int which) {
  sd_log(c);
  if (!R(4)) { R(0) = E_NULL; return; }
  int i = port_of(R(4));
  if (i >= 0 && i < 8) seq_port(i, which, (int8_t)R(5));
  R(0) = 0;
}
static void h_sdMidiSetVol(SH4 *c) { midi_param(c, 0); }
static void h_sdMidiSetPan(SH4 *c) { midi_param(c, 1); }

static void h_sd_SetMasterVol(SH4 *c) { sd_log(c); if (!inited) { R(0) = E_INIT; return; } aica_master((int)(R(4) & 15)); R(0) = 0; }
static void h_sd_SetStereoMono(SH4 *c) {
  sd_log(c);
  if (!inited) { R(0) = E_INIT; return; }
  if (R(4) == 0x4F4E4F4Du) aica_mono(1); /* 'MONO' */
  else if (R(4) == 0x4F525453u) aica_mono(0); /* 'STRO' */
  else { R(0) = E_RANGE; return; }
  R(0) = 0;
}
static void h_sd_Cmd53(SH4 *c) { /* the CDDA volume, left and right, 0..127 */
  sd_log(c);
  if (!inited) { R(0) = E_INIT; return; }
  int l = (int8_t)R(4), r = (int8_t)R(5);
  if (l < 0 || r < 0) { R(0) = E_RANGE; return; }
  cdda_volume(l, r);
  R(0) = 0;
}
static void h_sd_Cmd54(SH4 *c) { /* the CDDA pan, left and right channel */
  sd_log(c);
  if (!inited) { R(0) = E_INIT; return; }
  cdda_pan((int8_t)R(4), (int8_t)R(5));
  R(0) = 0;
}
static void h_sd_logged(SH4 *c) { sd_log(c); R(0) = inited ? 0 : E_INIT; }

const nj_impl_t sd_impls[] = {
  {"sdLibInit", h_sdLibInit}, {"sdDrvInit", h_sdDrvInit}, {"sdSysFinish", h_sdSysFinish}, {"sdSysServer", h_sdSysServer},
  {"sdMemBlkCreate", h_sdMemBlkCreate}, {"sdMemBlkSetPrm", h_sdMemBlkSetPrm}, {"sdMemBlkDestroy", h_sdMemBlkDestroy},
  {"sdMultiUnitDownload", h_sdMultiUnitDownload},
  {"sdShotOpenPort", h_sdShotOpenPort}, {"sdShotClosePort", h_sdShotClosePort}, {"sdShotPlay", h_sdShotPlay},
  {"sdShotStop", h_sdShotStop}, {"sdShotSetVol", h_sdShotSetVol}, {"sdShotSetPan", h_sdShotSetPan},
  {"sdShotSetPitch", h_sdShotSetPitch},
  {"sdMidiOpenPort", h_sdMidiOpenPort}, {"sdMidiClosePort", h_sdMidiClosePort}, {"sdMidiPlay", h_sdMidiPlay},
  {"sdMidiStop", h_sdMidiStop}, {"sdMidiSetVol", h_sdMidiSetVol}, {"sdMidiSetPan", h_sdMidiSetPan},
  {"sd_SetMasterVol", h_sd_SetMasterVol}, {"sd_SetStereoMono", h_sd_SetStereoMono},
  {"sd_Cmd53", h_sd_Cmd53}, {"sd_Cmd54", h_sd_Cmd54},
  {"sd_CmdBytes", h_sd_logged}, {"sd_CmdU8U8", h_sd_logged}, {"sd_CmdReset", h_sd_logged},
};
const int sd_nimpls = (int)(sizeof sd_impls / sizeof sd_impls[0]);
