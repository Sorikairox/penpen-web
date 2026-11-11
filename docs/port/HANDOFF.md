# HANDOFF

Newest session first. Read `docs/port/PLAN.md` (disc facts, survey, approach, phases, rules) before working.

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
