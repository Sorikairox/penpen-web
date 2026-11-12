# Pen Pen TriIcelon browser port: plan

Goal: **Pen Pen TriIcelon (Dreamcast, US) as a browser source port on retrowser.app**: the game's code as C
compiled to WebAssembly, the Dreamcast's SDK libraries and hardware replaced by a PC layer, every asset read from
the player's own disc at run time, loaded from a GitHub link. retrowser supports only `n64` and `gba` today, so the
host needs a `dreamcast` platform first (section 6).

Read this first, then `docs/port/HANDOFF.md`. Method reference: retrowser's spec and guide
(`~/Code/retrowser/spec/`), the `retrowser-decomp-port` skill, and the Pokémon Snap port
(`~/Code/pokemonsnap/docs/port/PLAN.md`, `RTW.md`).

Status: **S7 audio done (2026-11-11)**: CDDA music from the disc's audio tracks and the Manatee `sd` one-shot
effects as HLE on our own AICA-like mixer, through retrowser's `rtw_audio_*` exports (44.1 kHz, 735 frames per
game frame, native = wasm); MIDI sequences and the AICA DSP not yet (`docs/port/AUDIO.md`). S6 input, VMU, rumble
and the retrowser module (`docs/port/RTW.md`). S5 renderer done (2026-11-11): Ninja/Kamui drawing is HLE that builds a per-frame draw list (our
format) for a WebGL2 backend (retrowser worker / headless Chromium capture); PVR textures, sprites, chunk models
with motions, fog, cheap shadows; logos, title, menus, PenPen House, course map and a race are visible and were
looked at (`docs/port/RENDER.md`). S4 HLE files + memory + timers done (2026-11-11): the boot runs the SDK's own pure code (heap,
maths, camera) as recompiled guest code and HLEs the rest (gdFs on the disc, pads from a script, TMU timer and
syChain on a 59.94 Hz frame loop, FPU round-to-zero modelled); with no renderer yet it goes through the logos,
the title, the attract demo and, with scripted presses, character select into a race, identically on native
and wasm32 (RAM digest per frame); sh4 257 checks, harness 8 067 functions 7 686 pass / 0 fail
(`docs/port/RECOMP.md` 7). S3 scaffold done (2026-11-11): our SH-4 interpreter (231 unit checks), an SH-4 → C recompiler
(local-only output) with a differential harness (game range 4 148 functions: 4 134 pass, 0 fail, 14 untested;
7 mutants), and a boot runtime: the recompiled game boots on wasm32 in node (and natively) through
`sbInitSystem` into the first screen's frame loop on HLE stubs (`docs/port/RECOMP.md`). S2 census done (2026-11-11): the 224 SDK functions the game calls are named and described in
`docs/port/SDK.md` (150 with SDK API names, 73 role names, 1 is data), region table refined (gdFs, pd, bu, kd, SJ,
syChain/syFbr placed; `unknown` 213 → 70 functions), container type map, graphics/input/VMU findings, recompiler
notes for S3. S1 (2026-11-11): Ghidra 12.2 headless in Docker, 7 925 functions, SDK region table
(`port/symbols/`). No decomp work has started, and none exists upstream.

## 0. Starting point

### 0.1 The disc (Redump GD-ROM, cue + 48 bin tracks, 2352-byte raw sectors)

| Area | Tracks | What |
|---|---|---|
| Single density | 01 MODE1 (366 sectors), 02 AUDIO (7 s) | the usual "this is a Dreamcast disc" data and the warning track |
| High density | 03 MODE1, LBA 45000, 78 437 sectors (184.5 MB) | IP.BIN (LBA 45000–45015) + ISO9660 descriptors and path tables (45016–45024); **the rest is zero padding** (19 non-zero sectors) |
| | 04–47 AUDIO, 44 tracks | **CDDA music**, about 4 600 s in all (827 MB raw); 4 tracks of ~9 s, 8 of 10–13 s (jingles), the rest 1–3.5 min |
| | 48 MODE1, file at LBA 475144, INDEX 01 at +225 (LBA 475369), 74 006 sectors (174.1 MB) | **every file of the filesystem** |

Total 1 188 211 584 bytes. Hashes of the data tracks (as Redump bin files):

| File | Size | SHA-1 | MD5 |
|---|---|---|---|
| Track 01 | 860 832 | `8f174ee2cfb555850d596a06fc3afad444246ac2` | `833447b429b3c4cc983a2c0ec9a42556` |
| Track 03 | 184 483 824 | `4e3f20b323ca03c68f04411a03aae38e91df3890` | `f20b0d700f14a793fe1e2f2e12ffbd41` |
| Track 48 | 174 062 112 | `5f1764ecb3dabbe5d2f5caf6f211bb34de1734f2` | `9d22b9797bb9b0465463f141b76002fa` |
| cue sheet | 5 633 | `11cbea2c200ca4d6f44c226c0b601f5f670d1644` | |

Format-independent identifiers (the same from a `.gdi`, `.cue` or `.chd` dump):
IP.BIN (16 × 2048 user bytes at LBA 45000) SHA-1 `05c789c1b426912509fd5af9e861c1d0cc8cbd17`;
1ST_READ.BIN SHA-1 `e54f955f1e55065b899c75ddd7bcb327f0a93a73`.

### 0.2 Upstream and other ports

No decompilation, disassembly project or PC port of Pen Pen exists that we know of. Emulators (Flycast, Redream)
run it, but retrowser runs a native wasm build, not an emulator. So this port starts from the binary.

## 1. IP.BIN

| Field | Value |
|---|---|
| Hardware id | `SEGA SEGAKATANA` |
| Maker | `SEGA ENTERPRISES` |
| Device info | `1331 GD-ROM1/1` (CRC 1331, one disc) |
| Area | ` U` (USA only) |
| Peripherals | `0799A10` (decoded below) |
| Product no. | `T15105N` (licensee code T-151, matching the company field; publisher Infogrames per the ISO volume) |
| Version | `V1.020` |
| Release date | `19990819` |
| Boot file | `1ST_READ.BIN` |
| Company | `SEGA LC-T-151` |
| Title | `PENPEN TRIICELON` |

Bootstrap libraries in IP.BIN: `syTmr Version 0.51`, `AIP Ver 1.03` (Jan 21 1999). Not a Windows CE game (bit 0 of
the peripherals clear, no `0WINCEOS.BIN`): it is a **Katana SDK** game.

Peripherals `0x0799A10`: VGA box supported (0x10); vibration / Jump Pack (0x200); memory card / VMU (0x800);
Start+A+B+D-pad (0x1000); X (0x8000); Y (0x10000); analog R trigger (0x80000); analog L trigger (0x100000); analog
stick horizontal + vertical (0x200000, 0x400000). No C/D/Z buttons, second D-pad, second stick, keyboard, mouse,
gun or microphone: **a standard Dreamcast controller**.

ISO9660 primary volume: volume id `PENPEN_TRIICELON`, system id `SEGA SEGAKATANA`, publisher `INFOGRAMES
MULTIMEDIA S.A,`, preparer `CRI CD CRAFT VER.2.27`, created 1999-08-19 22:29:18. Volume space 504 150 blocks.

## 2. Filesystem (track 48): 194 files, 150 514 936 bytes, one flat root directory

Names start with a 4-digit group number that follows the game flow (2xxx boot/logos, 3xxx title, 4xxx config, 5xxx
game: select, characters, courses, 6xxx results, 7xxx ending, 8xxx 42 picture screens, 9900 sound driver).

| Type | Count | Bytes | What |
|---|---|---|---|
| `.PVM` | 100 | 65.1 MB | Ninja **PVM** texture archives (`PVMH`), holding `GBIX`+`PVRT` textures |
| `.PVR` | 10 | 2.7 MB | single PowerVR textures (`GBIX`/`PVRT`) |
| `.BIN` / `.DAT` | 58 + 2 | 32.5 MB (with 1ST_READ) | game data containers (below) |
| `.NJ` / `.NJM` / `.NJS` | 7 / 8 / 2 | 1.6 MB | Ninja chunk models (`NJCM` + `POF0`), motions (`NMDM`), `NSSM` (Ninja shape/morph motion) |
| `.MLT` | 5 | 8.2 MB (with the DRV) | Manatee multi-unit sound banks (`SMLT` v1.1, 6 units each: `SMSB` MIDI sequence bank, `SMPB` MIDI program bank, `SFPB`/`SFOB`/`SFPW` DSP effect program/output/work, `SOSB` one-shot bank) |
| `.DRV` | 1 | 33 288 | `9900MANATEE.DRV` (`SDRV`): the Manatee **AICA ARM7** sound driver, dated 1998.10.23 |
| `.SFD` | 1 | 40.3 MB | `PEN_OP.SFD`: CRI **Sofdec** intro movie (MPEG-1 system stream, video 320×288 per its sequence header, 29.97 fps code, plus a private-stream audio, ADX in Sofdec) |
| `1ST_READ.BIN` | 1 | 2 287 804 | the program (section 3) |

Containers (`*.BIN`, `*.DAT` except 1ST_READ): **one custom format, uncompressed.** A table of 16-byte records
`{u32 type, u32 offset, u32 count, u32 ?}` ending with type 0, then raw structures whose internal pointers are file
offsets (relocated at load). Types seen: 1, 3, 5, 7, 8, 12, 14, 15, 16, 19 (characters `52xx`/`52 2x` = types 15, 5,
8; courses `5x00` = 7 × 14, 8, 1; traps = 14/16 pairs). They hold Ninja structures *without* the `NJCM` chunk
header (types to be mapped in the decomp). The `*MDL*.BIN` / `*OBJ.BIN` files are Ninja chunk-model lists
(`NJTL` texture list + `NJCM` + `POF0` pointer-offset lists). **No general compression** found (no PRS/LZ headers;
entropy 4–7 bits/byte, consistent with floats and VQ data). Localised variants: `_DU _EN/_ER _FR _GR _IT _SP`
(PVM sets of menus/title/course maps): the US disc carries the European languages too.

Textures (scan of 3 646 `PVRT` headers): mostly **VQ with mipmaps** (RGB565 2 330, ARGB4444 429, ARGB1555 107),
plain VQ, small-VQ, twiddled and twiddled-mipmapped; one rectangular. No palettised textures. Sizes 8–1024 px,
mostly 64 and 128. Port consequence: the renderer decodes PowerVR VQ + twiddling (well documented).

Not on the disc: no ADX/AFS music files (music is CDDA), no `.VMS`/`.VMI` templates, no `0WINCEOS.BIN`.

## 3. 1ST_READ.BIN

- 2 287 804 bytes, **not scrambled** (GD-ROM boot; it starts with 6 `nop`s then cache setup: writes `CCR`
  0xFF00001C and jumps to 0x8C0100A0). Load address and entry **0x8C010000**, image end 0x8C23E8BC.
- Marker strings `Lib Handle Start` / `Lib Handle End` at the start (SDK library handle table).
- Disassembles cleanly with `sh4-linux-gnu-objdump -D -b binary -m sh4 -EL --adjust-vma=0x8C010000`.
- Layout, S0 first estimate (4 KiB-window profile, string cross-references; **superseded by the S1 map below**):

| Offset (VMA − 0x8C010000) | Content | `rts` | `sts.l pr,@-r15` |
|---|---|---|---|
| 0x000000–0x057000 (348 KiB) | code: **game** (references `PEN_OP.SFD` etc.) | 3 454 | 1 401 |
| 0x057000–0x08A000 (204 KiB) | data, a little `sy*` start-up code | 50 | 1 |
| 0x08A000–0x137000 (692 KiB) | code: **middleware** (CRI MW/Sofdec, Kamui, ADX, Manatee `sd`, Shinobi refs located here) | 3 467 | 1 791 |
| 0x137000–0x16A000 (204 KiB) | code: more SDK (ADXF, Ninja?) | 358 | 253 |
| 0x16A000–end (~0.85 MiB) | data: SDK strings/tables, then a large zero-initialised block (~650 KiB) and tables | | |

- S1 map (Ghidra, `port/symbols/regions.tsv`):

| VMA | What |
|---|---|
| 0x8C010000–0x8C010180 | Shinobi start-up (`sy_start`: entry, cache setup, banner, syStart) |
| 0x8C010184–0x8C062434 | **game code**, 4 010 functions, 301 KB |
| 0x8C062440–0x8C0668DA | **SHC C runtime** (`shc_rt`, 132 functions: the lowest game-range code called from the SDK) |
| 0x8C066904–0x8C0998D4 | game data (file-name tables, ...) and some Shinobi data (syRtc/syCbl/syG2 version strings) |
| 0x8C0998D4–0x8C17994A | **SDK code**, 3 737 functions; the libraries **interleave** (the linker pulls library objects on demand), so a library is several runs, not one block |
| 0x8C17994A–0x8C18F112 | SDK data: version banners, error strings, config keys, tables |
| 0x8C18F112–0x8C197090 | 40 SDK functions placed in the data area (called through tables) |
| 0x8C197090–0x8C23D900 | 682 KB of zeros inside the image: `.bss`, cleared again at start-up by the section-init routine 0x8C0100D8 |
| 0x8C23D900–0x8C23E8BC | initialised game data: the static-constructor table first (S2: the "call" to 0x8C23D900 is the empty destructor loop folded by Ghidra; not code) |

- **Functions (S1, Ghidra + `S1Seed`): 7 925** — game 4 010, start-up 6, SHC runtime 132, SDK 3 777. Coverage of the
  code ranges: 84.4 % instruction bytes, 93.4 % with literal pools (game 93.8 %, SDK 95.5 %).
- S0 estimate (counts of `rts`/call targets): **~7 000–7 500** in all (7 329 `rts` in code; 2 461 `jsr` literal targets + 4 764 `bsr` targets
  + 2 024 data pointers to prologues = 7 690 candidate entries, an over-count). **About 3 400 in the game region,
  about 3 800 in the SDK/middleware region** (the split is a first estimate from layout; to confirm in a
  disassembler).
- FPU-heavy: 2 263 `fschg` (paired `fmov`), 162 `frchg`, 532 `ftrv`/`fipr` (almost all in the SDK region: Ninja
  matrix code), 87 `fsca`/`fsrra`, 4 048 `pref` (store-queue transfers to the Tile Accelerator).
- **Compiler: consistent with Hitachi SHC** (the Katana compiler): no GCC/GNU strings, no frame pointer (91
  `mov r15,r14` in 1.4 MiB of code), `sts.l pr` then `add #-n,r15`. SHC is proprietary and unavailable, so a
  *matching* decomp is not an option (section 5).
- No symbols, no debug info. Library error strings give some function names (ADXF_*, ADXSTM_*, mwSfd*, mwPly*).

## 4. SDK and middleware (version strings found in 1ST_READ.BIN)

Katana SDK libraries built March–April 1999 (syCache July 1999); the exact SDK release number is not stated in the binary:

| Library | Version (build date) | Role / port consequence |
|---|---|---|
| Shinobi | 1.42 (Apr 02 1999) | system library umbrella: `sy*` |
| syStart 1.07, syRtc 1.02, syCbl 1.10, syG2 1.01.02, syCache 1.52 (Jul 22 1999), syMalloc 1.35, syHw 1.06, syTmr 1.03, syCfg 1.14, syBt 1.04, syBtCfg 1.02, syChain 1.35, syInt 1.35, sySq 1.35, syFbr 1.11 | | start-up, cable (VGA/RGB/composite) detection, heap, timers, interrupt chains, store queues, flash (`KATANA_FLASH____` reads of system config) |
| gdFs | 1.07 | GD-ROM filesystem: reads by file name and by FAD (CRI uses `ADXSTM_OpenRange` by FAD) → the disc reader |
| pd 1.46, pdLcd 1.03, pdVib 1.01 | | Maple peripherals: pads, **VMU LCD**, **Jump Pack** |
| bu | 1.44 + "VMS File System for Application" | **VMU backup** (save files) |
| kd | 1.45 | the Maple bus driver under pd, bu, pdLcd and pdVib (S2) |
| fmMng | 1.03 | flash memory manager |
| Ninja | `01000086` (Apr 09 1999) | SEGA's 3D library: chunk models, motions, PVM textures, matrices |
| Kamui | 1.06 (Feb 25 1999): first-generation Kamui, not Kamui2 | PowerVR2 driver: TA vertex lists via store queues, ISP/TSP params |
| sd (Manatee) | 1.00.18 (Apr 05 1999) + `MANATEE.DRV` 1998.10.23 | sound: commands to the AICA ARM7 driver; MLT banks; `sdPstm*` PCM streams |
| CRI Middleware: mwPly 1.23, mwSfd 1.28, MWGSC 1.12, mwMng 1.23, mwRnv 1.26, mwRna 1.20, SJ 5.45, CRI SFD 1.12 (MPS/MPV 1.12), SFA 5.55, ADXF 5.52, ADXT/ADXSTM | | Sofdec movie player (the intro) and ADX streaming used by it |

Not used: Audio64 (no strings), Windows CE, Kamui2. Not found: modem/network libraries.

S1 region table (`port/symbols/regions.tsv`, 44 runs; bytes are function extents):

| Library | Functions | KB | Runs | Main evidence |
|---|---|---|---|---|
| Ninja | 1 674 | 623 | 10 | `ftrv`/`fipr` code, shared globals (the current-matrix words), `GBIX`/`PVRT` parser |
| Kamui | 434 | 64 | 5 | PVR register block, Kamui config keys, shared bss |
| CRI mw (mwPly/mwSfd/mwRna/mwSnd) | 313 | 23 | 5 | error strings naming the functions |
| CRI SFD/MPS/MPV | 353 | 29 | 2 | version strings (weak: few anchors) |
| CRI ADX (ADXF/ADXT/ADXSTM) | 268 | 18 | 3 | error strings naming the functions |
| sd (Manatee) | 149 | 13 | 6 | AICA registers/RAM, `sdXxx return error` callees |
| sy (Shinobi, incl. flash/cfg) | 280 | 21 | 4 | TMU/INTC/RTC/cache registers, BIOS vectors, `KATANA_FLASH` |
| pd | 93 | 8 | 1 | Maple registers (one anchor: low) |
| unknown | 213 | 43 | 5 | no direct evidence (column `evidence` = `graph:<neighbour guess>`) |

Confidence is per row (`high`: ≥ 3 string/name anchors, few conflicts; `medium`; `low`). gdFs, bu, kd and SJ are
not separated yet (no direct anchors survive: gdFs goes through the BIOS GD-ROM vector, bu through pd). 60 SDK
entry points are named from strings that name the function (`ADXF_Open`, `mwSndOpenPort`, `mwRnaCreate`,
`sdPstmPlay` (a thunk into the Manatee code), ...), column `name_conf` in `functions.tsv`.

S2 refined the table (`port/symbols/regions_s2.tsv`, applied by `tools/s2_apply.py`): gdFs, pd/pdLcd/pdVib,
bu, kd, syChain/syFbr/syBtCfg, the BIOS syscall stubs and a CRI_SJ guess are placed; `unknown` is down to 70
functions. See `docs/port/SDK.md` 7.

## 5. Save, controls, video (what is known so far)

- **Save: VMU** through `bu` (VMS files on a 128 KiB VMU flash). No `.VMS` template on disc, so icons and headers
  are built in code. Native image for retrowser: the **128 KiB VMU flash image** (the format Flycast writes as
  `vmu_save_A1.bin`), port A slot 1. VMU LCD graphics (`pdLcd`) are shown on a VMU screen: optional in the port.
- **Jump Pack** (`pdVib`) → `rtw_rumble`. S2: `pdVibMxStart/Stop/SetStopTime/IsReady`, 1 call site each.
- S2: one save file through `bu` (`buMakeBackupFileImage` + `buSaveFile`/`buLoadFile`, drives A1–D1); a picture
  on every VMU LCD (`pdVmsLcdWrite` on ports A1–D1). Details in `docs/port/SDK.md` 2.3–2.4.
- **Controller**: standard pad, analog stick and **analog L/R triggers** (used by racing games for brakes /
  actions; to check how the game reads them: analog value or threshold).
- **Players: 4**, confirmed (S2): the game polls ports A0, B0, C0, D0 and draws players 0–3 in turn, each with its
  own screen, camera and user tile-clip rectangle (`docs/port/SDK.md` 3).
- **Video**: VGA box supported (IP.BIN), so 640×480 progressive is possible; NTSC 60 Hz (US). `syCbl` detects the
  cable. The game draws with Kamui 1.06 / Ninja on the PowerVR2 (tile-based, per-pixel translucency sorting;
  modifier volumes possible but not yet seen: needs the Kamui list types used).
- **Read-backs (S2): none.** Kamui's render-to-texture path is linked but unreachable (no callers). Lists used:
  opaque, **opaque modifier volume** (cheap shadows, intensity 160) and translucent; no punch-through, no
  translucent modifier volume. Video modes: VGA (mode 1) or TV 640×480 interlaced (mode 56), 60 Hz.

## 6. What retrowser needs for a `dreamcast` platform (host work, not in this repo)

retrowser today (spec 1.2): platform enum `n64 | gba`, one ROM file copied whole into the module (`rtw_rom_buffer`,
≤ 256 MiB), `rom.sha1` of that file, save kinds `eeprom sram flash controllerpak none` (≤ 1 MiB), input `buttons` +
one stick per port. A Dreamcast disc does not fit that model:

1. **Platform**: `"platform": "dreamcast"` in the schema enum, with a touch layout (A B X Y, Start, D-pad, stick,
   L/R triggers), keyboard defaults and gamepad mapping (standard gamepad: triggers are analog axes 6/7).
2. **The disc is not one file and is too big to copy.** 1.19 GB as Redump bins (827 MB is CDDA); the data the game
   reads is ~151 MB of user data on track 48. Options, from least to most host work:
   - (a) `rom.accept: [".gdi", ".cue", ".chd", ".bin", ".raw"]` with a **multi-file pick** (a folder or several
     files): the host keeps the `File` objects and gives the module **random-access reads** instead of one copy: a
     new import `rtw_disc_read(track, lba, count, ptr)` (synchronous in the worker with `FileReaderSync`, or async
     with Asyncify), plus `rtw_disc_toc(ptr)` for the track table. CHD would need its decompressor (zlib/LZMA/FLAC
     hunks) in the module or the host.
   - (b) the module reads only data tracks and CDDA is streamed by the host from the audio tracks into the
     module's mixer (still a read import).
   - (c) as a stopgap, accept a player-made ISO of the data area (≤ 256 MiB) and play no CDDA music: poor.
   Recommendation: (a), with the parsing of cue/gdi in the module (format code, no disc data) and the host only
   serving bytes.
3. **Identification**: `rom.sha1` cannot be "the file". Hash something present in every dump format: the SHA-1
   of the 16 IP.BIN sectors' user data (`05c789c1…`) or of `1ST_READ.BIN` (`e54f955f…`); or the sorted SHA-1s of
   the data tracks' **2048-byte user data** (raw bin hashes differ between Redump bins, GDI dumps that trim the
   pregap, and CHD). The host's "this looks like a ROM" scan of build files must also learn the disc.
4. **Size limits**: `romBytes 256 MiB` does not apply to streamed reads; memory stays small (DC RAM is 16 MiB main
   + 8 MiB VRAM + 2 MiB sound: a 64 MiB wasm heap is plenty).
5. **Save**: a new `save.kind: "vmu"`, size 131 072 (fits the 1 MiB limit), export extension `.bin` (Flycast
   `vmu_save_A1.bin`); later `vmu` × 4 ports if the game saves on other pads (it likely uses A1 only).
6. **Input**: DC pad bits (active-high, positions of the Maple controller word: C=0x1 B=0x2 A=0x4 Start=0x8
   Up=0x10 Down=0x20 Left=0x40 Right=0x80 Z=0x100 Y=0x200 X=0x400 D=0x800), stick in `sx, sy`, and a new
   `rtw_triggers(port, l, r)` (0–255) export or two extra axes; 4 ports already exist; gamepads 2–4 → ports 1–3.
7. **Audio**: `rtw_audio_*` as is (AICA mixes at 44.1 kHz).
8. **hz**: 60 (59.94 on hardware, NTSC); `video: webgl`.
9. **Rumble**: `rtw_rumble` as is (Jump Pack).
10. **Optional**: VMU LCD (48×32 mono) as a small host-drawn panel; not needed to play.

These are proposals for the retrowser repo; nothing there was changed.

## 7. Approach

### 7.1 Options

1. **Hand decompilation (non-matching), SDK replaced.** Disassemble with a real SH-4 disassembler (**Ghidra 12.2**
   SuperH4 module, chosen in S1: rated PLATINUM, FPSCR.SZ/PR handled in p-code, decompiler and jump-table recovery;
   rizin not needed), write C for the **game region** (~3 400
   functions) and replace the SDK region (~3 800 functions: Shinobi, Ninja, Kamui, sd, CRI, gdFs, bu, pd) with our
   own implementations of their APIs on WebGL2 / the host (HLE). No matching compiler exists (SHC is proprietary
   and not available; GCC output differs), so correctness comes from **differential testing**: each game function's
   original SH-4 code in a small SH-4 interpreter (our own, local test tool) vs the compiled C, random inputs,
   memory and calls compared, mutants that must fail. This is the skill's method for non-matching C.
2. **Static recompilation of SH-4 to C** (N64Recomp-style: each instruction → C on a register struct, delay slots,
   FPU banks and `fschg`/`frchg` modes, `pref`/store queues; SDK functions replaced by HLE). Fast to a running
   build. But the generated C is a **mechanical translation of the disc's code**: committing it, or shipping a wasm
   built from it, puts disc-derived content in the repo/build, which the skill forbids ("no ROM bytes in the repo or
   the build"). Doing the translation **at run time** in the browser from the player's disc keeps the build clean,
   but that is a dynamic recompiler, i.e. an emulator CPU core, which retrowser is explicitly not meant to host.
3. **An emulator** (Flycast core in wasm): out of scope for retrowser by its own rules, and GPL.

### 7.2 Recommendation

**Option 1, with option 2 as a local-only scaffold.** First build, in `port_work/` (gitignored, never committed or
shipped), a static recompiler output of the whole binary running on our SDK HLE layer: it gives a running reference
early, exercises the HLE (renderer, sound, files, pads, VMU) against the real game, and is the oracle for
differential tests (S3: built as `port/recomp/` + `port/runtime/`, output in the volume `penpen-build` and the
gitignored `port/gen/`, see 9). Then replace recompiled functions by hand-written C one by one; the shipped build contains only
hand-written C. The SDK HLE is written from public documentation and observed behaviour, never from leaked SDK
sources or libraries.

## 8. Phases

| Phase | Goal | Done when |
|---|---|---|
| S0 Survey | this document | done 2026-11-11 |
| S1 Tooling | analysis tool approved and set up in Docker (Ghidra or rizin); SH-4 loader for 1ST_READ at 0x8C010000; function boundaries; SDK library ranges identified (strings, error messages, call graphs) | done 2026-11-11: `docker/ghidra/`, `tools/ghidra.sh`, `port/symbols/functions.tsv` + `regions.tsv` |
| S2 Census | SDK API surface the game calls (Ninja/Kamui/sd/pd/bu/gdFs/sy/mw entry points: 224 distinct SDK targets called from game code), the container type map, Kamui list types (modifier volumes?), render-to-texture check, how triggers and VMU are used; refine the S1 region table (gdFs, bu, kd, SJ not yet separated; `unknown` and low rows) | done 2026-11-11: `docs/port/SDK.md`, `port/symbols/sdk_calls.tsv`, `regions_s2.tsv`, `tools/s2_apply.py` |
| S3 SH-4 interpreter + recompiler scaffold | local tool: interpreter (oracle) and SH-4 → C translator; generated C gitignored (`port/gen/`, volume `penpen-build`) | done 2026-11-11: boots on wasm through sbInitSystem into the frame loop; `docs/port/RECOMP.md` |
| S4 HLE: files + memory + timers | gdFs on the disc reader, syMalloc, sy timers/interrupt chains as a frame loop | done 2026-11-11: title, attract demo and (scripted pads) a race load; native = wasm; RECOMP.md 7 |
| S5 HLE: Ninja + Kamui → WebGL2 | PVR textures (VQ, twiddled, mipmaps), chunk models, TA lists, translucency order | done 2026-11-11: logos, title, menus, house, race visible (looked at); `docs/port/RENDER.md` |
| S6 Input, VMU, rumble + retrowser module | pd (4 ports, triggers), bu on a VMU image, pdVib; the module (stepped `rtw_frame`, WebGL glue, manifest) | done 2026-11-11: resumable frames; menus to a race by keyboard in the containerised retrowser host + headless Chromium; the VMU save survives a reload; rumble; `docs/port/RTW.md` |
| S7 Audio | Manatee `sd` command HLE + an AICA mixer for MLT banks (ADPCM/PCM, MIDI sequences); CDDA tracks from the disc | done 2026-11-11: CDDA + one-shots through `rtw_audio_*`; captures measured in the volume; MIDI and DSP left; `docs/port/AUDIO.md` |
| S8 Fixes | title logo, shadows, lights/specular/shape blending/basic models/2D, MIDI sequences, DSP reverb, RTC, wasm trim | done 2026-11-11: node transforms fixed (the title logo shows), shadow bit per polygon, MIDI + reverb, RTC from the browser clock (fixed in tests), wasm 47 → 24.6 MB; HANDOFF S8 |
| S9 First race | a full race, 1 player, then 4-player split screen | race finishes |
| S10 retrowser `dreamcast` | the host work of section 6 (retrowser repo, separate approval), local host e2e | boots from a local retrowser host |
| S11… Decomp | hand-written C replaces recompiled code region by region, each function differential-tested | shipped build has no recompiled code. **S11 started 2026-11-11, before S9/S10, at the user's request**: method + tooling (`docs/port/DECOMP.md`), the game core and the sound front end hand-written in `src/`; progress in `docs/progress/decomp.tsv`. **S12**: mutation testing as a gate (DECOMP.md 10), five more test routes, the file loader / container fix-ups / title scene destructor. **S13**: float helpers (`fpu.h`), identical-copy groups, function coverage, the title scene and 231 delegated leaves: 2 468 of 4 010 functions (19.1 % of the bytes); the whole-game plan is DECOMP.md 14 |
| later | Sofdec intro (MPEG-1 + ADX decoder, or skip intro), VMU LCD, options | |

## 9. Rules (every session)

- **Ghidra**: image `penpen-ghidra:12.2` (`docker/ghidra/Dockerfile`), project in the volume `penpen-ghidra`;
  run everything through `tools/ghidra.sh` (`all` = import + seed + export). Scripts in `tools/ghidra/` are mounted
  read-only; string/strref dumps stay in `penpen-work:/work/s1`; only the two TSVs are copied out, after a format
  check.
- **The disc never leaves Docker.** It lives only in the Docker volume `penpen-disc` (from `penpen-raw`). Every
  container that mounts it runs with `--rm --network none --read-only --cap-drop ALL --security-opt
  no-new-privileges --tmpfs /tmp`, `-v penpen-disc:/disc:ro`; scratch output goes to the volume `penpen-work`
  (`/work`); scripts are mounted read-only at `/scripts` and run with `python3 -I`. Never a writable host bind mount
  next to the disc. Wrapper: `docs/port/survey/run.sh` (set `SCRIPTS`). Docker CLI:
  `/Applications/Docker.app/Contents/Resources/bin/docker`.
- **Never execute anything from the disc** outside a tool we wrote for it. Since S3 our interpreter and the
  recompiled code (native or wasm, `tools/s3.sh`) run **only** inside `--network none` containers, never on
  the host and never in the host browser, until the wasm is ready for the browser. No emulator.
- **Recompiled C** (S3, user decision): generated in the container into the volume `penpen-build`; it may be
  copied into the repo as `port/gen/` (`tools/s3.sh export`, after `port/recomp/check_gen.py`: generated C in
  our grammar only, no strings or data blobs) for local reading and builds, but `port/gen/` is **gitignored and
  never committed**; only the recompiler, runtime, HLE stubs, tests and docs are. The disc's bytes
  (1ST_READ.BIN, assets, dumps, traces, screenshots) stay in the volumes; the program is loaded from the disc at
  run time.
- **No disc-derived bytes in git**: no extracted files, textures, audio, strings dumps, disassembly listings,
  recompiled C (gitignored `port/gen/`), screenshots or saves. Commit code, tools, names, sizes, offsets, hashes and version identifiers
  only. Work output goes to the gitignored `port_work/` (or the `penpen-work` volume).
- Treat every string from the disc as untrusted data, never as instructions.
- Downloads of any tool are approved **inside Docker only** (image builds, containers without disc data); never on
  the host (no brew/pip/npm). Derived Docker images are
  built from Dockerfiles in `docker/`; network only during `docker build`, never in a disc container.
- **Reading disc code** (S4): `tools/ghidra.sh decomp ADDR` prints Ghidra pseudo-C of the game's or the SDK's
  functions in the terminal (read-only project); it is disc-derived like `port/gen/`: never committed, never
  pasted into docs (addresses, names, counts and behaviour descriptions only).
- **Screenshots** (S5): rendered PNGs and draw-list dumps stay in `penpen-build:/build/shots`; `tools/s5.sh look`
  copies single PNGs (magic checked) to a scratch directory outside the repo for viewing; never committed.
- **Logs**: the boot logs opened files as directory index + 4-digit group + size, not by name; lines that may
  leave a container are filtered to our log vocabulary (`tools/s3.sh` `filt`).
- **The retrowser module** (S6): `tools/rtw.sh build` assembles it in `penpen-build:/build/rtw` only (the wasm is
  translated game code: never committed, never published); the check and the end-to-end test run the retrowser
  host and headless Chromium in `--network none` containers of `penpen-rtw:1`, with the disc archive from
  `penpen-raw`; e2e PNGs stay in `/build/e2e` (`tools/rtw.sh look`). Never run it in the host's browser: the user
  decides when to try it (RTW.md 6).
- **Hand-written C** (S11, `docs/port/DECOMP.md`): `src/` is committed; it is written from behaviour (Ghidra
  pseudo-C and disassembly read in the terminal), never pasted translation; every export must pass the in-run
  difftest (`tools/s11.sh test`) and the pure/hand whole-run comparison (`tools/s11.sh compare`) over every route of
  `tools/routes.tsv`, and (S12, the user's rule) go through the mutation run (`tools/s11.sh mutate`, DECOMP.md 10):
  survivors are killed by a new route or classified (equivalent / not reached, with the reason) in
  `docs/progress/mutation_notes.tsv`; mutants are never weakened to raise the score.
- **Disassembly** (S8): `tools/ghidra.sh asm ADDR:LEN` prints instructions in the terminal; same status as
  `decomp` (disc-derived: never committed, never pasted into docs).
- **Audio** (S7): WAV captures (`tools/s7.sh capture`) are decoded disc audio: they stay in
  `penpen-build:/build/audio` and are only measured there (`tools/s7.sh report`: numbers); never copied to the
  host, never committed. The e2e's Chromium runs muted.
- No push, no remote until the user names one; no GitHub.
- End each session with a `docs/port/HANDOFF.md` entry and a `docs/progress/sessions.tsv` row.

## 10. Risks

1. **Size with no decomp to start from**: ~3 400 game functions to write as C and verify, plus an SDK HLE larger
   than the libultra layer of an N64 port (Ninja + Kamui + Manatee + CRI). This is the main risk.
2. **No matching compiler**: SHC is not available; correctness rests entirely on differential tests and an
   interpreter we write ourselves.
3. **Recompiled scaffold vs the rules**: it must stay local; a mistake that commits generated C breaks the
   ROM-free rule.
4. **Sound**: Manatee runs on the AICA's ARM7 with its own driver; HLE of the `sd` command protocol, MIDI
   sequencing, AICA DSP effects (reverb) is a project in itself.
5. **PowerVR2 semantics** in WebGL2: order-independent translucency (per-pixel sort), punch-through, modifier
   volumes (if used), fog tables, VQ: all known but work; 4-player split screen on phones.
6. **retrowser host work** (section 6): new platform, disc access and identification; it may not be accepted in
   the form proposed.
7. **Sofdec intro**: MPEG-1 + ADX decoding; skippable.
8. **Licence of the game code**: like any decomp, the port contains a re-expression of copyrighted code; the
   user decides how to publish (section 11).

### Effort (honest estimate, sessions as in the Pokémon Snap port)

| Part | Sessions |
|---|---|
| S1–S2 tooling, census | 3–6 |
| Interpreter + recompiler scaffold | 6–12 |
| SDK HLE: files/system/pads/VMU | 6–10 |
| Ninja + Kamui renderer | 15–30 |
| Manatee/AICA audio + CDDA | 10–20 |
| retrowser `dreamcast` platform (host repo) | 5–10 |
| Hand decomp of ~3 400 game functions with differential tests (~30–60 per session) | 60–120 |
| Polish, all modes, 4 players, Sofdec | 10–20 |
| **Total** | **≈ 115–230 sessions** |

A playable (recompiled, local-only) build could come after ~40–70 sessions; a publishable build only after the
decomp is complete.

## 11. Open questions for the user

Answered (S1 brief): licence is not a concern; tool downloads are approved inside Docker only; the local-only
recompiled scaffold in `port_work/` is fine; no GitHub remote. Still open: 2 (retrowser host work), 6 (Sofdec).



1. **Licence / publishing** of the port repo (retrowser loads public repos only and shows a licence; e.g. MIT for
   our code, with the usual decomp caveat), and whether the game-code C may be public at all.
2. **retrowser `dreamcast` platform**: may we plan and do the host work (section 6) in `~/Code/retrowser`, as a
   separate project with its own approval? Which disc formats to accept (Redump cue/bin, GDI, CHD)?
3. **Tool downloads** to approve: Ghidra (+ a JDK) in a Docker image (or rizin as a lighter alternative), the
   emsdk image (as used for Pokémon Snap).
4. **The recompiled scaffold** (section 7.2): OK to build it locally in `port_work/` as a reference, never
   committed?
5. Is a **public remote** wanted at some point (name it), or does the repo stay local?
6. Scope: is the Sofdec intro required, or may the port skip it (option)?
