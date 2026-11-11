/* Pen Pen port, S8: Manatee MIDI sequences (SMSB) on MIDI programs (SMPB) through our AICA-like mixer
 * (docs/port/AUDIO.md 2.4). Our own code; the formats were read from the disc's banks in the sandbox
 * (structure only) and checked by walking all 51 sequences of the five MLT banks.
 *
 * SMSB unit: 'SMSB', version 1, size, count, then count offsets (from the unit start) to SMSD sequences:
 *   'SMSD', u32 0x88, u32 tempo (the first event repeats it), events, 0x83, padding, 'ENDD'.
 * Events (one track; the channel is the low nibble; times in ticks; 16-bit values big-endian):
 *   0x00-0x7F note: [s] key velocity gate delta; s & 0x20: gate is 2 bytes, s & 0x10: delta is 2 bytes
 *   0x81 hi lo n    reference: play n events from offset hi:lo (from the SMSD start), then come back
 *   0x83            end of the sequence
 *   0x84 hi lo d    tempo (the quarter note in ms: our reading, see AUDIO.md), delta
 *   0xBn c v d      control change (c & 0x80: the delta is 2 bytes); used: 7 volume, 10 pan, 11 expression,
 *                   32 bank select (the SMPB bank), 1 modulation and 80 (ramps in the long songs): ignored
 *   0xCn p d        program change
 *   0xDn v d        channel pressure (ignored)
 *   0xEn v d        pitch bend, one byte, 64 = centre, the layer's bend range
 * SMPB unit: 'SMPB', version 1, size, 0, program table offset, program count, velocity curve offset (128
 *   bytes: velocity -> level 0..127) and count, two more {offset, count} pairs (not used), then the programs:
 *   6 layer offsets (0 = none); layer {split count, splits offset, 0, bend range (low byte, semitones)};
 *   split (0x30 bytes) = 18 halfwords of AICA slot registers 0x00-0x44 (sample addresses from the unit start,
 *   as in the one-shot bank), then key low, key high, base key, fine tune, 0xFFFF, flags, 0x7F, 0.
 * A note keys on one voice per layer whose split covers the key: pitch (key - base key) semitones from the
 * split's own OCT/FNS (the disc's samples are 22 050 Hz stored at OCT 0 with base = key + 12), level =
 * velocity curve x volume^2 x expression^2 x the port volume, pan from CC 10 (else the split's DIPAN).
 * Timing: 480 ticks per quarter note (assumed: the files do not say; it makes the jingles' 120-tick steps
 * 16th notes), advanced in blocks of 49 output samples (1.1 ms) with integer arithmetic. */
#include "audio.h"
#include "seq.h"

#define TPQ 480u
#define MAXNOTE 48
#define NPORT 8
static struct { uint32_t addr, size; int present; } smsb[4], smpb[4];

typedef struct {
  int on, bank;
  uint32_t seq;        /* the SMSD's sound-RAM address */
  uint32_t pc, ret;    /* event pointer; return address of a reference (0 = none) */
  int refleft;         /* events left in the reference */
  uint32_t wait;       /* ticks to the next event */
  uint32_t tempo;      /* ms per quarter note */
  uint64_t acc;        /* tick accumulator: units of 1 / (tempo * 44100) ticks... see seq_advance */
  int vol, pan;        /* port volume / pan (sdMidiSetVol/SetPan, -128..127) */
  uint8_t prog[16], cbank[16], cvol[16], cexp[16], cpan[16], cbend[16];
  int has_pan[16];
} track_t;
static track_t tr[NPORT];
typedef struct { int used, port, ch, key, bend_range; uint32_t voice, left; int32_t base_cents, gain; } note_t;
static note_t notes[MAXNOTE];
static uint32_t n_notes, n_events, n_unknown;

static uint8_t ar8(uint32_t a) { return aica_ram[a & (ARAM_SIZE - 1)]; }
static uint32_t ar32(uint32_t a) { return ar8(a) | (uint32_t)ar8(a + 1) << 8 | (uint32_t)ar8(a + 2) << 16 | (uint32_t)ar8(a + 3) << 24; }
static uint16_t ar16(uint32_t a) { return (uint16_t)(ar8(a) | ar8(a + 1) << 8); }

void seq_bank(uint32_t tag, int bank, uint32_t addr, uint32_t size) {
  if (bank < 0 || bank > 3) return;
  if (tag == 0x42534D53u) { smsb[bank].addr = addr; smsb[bank].size = size; smsb[bank].present = 1; } /* SMSB */
  if (tag == 0x42504D53u) { smpb[bank].addr = addr; smpb[bank].size = size; smpb[bank].present = 1; } /* SMPB */
  /* a download over a playing sequence's data ends it */
  for (int p = 0; p < NPORT; p++)
    if (tr[p].on && tr[p].seq >= addr && tr[p].seq < addr + size) seq_stop(p);
}

/* the note's level: velocity curve x volume^2 x expression^2 x port volume, as Q15 */
static int32_t note_gain(const track_t *t, int ch, int vel, int pbank) {
  int32_t g = 32767;
  if (smpb[pbank].present) {
    uint32_t u = smpb[pbank].addr, vc = ar32(u + 0x18);
    if (ar32(u + 0x1C) && vc + 128 <= smpb[pbank].size) g = (int32_t)ar8(u + vc + (uint32_t)(vel & 127)) * 32767 / 127;
  }
  int v = t->cvol[ch], e = t->cexp[ch];
  g = (int32_t)((int64_t)g * (v * v) / (127 * 127));
  g = (int32_t)((int64_t)g * (e * e) / (127 * 127));
  int pv = t->vol < -128 ? 0 : t->vol > 127 ? 255 : t->vol + 128; /* the port volume as the shot ports: linear 0..255 */
  return (int32_t)((int64_t)g * pv / 255);
}
static void note_apply(const note_t *n) {
  const track_t *t = &tr[n->port];
  int pan = t->has_pan[n->ch] ? (t->cpan[n->ch] - 64) * 2 : 0;
  pan += t->pan;
  int32_t cents = n->base_cents + ((int)t->cbend[n->ch] - 64) * n->bend_range * 100 / 64;
  /* aica_set_port takes the shot-port volume (-128..127 = linear 0..255 / 255): map the Q15 gain onto it */
  int vol = (int)((int64_t)n->gain * 255 / 32767) - 128;
  aica_set_port(n->voice, vol, pan, cents);
}
static void note_on(int p, int ch, int key, int vel, uint32_t gate) {
  track_t *t = &tr[p];
  int pb = t->cbank[ch] & 3;
  if (!smpb[pb].present || !vel) return;
  uint32_t u = smpb[pb].addr, tab = ar32(u + 0x10), np = ar32(u + 0x14), pr = t->prog[ch];
  if (pr >= np || tab + 4 * pr + 4 > smpb[pb].size) return;
  uint32_t po = ar32(u + tab + 4 * pr);
  for (int l = 0; l < 6; l++) {
    uint32_t lo = ar32(u + po + 4u * (uint32_t)l);
    if (!lo || lo + 16 > smpb[pb].size) continue;
    uint32_t ns = ar32(u + lo), so = ar32(u + lo + 4), br = ar8(u + lo + 12);
    for (uint32_t k = 0; k < ns && so + 0x30 * (k + 1) <= smpb[pb].size; k++) {
      uint32_t s = u + so + 0x30 * k;
      int klo = ar8(s + 0x24), khi = ar8(s + 0x25), base = ar8(s + 0x26), fine = (int8_t)ar8(s + 0x27);
      if (key < klo || key > khi) continue;
      uint16_t regs[24];
      for (int r = 0; r < 24; r++) regs[r] = ar16(s + 2u * (uint32_t)r);
      if (!(((uint32_t)(regs[0] & 0x7F) << 16 | regs[1]) || regs[3])) continue; /* an empty split (no sample) */
      int slot = -1;
      for (int i = 0; i < MAXNOTE; i++) if (!notes[i].used) { slot = i; break; }
      if (slot < 0) return;
      uint32_t h = aica_key_on(regs, u, 16);
      if (!h) return;
      note_t *n = &notes[slot];
      n->used = 1; n->port = p; n->ch = ch; n->key = key; n->bend_range = (int)(br ? br : 2);
      n->voice = h; n->left = gate ? gate : 1;
      n->base_cents = (key - base) * 100 + fine;
      n->gain = note_gain(t, ch, vel, pb);
      note_apply(n);
      n_notes++;
      break; /* one split per layer */
    }
  }
}
static void channel_update(int p, int ch) {
  for (int i = 0; i < MAXNOTE; i++)
    if (notes[i].used && notes[i].port == p && notes[i].ch == ch) note_apply(&notes[i]);
}
static void notes_off(int p) {
  for (int i = 0; i < MAXNOTE; i++)
    if (notes[i].used && notes[i].port == p) { aica_key_off(notes[i].voice); notes[i].used = 0; }
}

void seq_play(int p, int bank, int no) {
  if (p < 0 || p >= NPORT) return;
  seq_stop(p);
  if (bank < 0 || bank > 3 || !smsb[bank].present) { rt_log("hle sd midi: bank %d has no sequences", bank); return; }
  uint32_t u = smsb[bank].addr, cnt = ar32(u + 12);
  if ((uint32_t)no >= cnt) { rt_log("hle sd midi: sequence %d of %u", no, cnt); return; }
  uint32_t o = ar32(u + 16 + 4u * (uint32_t)no);
  if (o + 16 > smsb[bank].size || ar32(u + o) != 0x44534D53u) { rt_log("hle sd midi: sequence %d is not an SMSD", no); return; }
  track_t *t = &tr[p];
  int vol = t->vol, pan = t->pan;
  memset(t, 0, sizeof *t);
  t->vol = vol; t->pan = pan;
  t->on = 1; t->bank = bank; t->seq = u + o; t->pc = u + o + 12; t->tempo = ar32(u + o + 8);
  if (!t->tempo) t->tempo = 500;
  for (int ch = 0; ch < 16; ch++) { t->cvol[ch] = 100; t->cexp[ch] = 127; t->cpan[ch] = 64; t->cbend[ch] = 64; }
}
void seq_stop(int p) {
  if (p < 0 || p >= NPORT) return;
  notes_off(p);
  tr[p].on = 0;
}
void seq_port(int p, int which, int v) {
  if (p < 0 || p >= NPORT) return;
  if (which == 0) tr[p].vol = v; else tr[p].pan = v;
  for (int ch = 0; ch < 16; ch++) channel_update(p, ch);
}
int seq_playing(int p) { return p >= 0 && p < NPORT && tr[p].on == 1; }

static uint32_t be16(uint32_t a) { return (uint32_t)ar8(a) << 8 | ar8(a + 1); }
/* run one track's events due now; returns 0 when it ended */
static int run_events(int p) {
  track_t *t = &tr[p];
  for (int guard = 0; guard < 4096; guard++) {
    if (t->wait) return 1;
    if (t->ret && t->refleft <= 0) { t->pc = t->ret; t->ret = 0; }
    uint32_t a = t->pc, s = ar8(a), d = 0;
    int counted = 1;
    n_events++;
    if (s < 0x80) {
      int ch = (int)(s & 15), key = ar8(a + 1), vel = ar8(a + 2);
      uint32_t q = a + 3, gate;
      if (s & 0x20) { gate = be16(q); q += 2; } else gate = ar8(q++);
      if (s & 0x10) { d = be16(q); q += 2; } else d = ar8(q++);
      note_on(p, ch, key, vel, gate);
      t->pc = q;
    } else if (s == 0x81) {
      if (t->ret) { n_unknown++; return 0; } /* references do not nest */
      t->ret = a + 4; t->refleft = ar8(a + 3); t->pc = t->seq + be16(a + 1);
      counted = 0;
    } else if (s == 0x83) {
      return 0;
    } else if (s == 0x84) {
      t->tempo = be16(a + 1); if (!t->tempo) t->tempo = 500;
      d = ar8(a + 3); t->pc = a + 4;
    } else if ((s & 0xF0) == 0xB0) {
      int ch = (int)(s & 15), cc = ar8(a + 1), v = ar8(a + 2);
      if (cc & 0x80) { d = be16(a + 3); t->pc = a + 5; } else { d = ar8(a + 3); t->pc = a + 4; }
      cc &= 0x7F;
      if (cc == 7) t->cvol[ch] = (uint8_t)v;
      else if (cc == 11) t->cexp[ch] = (uint8_t)v;
      else if (cc == 10) { t->cpan[ch] = (uint8_t)v; t->has_pan[ch] = 1; }
      else if (cc == 32) t->cbank[ch] = (uint8_t)v;
      if (cc == 7 || cc == 11 || cc == 10) channel_update(p, ch);
    } else if ((s & 0xF0) == 0xC0) {
      t->prog[s & 15] = ar8(a + 1); d = ar8(a + 2); t->pc = a + 3;
    } else if ((s & 0xF0) == 0xD0) {
      d = ar8(a + 2); t->pc = a + 3;
    } else if ((s & 0xF0) == 0xE0) {
      t->cbend[s & 15] = ar8(a + 1); d = ar8(a + 2); t->pc = a + 3;
      channel_update(p, (int)(s & 15));
    } else { n_unknown++; return 0; } /* unknown event (or the filler after a song without an end) */
    if (counted && t->ret) t->refleft--;
    t->wait = d;
  }
  return 1;
}
/* advance every playing sequence by n output samples: ticks = samples * TPQ * 1000 / (tempo * 44100) */
void seq_advance(int n) {
  for (int p = 0; p < NPORT; p++) {
    track_t *t = &tr[p];
    if (!t->on) continue;
    if (t->on == 1 && !run_events(p)) t->on = 2; /* ended: the notes still run to their gate */
    t->acc += (uint64_t)n * TPQ * 1000u;
    uint64_t unit = (uint64_t)t->tempo * AUDIO_RATE;
    while (t->acc >= unit) {
      t->acc -= unit;
      /* one tick: gates, then events */
      for (int i = 0; i < MAXNOTE; i++)
        if (notes[i].used && notes[i].port == p && notes[i].left && !--notes[i].left) { aica_key_off(notes[i].voice); notes[i].used = 0; }
      if (t->wait) t->wait--;
      if (t->on == 1 && !run_events(p)) t->on = 2;
      unit = (uint64_t)t->tempo * AUDIO_RATE;
    }
    if (t->on == 2) {
      int any = 0;
      for (int i = 0; i < MAXNOTE; i++) any |= notes[i].used && notes[i].port == p;
      if (!any) t->on = 0;
    }
  }
  /* notes whose voice ended by itself free their slot */
  for (int i = 0; i < MAXNOTE; i++) if (notes[i].used && !aica_alive(notes[i].voice)) notes[i].used = 0;
}
void seq_log(void) { rt_log("audio midi notes %u events %u unknown %u", n_notes, n_events, n_unknown); }
