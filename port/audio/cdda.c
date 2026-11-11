/* Pen Pen port, S7: CDDA. gdFsDaPlay(first track, last track, repeat) as the GD-ROM's CD play command: the
 * tracks first..last are played in order, then the range again `repeat` more times (15 = for ever); pause
 * keeps the position, release resumes it, stop ends it. The audio sectors come straight from the player's disc
 * through rtw_disc_read (2352 bytes = 588 stereo frames of 16-bit little-endian at 44.1 kHz: the output rate,
 * so no resampling), read in runs of 16 sectors. Level: the Manatee CDDA commands the game sends (sd.c):
 * volume 0..127 per channel (a linear level, as the one-shot ports) and pan per channel; the AICA master
 * volume applies (the CDDA enters the AICA's mixer). */
#include "audio.h"

#define RUN 16
static struct {
  int playing, paused, first, last, repeat, track;
  uint32_t lba, count; /* the current track */
  uint32_t pos;        /* stereo frame within the track */
} cd;
static int16_t buf[RUN * 588 * 2];
static uint32_t buf_lba, buf_n;
static int buf_track;
static int vol_l, vol_r, pan_l = -128, pan_r = 127;

static int open_track(int t) {
  int flags;
  if (disc_track(t, &cd.lba, &cd.count, &flags) || (flags & 1) || !cd.count) {
    rt_log("hle cdda track %d is not an audio track", t);
    return -1;
  }
  cd.track = t; cd.pos = 0;
  return 0;
}
void cdda_play(int first, int last, int repeat) {
  if (last < first) last = first;
  cd.first = first; cd.last = last; cd.repeat = repeat & 15;
  cd.playing = cd.paused = 0;
  if (open_track(first)) return;
  cd.playing = 1;
}
void cdda_pause(void) { if (cd.playing) { cd.playing = 0; cd.paused = 1; } }
void cdda_stop(void) { cd.playing = cd.paused = 0; }
void cdda_release(void) { if (cd.paused) { cd.paused = 0; cd.playing = 1; } }
int cdda_is_playing(void) { return cd.playing; }
int cdda_track(void) { return cd.playing ? cd.track : 0; }
void cdda_volume(int l, int r) { vol_l = l; vol_r = r; }
void cdda_pan(int l, int r) { pan_l = l; pan_r = r; }

/* the end of a track: the next one, the range again, or the end of the play */
static void next_track(void) {
  if (cd.track < cd.last && !open_track(cd.track + 1)) return;
  if (cd.repeat == 15 || cd.repeat-- > 0) { if (!open_track(cd.first)) return; }
  rt_log("hle cdda end of play (track %d) at frame %u", cd.track, hle_frame());
  cd.playing = 0;
}
static int fetch(void) { /* the sector holding cd.pos into buf; 0 ok */
  uint32_t lba = cd.lba + cd.pos / 588;
  if (buf_n && buf_track == cd.track && lba >= buf_lba && lba < buf_lba + buf_n) return 0;
  uint32_t n = cd.lba + cd.count - lba;
  if (n > RUN) n = RUN;
  int r = rtw_disc_read(cd.track, (int)lba, (int)n, buf);
  if (r <= 0) { buf_n = 0; rt_log("hle cdda read failed (track %d, %d)", cd.track, r); return -1; }
  buf_lba = lba; buf_n = (uint32_t)r; buf_track = cd.track;
  return 0;
}
static int32_t lin(int v) { v = v < 0 ? 0 : v > 127 ? 127 : v; return (int32_t)v * 32767 / 127; }
static void pan_gains(int p, int32_t *l, int32_t *r) {
  p = p < -127 ? -127 : p > 127 ? 127 : p;
  *l = p > 0 ? (127 - p) * 32767 / 127 : 32767;
  *r = p < 0 ? (127 + p) * 32767 / 127 : 32767;
}
void cdda_mix(int32_t *mix, int n) {
  if (!cd.playing) return;
  int32_t gl = lin(vol_l), gr = lin(vol_r), ll, lr, rl, rr;
  pan_gains(pan_l, &ll, &lr);
  pan_gains(pan_r, &rl, &rr);
  int32_t m = aica_master_gain();
  int32_t a = (int32_t)((int64_t)gl * ll >> 15) * m >> 15, b = (int32_t)((int64_t)gl * lr >> 15) * m >> 15;
  int32_t c = (int32_t)((int64_t)gr * rl >> 15) * m >> 15, d = (int32_t)((int64_t)gr * rr >> 15) * m >> 15;
  for (int s = 0; s < n && cd.playing; s++) {
    if (cd.pos >= cd.count * 588) { next_track(); if (!cd.playing) break; }
    if (fetch()) { cd.playing = 0; break; }
    uint32_t k = (cd.lba + cd.pos / 588 - buf_lba) * 588 + cd.pos % 588;
    int32_t L = buf[2 * k], R = buf[2 * k + 1];
    mix[2 * s] += (L * a >> 15) + (R * c >> 15);
    mix[2 * s + 1] += (L * b >> 15) + (R * d >> 15);
    cd.pos++;
  }
}
