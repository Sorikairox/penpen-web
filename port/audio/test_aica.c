/* Pen Pen port, S7: unit checks of our mixer on our own signals (no disc data): a sine is encoded with our own
 * Yamaha 4-bit ADPCM encoder (the inverse of the decoder's adaptation rule), keyed on from a slot image, and
 * the mixed output must follow it (SNR), at OCT -1 at half the rate (zero crossings), with loops, 8/16-bit PCM,
 * TL and DIPAN levels, port volume/pan/pitch, the envelope's release and the 6 dB output headroom.
 * Build: cc -I.. test_aica.c aica.c -lm (tools/s7.sh test). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"
#include "seq.h"

int audio_enabled = 1;
void cdda_mix(int32_t *mix, int n) { (void)mix; (void)n; }
void rt_log(const char *fmt, ...) { (void)fmt; }

static int fails, checks;
static void check(int ok, const char *what, double v) {
  checks++;
  if (!ok) { fails++; printf("audio FAIL %s (%g)\n", what, v); }
}

static const int sc[8] = {230, 230, 230, 230, 307, 409, 512, 614};
static void encode(const int16_t *x, int n, uint8_t *out) { /* greedy encoder tracking the decoder state */
  int s = 0, st = 127;
  for (int k = 0; k < n; k++) {
    int best = 0, bs = 0, be = 1 << 30;
    for (int nib = 0; nib < 16; nib++) {
      int m = nib & 7, d = ((2 * m + 1) * st) >> 3, t = (nib & 8) ? s - d : s + d;
      t = t > 32767 ? 32767 : t < -32768 ? -32768 : t;
      int e = abs(t - x[k]);
      if (e < be) { be = e; best = nib; bs = t; }
    }
    s = bs;
    st = (st * sc[best & 7]) >> 8;
    st = st < 127 ? 127 : st > 24576 ? 24576 : st;
    if (k & 1) out[k / 2] |= (uint8_t)(best << 4); else out[k / 2] = (uint8_t)best;
  }
}
static void slot(uint16_t *r, int pcms, int loop, uint32_t sa, int lsa, int lea, int oct) {
  memset(r, 0, 48);
  r[0] = (uint16_t)(loop << 9 | pcms << 7 | (sa >> 16 & 0x7F)); r[1] = (uint16_t)sa;
  r[2] = (uint16_t)lsa; r[3] = (uint16_t)lea;
  r[4] = 31;              /* AR 31, D1R 0, D2R 0 */
  r[5] = 0xF << 10 | 31;  /* KRS off, DL 0, RR 31 */
  r[6] = (uint16_t)((oct & 15) << 11);
  r[9] = 0xF << 8;        /* DISDL 15, DIPAN centre */
}
static int32_t mix[2 * 8192];
static void run(int n) { memset(mix, 0, sizeof mix); aica_mix(mix, n); }

int main(void) {
  enum { N = 8000 };
  static int16_t x[N];
  for (int i = 0; i < N; i++) x[i] = (int16_t)(12000 * sin(2 * M_PI * 441.0 * i / 44100.0));
  uint16_t r[24];
  aica_master(15);

  /* ADPCM at the output rate: follows the sine */
  encode(x, N, aica_ram + 0x1000);
  slot(r, 2, 0, 0x1000, 0, N - 1, 0);
  uint32_t h = aica_key_on(r, 0, 0);
  check(h != 0, "key on", 0);
  run(4000);
  double se = 0, ss = 0;
  for (int i = 100; i < 4000; i++) { double d = mix[2 * i] - x[i]; se += d * d; ss += (double)x[i] * x[i]; }
  double snr = 10 * log10(ss / se);
  check(snr > 20, "ADPCM SNR > 20 dB", snr);
  check(mix[2 * 1000] == mix[2 * 1000 + 1], "centre pan: L = R", mix[2 * 1000] - mix[2 * 1000 + 1]);
  /* the voice ends at LEA */
  run(8192);
  check(!aica_alive(h), "a one-shot ends after LEA", 0);

  /* OCT -1: half the rate (zero crossings of a 441 Hz sine played at half speed: 220.5 Hz) */
  slot(r, 2, 0, 0x1000, 0, N - 1, -1);
  h = aica_key_on(r, 0, 0);
  run(8192);
  int zc = 0;
  for (int i = 1; i < 8192; i++) zc += (mix[2 * i - 2] < 0) != (mix[2 * i] < 0);
  check(zc >= 80 && zc <= 84, "OCT -1 halves the pitch (zero crossings ~82)", zc);

  /* port pitch +1200 cents on OCT -1 = the original rate again */
  slot(r, 2, 0, 0x1000, 0, N - 1, -1);
  h = aica_key_on(r, 0, 0);
  aica_set_port(h, 127, 0, 1200);
  run(4000);
  zc = 0;
  for (int i = 1; i < 4000; i++) zc += (mix[2 * i - 2] < 0) != (mix[2 * i] < 0);
  check(zc >= 78 && zc <= 82, "+1200 cents doubles the rate (zero crossings ~80)", zc);
  run(8192);

  /* 16-bit PCM with a loop: still sounding after many times its length */
  for (int i = 0; i < 1000; i++) { aica_ram[0x40000 + 2 * i] = (uint8_t)x[i]; aica_ram[0x40001 + 2 * i] = (uint8_t)(x[i] >> 8); }
  slot(r, 0, 1, 0x40000, 0, 999, 0);
  h = aica_key_on(r, 0, 0);
  run(8192);
  check(aica_alive(h), "a looping voice keeps playing", 0);
  int pk = 0;
  for (int i = 7000; i < 8192; i++) pk = abs(mix[2 * i]) > pk ? abs(mix[2 * i]) : pk;
  check(pk > 11000 && pk <= 12000, "PCM16 loop level", pk);
  /* port volume 0 = half level (0x80 of 255), pan hard right silences the left */
  aica_set_port(h, 0, 127, 0);
  run(1000);
  int pl = 0, pr = 0;
  for (int i = 0; i < 1000; i++) { pl = abs(mix[2 * i]) > pl ? abs(mix[2 * i]) : pl; pr = abs(mix[2 * i + 1]) > pr ? abs(mix[2 * i + 1]) : pr; }
  check(pl == 0, "pan 127: left silent", pl);
  check(pr > 5500 && pr < 6100, "vol 0: half level", pr);
  /* release: RR 31 ends the voice within a few ms */
  aica_key_off(h);
  run(1000);
  check(!aica_alive(h), "release ends the voice", 0);

  /* TL 16 = -6 dB; DIPAN 0x1F (left, right side off) */
  for (int i = 0; i < 1000; i++) aica_ram[0x50000 + i] = (uint8_t)(int8_t)(x[i] >> 8);
  slot(r, 1, 1, 0x50000, 0, 999, 0);
  r[10] = 16 << 8;
  r[9] = 0xF << 8 | 0x1F;
  h = aica_key_on(r, 0, 0);
  run(2000);
  pl = pr = 0;
  for (int i = 1000; i < 2000; i++) { pl = abs(mix[2 * i]) > pl ? abs(mix[2 * i]) : pl; pr = abs(mix[2 * i + 1]) > pr ? abs(mix[2 * i + 1]) : pr; }
  check(pl > 5600 && pl < 6300, "PCM8, TL 16 = -6 dB", pl);
  check(pr == 0, "DIPAN 0x1F: right off", pr);
  aica_key_off(h);
  run(2000);

  /* the output: 735 frames per game frame, 6 dB headroom, drained by the host */
  slot(r, 0, 1, 0x40000, 0, 999, 0);
  h = aica_key_on(r, 0, 0);
  audio_frame();
  audio_frame();
  int n = rtw_audio_drain();
  check(n == 2 * AUDIO_PER_FRAME, "two frames drained", n);
  int16_t *o = audio_out();
  pk = 0;
  for (int i = 0; i < n; i++) pk = abs(o[2 * i]) > pk ? abs(o[2 * i]) : pk;
  check(pk > 5500 && pk <= 6000, "output headroom: a 12000 peak comes out at half", pk);
  check(rtw_audio_drain() == 0, "nothing pending after a drain", 0);
  check(rtw_audio_rate() == 44100, "rate", rtw_audio_rate());

  /* S8: a MIDI sequence on a program of our own: SMPB at 0x100000 (one program, one layer, one split = a looping
   * PCM16 sample, key 48..72, base 60), SMSB at 0x180000 (tempo 500 ms, CC 7, program 0, a reference to two
   * notes (key 72 vel 127, gate and delta 480 as 2 bytes; key 60, gate 240, delta 0x300 as 2 bytes), then back
   * to the end event */
  aica_key_off(h); for (int i = 0; i < 8; i++) audio_frame(); rtw_audio_drain();
  {
    uint8_t *P = aica_ram + 0x100000, *S = aica_ram + 0x180000;
    memset(P, 0, 0x2000); memset(S, 0, 0x200);
    memcpy(P, "SMPB", 4); P[4] = 1; P[0x10] = 0x40; P[0x14] = 1;               /* program table at 0x40, 1 program */
    P[0x40] = 0x50;                                                           /* program 0 at 0x50 */
    P[0x50] = 0x70;                                                           /* layer 0 at 0x70 */
    P[0x70] = 1; P[0x74] = 0x80; P[0x7C] = 2;                                 /* 1 split at 0x80, bend 2 */
    for (int i = 0; i < 1000; i++) { int16_t v = (int16_t)(8000 * sin(2 * M_PI * i / 100.0)); P[0x1000 + 2 * i] = (uint8_t)v; P[0x1001 + 2 * i] = (uint8_t)(v >> 8); }
    slot(r, 0, 1, 0x1000, 0, 999, 0);
    memcpy(P + 0x80, r, 36); P[0x80 + 0x24] = 48; P[0x80 + 0x25] = 72; P[0x80 + 0x26] = 60;
    memcpy(S, "SMSB", 4); S[4] = 1; S[12] = 1; S[16] = 0x20;                  /* 1 sequence at 0x20 */
    static const uint8_t ev[] = {'S', 'M', 'S', 'D', 0x88, 0, 0, 0, 0xF4, 1, 0, 0,
      0x84, 0x01, 0xF4, 0, 0xB0, 7, 127, 0, 0xC0, 0, 0, 0x81, 0x00, 0x1C, 2, 0x83,
      0x30, 72, 127, 0x01, 0xE0, 0x01, 0xE0, 0x10, 60, 127, 0xF0, 0x03, 0x00};
    memcpy(S + 0x20, ev, sizeof ev);
    seq_bank(0x42504D53u, 0, 0x100000, 0x2000);
    seq_bank(0x42534D53u, 0, 0x180000, 0x200);
    seq_play(0, 0, 0);
    /* 500 ms per quarter, 480 ticks: the first note lasts 0.5 s, the second starts at 0.5 s for 0.25 s */
    for (int f = 0; f < 15; f++) audio_frame(); /* 0.25 s */
    check(aica_voices() == 1, "midi: one note at 0.25 s", aica_voices());
    int16_t *q = audio_out(); int zc = 0;
    for (int i = 1; i < 735 * 15; i++) zc += (q[2 * (i - 1)] < 0) != (q[2 * i] < 0);
    /* key 72 over base 60 = one octave up: the 441 Hz sample cycle (100 samples) plays at 882 Hz */
    check(zc > 2 * 882 * 0.25 * 0.85 && zc < 2 * 882 * 0.25 * 1.15, "midi: key 72 on base 60 plays an octave up", zc);
    rtw_audio_drain();
    for (int f = 0; f < 21; f++) audio_frame(); /* 0.6 s: the second note */
    check(aica_voices() == 1 && seq_playing(0), "midi: the second note after the first gate", aica_voices());
    for (int f = 0; f < 15; f++) audio_frame(); /* 0.85 s: past its gate (0.75 s) and the end */
    check(aica_voices() == 0, "midi: released after the gate", aica_voices());
    check(seq_playing(0), "midi: still waiting the last delta (768 ticks = 0.8 s from 0.5 s)", 0);
    for (int f = 0; f < 30; f++) audio_frame(); /* 1.35 s */
    check(!seq_playing(0), "midi: the sequence ended at 1.3 s", 0);
    rtw_audio_drain();
  }

  printf("audio %d checks, %d failures\n", checks, fails);
  return fails != 0;
}
