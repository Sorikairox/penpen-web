/* Pen Pen port, S7: unit checks of our mixer on our own signals (no disc data): a sine is encoded with our own
 * Yamaha 4-bit ADPCM encoder (the inverse of the decoder's adaptation rule), keyed on from a slot image, and
 * the mixed output must follow it (SNR), at OCT -1 at half the rate (zero crossings), with loops, 8/16-bit PCM,
 * TL and DIPAN levels, port volume/pan/pitch, the envelope's release and the 6 dB output headroom.
 * Build: cc -I.. test_aica.c aica.c -lm (tools/s7.sh test). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

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

  printf("audio %d checks, %d failures\n", checks, fails);
  return fails != 0;
}
