/* Pen Pen port, S6: the Maple peripherals at the SDK's API: pads (pd), the VMU (bu file system on the host's
 * 128 KiB save image, pdLcd), the Jump Pack (pdVib -> rtw_rumble). Semantics from the SDK's own code (read
 * in the sandbox, docs/port/SDK.md 2.3/2.4); the HLE completes every VMU operation at once.
 *
 * Pads. pdGetPeripheral(port) returns the SDK's per-unit structure (port = 6 * connector + unit; the game
 * reads units 0 of A..D): id 0, support 4, on 8, off 0xC, press 0x10, release 0x14, r 0x18, l 0x1A, x1 0x1C,
 * y1 0x1E, x2 0x20, y2 0x22, name 0x24, extend 0x28, old 0x2C, info 0x30 (-> {device functions, ...}: 1
 * controller, 0x0E VMU = storage + LCD + clock, 0x100 Jump Pack). As the SDK converts a controller's condition:
 * buttons active-high in the Maple positions (C B A Start Up Down Left Right Z Y X D), the analog triggers
 * r/l 0..255 and as digital bits 16 (R) and 17 (L) with hysteresis (pressed from 192, released at 160 or
 * below), sticks = raw - 128 (x1 right positive, y1 down positive), off = ~on, press/release the changes
 * since the previous V-blank. Sources: the host (rtw_input: sx, sy up positive; rtw_triggers) on every port
 * it sends this frame, and on port A the test mode's pad script. A port the host did not send is empty. */
#include "hle_io.h"
#include "vmu.h"

#define R(i) HLE_R(i) /* S11: marks the argument registers read (rt.h) */
RTW_IMPORT("rtw_rumble") int rtw_rumble(int port, int on);

#define PAD_BASE (GUEST_SCRATCH + 0x000u) /* 4 connectors x 6 units x 0x40 */
#define INFO_NONE (GUEST_SCRATCH + 0x600u)
#define INFO_PAD (GUEST_SCRATCH + 0x640u)
#define INFO_VMU (GUEST_SCRATCH + 0x680u)
#define INFO_VIB (GUEST_SCRATCH + 0x6C0u)
#define PAD_STD_SUPPORT 0x000F06FEu /* B A Start U D L R Y X, analog R/L, analog X/Y */
#define DGT_R (1u << 16)
#define DGT_L (1u << 17)
#define TRIG_ON 192
#define TRIG_OFF 160

/* ---- the host's pads ---- */
static struct { uint32_t buttons; int sx, sy, l, r; } host[4];
static uint32_t host_seen, host_ports = 1; /* ports sent in the current / the last host frame */
static int host_any;                       /* the host has sent input at all (else: test mode, port A only) */
void io_host_input(int port, uint32_t buttons, int sx, int sy) {
  if (port < 0 || port > 3) return;
  host[port].buttons = buttons; host[port].sx = sx; host[port].sy = sy;
  host_seen |= 1u << port; host_any = 1;
}
void io_host_triggers(int port, int l, int r) {
  if (port < 0 || port > 3) return;
  host[port].l = l < 0 ? 0 : l > 255 ? 255 : l;
  host[port].r = r < 0 ? 0 : r > 255 ? 255 : r;
}
void io_host_frame(void) {
  if (host_any) host_ports = host_seen | 1u; /* port A always holds a controller */
  host_seen = 0;
}

/* ---- the test mode's pad script (port A) ---- */
typedef struct { uint32_t from, to, buttons; } pad_ev;
#define NPAD 256
static pad_ev pad_script[NPAD];
static int npad;
static const struct { const char *n; uint32_t bit; } pad_names[] = {
  {"C", 1u << 0}, {"B", 1u << 1}, {"A", 1u << 2}, {"START", 1u << 3}, {"UP", 1u << 4}, {"DOWN", 1u << 5},
  {"LEFT", 1u << 6}, {"RIGHT", 1u << 7}, {"Z", 1u << 8}, {"Y", 1u << 9}, {"X", 1u << 10}, {"D", 1u << 11},
  {"RT", DGT_R}, {"LT", DGT_L}, /* the analog triggers pulled fully */
  {"SL", 1u << 20}, {"SR", 1u << 21}, {"SU", 1u << 22}, {"SD", 1u << 23}, /* the stick pushed fully */
};
/* "F[+N]:BTN[|BTN...],...": hold the buttons on port A during frames F..F+N-1 (N default 4) */
int hle_set_pad_script(const char *s) {
  npad = 0;
  while (*s && npad < NPAD) {
    uint32_t f = 0, n = 4, b = 0;
    while (*s >= '0' && *s <= '9') f = f * 10 + (uint32_t)(*s++ - '0');
    if (*s == '+') { s++; n = 0; while (*s >= '0' && *s <= '9') n = n * 10 + (uint32_t)(*s++ - '0'); }
    if (*s++ != ':') { rt_log("hle pad script: syntax error"); npad = 0; return -1; }
    while (*s && *s != ',') {
      char nm[8]; int k = 0;
      while (*s && *s != '|' && *s != ',' && k < 7) nm[k++] = *s++;
      nm[k] = 0;
      if (*s == '|') s++;
      int ok = 0;
      for (unsigned j = 0; j < sizeof pad_names / sizeof pad_names[0]; j++)
        if (!strcmp(nm, pad_names[j].n)) { b |= pad_names[j].bit; ok = 1; }
      if (!ok && k) { rt_log("hle pad script: unknown button"); npad = 0; return -2; }
    }
    if (*s == ',') s++;
    pad_script[npad].from = f; pad_script[npad].to = f + n; pad_script[npad].buttons = b; npad++;
  }
  rt_log("hle pad script %d events", npad);
  return npad;
}
static uint32_t script_buttons(uint32_t f) {
  uint32_t b = 0;
  for (int i = 0; i < npad; i++) if (f >= pad_script[i].from && f < pad_script[i].to) b |= pad_script[i].buttons;
  return b;
}

/* ---- the SDK's peripheral structures ---- */
static uint32_t pad_on[4];
static int trig_state[4][2]; /* digital R, L after the hysteresis */
uint32_t io_pad_on(int port) { return pad_on[port & 3]; }
static int clamp8s(int v) { return v < -128 ? -128 : v > 127 ? 127 : v; }
static int trig_bit(int *st, int v) {
  if (v >= TRIG_ON) *st = 1;
  else if (v <= TRIG_OFF) *st = 0;
  return *st;
}
static void unit_clear(uint32_t p, uint32_t id, uint32_t info) {
  for (uint32_t k = 0; k < 0x40; k += 4) wr32(p + k, 0);
  wr32(p + 0, id);
  wr32(p + 0x30, info);
}
static void pads_update(void) {
  wr32(INFO_NONE, 0); wr32(INFO_PAD, 1); wr32(INFO_VMU, 0x0E); wr32(INFO_VIB, 0x100);
  uint32_t f = hle_frame();
  for (int port = 0; port < 4; port++) {
    int present = (host_ports >> port) & 1;
    uint32_t base = PAD_BASE + (uint32_t)port * 6 * 0x40;
    uint32_t prev = pad_on[port], on = 0;
    int l = 0, r = 0, sx = 0, sy = 0;
    if (present) {
      on = host[port].buttons & 0x0FFFu;
      l = host[port].l; r = host[port].r; sx = host[port].sx; sy = host[port].sy;
      if (port == 0) {
        uint32_t s = script_buttons(f);
        on |= s & 0x0FFFu;
        if (s & DGT_R) r = 255;
        if (s & DGT_L) l = 255;
        if (s & (1u << 20)) sx = -127;
        if (s & (1u << 21)) sx = 127;
        if (s & (1u << 22)) sy = 127;
        if (s & (1u << 23)) sy = -127;
      }
      if (trig_bit(&trig_state[port][0], r)) on |= DGT_R;
      if (trig_bit(&trig_state[port][1], l)) on |= DGT_L;
    }
    pad_on[port] = on;
    for (int unit = 0; unit < 6; unit++) {
      uint32_t p = base + (uint32_t)unit * 0x40, id = (uint32_t)(port * 6 + unit);
      if (!present || unit > 2 || (unit == 1 && port != 0)) { unit_clear(p, id, INFO_NONE); continue; }
      if (unit == 1) { unit_clear(p, id, INFO_VMU); continue; } /* the VMU in A1 */
      if (unit == 2) { unit_clear(p, id, INFO_VIB); continue; } /* a Jump Pack in slot 2 */
      uint32_t chg = prev ^ on;
      wr32(p + 0, id);
      wr32(p + 0x04, PAD_STD_SUPPORT);
      wr32(p + 0x08, on);
      wr32(p + 0x0C, ~on);
      wr32(p + 0x10, on & chg);
      wr32(p + 0x14, ~on & chg);
      wr16(p + 0x18, (uint16_t)r);
      wr16(p + 0x1A, (uint16_t)l);
      wr16(p + 0x1C, (uint16_t)clamp8s(sx));
      wr16(p + 0x1E, (uint16_t)clamp8s(-sy));
      wr16(p + 0x20, 0); wr16(p + 0x22, 0); /* no second stick on the standard controller */
      wr32(p + 0x24, 0); wr32(p + 0x28, 0);
      wr32(p + 0x2C, prev);
      wr32(p + 0x30, INFO_PAD);
    }
  }
}
static void h_pdInitPeripheral(SH4 *c) { hle_logcall(c, ""); R(0) = 1; }
static void h_pdExitPeripheral(SH4 *c) { hle_logcall(c, ""); R(0) = 0; }
static void h_pdGetPeripheral(SH4 *c) {
  hle_logcall(c, "");
  uint32_t port = R(4);
  R(0) = port < 24 ? PAD_BASE + port * 0x40 : 0;
}
static int unit_present(uint32_t port, int unit) {
  return port < 24 && (int)(port % 6) == unit && ((host_ports >> (port / 6)) & 1) && (unit != 1 || port / 6 == 0);
}

/* ---- VMU LCD (pdLcd): accepted and dropped ---- */
static uint32_t lcd_writes;
static void h_pdVmsLcdIsReady(SH4 *c) { hle_logcall(c, ""); R(0) = unit_present(R(4), 1) ? 1 : 0; }
static void h_pdVmsLcdWrite(SH4 *c) {
  hle_logcall(c, "");
  if (!lcd_writes++) rt_log("hle vmu lcd: picture for port %u dropped (no LCD panel yet)", R(4));
  R(0) = 0;
}

/* ---- Jump Pack (pdVib, 'Mx' = the multi-unit API): rtw_rumble(connector, on) ---- */
static int rumble_on[4];
static uint32_t rumble_events;
static void rumble(int conn, int on) {
  if (conn < 0 || conn > 3 || rumble_on[conn] == on) return;
  rumble_on[conn] = on;
  rumble_events++;
  if (rumble_events <= 16) rt_log("hle rumble port %d %s at frame %u", conn, on ? "on" : "off", hle_frame());
  rtw_rumble(conn, on);
}
static uint32_t vib_logged;
static void vib_log(SH4 *c, const char *what) {
  if (vib_logged++ < 8) rt_log("hle vib %s(%x, %x) at frame %u", what, R(4), R(5), hle_frame());
}
static void h_pdVibMxIsReady(SH4 *c) { hle_logcall(c, ""); vib_log(c, "ready"); R(0) = unit_present(R(4), 2) ? 1 : 0; }
static void h_pdVibMxSetStopTime(SH4 *c) { hle_logcall(c, ""); vib_log(c, "stop-time"); R(0) = 0; }
static void h_pdVibMxStart(SH4 *c) {
  hle_logcall(c, "");
  vib_log(c, "start");
  if (unit_present(R(4), 2)) rumble((int)(R(4) / 6), 1);
  R(0) = 0;
}
static void h_pdVibMxStop(SH4 *c) {
  hle_logcall(c, "");
  if (R(4) < 24) rumble((int)(R(4) / 6), 0);
  R(0) = 0;
}

/* ---- bu: the VMU file system (vmu.c) behind the SDK's backup API. Drive 0 = A1 (the game's only one).
 * Return codes as the SDK's: 0 ok, -1 busy (never: everything completes at once), -255 no card,
 * -253 unformatted, -251 file not found, -252 not enough free blocks (the game's "no room" message).
 * The complete callback gets (drive, operation, result): 1 when the card is mounted (the first V-blank after
 * buInit), 0x0C after a save, 0x0E after a load. */
#define BU_ERR_NO_DISK ((uint32_t)-255)
#define BU_ERR_UNFORMAT ((uint32_t)-253)
#define BU_ERR_FULL ((uint32_t)-252)
#define BU_ERR_NOT_FOUND ((uint32_t)-251)
#define BU_OP_MOUNT 1u
#define BU_OP_SAVE 0x0Cu
#define BU_OP_LOAD 0x0Eu
static int bu_inited, bu_mounted;
static uint32_t bu_complete_cb, bu_progress_cb, bu_last_err;
static int bu_ready(uint32_t drive) { return drive == 0 && bu_inited && bu_mounted; }
static void bu_complete(SH4 *c, uint32_t drive, uint32_t op, uint32_t result) {
  if (bu_complete_cb) rt_guest_call(c, bu_complete_cb, drive, op, result, 0);
}
static void guest_name(uint32_t a, char out[13]) {
  int i = 0;
  for (; i < 12; i++) { char ch = (char)rd8(a + (uint32_t)i); if (!ch) break; out[i] = ch; }
  for (int k = i; k < 13; k++) out[k] = 0;
}
static uint8_t bcd(uint32_t v) { return (uint8_t)(((v / 10) % 10) << 4 | v % 10); }
static void h_buInit(SH4 *c) {
  hle_logcall(c, "");
  uint32_t cb = R(7);
  bu_inited = 1; bu_mounted = 0; bu_last_err = 0;
  rt_log("hle vmu: card in A1, %s, %d free blocks", vmu_formatted() ? "formatted" : "not formatted",
         vmu_formatted() ? vmu_free_blocks() : 0);
  if (cb) rt_guest_call(c, cb, 0, 0, 0, 0);
  R(0) = 0;
}
static void h_buExit(SH4 *c) { hle_logcall(c, ""); bu_inited = 0; bu_mounted = 0; R(0) = 0; }
static void h_buSetCompleteCallback(SH4 *c) { hle_logcall(c, ""); bu_complete_cb = R(4); R(0) = 0; }
static void h_buSetProgressCallback(SH4 *c) { hle_logcall(c, ""); bu_progress_cb = R(4); R(0) = 0; }
static void h_bu_IsReady(SH4 *c) { hle_logcall(c, ""); R(0) = bu_ready(R(4)) ? 1 : 0; }
static void h_bu_IsFormatted(SH4 *c) {
  hle_logcall(c, "");
  R(0) = !bu_ready(R(4)) ? BU_ERR_NO_DISK : vmu_formatted() ? 0 : BU_ERR_UNFORMAT;
}
static void h_buStat(SH4 *c) { hle_logcall(c, ""); R(0) = 0; }
static void h_buGetLastError(SH4 *c) { hle_logcall(c, ""); R(0) = R(4) == 0 ? bu_last_err : BU_ERR_NO_DISK; }
/* BUS_DISKINFO as the SDK fills it: the volume label (root 0x10, 32 bytes), u16 blocks 256, user blocks 200,
 * free blocks (twice), the VMU game area 128, its free blocks, 0x2C (an SDK count not used by the game: 0),
 * the volume icon; at 0x30 the root block's time stamp as BUS_TIME (u16 year, month, day, hour, minute,
 * second, day of week) */
static uint32_t unbcd(uint8_t v) { return (uint32_t)(v >> 4) * 10 + (v & 15); }
static void h_buGetDiskInfo(SH4 *c) {
  hle_logcall(c, "");
  uint32_t d = R(5);
  if (!bu_ready(R(4))) { R(0) = BU_ERR_NO_DISK; return; }
  if (!vmu_formatted()) { R(0) = BU_ERR_UNFORMAT; return; }
  const uint8_t *root = vmu_root();
  for (uint32_t i = 0; i < 32; i++) wr8(d + i, root[0x10 + i]);
  int fr = vmu_free_blocks();
  wr16(d + 0x20, 256); wr16(d + 0x22, VMU_USER_BLOCKS); wr16(d + 0x24, (uint16_t)fr); wr16(d + 0x26, (uint16_t)fr);
  wr16(d + 0x28, VMU_EXEC_BLOCKS); wr16(d + 0x2A, (uint16_t)vmu_free_exec()); wr16(d + 0x2C, 0);
  wr16(d + 0x2E, root[0x4E]);
  wr16(d + 0x30, (uint16_t)(unbcd(root[0x30]) * 100 + unbcd(root[0x31])));
  for (int i = 0; i < 6; i++) wr8(d + 0x32 + (uint32_t)i, (uint8_t)unbcd(root[0x32 + i]));
  R(0) = 0;
}
static void h_bu_GetBlocks(SH4 *c) {
  hle_logcall(c, "");
  if (!bu_ready(R(4))) { R(0) = BU_ERR_NO_DISK; return; }
  if (!vmu_formatted()) { R(0) = BU_ERR_UNFORMAT; return; }
  R(0) = (uint32_t)(R(5) == 1 ? vmu_free_exec() : vmu_free_blocks());
}
static void h_bu_FindFile(SH4 *c) {
  hle_logcall(c, "");
  char name[13];
  guest_name(R(5), name);
  if (!bu_ready(R(4))) { R(0) = BU_ERR_NO_DISK; return; }
  if (!vmu_formatted()) { R(0) = BU_ERR_UNFORMAT; return; }
  R(0) = vmu_find(name) >= 0 ? 0 : BU_ERR_NOT_FOUND;
}
/* buSaveFile(drive, name, image, blocks, &BUS_TIME, flags) */
static void h_buSaveFile(SH4 *c) {
  hle_logcall(c, "");
  uint32_t drive = R(4), img = R(6), blocks = R(7), tm = rd32(R(15)), flags = rd32(R(15) + 4);
  char name[13];
  guest_name(R(5), name);
  uint32_t err = 0;
  if (!bu_ready(drive)) err = BU_ERR_NO_DISK;
  else if (!vmu_formatted()) err = BU_ERR_UNFORMAT;
  else {
    static uint8_t buf[VMU_USER_BLOCKS * VMU_BLOCK];
    if (blocks > VMU_USER_BLOCKS) blocks = VMU_USER_BLOCKS;
    for (uint32_t i = 0; i < blocks * VMU_BLOCK; i++) buf[i] = rd8(img + i);
    uint8_t t[8] = {0x19, 0x99, 0x01, 0x01, 0, 0, 0, 0};
    if (tm) {
      uint32_t y = rd16(tm);
      t[0] = bcd(y / 100); t[1] = bcd(y % 100);
      for (int i = 0; i < 6; i++) t[2 + i] = bcd(rd8(tm + 2 + (uint32_t)i));
    }
    int r = vmu_write(name, buf, (int)blocks, t);
    err = r == 0 ? 0 : BU_ERR_FULL;
    rt_log("hle vmu save %u blocks flags %x: %s, %d free blocks, image generation %u (check %d), time %02x%02x-%02x-%02x %02x:%02x:%02x", blocks, flags,
           r == 0 ? "written" : r == -1 ? "not enough free blocks" : "directory full", vmu_free_blocks(),
           vmu_generation, vmu_check(), t[0], t[1], t[2], t[3], t[4], t[5], t[6]);
  }
  bu_last_err = err;
  bu_complete(c, drive, BU_OP_SAVE, err);
  R(0) = 0;
}
/* buLoadFile(drive, name, buf, 0, 0): the whole file, VMS header included */
static void h_buLoadFile(SH4 *c) {
  hle_logcall(c, "");
  uint32_t drive = R(4), dst = R(6);
  char name[13];
  guest_name(R(5), name);
  uint32_t err = 0;
  if (!bu_ready(drive)) err = BU_ERR_NO_DISK;
  else if (!vmu_formatted()) err = BU_ERR_UNFORMAT;
  else {
    int e = vmu_find(name);
    if (e < 0) err = BU_ERR_NOT_FOUND;
    else {
      static uint8_t buf[VMU_USER_BLOCKS * VMU_BLOCK];
      int n = vmu_read(e, buf, VMU_USER_BLOCKS);
      for (int i = 0; i < n * VMU_BLOCK; i++) wr8(dst + (uint32_t)i, buf[i]);
      rt_log("hle vmu load %d blocks", n);
    }
  }
  if (err) rt_log("hle vmu load: error %d", (int)err);
  bu_last_err = err;
  bu_complete(c, drive, BU_OP_LOAD, err);
  R(0) = 0;
}

void io_vblank(SH4 *c) {
  pads_update();
  if (bu_inited && !bu_mounted) {
    bu_mounted = 1;
    rt_log("hle vmu: A1 mounted at frame %u", hle_frame());
    bu_complete(c, 0, BU_OP_MOUNT, 0);
  }
}
void io_init(SH4 *c) {
  (void)c;
  if (vmu_blank()) {
    static const uint8_t t[8] = {0x19, 0x99, 0x09, 0x09, 0, 0, 0, 0x03}; /* the console's launch day */
    vmu_format(t);
    rt_log("hle vmu: blank image, formatted as a new card");
  }
  pads_update();
}

const nj_impl_t io_impls[] = {
  {"pdInitPeripheral", h_pdInitPeripheral}, {"pdExitPeripheral", h_pdExitPeripheral},
  {"pdGetPeripheral", h_pdGetPeripheral},
  {"pdVmsLcdIsReady", h_pdVmsLcdIsReady}, {"pdVmsLcdWrite", h_pdVmsLcdWrite},
  {"pdVibMxIsReady", h_pdVibMxIsReady}, {"pdVibMxSetStopTime", h_pdVibMxSetStopTime},
  {"pdVibMxStart", h_pdVibMxStart}, {"pdVibMxStop", h_pdVibMxStop},
  {"buInit", h_buInit}, {"buExit", h_buExit},
  {"buSetCompleteCallback", h_buSetCompleteCallback}, {"buSetProgressCallback", h_buSetProgressCallback},
  {"bu_IsReady", h_bu_IsReady}, {"bu_IsFormatted", h_bu_IsFormatted}, {"buStat", h_buStat},
  {"buGetLastError", h_buGetLastError}, {"buGetDiskInfo", h_buGetDiskInfo}, {"bu_GetBlocks", h_bu_GetBlocks},
  {"bu_FindFile", h_bu_FindFile}, {"buSaveFile", h_buSaveFile}, {"buLoadFile", h_buLoadFile},
};
const int io_nimpls = (int)(sizeof io_impls / sizeof io_impls[0]);
