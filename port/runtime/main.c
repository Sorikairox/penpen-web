/* Pen Pen port, S3: boot. Read IP.BIN and 1ST_READ.BIN from the player's disc into guest RAM (as the
 * BIOS does: IP.BIN at 0x8C008000, the program at 0x8C010000), set the CPU up as the BIOS leaves it,
 * and call the entry point; the recompiled code then runs until the frame limit, a stop or a fault.
 * Options (native argv / wasm exports): --frames N, --verbose N (first HLE calls logged), --pad SCRIPT
 * (hle.c: "F[+N]:BTN|BTN,..." on port A), --digest N (RAM digest every N frames, 0 = off),
 * --frame-log 0|1 (the per-frame line), --watch A,B,... (guest words shown on the frame line); S7: --audio 0|1 (the
 * sd HLE + CDDA + mixer, default on), --audio-solo cdda|sfx (hear one source), --wav FILE (native: every drained sample into a 44.1 kHz stereo WAV). */
#include "rt.h"
#include "hle_io.h"
#include "vmu.h"
#include "../audio/audio.h"

#define ENTRY 0x8C010000u

static SH4 cpu;

static int load_file(const char *name, uint32_t at) {
  uint32_t lba, size;
  if (disc_find(name, &lba, &size)) return -1;
  static uint8_t buf[256 * 2048];
  for (uint32_t done = 0; done < size;) {
    uint32_t n = (size - done + 2047) / 2048;
    if (n > 256) n = 256;
    if (disc_read_lba(lba + done / 2048, n, buf)) return -2;
    uint32_t k = n * 2048 < size - done ? n * 2048 : size - done;
    memcpy(sh4_ram + ((at + done) & 0xFFFFFF), buf, k);
    done += k;
  }
  return (int)size;
}

#ifdef __wasm__
#define EXPORT(n) __attribute__((export_name(n)))
#else
#define EXPORT(n)
#endif

/* rtw_start: load the program from the disc and set the CPU up; the game runs in rtw_frame. */
EXPORT("rtw_start") int rtw_start(void) {
  SH4 *c = &cpu;
  rt_cpu = c;
  sh4_init_tables();
  if (disc_init() < 0) rt_stop("no disc");
  rt_log("boot disc files %d", disc_file_count());
  static uint8_t ip[16 * 2048];
  if (disc_read_lba(45000, 16, ip)) rt_stop("cannot read IP.BIN");
  memcpy(sh4_ram + 0x8000, ip, sizeof ip);
  int n = load_file("1ST_READ.BIN", ENTRY);
  if (n <= 0) rt_stop("1ST_READ.BIN not found");
  rt_log("boot 1ST_READ.BIN %u bytes at %08x, %d recompiled functions", (uint32_t)n, ENTRY, rt_fn_count);
  sh4_reset(c);
  hle_init(c);
  /* the state the BIOS boot leaves: privileged, bank 1, interrupts masked; stack below IP.BIN's end */
  sh4_set_sr(c, 0x600000F0u);
  c->r[15] = 0x8C00F400u;
  c->vbr = 0x8C00F400u;
  c->fpscr = 0x00040001u;
  c->pr = 0x8C000F00u;
  return 0;
}

/* rtw_frame: run the game until its next njWaitVSync (S6, rt_gen.h): the first call enters the program,
 * the next ones rewind into the njWaitVSync where the previous frame stopped. */
static int started;
EXPORT("rtw_frame") int rtw_frame(void) {
  SH4 *c = &cpu;
  c->budget = 200000000; /* the watchdog counts back edges per frame */
  io_host_frame();
  if (!started) { started = 1; rt_log("boot calling the entry point"); }
  else rt_rew = 1;
  rt_call(c, ENTRY);
  if (rt_unw) { rt_unw = 0; return 0; }
  rt_log("boot the entry point returned to %08x", c->pc);
  rt_stop("returned");
}

/* input (retrowser spec 4.5): Dreamcast Maple bits active-high, stick -127..127 up positive; triggers 0..255 */
EXPORT("rtw_input") int rtw_input(int port, int buttons, int sx, int sy) { io_host_input(port, (uint32_t)buttons, sx, sy); return 0; }
EXPORT("rtw_triggers") int rtw_triggers(int port, int l, int r) { io_host_triggers(port, l, r); return 0; }
/* the VMU in A1 (save.kind vmu): the 128 KiB flash image; the generation changes on every write */
EXPORT("rtw_save_ptr") int rtw_save_ptr(void) { return (int)(intptr_t)vmu_img; }
EXPORT("rtw_save_size") int rtw_save_size(void) { return VMU_SIZE; }
EXPORT("rtw_save_generation") int rtw_save_generation(void) { return (int)vmu_generation; }
EXPORT("rtw_save_load") int rtw_save_load(int ptr, int size) {
  (void)ptr;
  rt_log("boot vmu image from the host: %d bytes, %s", size, vmu_formatted() ? "formatted" : vmu_blank() ? "blank" : "not formatted");
  return 0;
}

#ifndef __wasm__
#include <stdio.h>
#include <stdlib.h>
extern const char *disc_dir;
int rtw_log(const char *p, int len) { fwrite(p, 1, (size_t)len, stdout); fputc('\n', stdout); fflush(stdout); return 0; }
int rtw_rumble(int port, int on) { (void)port; (void)on; return 0; }
/* --vmu FILE: the A1 image, read at start (if present) and written back whenever the game saved */
static const char *vmu_path;
static uint32_t vmu_saved_gen;
static void vmu_store(void) {
  if (!vmu_path || vmu_generation == vmu_saved_gen) return;
  FILE *f = fopen(vmu_path, "wb");
  if (f) { fwrite(vmu_img, 1, VMU_SIZE, f); fclose(f); vmu_saved_gen = vmu_generation; rt_log("boot vmu image written (generation %u)", vmu_generation); }
}
/* --wav FILE: what the host would drain after each frame, as a WAV (the header is rewritten at exit) */
static FILE *wav;
static uint32_t wav_frames;
static void wav_header(void) {
  uint8_t h[44];
  uint32_t data = wav_frames * 4, v[] = {0x46464952u, 36 + data, 0x45564157u, 0x20746D66u, 16, 0x00020001u, AUDIO_RATE,
                                          AUDIO_RATE * 4, 0x00100004u, 0x61746164u, data};
  for (int i = 0; i < 11; i++) for (int k = 0; k < 4; k++) h[4 * i + k] = (uint8_t)(v[i] >> (8 * k));
  fseek(wav, 0, SEEK_SET); fwrite(h, 1, 44, wav); fseek(wav, 0, SEEK_END);
}
static void wav_drain(void) {
  int n = rtw_audio_drain();
  if (wav && n > 0) { fwrite(audio_out(), 4, (size_t)n, wav); wav_frames += (uint32_t)n; }
}
/* --ram-dump FILE: guest main RAM at exit (to locate what a change touched; the file stays in the volume) */
static const char *ram_dump;
static void ram_dump_write(void) {
  FILE *f = ram_dump ? fopen(ram_dump, "wb") : 0;
  if (f) { fwrite(sh4_ram, 1, 16u << 20, f); fclose(f); }
}
static void wav_close(void) { if (wav) { wav_drain(); wav_header(); fclose(wav); wav = 0; rt_log("boot wav written: %u frames", wav_frames); } }
int main(int argc, char **argv) {
  hle_max_frames = 3; hle_verbose = 600; hle_digest_every = 1; hle_frame_log = 1; /* the S3/S4 test defaults */
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--disc")) disc_dir = argv[++i];
    else if (!strcmp(argv[i], "--frames")) hle_max_frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--verbose")) hle_verbose = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--pad")) { if (hle_set_pad_script(argv[++i]) < 0) return 2; }
    else if (!strcmp(argv[i], "--watch")) { if (hle_set_watch(argv[++i]) < 0) return 2; }
    else if (!strcmp(argv[i], "--digest")) hle_digest_every = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--frame-log")) hle_frame_log = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--vmu")) vmu_path = argv[++i];
    else if (!strcmp(argv[i], "--ram-dump")) ram_dump = argv[++i];
    else if (!strcmp(argv[i], "--audio")) audio_enabled = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--audio-solo")) audio_solo = !strcmp(argv[++i], "cdda") ? 1 : !strcmp(argv[i], "sfx") ? 2 : 0;
    else if (!strcmp(argv[i], "--wav")) { wav = fopen(argv[++i], "wb+"); if (!wav) return 2; wav_header(); }
    else if (!strcmp(argv[i], "--force-interp")) { extern uint32_t rt_force_interp; rt_force_interp = (uint32_t)strtoul(argv[++i], 0, 16); }
  }
  if (vmu_path) {
    FILE *f = fopen(vmu_path, "rb");
    if (f) { size_t n = fread(vmu_img, 1, VMU_SIZE, f); fclose(f); rtw_save_load(0, (int)n); }
  }
  atexit(vmu_store);
  atexit(wav_close);
  atexit(ram_dump_write);
  rtw_start();
  for (;;) { rtw_frame(); vmu_store(); wav_drain(); }
}
#else
__attribute__((export_name("rt_set_frames"))) void rt_set_frames(int n) { hle_max_frames = n; }
__attribute__((export_name("rt_set_verbose"))) void rt_set_verbose(int n) { hle_verbose = n; }
__attribute__((export_name("rt_set_digest"))) void rt_set_digest(int n) { hle_digest_every = n; }
__attribute__((export_name("rt_set_frame_log"))) void rt_set_frame_log(int n) { hle_frame_log = n; }
__attribute__((export_name("rt_set_audio"))) void rt_set_audio(int n) { audio_enabled = n; }
/* the host writes a NUL-terminated pad script into rt_arg_buf, then calls rt_set_pad */
static char arg_buf[4096];
__attribute__((export_name("rt_arg_buf"))) char *rt_arg_buf(void) { return arg_buf; }
__attribute__((export_name("rt_set_pad"))) int rt_set_pad(void) { arg_buf[sizeof arg_buf - 1] = 0; return hle_set_pad_script(arg_buf); }
#endif
