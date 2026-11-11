/* Pen Pen port, S7: an AICA-like mixer of our own (docs/port/AUDIO.md 3), from the public description of the
 * AICA's channel ("slot") registers:
 *   0x00 KYONB/SSCTL/LPCTL/PCMS (bits 14/10/9/8-7) + SA bits 22-16    0x04 SA bits 15-0
 *   0x08 LSA (loop start, samples)    0x0C LEA (loop end / end, samples)
 *   0x10 D2R(15-11) D1R(10-6) AR(4-0)  0x14 LPSLNK(14) KRS(13-10) DL(9-5) RR(4-0)
 *   0x18 OCT(14-11, signed) FNS(10-0)  0x1C LFO   0x20 ISEL(3-0) IMXL(7-4)   0x24 DISDL(11-8) DIPAN(4-0)
 *   0x28 TL(15-8) Q(4-0)   0x2C..0x3C FLV0-4   0x40/0x44 filter envelope rates
 * Formats: PCMS 0 = 16-bit signed, 1 = 8-bit signed, 2 = Yamaha 4-bit ADPCM (low nibble first; the step
 * adapts by 230/230/230/230/307/409/512/614 / 256 per magnitude 0..7, clamped to 127..24576; the sample by
 * (2m+1) * step / 8, clamped to 16 bits). Pitch: 44100 Hz * 2^OCT * (1 + FNS/1024), linear interpolation.
 * Levels: TL 0.375 dB per step, the envelope 0.09375 dB per step (10 bits), DISDL and MVOL 3 dB per step
 * from 15 (0 = off), DIPAN 3 dB per step on one side (bit 4 = to the left), 15 = that side off.
 * Not modelled: the DSP (ISEL/IMXL sends; a voice with no direct level is sent dry at its IMXL level),
 * the filter (FLV/FEG) and the LFOs. Integer mixing; every table is built with + * / only. */
#include "audio.h"
#include "seq.h"

uint8_t aica_ram[ARAM_SIZE];

typedef struct {
  uint32_t tag;              /* 0 = free; else the handle of the sound playing */
  int fmt, loop, prio;
  uint32_t sa, lsa, lea;     /* byte address; loop start/end in samples */
  uint32_t ipos, frac, step; /* sample index, fraction (Q20), step (Q20) */
  uint32_t base_step;
  /* ADPCM: decoded samples d[ipos], d[ipos + 1]; the decoder state after d[dk]; a snapshot at the loop start */
  int32_t d0, d1, ds, dstep;
  uint32_t dk;
  int snap_ok;
  int32_t sd0, sd1, sds, sdstep;
  /* envelope: attenuation Q16 of 0..0x3FF, state 0 attack 1 decay1 2 decay2 3 release */
  int eg_state;
  int32_t eg, inc[4], dl;
  int st_l, st_r;            /* static attenuation per side (TL, DISDL/DIPAN), 0x3FF+ = off */
  int st_fx;                 /* S8: attenuation of the DSP send (TL, IMXL), 0x400 = no send */
  int32_t pv_l, pv_r;        /* port volume/pan multipliers, Q15 */
} voice;

static voice vo[NVOICE];
static uint32_t next_tag = 1;
static int mvol = 15, mono;
static int32_t gain_tab[1025];   /* attenuation (0.09375 dB steps) -> Q15 gain; [1024] = 0 */
static int32_t eg_ms10[64];      /* envelope decay time over the full range, 0.1 ms, per effective rate */
static uint32_t cents_tab[1200]; /* 2^(c/1200), Q16 */
static int tables;

static void build_tables(void) {
  double g = 32767.0;
  for (int i = 0; i < 1024; i++) { gain_tab[i] = (int32_t)(g + 0.5); g *= 0.9892646717677409; }
  gain_tab[1024] = 0;
  /* decay time of the full range: 118.2 s at rate 2, halving every 4 rates (the AICA's documented table follows
   * this curve within a few per cent); rates 0/1 never move */
  double t = 1182000.0;
  for (int r = 0; r < 64; r++) {
    if (r < 2) { eg_ms10[r] = 0; continue; }
    eg_ms10[r] = (int32_t)(t + 0.5);
    if (eg_ms10[r] < 1) eg_ms10[r] = 1;
    t *= 0.8408964152537145;
  }
  double c = 65536.0;
  for (int i = 0; i < 1200; i++) { cents_tab[i] = (uint32_t)(c + 0.5); c *= 1.0005777895065548; }
  tables = 1;
}

/* per-sample envelope increment (Q16 of the 10-bit range) for a rate; attack runs ~14.6 times faster */
static int32_t eg_inc(int eff, int attack) {
  if (eff < 2) return 0;
  if (eff > 63) eff = 63;
  if (attack && eff >= 62) return 0x3FF << 16; /* instant */
  int64_t samples = (int64_t)eg_ms10[eff] * AUDIO_RATE / 10000; /* samples over the full range */
  if (attack) samples = samples * 10 / 146;
  if (samples < 1) samples = 1;
  int64_t d = ((int64_t)0x3FF << 16) / samples;
  return d < 1 ? 1 : (int32_t)d;
}
static int eff_rate(int r, int krs, int oct, int fns) {
  if (!r) return 0;
  int e = 2 * r;
  if (krs != 0xF) e += (krs + oct) * 2 + ((fns >> 9) & 1);
  return e < 0 ? 0 : e > 63 ? 63 : e;
}

static int16_t ram16(uint32_t a) { a &= ARAM_SIZE - 2; return (int16_t)(aica_ram[a] | aica_ram[a + 1] << 8); }

static const int ad_scale[8] = {230, 230, 230, 230, 307, 409, 512, 614};
static int32_t adpcm_next(voice *v) { /* decode d[dk + 1] */
  uint32_t k = v->dk + 1;
  uint8_t b = aica_ram[(v->sa + k / 2) & (ARAM_SIZE - 1)];
  int n = (k & 1) ? b >> 4 : b & 15, m = n & 7;
  int32_t delta = ((2 * m + 1) * v->dstep) >> 3;
  v->ds += (n & 8) ? -delta : delta;
  if (v->ds > 32767) v->ds = 32767;
  if (v->ds < -32768) v->ds = -32768;
  v->dstep = (v->dstep * ad_scale[m]) >> 8;
  if (v->dstep < 127) v->dstep = 127;
  if (v->dstep > 24576) v->dstep = 24576;
  v->dk = k;
  return v->ds;
}
static void adpcm_start(voice *v) {
  v->ds = 0; v->dstep = 127; v->dk = (uint32_t)-1;
  v->d0 = adpcm_next(v);
  v->d1 = adpcm_next(v);
  v->snap_ok = 0;
}
static void adpcm_snap(voice *v) {
  if (v->loop && !v->snap_ok && v->ipos == v->lsa) {
    v->snap_ok = 1; v->sd0 = v->d0; v->sd1 = v->d1; v->sds = v->ds; v->sdstep = v->dstep;
  }
}
static int32_t pcm_at(voice *v, uint32_t i) {
  if (i > v->lea) i = v->loop ? v->lsa : v->lea;
  if (v->fmt == 0) return ram16(v->sa + 2 * i);
  return (int32_t)(int8_t)aica_ram[(v->sa + i) & (ARAM_SIZE - 1)] << 8;
}

static void set_static(voice *v, const uint16_t *r) {
  int tl = r[10] >> 8, disdl = (r[9] >> 8) & 15, dipan = r[9] & 31, imxl = (r[8] >> 4) & 15;
  int lvl = disdl; /* S8: the DSP send goes to the reverb (fx_*) instead of being heard dry */
  int base = tl * 4 + (lvl ? (15 - lvl) * 32 : 0x400), side = (dipan & 15) == 15 ? 0x400 : (dipan & 15) * 32;
  v->st_fx = imxl ? tl * 4 + (15 - imxl) * 32 : 0x400;
  v->st_l = base + ((dipan & 16) ? 0 : side);
  v->st_r = base + ((dipan & 16) ? side : 0);
}

uint32_t aica_key_on(const uint16_t *r, uint32_t base, int prio) {
  if (!tables) build_tables();
  int k = -1;
  for (int i = 0; i < NVOICE; i++) if (!vo[i].tag) { k = i; break; }
  if (k < 0) { /* steal: the quietest releasing voice, else the lowest priority (higher number), oldest */
    for (int i = 0; i < NVOICE; i++)
      if (vo[i].eg_state == 3 && (k < 0 || vo[i].eg > vo[k].eg)) k = i;
    if (k < 0)
      for (int i = 0; i < NVOICE; i++)
        if (vo[i].prio >= prio && (k < 0 || vo[i].prio > vo[k].prio || (vo[i].prio == vo[k].prio && vo[i].tag < vo[k].tag))) k = i;
    if (k < 0) return 0;
  }
  voice *v = &vo[k];
  memset(v, 0, sizeof *v);
  v->fmt = (r[0] >> 7) & 3;
  if (v->fmt == 3) v->fmt = 2;
  v->loop = (r[0] >> 9) & 1;
  v->sa = (base + ((uint32_t)(r[0] & 0x7F) << 16 | r[1])) & (ARAM_SIZE - 1);
  v->lsa = r[2]; v->lea = r[3];
  if (v->lsa > v->lea) v->lsa = v->lea;
  int oct = (r[6] >> 11) & 15, fns = r[6] & 1023;
  if (oct > 7) oct -= 16;
  v->base_step = v->step = (uint32_t)(1024 + fns) << (10 + oct);
  int ar = r[4] & 31, d1r = (r[4] >> 6) & 31, d2r = (r[4] >> 11) & 31, rr = r[5] & 31, krs = (r[5] >> 10) & 15;
  v->dl = (int32_t)((r[5] >> 5) & 31) << (5 + 16);
  v->inc[0] = eg_inc(eff_rate(ar, krs, oct, fns), 1);
  v->inc[1] = eg_inc(eff_rate(d1r, krs, oct, fns), 0);
  v->inc[2] = eg_inc(eff_rate(d2r, krs, oct, fns), 0);
  v->inc[3] = eg_inc(eff_rate(rr, krs, oct, fns), 0);
  v->eg = 0x3FF << 16;
  v->eg_state = 0;
  set_static(v, r);
  v->pv_l = v->pv_r = 32767;
  v->prio = prio;
  if (v->fmt == 2) adpcm_start(v);
  v->tag = next_tag++;
  if (!next_tag) next_tag = 1;
  return (uint32_t)k << 24 | (v->tag & 0xFFFFFF);
}
static voice *by_handle(uint32_t h) {
  if (!h) return 0;
  voice *v = &vo[(h >> 24) % NVOICE];
  return v->tag && (v->tag & 0xFFFFFF) == (h & 0xFFFFFF) ? v : 0;
}
int aica_alive(uint32_t h) { return by_handle(h) != 0; }
void aica_key_off(uint32_t h) { voice *v = by_handle(h); if (v) v->eg_state = 3; }
void aica_set_port(uint32_t h, int vol, int pan, int cents) {
  voice *v = by_handle(h);
  if (!v) return;
  /* the wrapper sends vol + 0x80: 0..255, taken as a linear level (127 = full; the game uses -85..127) */
  vol = vol < -128 ? 0 : vol > 127 ? 255 : vol + 128;
  int32_t g = (int32_t)vol * 32767 / 255;
  int32_t pl = pan > 0 ? (127 - (pan > 127 ? 127 : pan)) * 32767 / 127 : 32767;
  int32_t pr = pan < 0 ? (127 + (pan < -127 ? -127 : pan)) * 32767 / 127 : 32767;
  v->pv_l = g * pl >> 15; v->pv_r = g * pr >> 15;
  /* pitch: cents relative to the bank's own OCT/FNS */
  int oct = 0;
  while (cents < 0) { cents += 1200; oct--; }
  while (cents >= 1200) { cents -= 1200; oct++; }
  uint64_t s = (uint64_t)v->base_step * cents_tab[cents] >> 16;
  s = oct >= 0 ? s << oct : s >> -oct;
  v->step = s > 0x7FFFFFFFu ? 0x7FFFFFFFu : (uint32_t)s;
}
void aica_master(int m) { mvol = m & 15; }
int32_t aica_master_gain(void) { if (!tables) build_tables(); return mvol ? gain_tab[(15 - mvol) * 32] : 0; }
void aica_mono(int on) { mono = on; }
/* a bank is downloaded over sound RAM: voices reading from it stop (else they would play the new bytes) */
void aica_stop_range(uint32_t a, uint32_t n) {
  for (int i = 0; i < NVOICE; i++) {
    voice *v = &vo[i];
    if (!v->tag) continue;
    uint32_t end = v->sa + (v->fmt == 0 ? 2 * (v->lea + 2) : v->fmt == 1 ? v->lea + 2 : v->lea / 2 + 2);
    if (v->sa < a + n && end > a) v->tag = 0;
  }
}
int aica_voices(void) { int n = 0; for (int i = 0; i < NVOICE; i++) n += vo[i].tag != 0; return n; }

/* ---- S8: the AICA DSP, approximated (AUDIO.md 3.1). The banks' DSP program (SFPB, one program) is not
 * interpreted; voices' IMXL sends feed a stereo reverb of our own (Schroeder/Moorer: 4 damped combs + 2
 * allpasses per side, integer Q15), output at the SFOB's effect levels (EFSDL 15 hard left / hard right = full)
 * and the master volume. ---- */
#define FX_MAX 64
static int32_t fx_in[FX_MAX];
static const int comb_len[2][4] = {{1116, 1188, 1277, 1356}, {1139, 1211, 1300, 1379}};
static const int ap_len[2][2] = {{556, 441}, {579, 464}};
static int32_t comb_buf[2][4][1400], comb_lp[2][4], ap_buf[2][2][600];
static int comb_pos[2][4], ap_pos[2][2];
#define FX_FB 27525    /* comb feedback 0.84 */
#define FX_DAMP 6554   /* damping 0.2 */
#define FX_WET 18022   /* output gain 0.55: a steady sound sent at 0 dB comes back about 2.4 dB below its dry level (test_aica) */
int fx_enabled = 1;
static void fx_run(int32_t *mix, int n) {
  int32_t mg = mvol ? gain_tab[(15 - mvol) * 32] : 0;
  for (int s = 0; s < n; s++) {
    int32_t in = fx_in[s] >> 2; /* four combs in parallel */
    for (int c = 0; c < 2; c++) {
      int32_t acc = 0;
      for (int k = 0; k < 4; k++) {
        int32_t *b = comb_buf[c][k];
        int p = comb_pos[c][k];
        int32_t o = b[p];
        comb_lp[c][k] = (int32_t)(((int64_t)o * (32768 - FX_DAMP) + (int64_t)comb_lp[c][k] * FX_DAMP) >> 15);
        b[p] = in + (int32_t)(((int64_t)comb_lp[c][k] * FX_FB) >> 15);
        if (++p >= comb_len[c][k]) p = 0;
        comb_pos[c][k] = p;
        acc += o;
      }
      for (int k = 0; k < 2; k++) {
        int32_t *b = ap_buf[c][k];
        int p = ap_pos[c][k];
        int32_t bo = b[p], o = bo - acc;
        b[p] = acc + (bo >> 1);
        if (++p >= ap_len[c][k]) p = 0;
        ap_pos[c][k] = p;
        acc = o;
      }
      mix[2 * s + c] += (int32_t)(((int64_t)acc * FX_WET >> 15) * mg >> 15);
    }
    fx_in[s] = 0;
  }
}

static void voice_mix(voice *v, int32_t *mix, int n) {
  int mv = mvol ? (15 - mvol) * 32 : 0x400;
  for (int s = 0; s < n; s++) {
    /* envelope */
    switch (v->eg_state) {
      case 0: v->eg -= v->inc[0]; if (v->eg <= 0) { v->eg = 0; v->eg_state = 1; } break;
      case 1: v->eg += v->inc[1]; if (v->eg >= v->dl) v->eg_state = 2; break;
      case 2: v->eg += v->inc[2]; break;
      default: v->eg += v->inc[3]; break;
    }
    if (v->eg >= (0x3FF << 16)) {
      v->eg = 0x3FF << 16;
      if (v->eg_state == 3) { v->tag = 0; return; }
    }
    /* sample, linear interpolation */
    int32_t a, b;
    if (v->fmt == 2) { a = v->d0; b = v->d1; }
    else { a = pcm_at(v, v->ipos); b = pcm_at(v, v->ipos + 1); }
    int32_t x = a + (int32_t)(((int64_t)(b - a) * (int32_t)(v->frac >> 4)) >> 16);
    int e = v->eg >> 16;
    int al = v->st_l + e + mv, ar = v->st_r + e + mv;
    int32_t gl = gain_tab[al > 1024 ? 1024 : al], gr = gain_tab[ar > 1024 ? 1024 : ar];
    mix[2 * s] += (int32_t)(((int64_t)x * gl >> 15) * v->pv_l >> 15);
    mix[2 * s + 1] += (int32_t)(((int64_t)x * gr >> 15) * v->pv_r >> 15);
    if (v->st_fx < 0x400) { /* the DSP send (mono, after the port level), master volume applied at the reverb's output */
      int af = v->st_fx + e;
      fx_in[s] += (int32_t)(((int64_t)x * gain_tab[af > 1024 ? 1024 : af] >> 15) * ((v->pv_l + v->pv_r) >> 1) >> 15);
    }
    /* advance */
    v->frac += v->step;
    uint32_t adv = v->frac >> 20;
    v->frac &= 0xFFFFF;
    while (adv--) {
      v->ipos++;
      if (v->ipos > v->lea) {
        if (!v->loop) { v->tag = 0; return; }
        v->ipos = v->lsa;
        if (v->fmt == 2) {
          if (v->snap_ok) { v->d0 = v->sd0; v->d1 = v->sd1; v->ds = v->sds; v->dstep = v->sdstep; v->dk = v->lsa + 1; }
          else adpcm_start(v);
        }
        continue;
      }
      if (v->fmt == 2) { v->d0 = v->d1; v->d1 = adpcm_next(v); adpcm_snap(v); }
    }
  }
}
void aica_mix(int32_t *mix, int n) {
  while (n > 0) {
    int k = n > FX_MAX ? FX_MAX : n;
    for (int i = 0; i < NVOICE; i++) if (vo[i].tag) voice_mix(&vo[i], mix, k);
    if (fx_enabled) fx_run(mix, k); else memset(fx_in, 0, sizeof fx_in);
    mix += 2 * k; n -= k;
  }
}

/* ---- output: the retrowser pull model (spec 4.6) ---- */
#ifdef __wasm__
#define EXPORT(n) __attribute__((export_name(n)))
#else
#define EXPORT(n)
#endif
#define OUT_CAP 16384 /* stereo frames kept while nobody drains (~0.37 s); the oldest are dropped */
static int16_t out[2 * OUT_CAP];
static uint32_t pending, digest = 2166136261u, made;
static int peak;
int audio_solo; /* test mode --audio-solo: 1 = only the CDDA heard, 2 = only the voices (both still run) */
void audio_frame(void) {
  static int32_t mix[2 * AUDIO_PER_FRAME], mute[2 * AUDIO_PER_FRAME];
  memset(mix, 0, sizeof mix);
  cdda_mix(audio_solo == 2 ? mute : mix, AUDIO_PER_FRAME);
  /* the MIDI sequencer steps between blocks of 49 samples (1.1 ms; 15 per frame) */
  for (int b = 0; b < AUDIO_PER_FRAME; b += 49) {
    seq_advance(49);
    aica_mix((audio_solo == 1 ? mute : mix) + 2 * b, 49);
  }
  if (pending + AUDIO_PER_FRAME > OUT_CAP) {
    uint32_t drop = pending + AUDIO_PER_FRAME - OUT_CAP;
    memmove(out, out + 2 * drop, (size_t)(pending - drop) * 4);
    pending -= drop;
  }
  int16_t *o = out + 2 * pending;
  peak = 0;
  for (int i = 0; i < AUDIO_PER_FRAME; i++) {
    int32_t l = mix[2 * i] >> 1, r = mix[2 * i + 1] >> 1; /* 6 dB of headroom: the AICA mixes on a wider bus */
    if (mono) l = r = (l + r) / 2;
    l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
    r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
    o[2 * i] = (int16_t)l; o[2 * i + 1] = (int16_t)r;
    int al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
    if (al > peak) peak = al;
    if (ar > peak) peak = ar;
    uint32_t w = (uint16_t)l | (uint32_t)(uint16_t)r << 16;
    for (int k = 0; k < 4; k++) { digest ^= (w >> (8 * k)) & 0xFF; digest *= 16777619u; }
  }
  pending += AUDIO_PER_FRAME;
  made += AUDIO_PER_FRAME;
}
uint32_t audio_digest(void) { return digest; }
uint32_t audio_frames_made(void) { return made; }
int audio_peak(void) { return peak; }

EXPORT("rtw_audio_rate") int rtw_audio_rate(void) { return audio_enabled ? AUDIO_RATE : 0; }
/* the frames made since the last call, at rtw_audio_buffer() (they stay there until the next game frame) */
EXPORT("rtw_audio_drain") int rtw_audio_drain(void) { int n = (int)pending; pending = 0; return n; }
EXPORT("rtw_audio_buffer") int rtw_audio_buffer(void) { return (int)(intptr_t)out; }
int16_t *audio_out(void) { return out; } /* the same buffer as a host pointer (native test mode) */
