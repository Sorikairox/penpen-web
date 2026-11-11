/* Pen Pen port, S7: audio. The Manatee sd host-command layer as HLE (sd.c), our own AICA-like mixer (aica.c)
 * for the one-shot banks the game downloads, CDDA from the disc's audio tracks (cdda.c), and the retrowser
 * audio exports (pull model: rtw_audio_rate / rtw_audio_drain / rtw_audio_buffer). Written from public
 * documentation of the AICA (slot registers, Yamaha 4-bit ADPCM) and from what the SDK's own wrappers do
 * (docs/port/AUDIO.md); no emulator code. Deterministic: 735 stereo frames per game frame, integer mixing,
 * tables built with basic IEEE arithmetic only (native = wasm, bit for bit). */
#ifndef AUDIO_H
#define AUDIO_H
#include "../runtime/rt.h"
#include "../render/nj.h"

#define AUDIO_RATE 44100
#define AUDIO_PER_FRAME 735 /* 44100 / 60: the host steps the game at 60 Hz */
extern int audio_enabled;   /* test mode --audio 0: the S6 stubs (no sd HLE, CDDA only logged) */
extern int audio_solo;      /* test mode: 1 = CDDA only, 2 = voices only */

/* ---- the mixer (aica.c) ---- */
#define ARAM_SIZE (2u << 20) /* the AICA's 2 MiB of sound RAM; banks are downloaded at their own addresses */
extern uint8_t aica_ram[ARAM_SIZE];
#define NVOICE 64
/* key on a voice from a 24-halfword slot image (the AICA channel registers 0x00..0x44 packed, as a one-shot
 * bank stores them); base = the bank's sound-RAM address (sample addresses are bank-relative). Returns a
 * voice handle (> 0) or 0 when no voice is free. */
uint32_t aica_key_on(const uint16_t *regs, uint32_t base, int prio);
void aica_key_off(uint32_t h);                       /* release (envelope RR) */
int aica_alive(uint32_t h);                          /* the voice still plays this sound */
void aica_set_port(uint32_t h, int vol, int pan, int cents); /* vol, pan -128..127; pitch in cents */
void aica_master(int mvol);                          /* 0..15 */
int32_t aica_master_gain(void);                      /* Q15 */
void aica_mono(int on);
void aica_stop_range(uint32_t addr, uint32_t size);  /* cut the voices playing from that sound RAM */
void aica_mix(int32_t *mix, int n);                  /* add n stereo frames */
int aica_voices(void);                               /* voices playing (log) */

/* ---- CDDA (cdda.c) ---- */
void cdda_play(int start, int end, int repeat);
void cdda_pause(void);
void cdda_stop(void);
void cdda_release(void);
int cdda_is_playing(void);
void cdda_volume(int l, int r); /* 0..127 per channel */
void cdda_pan(int l, int r);    /* -128..127 per channel */
void cdda_mix(int32_t *mix, int n);
int cdda_track(void);

/* ---- output (aica.c) ---- */
void audio_frame(void);         /* one game frame: AUDIO_PER_FRAME stereo frames into the output buffer */
uint32_t audio_digest(void);    /* FNV-1a of every sample made so far */
int rtw_audio_rate(void);         /* the retrowser exports (aica.c) */
int rtw_audio_drain(void);
int rtw_audio_buffer(void);
int16_t *audio_out(void);
uint32_t audio_frames_made(void);
int audio_peak(void);           /* the largest |sample| of the last frame */

/* ---- the Manatee HLE (sd.c) ---- */
extern const nj_impl_t sd_impls[];
extern const int sd_nimpls;

#endif
