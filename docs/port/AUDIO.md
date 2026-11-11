# Pen Pen TriIcelon: audio (S7)

The game's sound has two sources: **CDDA music** (44 audio tracks on the disc, played by the GD-ROM drive) and
**Manatee** (the SDK's `sd` library, which drives the AICA's ARM7 sound driver `9900MANATEE.DRV` with host
commands; the game uses it for one-shot effects and one MIDI sequence). The port does not run the ARM7 driver: the
`sd` API is HLE on a mixer of our own, the CDDA sectors are read from the player's disc, and the mix goes to the
host through retrowser's pull-model exports. Everything here is our own code (MIT), written from public
documentation of the AICA (slot registers, Yamaha ADPCM) and from what the SDK's own wrappers do (read in the
sandbox; only behaviour, names and addresses are written down). No emulator code was used.

| File | What |
|---|---|
| `port/audio/sd.c` | the `sd` HLE: memory blocks, `sdMultiUnitDownload` (MLT banks into our sound RAM), ports, one-shots, the global and CDDA commands |
| `port/audio/aica.c` | the mixer (64 voices, 2 MiB sound RAM) and the output buffer + `rtw_audio_rate/drain/buffer` |
| `port/audio/cdda.c` | CDDA play/pause/stop/release over `rtw_disc_read` on the audio tracks |
| `port/audio/test_aica.c` | unit checks of the mixer on our own signals (`tools/s7.sh test`) |
| `port/audio/wav_report.py` | numbers from a captured WAV (in the sandbox) |
| `tools/s7.sh` | `test`, `capture`, `wasm`, `report` |

## 1. CDDA

`gdFsDaPlay(first, last, repeat)` (0x8C09EC7C) is the drive's CD-play command by track: tracks `first..last`
in order, then the range again `repeat` times; **15 = for ever** (title, menus and race music use 15; the
pre-race jingle, track 11, uses 0). `gdFsDaPause` keeps the position, `gdFsDaRelease` resumes it, `gdFsDaStop`
ends the play. `gdFsGetDrvStat` returns PLAY (3) while the drive plays and PAUSE (1) otherwise, so the drive
status now follows the real play (S4–S6 kept "playing" until a pause or stop; the end of track 11 changes nothing
in the game's flow, see 5).

The sectors are read through `rtw_disc_read(track, lba, n, buf)` in runs of 16 (2352 bytes = 588 stereo frames of
16-bit little-endian PCM at 44.1 kHz, the output rate: no resampling). Level: the game sets the CDDA volume and
pan with two Manatee commands (2.3); the AICA master volume applies too.

The game's CDDA logic (0x8C015D10, every frame): when the requested track is the one playing, it sends the CDDA
volume **127 if the drive status is PLAY, else −127**; a fade-in sends `f − 127`. The SDK's wrapper refuses
negative volumes (error 0x03000001), so "−127" never reaches the driver: the volume stays at 127 from the first
play. The HLE does the same.

## 2. Manatee (`sd`) HLE

### 2.1 Banks

`sdMultiUnitDownload(block)`: the game loads `3000.MLT` at the title and one of `58x2.MLT` per course into a
fixed buffer and downloads it. **SMLT** layout: `'SMLT'`, version 0x101, unit count (6), padding to 0x20, then one
32-byte record per unit `{fourcc, bank, sound-RAM address, sound-RAM size, file offset, file size, 0, 0}`. Units:
`SMSB` (MIDI sequences), `SMPB` (MIDI programs), `SFPB`/`SFOB`/`SFPW` (DSP program, output, work: SFPW has no file
data), `SOSB` (one-shots). Each unit is copied to its sound-RAM address (our 2 MiB array), as the driver does; voices
still playing from those addresses are cut first.

**SOSB** (one-shot bank): `'SOSB'`, version 1, size, count, then `count` offsets (from the unit start) to 0x38-byte
**SOSP** records: `'SOSP'`, **24 halfwords = the AICA slot registers 0x00–0x44 packed** (+ base note/fine tune,
loop start/end copies), `'ENDP'`. Sample addresses are relative to the unit. Seen on the disc: 25 sounds in
`3000.MLT`, 62 in `5802.MLT`; all Yamaha ADPCM, mostly 22 050 Hz (OCT −1), attack and release 31, no decay, TL 0,
DISDL 15 centred; four sounds loop and are sent **only to the DSP** (DISDL 0, ISEL 1, IMXL 14).

ADPCM nibble order checked on the disc's banks: low nibble first gives mean ≈ 0 and almost no clipped samples per
sound; high nibble first drifts by thousands and clips hundreds of samples.

### 2.2 Ports and one-shots

The library has 8 MIDI ports and 8 one-shot ports. The game opens all 16 at start-up: **the S2 names of the
open/close functions were swapped** (0x8C09AB08 = `sdShotOpenPort(&handle)`, 0x8C09A878 = `sdMidiOpenPort(&handle)`:
they allocate a port and write a handle = a pointer to a port record `{kind, port number, …, in use}`). The HLE
writes the same kind of record in its scratch area (0x8C001C00).

Every frame the game's sound server (0x8C015D10) plays the requested sounds on its 8 shot ports and sends volume,
pitch and pan changes:

| Call | Wrapper | HLE |
|---|---|---|
| `sdShotPlay(h, bank, no, prio)` | cmd 0x11; prio 0..31 else error | key on SOSP `no` of the bank on a free voice; a new play on a port releases the port's previous sound |
| `sdShotStop(h)` | cmd 0x12 | release (envelope RR) |
| `sdShotSetVol(h, v, time)` | cmd 0x15, sends v + 0x80 | port level, linear in 0..255 (v −128..127); the game plays menu sounds at 127 and the race's loops at −1 down to −121 |
| `sdShotSetPan(h, p, time)` | cmd 0x16, p + 0x80 | −127 left … 127 right |
| `sdShotSetPitch(h, c, time)` | cmd 0x18, −6399..6399 + 0x8000 | cents on top of the SOSP's OCT/FNS (the race's engine-like loop: ≈ −740) |

Port state (volume, pan, pitch) persists and applies to the next sound on the port. `time` is always 0 in the game.

### 2.3 Global commands

| Function | Wrapper | HLE |
|---|---|---|
| `sd_SetMasterVol(v)` | cmd 0x81, v << 4 | AICA master volume 0..15 (the game sets 0 around a download, then 15) |
| `sd_SetStereoMono('STRO'/'MONO')` | | mono = (L + R) / 2 |
| `sd_Cmd53(l, r)` | cmd 0x35, refuses negatives | **CDDA volume** per channel, linear 0..127 |
| `sd_Cmd54(l, r)` | cmd 0x36, + 0x80 | **CDDA pan** per channel (the game: −127, 127) |
| `sd_CmdBytes` (0x88), `sd_CmdU8U8` (0x84 then 0x82), `sd_CmdReset` (0x80) | | logged only |
| `sdLibInit`, `sdDrvInit`, `sdSysServer`, `sdSysFinish` | | init flag; the SDK's error codes when not initialised |

### 2.4 MIDI: not played yet

`sdMidiPlay(h, bank, seq, prio)` is called once on the S4 route (sequence 37 of bank 0 at the course selection,
frame 1501). SMSB holds 51 sequences (`SMSD` records: a compact MIDI-like event stream) and SMPB the programs
(layers/splits with AICA slot images and velocity tables). Not decoded yet: logged once, silent.

## 3. The mixer

64 voices from slot images (`aica_key_on`): 16-bit PCM, 8-bit PCM, **Yamaha 4-bit ADPCM** (step adapted by
230/230/230/230/307/409/512/614 / 256 per magnitude, clamped 127..24576; sample ± (2m+1)·step/8, clamped to 16 bits;
the decoder state is saved when the loop start is passed and restored at each loop), loops (LPCTL, LSA..LEA),
pitch 44 100 · 2^OCT · (1 + FNS/1024) with linear interpolation, envelope (attack / decay 1 to DL / decay 2 /
release; times from the AICA's documented curve: 118 s at rate 2, halving every 4 rates; KRS scaling), TL
(0.375 dB per step), DISDL (3 dB per step, 0 = off), DIPAN (3 dB per step on one side, bit 4 = to the left), the
master volume (3 dB per step, 0 = off), mono. Port volume, pan and pitch on top.

Not modelled: the **DSP** (reverb etc.: a voice with no direct level is heard dry at its IMXL send level), the
filter (FLV/FEG), the LFOs, sample-accurate timing of commands within a frame (all apply at the frame).

Output: the voices and the CDDA are summed in 32 bits, **halved (6 dB of headroom, the AICA mixes on a wider bus)**
and clamped to 16 bits. 735 stereo frames per game frame (44 100 / 60: the host steps the game at 60 Hz), made at
the end of each frame (`njWaitVSync`), so the sample count is a function of the frame count only. Integer
arithmetic throughout; the gain/time/pitch tables are built with + × ÷ only, so native and wasm are bit-identical.

## 4. retrowser

The module exports `rtw_audio_rate()` = 44100, `rtw_audio_drain()` (the stereo frames made since the last call)
and `rtw_audio_buffer()` (int16 L,R at a fixed buffer; up to 16 384 frames are kept when nobody drains, the oldest
dropped). The host's worker drains after each batch of frames and plays through its AudioWorklet with its own rate
control; it starts on the first user gesture. The manifest has **no audio fields** in retrowser's schema (spec 1.x):
the host detects sound from the three exports (all three, or none). The headless e2e runs Chromium with
`--mute-audio`, as retrowser's own harness does; the audio clock still runs, so the host's queue is checked there.

## 5. Tests and results (S7)

All guest code ran in `--network none --read-only --cap-drop ALL` containers; the WAV captures stay in
`penpen-build:/build/audio` and were only measured there (`tools/s7.sh report`); nothing decoded left the volume.

- **Unit checks** (`tools/s7.sh test`, our own signals): **17 checks, 0 failures**: ADPCM (our encoder → the mixer)
  SNR > 20 dB, a one-shot ends after LEA, OCT −1 halves the pitch, +1200 cents doubles it, a PCM16 loop keeps playing
  at its level, port volume 0 = half level, pan hard right silences the left, release ends a voice, PCM8 with TL 16
  = −6 dB, DIPAN 0x1F, 735 frames per game frame, the 6 dB headroom, drain semantics, rate 44 100.
- **Captures over the S4 route** (`tools/s7.sh capture`: 3 000 frames, pad
  `600:START,700:A,1100:START,1200:A,1500:A,1700:A,1760:A`; 2 205 000 stereo frames = 50.0 s, 735 per frame):

| Range (game frames) | Source | RMS L / R | Peak | DC L / R |
|---|---|---|---|---|
| 420–1770 | title track 5 from 429, menu track 6 from 706 (after the pause at 703), selection track 9 from 1563, paused at 1762 | −20.2 / −20.3 dBFS | −1.3 dBFS | −2.8 / −3.0 |
| 1860–2190 | pre-race jingle, track 11 from 1854 (repeat 0; its tail is silent) | −16.6 / −17.0 dBFS | −6.1 dBFS | −1.6 / −0.9 |
| 2235–3000 | race music track 12 from 2244 + race effects | −17.3 / −18.0 dBFS | −0.7 dBFS | 6.1 / 3.3 |

  The music alone (`--audio-solo cdda`) has the same three ranges, peaks −6.1 dBFS (the disc's 0 dBFS after the
  headroom). Effects alone (`--audio-solo sfx`): 600–825 (the Start and A presses at 600/700: port 2, sound 7,
  −19.4 dBFS RMS), 1200–1290, 1500–1590, 1695–1770 (A presses: port 1, sound 6, −23.4 dBFS; the sound started at
  1761 is cut by the course bank's download at 1764), 2235–2355 (the race start: port 7 sound 0 at 2244, the
  count-down sounds 17/18 on port 3, −17.8 dBFS, peak −1.5 dBFS), 2370–2460 (the race's looping sounds 25/29 at
  port volume −1 falling to −121 while the player stands still). Level just after each press vs just before (route
  capture): +5 to +12 dB.
  **Clipping: 0 samples at full scale** (route, music, effects). **DC offset** of the whole route: L 0.09, R −0.62.
- **Native = wasm**: the per-50-frame audio digests (`audio frame` lines) and the frame lines are identical over
  3 000 frames, and the two WAVs have the same SHA-256 (`ea9d6796…`).
- **Game RAM vs S6**: with `--audio 0` the RAM digests equal S6's (every 50th frame of 3 000). With sound, the
  digests differ from frame 50 on; a RAM dump at frames 1 000 and 3 000 differs from the `--audio 0` run in
  **95 bytes only**: the HLE's port and memory-block records (0x8C001C00–0x8C001C8C) and the game's 16 port
  handles (0x8C1B3B44–0x8C1B3B83), which the S6 stubs left at 0. The game's flow is unchanged: the same screens,
  files and CDDA calls at the same frames, and the drive status that now ends with track 11 changes nothing.
- **sh4** 257 checks, 0 failures; the differential harness as in S6 (8 068 functions: 7 687 pass / 0 fail / 381
  untested).
- **retrowser**: `rtw.mjs check --disc`: OK; the containerised e2e (headless Chromium, `--mute-audio`): the host's
  AudioWorklet runs at 44 100 Hz, queue 109 ms at the mode menu and 88 ms in the race, 3.5 M frames played,
  0 dropped, 1 and 4 underruns (SwiftShader runs the race at 58.7 frames/s, below the host's 60); VMU, rumble and
  the reload as in S6.

## 6. Gaps

- MIDI sequences (SMSB/SMPB): one request on the route (2.4), silent.
- The AICA DSP (SFPB programs: reverb/effects): the four DSP-only loops are heard dry at their send level.
- The filter envelope and LFOs; the `time` argument of the port commands (always 0 in the game so far).
- The port volume scale (linear 0..255) and the 6 dB headroom are our choices; the real driver's curve is not known.
- `sd_CmdBytes`, `sd_CmdU8U8`, `sd_CmdReset`: logged only (meaning unknown).
