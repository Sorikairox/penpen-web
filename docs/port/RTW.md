# Pen Pen TriIcelon: the retrowser module (S6)

The port runs as a [retrowser](../../../retrowser/spec/retrowser.md) module, platform `dreamcast`: the host hands it
the player's disc (`rtw_disc_toc` / `rtw_disc_read`), steps it once per frame (`rtw_frame`), gives it the pads
(`rtw_input`, `rtw_triggers`), stores the VMU (`rtw_save_*`) and relays rumble (`rtw_rumble`). The module draws
with WebGL2 on the worker's OffscreenCanvas through our glue. No audio yet (S7). Everything here is our own code
(MIT); the module's wasm contains the translated game code (the recompiled C of `port/gen/`), so the built module
directory is disc-derived: it stays in the Docker volume `penpen-build:/build/rtw` and is never committed or
published (PLAN 9).

## 1. Pieces

| File | What |
|---|---|
| `port/recomp/recomp.py`, `rt_gen.h` | resumable frames: every call in the translated C is a numbered site (2) |
| `port/runtime/main.c` | exports: `rtw_start` (load IP.BIN + 1ST_READ.BIN, set the CPU up), `rtw_frame`, `rtw_input`, `rtw_triggers`, `rtw_save_ptr/size/generation/load` |
| `port/runtime/hle_io.c` | pd (4 ports), pdVib → `rtw_rumble`, pdLcd (dropped), bu (the VMU API) |
| `port/runtime/vmu.c` | the VMU flash file system on the 128 KiB image |
| `port/rtw/glue.js` | `files.js` factory `createPenpen`: instantiates the wasm with the host's `env.rtw_*` plus `penpen_gl_frame` (→ `PenpenGL.frame`) and `emscripten_notify_memory_growth` (no-op) |
| `port/rtw/retrowser.json` | the manifest template (hashes filled by `rtw.mjs hash`) |
| `port/rtw/e2e/penpen.e2e.ts` | the end-to-end test (5) |
| `docker/rtw-host/Dockerfile` | image `penpen-rtw:1`: retrowser main (`npm ci && npm run build`: the local host, `dist/tools/rtw.mjs`) + Playwright Chromium |
| `tools/rtw.sh` | `image`, `build`, `check`, `e2e`, `look` |

Manifest: id `penpen-triicelon`, platform `dreamcast`, `rom.sha1` = the US disc's IP.BIN SHA-1
`05c789c1b426912509fd5af9e861c1d0cc8cbd17`, `video: webgl`, `hz: 60`, `save: vmu 131072`, `files.wasm`
`penpen.wasm`, `files.js` `penpen.js` (factory `createPenpen`), licence `MIT` (our code; the user's choice for now:
the game itself always comes from the player's disc). No `controls.extra`: the host's standard Dreamcast pad covers
the game (A, B, X, Y, Start, D-pad, stick, analog L/R).

## 2. Stepping: resumable frames

The game runs its own loop and waits in `njWaitVSync`; the host wants `rtw_frame` to return after each frame.
Asyncify was not an option for a 45 MB module where almost every call is indirect (size and speed), and JSPI is
not in every browser. Instead the recompiler makes its output resumable, which is cheap because the translated C
keeps no state in C locals across a call (registers are in the `SH4` struct, the guest stack in guest RAM):

- every call statement (`CALL_D`, `CALL_I`, `TAIL_D`, `TAIL_I`) gets a label `R_n`; after the call,
  `if (rt_unw) { save (function, n, call target); return; }`;
- on entry, `if (rt_rew)`: pop the saved site (checked to belong to this function), restore the call target and
  jump to `R_n`, i.e. straight back into the same call;
- a function with call sites and back edges is emitted in "loop mode": its entry and every back edge go through
  its label switch `D_` (as the computed-jump functions already did), and the resume switch sits in front of it,
  so the resume entries never make the control flow irreducible for clang's wasm backend. Resuming is checked on
  `rt_rew` only, never mixed into the label switch (a jump target could normalise onto a key: the harness found
  that).

`h_njWaitVSync` runs the end-of-render half (EOR callback, chain 0x920, draw list to the host, frame log), sets
`rt_unw` and returns: every frame saves its site on the way out, `rtw_frame` returns. The next `rtw_frame` sets
`rt_rew` and calls the entry point again: the chain is rebuilt (12 frames deep on the S4 route), `h_njWaitVSync`
is entered again, clears `rt_rew`, runs the V-blank half (time, drive, pads with the host's newest input, chain
0x900, V-sync callback) and returns into the game. The interpreter fallback is resumable as well: when a native call
it made yields, it saves its caller's return address, stack and the call's target, and the rewind repeats that
call (tested by interpreting the program entry: `--force-interp 8c010000`, same digests). A yield inside a guest
call made by the HLE (callbacks) stops the run with a message (never seen). The watchdog's back-edge budget is per frame now.

Checks: the S4 route gives the **same RAM digests as S5** (every 50th of 3 000 frames, native and wasm); the
harness is unchanged (8 067 functions: 7 686 pass / 0 fail / 381 untested); the mutants are still caught; same
speed (~340 frames/s in node).

## 3. Input, VMU, rumble

Semantics from the SDK's own code: `docs/port/SDK.md` 2.3 and 2.4.

- **Pads**: `rtw_input(port, buttons, sx, sy)` (Dreamcast Maple bits, active-high) and `rtw_triggers(port, l, r)`
  are latched; at the start of each `rtw_frame` the ports the host sent in the previous host frame are the
  connected controllers (port A always). At V-blank the SDK's structures are filled as its own conversion does:
  on/off/press/release/old, r/l at 0x18/0x1A, the trigger bits 16 (R) / 17 (L) with the SDK's 192/160 hysteresis,
  x1 = sx, y1 = −sy, info → controller. The test mode's pad script still works on port A (`--pad`; RT/LT, and new
  SL/SR/SU/SD for the stick); the node host's `--input` gives the same script through `rtw_input`/`rtw_triggers`.
- **VMU**: a card in A1 (`rtw_save_ptr` = the 131072-byte image, the layout of a real card and of Flycast's
  `vmu_save_A1.bin`): root block 255 (16 × 0x55, BCD time, media description at 0x40), FAT 254 (0xFFFC free,
  0xFFFA end), directory 253–241, user blocks 0–199, data files allocated from 199 down. A blank image (a player's
  first run) is formatted at start. `bu` completes everything at once (`buStat` never busy) and calls the game's
  complete callback; the card is mounted at the first V-blank after `buInit`. `buMakeBackupFileImage` runs as guest
  SDK code, so the VMS file is byte for byte what the game's SDK builds. Every save bumps `rtw_save_generation`;
  the host then stores the image.
- **Rumble**: a Jump Pack in slot 2 of each connected controller; `pdVibMxStart`/`Stop` → `rtw_rumble(port, on)`
  on changes (the game calls Stop every frame, Start every frame of a pattern).
- **VMU LCD**: `pdVmsLcdIsReady(A1)` = 1, the pictures are dropped (logged once).

## 4. Build and check

```
tools/s3.sh gen                 # the translation (once per recompiler change)
tools/rtw.sh image              # once: penpen-rtw:1 (network only during this build)
tools/rtw.sh build              # wasm + glue + manifest with hashes in penpen-build:/build/rtw
tools/rtw.sh check              # rtw.mjs check /build/rtw --disc /raw/archive.7z, --network none
```

Result (S6): **`OK`, 0 errors, 0 warnings**; the disc is read as the host does (48 tracks from the cue in the
`.7z`), its IP.BIN SHA-1 is `05c789c1b426912509fd5af9e861c1d0cc8cbd17` = `rom.sha1`. The leak scan (runs of the
disc's first MiB of every data track in the module files) finds nothing. It does not cover the rest of the disc:
the wasm holds the translated game code (each SH-4 instruction becomes an `op_*` call with its opcode as a
constant, literal pools and data are read from guest RAM at run time), which is the accepted local-only design
(PLAN 9). Sizes: `penpen.wasm` 47 MB (limit 64 MiB), `penpen.js` 15 KB.

## 5. End-to-end test

`tools/rtw.sh e2e` runs, in one `--network none --read-only --cap-drop ALL` container of `penpen-rtw:1`, retrowser's
own e2e harness (its Worker handler on `dist/` + the GitHub mock serving `/build/rtw` as `penpen/triicelon`) and
Playwright's headless Chromium (SwiftShader WebGL). The script `port/rtw/e2e/penpen.e2e.ts` opens the game page,
picks `/raw/archive.7z` on the disc screen, lets the host unpack it into OPFS, and drives the host's keyboard
(Enter = Start, X = A, arrows = stick) through logo, title, mode menu, house, character and course selection into a
race, holding A and steering for 40 s; screenshots of the game screen go to `/build/e2e` (`tools/rtw.sh look NAME`
copies one to the scratch directory for viewing). Then it reloads the page: the disc is remembered, the host gives
the module the stored VMU, and the game must load its file.

One detail: retrowser's own harness uses an off-the-record Chromium context, whose OPFS is in memory with a small
quota; the 1.2 GB disc did not fit ("Writing the disc to this browser's storage failed"). The script uses retrowser's
pieces (`serveWorker`, `startMockGithub`, `chromePath`) with a **persistent profile on the volume `penpen-e2e`**
instead. Not a host bug (a real browser profile is persistent).

Result (S6, all in the container, SwiftShader WebGL2):

- the archive (837 MB) is unpacked and the game is running after **71 s**; logo (frame ~158), title (~520: the
  ice funnel, the S5 known issue), mode menu (~700), Enter / X through menu → **PenPen House** (~935: the menu
  and the six characters, "1P" cursor), character selection, course map (SWEETS, Mrs. CREAM), **race start**
  (~2350: TIME, GO!, 5/5, START arch, rivals, the player with its shadow), racing with A held and the stick
  left/right (the player turns round: "NOT THAT WAY!", later swims: "STROKE / PRESS A RELEASE");
- **VMU**: at the mode menu the game finds no file (−251), creates its 5-block file; the host stores the image
  (IndexedDB `penpen-triicelon:vmu`: 131072 bytes, formatted, one data file of 5 blocks at block 199). After a
  **page reload** the host gives the module the stored image (`rtw_save_load`: formatted, 195 free blocks) and the
  game **loads its file** ("hle vmu load 5 blocks") at the same menu step;
- **rumble**: the Jump Pack starts at frame ~2456 and stops 13 frames later; the page's `navigator.vibrate` is
  called (2 calls: on, off);
- **frame rate**: **58.8 and 58.7 game frames/s** over 40 s of racing in two runs (host target 60);
- no page errors. A first run had stopped at frame ~2613 of the race: the game entered 0x8C016EC0 (a gap of the
  function table), the interpreter ran it and it waited in `njWaitVSync`; it is now an extra entry, and the
  interpreter fallback itself is resumable (tested with `--force-interp 8c010000`: same digests).

## 6. Trying it in your own browser

Not done by any session (the user decides when). What it takes:

Nothing has been run in a host browser. To try it, on the user's own machine:

1. `tools/rtw.sh build` (and `check`), then copy the module out of the volume to a local folder, e.g.
   `docker run --rm -v penpen-build:/build:ro -v "$PWD/rtw-local":/out penpen-rtw:1 cp -r /build/rtw/. /out/`.
   The folder contains the translated game code: keep it local, outside the repo.
2. In the retrowser checkout: `npm run build`, then serve the host locally (`node tools/serve-worker.ts`, the dev
   Worker on `http://localhost:<port>`) and a GitHub mock for that folder (`tools/mock-github.ts` with a fixture
   `{ "penpen/triicelon": { refs: { main: SHA }, commits: [{ sha: SHA, dir: "<rtw-local>" }] } }`, as
   `port/rtw/e2e/penpen.e2e.ts` does), set `localStorage['rtw:test-endpoints']` in the host page as the mock
   prints, open `http://localhost:<port>/g/penpen/triicelon`.
3. Click the cartridge, pick the disc archive (`.7z`, or the `.cue` with its `.bin` tracks): the browser unpacks it
   into its own storage once (~1.2 GB; about a minute here). Keys: Enter = Start, X = A, C = B, Z = X, S = Y,
   arrows = stick, A / D = L / R triggers, T F G H = D-pad; or a gamepad.

A small launcher script for steps 1–2 would make this one command (next session). Not needed for the user's
real deployment later (the host fetches from GitHub), but that would publish translated game code: not before
the decomp replaces it (PLAN 7).
