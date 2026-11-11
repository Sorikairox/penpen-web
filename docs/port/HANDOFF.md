# HANDOFF

Newest session first. Read `docs/port/PLAN.md` (disc facts, survey, approach, phases, rules) before working.

## S12 — 2026-11-11/12: mutation testing with Mull, six more routes, the file loader / container fix-ups / title destructor

The user's instruction: "Don't forget mutation testing". All guest code ran in `--network none --read-only --cap-drop
ALL --security-opt no-new-privileges` containers; `git log --all -- port/gen` stays empty; Ghidra pseudo-C and
disassembly were read in the terminal only. Design and numbers: `docs/port/DECOMP.md` 5.1, 10, 11.

1. **Mutation tooling** (DECOMP.md 10): `tools/mutate.py` (mutant generation from `src/`: rel / arith / logic /
   const / negate / delete / return / args / argreg operators, stable ids; the report), `tools/mut_worker.py` (in the
   sandbox: pre-linked base, references from the pure build, the unmutated control through every gate, gcov coverage
   per route, then each mutant compiled, linked and run on the routes that execute its line, cheapest first, killed at
   the first difftest FAIL / frame line / audio line / early stop / crash / stall), `tools/s3.sh mutate`,
   `tools/s11.sh mutate | report`. Outputs `docs/progress/mutation.tsv` (per function), `mutation_survivors.tsv`,
   `mutation_sessions.tsv`; hand classification in `mutation_notes.tsv` (by id) and `mutation_reach.tsv`.
   **Stopped at the user's request** (switch to Mull) after 1 033 of 2 015 S12 mutants: too slow (a survivor runs all
   seven routes); its partial results are history, not a score (DECOMP.md 10).
2. **Routes** (`tools/routes.tsv`, used by `test`, `compare` and `mutate`): explore (stick, pause menu, PenPen House,
   DRESS UP, RESULTS, TIME TRIAL), vs2 (two controllers, split mode 2), vs4 (four, split mode 3), demo (Start in the
   attract demo), reset (soft reset from controller B in a race). The pad script takes a port (`F@P:...`, hle_io.c).
3. **Fixes found on the way**: `tools/s3.sh` passed run arguments unquoted into `sh -c` (a pad script with `|` became
   a pipe; S5-style screenshots of such scripts were wrong): quoted now; a failed build printed nothing: it says
   `build FAILED` now; `filt` is `grep -a --line-buffered`. **The difftest did not replay HLE writes into guest RAM**
   (file data read by gdFsRead): a loader's samples compared stale bytes. The HLE now reports them
   (`hwt_hle_wrote`) and both sandbox sides get them (DECOMP.md 5.1). A mutation runner that reads `src/` live gave
   wrong mutants while `src/` was being edited: it snapshots `src/` at its start now. Time-outs under load were false
   kills (3 of the S11 baseline's re-checked "timeouts" survive): a run is killed only after 120 s without output.
4. **Hand-written** (DECOMP.md 11): `src/core/file.{h,c}` (file_init, file_load, file_load_fixed, file_free + thunk,
   cd_pause), `src/core/fixup.c` (reloc, container_load and the fix-ups of model trees, models, mesh sets, motions,
   texture lists, chunk trees, motion / chunk-action lists), `src/game/title.c` (the title scene's constructor,
   destructor body, virtual destroy). **69 of 4 010 game functions (1.72 %), 7 438 of 301 130 bytes (2.47 %)** (+22).

Tests (before the Mull switch): difftest over 7 routes, 5 623 samples: **68 of 69 pass, 0 FAIL** (main: whole-run
only); pure vs hand **identical frame lines on all 7 routes, native and wasm; native = wasm**; sh4 **257 / 0**; mixer
**26 / 0**; harness **5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + check **OK**; screenshots from the hand
build (title f500, house f900, race f3000, 4-player split f2600 of vs4) look right; **browser e2e passed** on the hand
build (title, menu, house, select, course, race, audio, rumble, VMU save after reload; 24.6 game frames/s in the race
while 8 mutation jobs loaded the machine).

**Mull** (after the user stopped the first pipeline; this part of S12 ran with at most one background task and one
container of 6 workers): `docker/mull` (image `penpen-mull:1`), `tools/mull_run.py` + `tools/mull_gate.py`,
`tools/s3.sh mull`, `tools/s11.sh mutate | report`, `tools/mutate.py report-mull` (DECOMP.md 10).
- **One build of all mutants: 3 s**; setup (references + coverage) 23 s; the control (all 8 routes, unloaded) 42 s
  and gives the time-outs (3x + 30 s, stall 20 s); per mutant: killed 3.7 s mean (median 1 s), survivor 80 s, uncovered
  0 s; **whole run 26 min** (1 577 s) for 776 mutants, against hours for half of the first pipeline's 2 015. A pilot of
  14 mutants (`--sample 40`, 2 min) first; then a targeted run (`--only`, pad.c + game.c, 5.5 min) for new routes.
- **Score: 776 mutants, 672 killed, 33 equivalent, 71 unreached, 0 survived: 90.4 % (reach score 100 %)**; first Mull
  run 83.4 % (78 survived). Equivalent and unreached reasons per Mull id in `docs/progress/mutation_notes.tsv`.
- **New input** (hle_io.c pad script: `HL/HR/HU/HD` stick at the threshold, `NOAX/ONLYX/ONLYY`, `OUT`): route
  **inputs** (1 000 frames) and, in **route**, a pause then a soft reset from controller A at 5900. 8 routes now.
- **cd_play** (func_8C012D38) hand-written: **70 of 4 010 game functions (1.75 %), 7 474 of 301 130 bytes (2.48 %)**.

Tests (end of S12): difftest over 8 routes, 5 792 samples: **69 of 70 pass, 0 FAIL** (main: whole-run only); pure vs
hand **identical frame lines on all 8 routes, native and wasm; native = wasm**; sh4 **257 / 0**; mixer **26 / 0**; harness **5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + check **OK**. Not run after the Mull switch: the browser e2e and screenshots (the
runtime change is the test-only pad script tokens).

Next: see DECOMP.md 12.

## S11 — 2026-11-11: hand-written C: method, tooling, first regions (game core, sound front end)

Started before S9/S10 at the user's request ("hand written c with human readable code"); PLAN numbers this decomp
phase S11. Design and numbers: `docs/port/DECOMP.md`. All guest code ran in `--network none --read-only --cap-drop
ALL` containers; `git log --all -- port/gen` stays empty; Ghidra pseudo-C and disassembly were read in the terminal only.

1. **Override by link-time selection**: `HW_EXPORT(<addr>, ...)` in `src/` defines `f_<addr>`; `recomp.py --hand src/`
   leaves that function's body out of the translation (table.c flags it `hand`); `build.sh` compiles `src/` for such a
   translation. Two translations side by side: `/build/gen` (pure reference) and `/build/genhand` (`tools/s11.sh gen`).
2. **Hand-written C on the guest's terms** (`src/include/hw.h`, `port/runtime/hw.c`): structs with the original
   layout (offset/size checks), `gaddr` fields, `hw_call` into guest code (SH-4 convention, checked return), stack
   arguments, guest-stack locals for out-parameters. **Resumable** functions for the yielding chain (main, game_frame,
   task updates, game_update, mode starts): `HW_RESUMABLE/HW_YIELDING/HW_RESUME_AT` keep C locals in the runtime's
   unwind record; a non-resumable function that yields stops the run (never happened). `rt yield-chain` lists the
   chain after a run.
3. **In-run differential test** (`--hwtest`, `port/runtime/difftest.c`, `tools/s11.sh test`): sampled calls run live
   with their HLE results, HLE argument use, HLE stack out-parameters and hardware reads recorded; then the original
   (interpreter) and the hand-written code run from the same snapshot with those replayed; registers, HLE calls,
   hardware writes and all RAM but the dead stack compared; samples follow yielding calls across frames; functions
   reached only from hand-written callers count as tested inside passing samples. Found on the way: MACH/MACL are
   caller-saved in this compiler; HLE calls made by translated code carry garbage in unused argument registers (hence
   the HLE `R(i)` marks); HLE out-parameters on the stack (gdFsGetFileSize, syCfgGetSoundMode).
4. **Whole-run comparison** (`tools/s11.sh compare`): frame lines with a RAM digest every frame, `--digest-nostack 1`:
   the stack area is left out and stack pointers count as one token (an SDK variable holds a pointer to a translated
   caller's stack struct at frames 427-428: 0x8C00F31C pure vs 0x8C00F390 hand, same data).
5. **Region**: `src/core/{task,pad,view}`, `src/game/{main,game,sound}`: **47 game functions, 4 728 bytes (1.17 % /
   1.57 % of the game region)** — main loop and start-up, V-sync / end-of-render callbacks, GD-ROM drive watch, the
   task tree (Task, TaskVtbl, TaskTree: the C++ object system), Game (mode state machine, soft reset, demo time-out,
   the start routines of modes 0, 2, 3, 7, 8), pad reading, views/split modes, the sound front end (one-shot/MIDI
   request ports, CDDA music fades, start-up, banks, sound sets). Functions the runs never reach stay translated.

Tests: difftest boot 8 000 frames 991 samples, route 6 000 frames 1 046 samples: **every export passes in at least one
run, 0 failures** (main: never returns, covered by the whole-run comparison; the title's mode start had 1-3 samples with
an HLE replay mismatch that still passed); mutants: 12 deliberate bugs, the difftest flags 4, the whole-run digest 1
more, 7 lie on paths the runs never take (stick, pause/deactivation, demo Start, split screen). **Pure vs hand: route
6 000 and boot 8 000 frame lines identical** natively and in wasm; native = wasm (hand build); sh4 **257 / 0**; mixer
**26 / 0**; harness (pure translation) **5 943: 5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + `rtw.mjs check --disc` **OK** (the module in `/build/rtw` is now the hand build); screenshots from the hand build (title f500, PenPen House f900, race f3000 of the route) look right. Not run this session: the browser e2e.

Next: the scenes the route shows (title 0x8C01B77A, menus, course select), the file loader (func_8C01311C) and the
CDDA layer (func_8C012D38/5C/88); RZ/DN float helpers before any float code; routes that exercise more paths (pause,
stick, split screen, other modes). S9 (first race) and S10 (retrowser dreamcast) remain as planned.

## S8 — 2026-11-11: fixes: title logo, shadows, lights, basic models, MIDI, DSP reverb, RTC, wasm trim

All guest code ran in `--network none --read-only --cap-drop ALL` containers; PNGs, dumps, WAVs and RAM dumps
stayed in `penpen-build` (single PNGs looked at from the session scratchpad); `git log --all -- port/gen` stays
empty. New debugging aids: `--trace F0[-F1]` (every SDK call, node and strip of those frames), `tools/ghidra.sh
asm ADDR:LEN` (terminal only).

1. **Title logo** (RENDER.md 5): the real cause was not depth or sorting: **this SDK's njTranslate /
   njRotateXYZ / njRotateZXY / njScale return at once when the matrix argument is NULL** (S5 passed NULL for
   "current"), so every node transform of every model tree was dropped. The ice tube (nj_MotionL3b, node
   translate z −544, scale z .67/.27) sat at the logo's depth; the characters' limbs were all at their root.
   Now the current matrix pointer is passed: frames 500/900/1500 show the full logo in front of the tube;
   PenPen House and the races show articulated characters.
2. **Shadows**: the SDK's polygon context builder (0x8C0E4340) sets the PCW shadow bit from njControl3D
   0x400 (or 0x1000 for polygons without alpha); the game draws the course with 0xC04, the characters with
   0x004. The backend keeps the bit in stencil bit 7 and darkens only pixels with bit 7 + odd volume parity:
   the race frame's 4 227 shadowed pixels are all on the ground, none on the player's body. Also njControl3D
   0x800 = njSetConstantAttr masks on chunk strip headers (the course: no env map, flat).
3. **Renderer**: per-family lights (L1/L3/L4 one light each; L2 four lights + ambient, from the S8 trace's
   values), specular into the offset colour (not for L1), shape-motion keys blended linearly, **basic models**
   (the game's "nj_ObjectB" takes an NJS_MODEL: the characters' slide trails, seen on the snow in the attract
   demo at frame 3000), njDrawPolygon / njDrawTriangle2D / njDrawLine2D (attr bits read in the SDK; none of
   them, nor njPrint, is called in 6 000 frames of the boot or the route).
4. **MIDI** (AUDIO.md 2.4): SMSB/SMPB formats read from the banks (structure only; grammar checked on all 51
   sequences): Saturn-like events (notes with 1/2-byte gate and delta, references, tempo, CC, program, bend),
   programs = layers of key splits holding AICA slot images + base key, a velocity curve. `seq.c` plays them
   through the mixer (15 steps per frame, integer). Sequences 5–44 are one note each of program 11 = 40
   sampled clips; the route plays **sequence 37 at frame 1501** (2.1 s clip): effects-only capture
   1500–1635 **RMS −16.7 dBFS, peak −5.1 dBFS** (the press alone was −23.4), full mix −20.3 → −12.8 dBFS
   around 1501, 0 clipped samples. Assumed (not in the files): 480 ticks per quarter, tempo = ms per quarter.
5. **DSP**: the SFPB program is not interpreted; IMXL sends feed a stereo reverb of ours (4 damped combs + 2
   allpasses per side) at the SFOB's full hard-left/right outputs. DSP-only sounds (the race's loops) are no
   longer heard dry: 2370–2460 at −27.7 dBFS (S7 −27.8); route levels unchanged within 0.1 dB.
6. **RTC**: the game reads the AICA clock (0xA0710000/04) through syRtc (a V-blank handler reads it every
   frame into syRtc's cache; the only game use is the VMU save's time stamp). The module answers start value
   + frame time; **the glue sets the start from the browser's local clock** (`penpen_set_rtc` before
   `rtw_start`), test hosts use a **fixed 2000-01-01 00:00:00** (`--rtc N`). Saves: 2000-01-01 00:00:11 in
   tests, the real date in the e2e. retrowser has no time import: RTW.md 3.1 has a spec proposal
   (`rtw_time(ptr)`), retrowser unchanged.
7. **wasm trim** (RECOMP.md 9): `recomp.py --trim` (default) keeps the game range, entries, guest SDK
   functions, data-table callbacks and their closure, never entering HLE targets: **5 943 of 8 068
   functions, wasm 47.1 → 24.6 MB**; identical frame lines and RAM digests over 6 000 route frames and 8 000
   no-input frames, 0 interpreter calls (the fallback stays).

Tests: sh4 **257 / 0**; mixer **26 / 0** (9 new: MIDI timing/pitch/reference/end, DSP delay/level/decay);
harness on the trimmed translation **5 943: 5 736 pass / 0 fail / 207 untested**, on the full one **8 068:
7 687 / 0 / 381** (as S7); native = wasm (RAM and audio digests, 3 000 route frames); `rtw.mjs check --disc`
**OK**; e2e: playing after 70 s, title with the logo, menu, house, selection, course, race start and racing seen; the host's AudioWorklet runs (queue 113 / 92 ms, 0 dropped, 1 / 3 underruns), **58.8 game frames/s** in the race (SwiftShader, as S6/S7), rumble, the VMU save (stamped with the container's real date) loaded after the reload. **Route RAM vs S7** (native, 3 000 frames): every frame-line field equal (HLE/guest
counts, files, heap, pads); the RAM digests differ, and a RAM diff at frame 3 000 has **166 bytes in 4 runs**:
syRtc's clock cache (0x8C1BAC50, 4 bytes) and the Ninja matrix stack (0x8CE3F8C0–0x8CE3FBBA: the node
transforms now really written there).

Next: S9 first race (a full race, 1 player, then split screen); the per-pixel translucency sort; palettised
textures; flat shading; the MIDI tick resolution (find it in the ARM7 driver if a song is ever played);
`nj_ModelB` and the B-family motions if a route reaches them. The user restarts `tools/launch-local.sh` to get
this build.

## S7 — 2026-11-11: audio: CDDA music and Manatee one-shot effects through the retrowser audio exports

Design and numbers: `docs/port/AUDIO.md`. All guest code ran in `--network none --read-only --cap-drop ALL`
containers; the WAV captures stay in `penpen-build:/build/audio` (measured there only); `git log --all -- port/gen`
stays empty.

- **CDDA**: `gdFsDaPlay(first, last, repeat)` by track (15 = for ever), pause/release/stop, audio sectors read
  with `rtw_disc_read` in runs of 16, 44.1 kHz = the output rate. The drive status follows the real play.
- **Manatee HLE** (`port/audio/sd.c`): MLT banks (SMLT unit records → our 2 MiB sound RAM), the SOSB one-shot bank
  (SOSP = 24 halfwords of AICA slot registers), 8 + 8 ports with guest handles, shot play/stop/vol/pan/pitch,
  master volume, stereo/mono; **`sd_Cmd53` = CDDA volume, `sd_Cmd54` = CDDA pan** (from the game's own CDDA
  logic). **S2 had the port open/close names swapped** (fixed in the TSVs and `hle_names.h`).
- **Mixer** (`port/audio/aica.c`): 64 voices, PCM16/PCM8/Yamaha ADPCM (low nibble first: checked on the disc's
  banks), loops, OCT/FNS pitch + cents, envelope, TL/DISDL/DIPAN, master; integer mixing, 6 dB headroom; 735
  frames per game frame; `rtw_audio_rate` 44100 / `drain` / `buffer`. No manifest field exists for audio in
  retrowser's schema: the exports are the declaration.
- **Captures** (3 000-frame S4 route): music 420–1770 (tracks 5, 6, 9), 1860–2190 (11), 2235–3000 (12), RMS −17 to
  −20 dBFS; effects on every press (600, 700, 1200, 1500, 1700) and at the race start (2244–2355), +5 to +12 dB
  over the music; **0 clipped samples**, DC offset 0.09 / −0.62. **Native = wasm** (audio digests and WAV SHA-256).
- **RAM**: `--audio 0` = S6 digests; with sound only 95 bytes differ (HLE records + the game's 16 port handles,
  which the stubs left at 0); same screens/files/CDDA calls at the same frames.
- Tests: mixer unit checks **17 / 0**; sh4 **257 / 0**; harness **8 068: 7 687 pass / 0 fail / 381 untested**;
  `rtw.mjs check --disc` **OK**; e2e: the host's AudioWorklet plays (44 100 Hz, queue 88–109 ms, 3.5 M frames,
  0 dropped, 1–4 underruns at SwiftShader's 58.7 frames/s), VMU reload and rumble as in S6.
- Gaps: MIDI sequences (one request on the route: course selection), the AICA DSP (four DSP-only loops heard dry),
  filter/LFO, the port volume curve (a linear guess).

Next: MIDI sequences (SMSB `SMSD` event streams + SMPB programs/layers) if the game uses more than the one jingle;
the DSP (SFPB) or at least a reverb send; S8 first race (full race, 4 players); the RTC; renderer follow-ups (S5).

## S6 — 2026-11-11: input, VMU, rumble; the retrowser module runs end to end

Design and details: `docs/port/RTW.md`; pd/bu semantics read in the SDK's code: `docs/port/SDK.md` 2.3/2.4. All guest
code ran in `--network none --read-only --cap-drop ALL` containers; the module, e2e PNGs and VMU images stay in
`penpen-build` (`/build/rtw`, `/build/e2e`, `/build/vmu`); `git log --all -- port/gen` stays empty.

- **Stepped frames without Asyncify**: the recompiler makes its output resumable (every call a numbered site;
  `rt_unw` saves sites on the way out of `njWaitVSync`, `rt_rew` rewinds into it; functions with sites and back
  edges go through their label switch, so the wasm control flow stays reducible). `rtw_frame` = one frame; the
  interpreter fallback is resumable too. **Same digests as S5** on the S4 route (native and wasm), same speed;
  yield depth 12. The harness found one bug of the first version (a resume key colliding with a jump target).
- **pd**: the SDK's own conversion confirmed: r/l at +0x18/+0x1A, **bit 16 = R, bit 17 = L** with 192/160 hysteresis
  (the old pad script had them swapped), y1 = −sy; the game reads only on/press and the stick (never the analog
  trigger values). 4 ports from `rtw_input` (the ports the host sends are connected), `rtw_triggers`.
- **bu**: a VMU flash file system (`vmu.c`: root/FAT/directory, files from block 199 down) behind the SDK API with
  the SDK's result codes and complete-callback operations; `buMakeBackupFileImage` runs as guest code (the VMS CRC
  checks out with an independent CCITT computation). The S5 "frame 750 memory-card notice" is gone: the game
  creates its 5-block save at the mode menu and loads it on the next boot. **pdVib** → `rtw_rumble` (Jump Pack
  in slot 2: port 2 = A2); **pdLcd** pictures dropped.
- **Module**: `port/rtw/glue.js` (factory `createPenpen`, `penpen_gl_frame` → `PenpenGL.frame` on the
  OffscreenCanvas), `retrowser.json` (dreamcast, IP.BIN SHA-1, webgl, 60 Hz, vmu, MIT), `tools/rtw.sh
  image|build|check|e2e|look`, image `penpen-rtw:1` (`docker/rtw-host`: retrowser main + Playwright Chromium).
  **`rtw.mjs check --disc`: OK, 0 errors, 0 warnings**, leak scan clean (it covers the disc's first MiB per data
  track; the wasm is translated code, 47 MB).
- **End to end** (retrowser's local host + headless Chromium in one container; archive picked on the disc
  screen): unpacked and running in 71 s; logo, title, menu, PenPen House, selection, course map, race start and
  racing seen in screenshots; the host stored the VMU save and, **after a reload, the game loaded it**;
  `navigator.vibrate` called on rumble; **58.8 game frames/s** in the race (SwiftShader).
- Found by the e2e: 0x8C016EC0, a gap function that waits in `njWaitVSync`, ran in the interpreter → new entry
  (harness **8 068: 7 687 pass / 0 fail / 381 untested**); retrowser's off-the-record test context cannot hold a
  1.2 GB disc in OPFS → our e2e uses a persistent profile (no host change needed). **No retrowser change.**
- Tests: sh4 **257 / 0**; mutants caught; native = wasm and `--pad` = `--input` (the host path) digests over 4 000
  frames; VMU native save → reload → load.
- Known: the save's time stamp is 1950-01-01 (the RTC register reads 0); the title funnel (S5); the 0x2C field of
  the disk info and messages 5/7 unverified; the persistent-storage notice of the headless host (dismissed).

Next: S7 audio (Manatee `sd` HLE + AICA mixer, CDDA from `rtw_disc_read` audio sectors, `rtw_audio_*` exports);
a one-command local launcher so the user can try the module in their own browser (RTW.md 6); an RTC (a real clock
in the browser, fixed in test mode); more `rt interp` finds as play goes further (now harmless, but slower);
renderer follow-ups from S5.

## S5 — 2026-11-11: HLE Ninja + Kamui → draw list → WebGL2; logos, title, menus, house, race visible

Design and details: `docs/port/RENDER.md`. All guest code ran in `--network none --read-only --cap-drop ALL`
containers; dumps and PNGs stay in `penpen-build:/build/shots` (PNGs looked at from the session scratchpad only).

- **Architecture**: the Ninja HLE (`port/render/nj_draw.c`, `nj_model.c`) builds a per-frame **draw list in our
  format** (`rd.c`: PVR-style screen-space vertices with 1/w, lists OP/OM/TR/PT, polygon state, strips →
  triangles, culling, translucent list sorted back to front per triangle) and hands it to the host through one
  import, `env.penpen_gl_frame(ptr, len)`, at `njWaitVSync`. `gl/render_gl.js` is the WebGL2 backend (classic
  script for the retrowser worker's OffscreenCanvas). Capture: node writes self-contained dumps of chosen frames,
  headless Chromium (`docker/chromium`, SwiftShader) renders them with the same backend (`tools/s5.sh`).
- **Textures**: PVRT/GBIX decode (twiddled incl. rectangles, VQ/small VQ, mipmapped top levels, rectangle/stride;
  ARGB1555/RGB565/ARGB4444/RGB555/YUV422/ARGB8888; palettised not yet), PVM archives into the game's
  NJS_TEXMEMLIST (global-index sharing, counts), decoded at load time (the game overwrites the PVM buffer).
- **2D**: njDrawSprite2D (NJS_TEXANIM = 10 × Sint16, u/v in 1/256, pri → 1/w), blending modes, back colour,
  njClip2D (argument = {tile min, tile max, pixel min, pixel max}), table fog (FOG_DENSITY from the SDK's
  njGenerateFogTable3 run as guest code).
- **3D**: chunk objects with motions and shape motions; **the game's nj_Model* pass an object, not a model**;
  node transforms by the SDK's own matrix code (guest calls), the HLE reads the current matrix (pointer
  0x8C1EF2B8); view space +z forward, y down, checked at run time against njCalcPoint; near clipping; easy light
  (L1 setters); constant material (njControl3D 0x10); environment maps; njDrawSprite3D; cheap shadows
  (njCnkModDrawModel volumes → stencil parity, × 160/256).
- **Looked at** (route `--pad 600:START,700:A,1100:START,1200:A,1500:A,1700:A,1760:A`): 100 Infogrames logo;
  300 GE logo; 500 title (3D logo, ring, ice funnel, PRESS START BUTTON); 650 mode menu (TRICELON / TIME TRIAL /
  VSICELON); 750 = the S4 "167-sprite screen": a memory-card notice; 1150 PenPen House (menu + room + six
  characters in 3D); 1600 course map (SWEETS, Mrs. CREAM); 2300 race start (TIME, GO!, 5/5, START arch, course,
  rivals, player with shadow); 3000 race with fog haze (player idle: the script presses nothing).
- **Known wrong**: the title's ice funnel lies in front of the logo in our geometry and hides it once its
  alpha is 1 (~frame 900): motion evaluation suspect; shadows darken all opaque polygons in the volume; no
  per-pixel translucency sort; L2–L4 lights, specular, shape blending, basic models (nj_ObjectB, attract demo),
  njDrawPolygon/Line/Triangle2D, palettised textures not done. RENDER.md 7.
- **Determinism**: native = wasm RAM digests over 3 000 route frames (every 50th compared); same route timings
  as S4 (files at 1102/1104/1563/1762, CDDA 1854/2244). sh4 **257 checks, 0 failures**; harness **8 067
  functions: 7 686 pass / 0 fail / 381 untested** (unchanged).
- New: `port/render/*`, `docker/chromium/Dockerfile` (image `penpen-chromium:1`), `tools/s5.sh`; run.mjs
  `--shots/--shot-dir`; wasm built with memory growth (+ `emscripten_notify_memory_growth` import, a no-op).

Next: S6 input (pd: 4 ports, analog stick and triggers from `rtw_input`/`rtw_triggers`), VMU (`bu` on a 128 KiB
image = `rtw_save_*`, the notice at 750 then finds a card), rumble (pdVib → `rtw_rumble`); retrowser module
packaging (glue: `penpen_gl_frame` → `PenpenGL.create(OffscreenCanvas).frame(memory.buffer, ptr)`, stepped
`rtw_frame` instead of the njWaitVSync loop). Renderer follow-ups: the title funnel, L2/L3 lights, basic models
for the attract demo, shape-motion blending, shadow flag per polygon.

## S4 — 2026-11-11: HLE files + memory + timers; the boot reaches a race

All runs in `--network none --read-only --cap-drop ALL` containers (`tools/s3.sh`); generated C and logs stay
in `penpen-build` / gitignored `port/gen/` (`git log --all -- port/gen` empty). Details: `docs/port/RECOMP.md` 2, 4,
5 and **7**.

- **Root cause of the "stall"**: none. S3's run stopped at its 60-frame limit, and the first logo (alpha += k
  per frame, 90-frame hold, fade out) needs 211 frames. Run longer with S3's stubs it faulted at frame 2 230:
  `njRandom` (and the other maths) stubs returned nothing, the game turned the stale float into state and
  jumped to 0x8036A290. Timer, pads and gdFs status were not the cause.
- **Guest SDK code instead of maths stubs**: `recomp.py --ranges boot` translates the whole image (8 067
  functions incl. 142 extra entries) with SDK entry points always called through `rt_call`; `hle.c` runs 46
  pure SDK functions as their own code (heap, matrix/vector maths, njSin/njCos/njSqrt/njRandom, camera/view,
  chunk-file parser). All pass the differential harness.
- **sy**: syMalloc/syFree = the SDK allocator itself; syTmrGetCount/TCNT0 = TMU0 at 781 250 Hz from 59.94 Hz
  frames (+1 tick per read in a frame); syChain handlers by event/priority, 0x900 fired at V-blank, 0x920 at end
  of render; syCache no-ops (stubs).
- **gdFs**: guest handle structures with the SDK layout; sync read, async gdFsReqRd32 (next V-blank or 8th
  poll), status, drive handle + drive-status request (`gdFs_Api34`/`Api58` = driver slots 0x34/0x58), drive
  status PLAY while CDDA plays, CDDA calls logged. No directory functions are called (flat disc, root only).
- **FPSCR decision: round-to-zero + denormals-as-zero modelled in software** (the game's 0x00040001): with
  nearest the first logo ended a frame later and the random title picture / attract course differed.
- **Interpreter fallback entries**: the 3 SDK ones are translated, the C-runtime word-copy helper's entries
  and 16 functions missing from the table (gaps) are extra entries (`port/symbols/entries.tsv`, 52; 90 found
  statically). The 6 000-frame race run uses the fallback 0 times.
- **Test mode**: `--pad "F[+N]:BTN|BTN,..."`, `--digest N` (RAM digest), per-frame line with HLE/guest/interp
  counts, files, heap, pad; `hle screen` lines (call-pattern changes); `--watch`; last-calls ring on a stop.
- **How far**: no input: logos (0, 211), title (429, CDDA 5), attract demo race (2 228), title again (5 828).
  With `--pad 600:START,700:A,1100:START,1200:A,1500:A,1700:A,1760:A`: menu (607), a 167-sprite screen (706),
  character select (1 104, files 5100–5111, CDDA 8), next select (1 563, CDDA 9), **race** (1 762: 19 files,
  CDDA 11 then race music 12 at 2 244), still racing at 6 000 frames; 42 files, 150 SDK functions.
- **Native = wasm32 (node), RAM digests identical every frame** (6 000 frames); ~300 frames/s both. wasm now
  45 MB (whole image translated).
- **Tests**: sh4 **257 checks, 0 failures** (231 + 26 rounding checks); harness on the boot translation
  **8 067 functions: 7 686 pass / 0 fail / 381 untested**.
- New tool: `tools/ghidra.sh decomp ADDR|@PTR...` (read-only pseudo-C, terminal only).

Next (S5 renderer): implement Ninja/Kamui drawing behind the logged stubs (njDrawSprite2D first: the logos and
title are sprites; then njSetTexture/njLoadTexturePvmMemory with PVR decode, nj_ModelL1 and the chunk
families, njSetConstantMaterial/njColorBlendingMode/njControl3D state, njClip2D per player), reading the
matrices the guest Ninja code keeps (njInitMatrix's stack, current matrix pointer 0x8C1EF2B8) and the
screen/view globals; screenshots per frame index from the test mode (still in the volumes). Also: trim the
wasm to the SDK code the guest functions reach; keep adding `rt interp` finds to entries.tsv; identify the
706 screen (167 sprites; Start, not A, moves on) once it can be seen.

## S3 — 2026-11-11: SH-4 interpreter, recompiler scaffold, boot on wasm

Everything ran in `--network none --read-only --cap-drop ALL` containers of the emsdk image (`tools/s3.sh`);
disc-derived output (generated C, objects, native binary, wasm, logs) stays in the new volume `penpen-build`
and the gitignored `port/gen/`. Design and numbers: `docs/port/RECOMP.md`.

- **Interpreter** (`port/sh4/`): integer + FPU (single/double, PR/SZ/FR banks, pair moves, fipr/ftrv/fsca/
  fsrra), delay slots, T/S/Q/M, MAC with saturation, banked registers. One instruction table
  (`sh4_insns.def`) for the C decoder and the recompiler; semantics in `sh4_ops.h`, shared by both.
  `tools/s3.sh test`: **231 checks, 0 failures** on our own encodings (incl. the manual's div0u/div0s+div1
  sequences vs C, 2 000 random cases each).
- **Recompiler** (`port/recomp/recomp.py`): one C function per guest function, labels + gotos, delay slots
  inline, direct calls / literal-pool calls with a run-time check / `rt_call`, tail calls, one label switch per
  function with computed jumps (all cycles through it: clang's wasm backend died on the irreducible form). Game
  range: **4 148 functions, 131 392 instructions**, 47 with computed jumps.
- **Differential harness** (`diff_main.c`): interpreter vs recompiled C from the same random state and memory
  model, compares registers, written bytes, calls and the ending. Game: **4 134 pass / 0 fail / 14 untested**
  (40 trials, 100 000 steps); whole image 7 925: 7 546 / 0 / 379. 7 mutants: 5 caught in hundreds to thousands
  of functions each; the two slot-hazard mutants have 0 (rts/PR) and 30 (bt/s/T, whole image: 5 flagged) sites.
- **Runtime + HLE** (`port/runtime/`): dispatch HLE → recompiled → interpreter fallback; I/O register file;
  disc layer on retrowser's `rtw_disc_toc/rtw_disc_read` + ISO9660; IP.BIN and 1ST_READ.BIN loaded from the disc
  at run time. 223 SDK targets + 6 BIOS vectors as logging stubs; heap, gdFs, pads, V-sync, timer behave.
- **Boot: native and wasm32-in-node give the same log**: start-up → region check → `sbInitSystem` (syHwInit,
  syMallocInit, nj_SetWorkSize, njInitSystem, syHwInit2, pdInitPeripheral, gdFsInit) → graphics, sound and
  VMU set-up, 5 files read (sound driver, a container, 3 PVMs) → the first screen's frame loop (camera, lights,
  njDrawSprite2D, njWaitVSync + V-sync callback): 60 frames, 3 022 HLE calls to 83 functions. The wasm is 6.9 MB
  and imports only `env.rtw_*`.
- User decisions during the session: the recompiled C is **gitignored, never committed** (`port/gen/`, a
  briefly made commit of it was removed from history and the objects pruned; `git log --all -- port/gen` is
  empty); execution stays Docker-only. PLAN 9 updated.
- Files: `port/sh4/*`, `port/recomp/*`, `port/runtime/*`, `tools/s3.sh`, `tools/s3_hle_names.py`,
  `docs/port/RECOMP.md`.

Next (S4 HLE: files + memory + timers): find why the first screen does not advance (timer: syTmrGetCount
rate and the V-blank chain from syChainAddHandler(0x900, ...); pads all zero; SDK maths stubs returning 0:
translate or implement njSin/njCos/matrix functions, which are pure code); gdFs status/async semantics
(gdFsGetStat, gdFs_Api34 polled each frame); real syChain event calls; give the 8 interpreter-fallback entries
(3 SDK functions outside the census, 5 mid-function entries) proper handling; FPSCR rounding (round-to-zero)
decision; then the S5 renderer behind njDrawSprite2D etc.

## S2 — 2026-11-11: census of the SDK surface

All analysis in the S1 sandbox (`tools/ghidra.sh`, containers `--network none --read-only --cap-drop ALL`); the
working dumps (census, call sequences, data-flow summaries, hardware tags) stay in `penpen-work:/work/s2`. Only
addresses, counts, assigned names and our own descriptions reached the host.

- **The 224 SDK targets the game calls (2 091 call sites)** are in `port/symbols/sdk_calls.tsv`: 150 with SDK API
  names (37 high, 69 medium, 44 low), 73 role names (behaviour known, SDK name not pinned: mostly Ninja draw
  families and their light setters, some Manatee commands), 1 is data. Anchors: the game's copies of the SDK
  sample `sbInitSystem`/`sbExitSystem` (0x8C010184/0x8C01022C), constant arguments (sprite attributes, blending
  modes, `NJM_LOCATION`, gdFsInit's aligned buffers, the 4 port numbers), data-flow shapes (matrix rows, `fsca`,
  `fipr`), magic numbers (`SDRV`, `SMLT`, `PVMH`, `STRO`/`MONO`), PVR register offsets written through Kamui's
  register helper.
- `docs/port/SDK.md`: the surface by library with HLE semantics and session priority; graphics, input/VMU,
  container, 0x8C23D900 and recompiler sections.
- **Regions**: `port/symbols/regions_s2.tsv` (23 overrides) + `tools/s2_apply.py` (run by `ghidra.sh export`,
  idempotent): gdFs (API + driver table, GD/ISO9660 core), pd/pdLcd/pdVib, bu (API + core), **kd = the Maple bus
  driver**, syChain/syInt/sySq, syBtCfg, **syFbr** (fibers, used by CRI), BIOS syscall stubs inside the old
  `shc_rt` range, CRI_SJ (low), two big `unknown` runs → Ninja strip emitters. `unknown` 213 → 70. Method: call
  graph from the API functions + the order of the libraries' version strings in data, which follows code order.
- **Graphics**: lists opaque + opaque modifier volume (cheap shadows, `njSetCheapShadowMode(160)`, one
  `njCnkModDrawModel` site) + translucent; `njInitVertexBuffer(op=-0x80000, om=24576, tr=0x40000, tm=0, pt=0)`:
  no punch-through, no translucent MV. Render-to-texture code is linked but unreachable; no read-back path found.
  Split screen = per-player `njSetScreen` + camera + user tile clip (`njClip2D` → Kamui per list). Modes: VGA 1 or
  TV 56 (640×480i), 60 Hz.
- **Input/VMU**: 4 ports (A0–D0) through `pdGetPeripheral`; fields read +4, +8, +0x10, +0x1C–+0x22, +0x30 (the
  trigger fields +0x18/+0x1A not seen: to confirm); VMU LCD picture on A1–D1; one save file via `bu`; Jump Pack via
  `pdVibMx*`.
- **Containers**: loader 0x8C012EBE, 19 record types with their fix-up functions (basic vs chunk object trees,
  motions, texlists, actions, links, raw); table in SDK.md 4.
- **0x8C23D900** is the static-constructor table (3 game constructors + terminator, then empty destructor list);
  the "call" is Ghidra folding the empty destructor loop. The zero block is `.bss` cleared by 0x8C0100D8. The
  bogus function is now region `data` (and removed from the project by `S2Names`).
- **Recompiler notes** (SDK.md 6): 604 register-indirect `jsr` in game code, 21 `braf` switch tables, FPU mode
  switches only in the SDK (game code is FPSCR-default single precision; rt doubles toggle PR), no PC-relative
  loads in delay slots, no self-modifying code, **calls into the P2 mirror (0xAC0xxxxx)** of the cache code, cache
  and store-queue ops SDK-only, fibers (syFbr) and exception entries in the SDK.
- Files: `tools/ghidra/S2Census.java`, `S2Flow.java`, `S2Seq.java`, `S2Hw.java`, `S2Xref.java`, `S2Words.java`,
  `S2Reach.java`, `S2Tail.java`, `S2Lit.java`, `S2Switch.java`, `S2Recomp.java`, `S2Recomp2.java`,
  `S2Names.java`; `tools/ghidra.sh s2` (dumps + names into the project); `tools/s2_apply.py`.

Left for later sessions (see SDK.md 8): SDK names of the Ninja draw families; the unnamed Manatee commands; CDDA
function names; the pd trigger fields; 70 `unknown` functions and the SJ boundaries; VRAM helper directions.

Next: S3 (SH-4 interpreter + recompiler scaffold in `port_work/`), using SDK.md 6; the HLE stubs for the 224
targets are the first thing the recompiled binary will hit (`syHwInit`, `syMallocInit`, `njInitSystem`).

## S1 — 2026-11-11: Ghidra in Docker, function table, SDK region table

All analysis in sandboxed containers (`--network none --read-only --cap-drop ALL --security-opt no-new-privileges`,
tmpfs `/tmp` and home); 1ST_READ.BIN read from `penpen-work`, project in the new volume `penpen-ghidra`. Network was
used only to build the image. Nothing from the disc reached the host except addresses, sizes and assigned names.

- Tool: **Ghidra 12.2** (2026-10-16 release, SHA-256 checked) on Eclipse Temurin **JDK 25** (Ghidra 12.2 needs 25);
  image `penpen-ghidra:12.2` from `docker/ghidra/Dockerfile`. The release has no linux_arm_64 natives, so the image
  builds them (`gradlew buildNatives`; decompiler + sleigh) on Apple-silicon hosts. Ghidra chosen over rizin: SuperH4
  is rated PLATINUM, FPSCR.SZ/PR are handled in p-code, and the decompiler/switch recovery will be needed in S2+.
- Import: raw binary, `SuperH4:LE:32:default`, base and entry 0x8C010000; `S1Setup.java` adds the rest of main RAM,
  the Holly/AICA/VRAM/TA/store-queue/P4 blocks as uninitialised volatile memory, and the entry function. Auto-analysis
  finds 3 673 functions (only code reachable by calls from the entry).
- `S1Seed.java` (iterated with re-analysis until stable): orphan code runs, function pointers in data, the first
  valid subroutine after each function's literal pool, and far body chunks split off → **7 925 functions** (S0
  estimate 7 000–7 500). Code coverage 84.4 % instruction bytes, **93.4 % with literal pools** (game range 93.8 %,
  SDK 95.5 %); 851 functions are 4 bytes or less (`rts` getters/empty stubs, normal for SHC).
- Layout corrected (PLAN 3): game code 0x8C010184–0x8C062434 (4 010 functions); **SHC C runtime** 0x8C062440–
  0x8C0668DA (132; the only game-range code the SDK calls); SDK code 0x8C0998D4–0x8C17994A plus 40 functions in the
  data area at 0x8C18F112–0x8C197090; a 682 KB zero block inside the image. The game/SDK split of S0 holds (≈ 4 000
  game vs ≈ 3 800 SDK + runtime); the game calls 224 distinct SDK functions.
- **SDK libraries interleave** (linked object by object on demand), so `regions.tsv` has 44 runs, each with its
  evidence and confidence: strings (version/error/config, the text stays in the sandbox), function names in error
  strings, hardware register blocks, BIOS vectors, `ftrv`/`fipr`, shared globals (data-sharing components), then a
  segmentation (dynamic programming) with a weak contiguity prior. Ninja ≈ 623 KB in 10 runs, Kamui 64 KB, CRI
  ≈ 70 KB (mw/SFD/ADX), sd 13 KB, sy 21 KB, pd 8 KB, unknown 43 KB. gdFs, bu, kd, SJ not separated yet.
- **60 SDK functions named** (ADXF_*, ADXSTM_*, ADXT_SetOutPan, mwPly*/mwSfd*/mwRna*/mwSnd*, sdPstm*/sdMemBlkCreate/
  sdSndMem*/sdSysFlushHostCmd; `name_conf` high when 2+ strings agree); the sdPstm* names sit on 6-byte thunks into
  the Manatee code at 0x8C16CCxx.
- Oddity for S2: game code calls 0x8C23D900, in the image tail after the zero block (code placed there at run
  time?).
- Files: `docker/ghidra/Dockerfile`, `tools/ghidra.sh` (`import | seed | export | all | post Script.java`),
  `tools/ghidra/*.java` (S1Setup, S1Seed, S1Regions, S1Strings [writes strrefs.tsv in the volume only], S1Coverage,
  diagnostics S1Probe/S1Gaps/S1List/S1Mnem/S1DataMap/S1HwHist/S1Unref/S1XCalls/S1Stats), `port/symbols/functions.tsv`
  (address, extent, instruction bytes, name, region, name confidence, evidence; 342 Ghidra thunks are named
  `j_<target>_<address>`), `port/symbols/regions.tsv`.
  `tools/ghidra.sh all` reproduces everything in ~75 s.

Next (S2 census): the 224 SDK entry points the game calls, named by behaviour/API docs; refine regions (gdFs via the
BIOS GD-ROM vector callers, bu/pd split, SJ inside CRI, the `unknown` and `low` rows); Kamui list types,
render-to-texture check, trigger/VMU use, container type map; `docs/port/SDK.md`.

## S0 — 2026-11-11: survey of the US disc, port plan

Static survey only, all in sandboxed containers (`--network none --read-only --cap-drop ALL`, disc volume
`penpen-disc` read-only, scratch volume `penpen-work`). Nothing from the disc was executed or copied to the host.

- Disc: Redump GD-ROM, 48 tracks; track 03 is IP.BIN + ISO descriptors + padding, track 48 holds all 194 files
  (150.5 MB), tracks 04–47 are 44 CDDA music tracks (~77 min). Hashes in PLAN 0.1.
- IP.BIN: `T15105N` `V1.020` 1999-08-19, area U, peripherals `0799A10` (VGA, Jump Pack, VMU, standard pad with
  analog triggers), boot `1ST_READ.BIN`, Katana (not WinCE).
- Files: 100 PVM + 10 PVR (VQ-mipmapped mostly), Ninja NJ/NJM/NJS, one custom uncompressed typed-record container
  format (`.BIN`/`.DAT`), 5 Manatee MLT banks + `MANATEE.DRV`, a Sofdec intro `PEN_OP.SFD`. No compression found.
- 1ST_READ.BIN: 2 287 804 bytes, unscrambled, 0x8C010000; ~7 000–7 500 functions (~3 400 game, ~3 800 SDK, split
  to confirm); compiler consistent with Hitachi SHC; no symbols.
- SDK: Shinobi 1.42, Ninja 01000086, Kamui 1.06, Manatee sd 1.00.18, gdFs 1.07, pd 1.46 / pdVib / pdLcd, bu 1.44,
  CRI Sofdec/ADX (mwSfd 1.28, ADXF 5.52, SFD 1.12).
- retrowser needs a `dreamcast` platform: disc random-access reads (multi-file pick, cue/gdi/chd), a disc
  fingerprint (IP.BIN or 1ST_READ SHA-1), `save.kind: vmu` 131072, DC pad bits + analog triggers (PLAN 6).
- Recommended: hand decomp (non-matching, differential tests) with SDK HLE, using a local-only static recompile as
  a scaffold/oracle (PLAN 7). Estimate ≈ 115–230 sessions.
- Survey script sources: `docs/port/survey/` (outputs stayed in the `penpen-work` volume).

Next: the user answers PLAN 11 (licence, retrowser platform work, Ghidra/rizin + emsdk downloads, scaffold); then
S1 tooling.
