# Pen Pen TriIcelon: SH-4 interpreter, recompiler scaffold, boot runtime (S3)

The local-only scaffold of PLAN 7.2: the game's code translated mechanically from SH-4 to C, running on our
own HLE of the SDK. It is the reference that the hand decomp (S10+) is tested against, and it exercises the
HLE layers (S4–S7) against the real game. Everything here runs in Docker (`tools/s3.sh`); rules in PLAN 9.

## 1. Pieces

| Path | What |
|---|---|
| `port/sh4/sh4_insns.def` | the instruction table: name, 16-bit pattern, kind (`OP` = data/ALU/FPU/system, `BR` = control transfer). Read by the C decoder (X-macro) **and** by the recompiler (regex), so both decode alike |
| `port/sh4/sh4_ops.h` | semantics of every `OP` instruction, `op_<NAME>(c, op, pc)`; the interpreter calls them with run-time values, the generated C with constants |
| `port/sh4/sh4_interp.c` | decoder (65 536-entry table, conflict check), `sh4_step`: branches and delay slots, SR/FPSCR bank switches |
| `port/sh4/tests/test_sh4.c` | 231 checks on hand-encoded vectors (our own encodings from the manual's tables, so the decoder table is checked too) |
| `port/recomp/recomp.py` | the translator: 1ST_READ.BIN + `functions.tsv` → one C function per guest function, `table.c` (address → function), `meta.tsv`. S4: `--ranges boot` (section 4) and extra entry points (`port/symbols/entries.tsv` + static discovery) |
| `port/symbols/entries.tsv` | S4: entry points outside the function table's starts (callbacks in data, multi-entry helpers, run-time finds), one C function each |
| `port/symbols/hle_extra.tsv` | S4: SDK functions outside the census that the runtime takes over because guest SDK code calls them (gdFsReqRd32, gdFsCancel) |
| `port/recomp/rt_gen.h` | what the generated C expects (`CALL_D`, `CALL_I`, `TAIL_D`, `BACK`, `rt_call`) |
| `port/recomp/diff_main.c`, `build_diff.sh`, `mutants.sh` | differential harness (section 5) |
| `port/recomp/check_gen.py` | grammar check of generated C before it leaves a container |
| `port/runtime/` | boot runtime: dispatch, I/O, disc + ISO9660, HLE stubs, `rtw_start`; native disc reader; `node/run.mjs` host stand-in |
| `port/gen/` | **gitignored**: the generated C of the game range, copied out by `tools/s3.sh export` for local reading/building. Never committed (user decision, S3); regenerate with `tools/s3.sh gen && tools/s3.sh export` |

`tools/s3.sh test | gen | export | diff | mutants | native | wasm` (header of the script for options). Generated
C, objects, the native binary, the wasm and the mutant builds live in the volume `penpen-build`; the emsdk image
`emscripten/emsdk:4.0.15-arm64` (gcc, clang/emcc, python3, node 22) is used for everything.

## 2. CPU model

`SH4` struct: R0–R15, the inactive bank (`rbank`, swapped when SR.MD·RB changes), T/S/Q/M as separate words,
the other SR bits, GBR VBR SSR SPC SGR DBR MACH MACL PR PC, FPU banks `fr` (current) and `xf` (other: swapped on
`frchg` / an FPSCR.FR change), FPSCR, FPUL. Doubles: DRn = FR[n] (upper) : FR[n+1]; with FPSCR.SZ = 1 the
`fmov` forms move pairs, an odd register field selects XDn, memory holds FR[n] at the lower address. The
modes are tested at run time in the op functions (the game never leaves FPSCR = 0x00040001; the C runtime's
double helpers set PR).

**Rounding (S4 decision): FPSCR.RM = 01 (round to zero) and DN (denormals are zero) are modelled**, because
the game and its SDK run in exactly that mode (0x00040001) and their float logic depends on it: with the
host's round-to-nearest the first logo's fade ended one frame later (frame 212 instead of 211), the title's
random picture and the attract demo's course differed. wasm has no rounding modes, so `sh4_ops.h` computes
each single result exactly (products of singles, conversions) or with its exact error (two-sum for add and
subtract) in double and rounds toward zero in software; a quotient or square root that lands exactly on a
single is exact (it cannot come within 2^-53 of one otherwise), so the double result rounded toward zero is
right. Doubles (the C runtime helpers run with PR = 1 and the same RM): two-sum, Dekker two-product, and the
residuals x − q·y and x − q² give the sign of the error. DN flushes denormal operands and results to signed
zero. RM = 00 still uses the host's nearest. 26 unit checks compare both modes on values whose roundings
differ.

Deliberate deviations: no FPU exception flags; NaN results are the SH-4's own qNaN (0x7FBFFFFF /
0x7FF7FFFF_FFFFFFFF), which also makes interpreter and recompiled C bit-identical; `fsca`/`fsrra` and
`fipr`/`ftrv` computed in double, then one rounding (hardware: approximations); `fmac` rounds the product,
then the sum (`-ffp-contract=off` everywhere, or GCC fuses differently in each inlining context); cache ops
no-ops; `pref` of a store-queue address calls `sh4_io_pref` (no-op in the boot); `rte`, `trapa`, `sleep`,
`ldtlb` stop the run (never reached).

## 3. Memory map

Main RAM 16 MiB at physical 0x0C000000, reached through P0/P1/P2/P3 and the 0x0D–0x0F mirrors (test:
`((a >> 26) & 7) == 3` and not P4), little-endian host loads. **Calls through the P2 alias** (0xAC0xxxxx, the
start-up and syCache, SDK.md 6) work because the dispatcher normalises every code address to P1
(`(a & 0x1FFFFFFF) | 0x80000000`). Everything else (Holly/PVR, AICA, Maple, P4 registers, store queues, VRAM)
goes to `sh4_io_*`: in the boot a register file that remembers writes and counts touches (14 addresses in 60
frames), in the harness the random model of section 5. Fixed guest areas used by the runtime: BIOS vectors at
0x8C0000B0–0x8C0000E0 point to fake entries 0x8C000F10–0x8C000F38 (HLE); HLE structures at 0x8C001000 (the BIOS
work area, unused without a BIOS: pad structures); IP.BIN at 0x8C008000; stack 0x8C00F400 downwards; program at
0x8C010000.

## 4. Translation and call model

Per function (extent from `functions.tsv`; game range 0x8C010000–0x8C066900 minus the SDK entries = 4 148
functions, 131 392 instructions; `--ranges all`: 7 925 functions). **S4: `--ranges boot`** (the default of
`tools/s3.sh gen`) translates every function of the image like `all`, but calls to the SDK entry points of
`sdk_calls.tsv` and `hle_extra.tsv` always go through `rt_call`, never direct, so the runtime decides per
entry point between its HLE and the SDK's own translated code (section 7). **Extra entry points**: code that
is entered other than at a table start gets a C function of its own, from the entry to the end of the
function containing it (or, in a gap of the table, to the next start): bsr/bra and literal-pool jsr/jmp
targets inside other bodies are found by the translator (90), the rest are listed in
`port/symbols/entries.tsv` (52: function pointers in data that point into gaps of the table, whose first
instruction is a prologue or a leaf; the SHC word-copy helper's 14 entries by count; a setter table; and
entries the run-time fallback log found). Boot translation: **8 067 functions, 526 422 labels, 161 with a
label switch**.

- **Reachability**: from the entry, following fall-through, both arms of conditional branches, and the return
  point after calls. Delay slots are not instructions of their own unless something branches to them.
  Functions with a computed jump (`braf`, or `jmp @Rn` whose Rn is not a literal) decode every halfword (47
  game functions: switch tables).
- **Labels and gotos**: every reachable instruction is a label `L_<addr>`; branches inside the function are
  `goto`s, out of it are tail calls (call, then `return`). Falling off the end is a tail call to the next
  address.
- **Delay slots, resolved at translate time**: the slot is emitted inline after the target, PR (calls) and T
  (`bt/s`, `bf/s`) are computed, before the transfer, as on hardware. A slot that is also a branch target is
  emitted a second time at its own label. A branch in a slot is an illegal-slot stop.
- **Calls**: `bsr` to a translated function is a direct C call; `jsr @Rn` whose Rn was loaded from a literal
  pool just before is a direct call guarded by a run-time check of the value (`if (t_ == K) f_K(c); else
  rt_call(c, t_)`); other `jsr`/`bsrf` call `rt_call`. After every call the caller checks that the callee
  returned to the expected address (`rts` sets `c->pc = PR`) and stops otherwise (`rt_badret`): never seen.
- **Computed jumps**: one shared `switch` per function over its labels (`D_`); the function entry and every
  backward branch go through it, so the only loop header is `D_` and the control flow stays reducible (clang's
  wasm backend ran out of memory on the irreducible form). Targets outside the function: `rt_call`.
- **Literal pools**: `mov.l/mov.w @(disp,PC)` read guest RAM at run time (the image is loaded from the disc), so
  the generated C has no data, only instruction words and addresses as constants (`check_gen.py` enforces a
  token grammar with no strings).
- **Back edges** decrement `c->budget` (`BACK(addr)`): the harness's step limit and the boot's watchdog for
  stuck polling loops (2·10⁸ back edges).

`rt_call(c, a)` (runtime): normalise → HLE table (SDK targets + BIOS vectors) → translated function table
(binary search) → **interpreter fallback**: interpret from `a` until an `rts` comes back to the caller's PR with
the stack at or above its entry value; calls and jumps that reach HLE or translated code go native. In the S3
boot the fallback ran 199 times / 5 674 instructions in 60 frames, at 8 entry points: 3 in the SDK range that
are not in the census (called through pointers or thunks: 0x8C09CD78, 0x8C09D4F2, 0x8C0A2544) and 5 entries
into the middle of functions (C-runtime helpers 0x8C06261E–0x8C062636, game 0x8C01A4FE). S4: the SDK ones are
translated (boot range), the others are extra entries; 0x8C01A4FE and 15 more found later were not mid-function
but **functions missing from the table** (gaps). The fallback stays as the safety net (and logs `rt interp`
once per new entry); the 6 000-frame boot-to-race run below uses it **0 times**.

## 5. Differential harness: what it covers

For each translated function and N seeds: a random CPU state (registers mixing pointers into free RAM and into
the image data, small and random values, random T/Q/M, MAC, floats in both banks; FPSCR = the game's value;
PR = a return marker) is run once in the interpreter and once as recompiled C (built with `-DRT_DIFF
-DSH4_MEM_HOOK`). Both use the same memory model: RAM reads come from the real image (literal pools and data
are real), everything else is pseudo-random per address, writes go to a per-run overlay and are compared byte
by byte; the code ranges are read-only (else a random pointer can overwrite code that only the interpreter
fetches). Every call leaving the function goes to one deterministic stub (clobbers r0–r7, MACL, T, FR0 as the
ABI allows) on both sides, so a function is tested alone; the call sequence (target + r4–r7, FR4, r15) is
compared. Compared at the end: all registers and banks, SR parts, FPU state, the final PC (return / tail-call
target), the bytes written, the calls, and how the run ended (return, illegal instruction at the same address,
...). Runs where the interpreter exceeds the step limit are skipped; a function with no completed run is
"untested".

What it does **not** cover: the op semantics themselves (shared by both sides: that is the unit tests' job),
FPU modes other than the game's, interrupts/exceptions, and inter-function behaviour (the boot run is the test
of that).

Results (S3):

| Set | Functions | Pass | Fail | Untested (all runs hit the step limit) |
|---|---|---|---|---|
| game range, 12 trials / 20 000 steps | 4 148 | 4 131 | 0 | 17 |
| game range, 40 trials / 100 000 steps | 4 148 | 4 134 | 0 | 14 |
| whole image (`--ranges all`), 12 / 20 000 | 7 925 | 7 546 | 0 | 379 |
| S4 boot translation (`--ranges boot` + entries), 12 / 20 000, RZ/DN semantics | 8 067 | 7 686 | 0 | 381 |

The 46 SDK functions the boot runs as guest code (section 7) all pass.

**Mutants** (`tools/s3.sh mutants`; each translates only the functions it touches): the harness must flag them.

| # | Mutation | Sites | Functions flagged |
|---|---|---|---|
| 1 | `bt`/`bf` condition inverted | 4 643 | 739 of 763 (17 untested, 7 with no observable path) |
| 2 | `bt/s`/`bf/s` test T after a slot that writes T | 0 in game; 30 in the whole image | 5 of 27 (22 untested) |
| 3 | `bra` delay slot dropped | 3 599 | 349 of 581 (most others: `nop` slots) |
| 4 | PC-relative `mov.l` literal off by 4 | 8 691 | 2 113 of 2 184 |
| 5 | call return address PC+2 | 8 956 | 1 284 of 1 398 |
| 6 | `jsr`/`jmp` register read after a slot that writes it | 1 in game; 2 in the image | 1 of 1; 2 of 2 |
| 7 | `rts` PR read after a slot that writes PR | 0 (no such slot anywhere) | — |

Bugs the harness found in S3: the interpreter side kept running past a function's end after a call (harness
fix); `jmp @Rn` to a literal inside the same function was emitted as an external call (recompiler fix: now a
local jump); FMA contraction and NaN payloads differing between the two builds (flags + qNaN, section 2);
random stores overwriting code (code made read-only in the model).

## 6. Boot (S3 result; S4: section 7)

`rtw_start` (port/runtime/main.c): TOC → ISO9660 root (PVD at LBA 45016; 194 files) → IP.BIN (16 sectors at
LBA 45000) to 0x8C008000 → 1ST_READ.BIN to 0x8C010000 → CPU as the BIOS leaves it (SR 0x600000F0, r15 = VBR =
0x8C00F400, FPSCR 0x00040001) → `rt_call(0x8C010000)`. The disc interface is retrowser's
(`rtw_disc_toc`/`rtw_disc_read`, spec 4.10): `disc_native.c` implements it over the Redump cue/bin set for the
native build, `node/run.mjs` for the wasm in node; in the browser the host provides it.

HLE stubs (`port/runtime/hle.c`, names from `sdk_calls.tsv` via `tools/s3_hle_names.py`): every call is counted
and the first ones logged with r4–r7 and the caller; real behaviour only for syMallocInit/syMalloc/syFree
(first fit, 32-byte aligned, host-side block list), gdFsOpen/GetFileSize/GetFileSctSize/Read/Close (on the
ISO9660 reader; handles are tokens), gdFsGetDrvStat (ready), gdFsChangeDir, pdGetPeripheral (zeroed per-port
structures), njSetVSyncFunction/njSetEORFunction + njWaitVSync (frame count, calls the game's V-sync callback,
stops at the frame limit), syTmrGetCount (advancing), syChainAddHandler (handles), syCfgGetSoundMode, syBtExit
(stop). All others return 0.

Reached, identical on native and on **wasm32 in node** (emcc standalone, 6.9 MB, imports only `env.rtw_*`), in
order of first call: mw_PreInit, syCfgInit, syBtCfg_ReadFlash, bios_sysinfo (region check), **sbInitSystem**:
syHwInit, syMallocInit, nj_SetWorkSize, njInitSystem, syHwInit2, pdInitPeripheral, gdFsInit; syChainAddHandler,
gdFsEntryErrFuncAll; graphics set-up (syMalloc, njInitTextureBuffer, njInitVertexBuffer, njInitMatrix,
njInit3D, njInitShape, njInitTexture, njCreateLight, njTextureFilterMode, njControl3D, njSetConstantAttr,
njSetCheapShadowMode, nj_InitCamera, njInitView, njSetAspect); sound (sdLibInit, the sound driver file read
through gdFs, sdMemBlk*, sdDrvInit, port closes); a container file and two PVM files loaded
(njLoadTexturePvmMemory); pdGetPeripheral on the 4 ports, buInit, pdVmsLcdIsReady ×4, njSetVSyncFunction,
njSetEORFunction; a third PVM; then the **per-frame loop** of the first screen: gdFsGetDrvStat, sdSysServer,
camera and screen (njTan, njSetScreen, nj_SetCamera, njClip2D, njFogDisable), the light setters of all
families, njGetMatrix/njInvertMatrix, njSetConstantMaterial, **njDrawSprite2D**, gdFs status polls,
**njWaitVSync** → V-sync callback (njSetBackColor, njSetBorderColor, sdSysServer). 60 frames: 3 022 HLE calls
to 83 functions, 5 files read.

The run stops at the frame limit (`--frames N`); in 60 frames the first screen's loop does not move on. S4
found why (section 7): 60 frames are shorter than the first logo.

## 7. S4: HLE vs guest SDK code, the frame loop, the deterministic test mode

**Why S3's first screen "never advanced".** It did not stall: the S3 run stopped at its 60-frame limit, and
the first screen (a logo: alpha += a constant per frame up to 1.0, a 90-frame hold, the fade out, then the
next logo file) needs 211 frames. Run longer with the S3 stubs, the boot went through two logos and the
title and faulted at frame 2 230 (a call to 0x8036A290, right after the stubbed `njRandom`, whose float
result the game's random helper scales into its state): the SDK maths stubs returning nothing were the real
problem, the timer, pads and gdFs status were not.

**Guest SDK code.** `hle.c` keeps one table of the 231 SDK entry points (`sdk_calls.tsv` + `hle_extra.tsv` +
the BIOS vectors). 46 of them are pure code and run as the SDK's own translated code (`guest[]`): the heap
(`syMallocInit`/`syMalloc`/`syFree`: 32-byte units, a 32-byte header, blocks cut from the end of the first free
block that fits, NULL when none fits, coalescing; the log reads its free list), the Ninja matrix stack and
vector maths (`njInitMatrix`, push/pop, rotate/translate/scale, `njInvertMatrix`, `njCalcPoint`/`Vector`,
inner/outer products, `njUnitVector`, `njSin`/`njCos`/`njTan` (`fsca`), `njSqrt`, `njRandom`), camera/view/
screen/aspect, `nj_ReadChunkFile`, `nj_MemCopy4`, two empty functions. They pass the differential harness; the
S5 renderer will read the matrices they maintain. Everything else is HLE (`impl[]`) or a logging stub.

**Frame loop** (in `njWaitVSync`, one V-blank per call, no wall time): end of render (the `njSetEORFunction`
callback, then the syChain 0x920 handlers) → per-frame log line → V-blank: TMU time base, completion of
asynchronous drive requests, pad structures from the script, syChain 0x900 handlers (Kamui's V-blank
callback slot; the game's handler at 0x8C061DF0), then the `njSetVSyncFunction` callback (which counts
V-blanks for the game's frame skip). Chains are kept host-side, sorted by priority (`syChainAddHandler`
returns an id, `syChainDeleteHandler` removes it); other event codes are logged as not fired.

- **Timer**: `syHwInit` programs TMU0 with TCR0 = 2 (P-clock 50 MHz / 64 = 781 250 Hz), counting down from
  0xFFFFFFFF; `syTmrGetCount` = ~TCNT0. HLE: count = ⌊frame · 781 250 · 1001 / 60 000⌋ (59.94 Hz) + one tick
  per read within the frame (capped below the next frame); reads of TCNT0 by guest code see the same clock.
- **gdFs**: handles are guest structures in the work area of `gdFsInit`, with the SDK's layout (+8 first LBA,
  +0x10 sectors, +0x14 position, +0x18 transferred, +0x4C open, +0x50 status 0 idle/1 complete/2 reading/4
  busy/5 error), so the SDK's `gdFsTell` (guest) and CRI read them. `gdFsRead` synchronous (clamped to the file,
  0 or −10), `gdFsReqRd32` asynchronous (−14 unaligned; completes at the next V-blank or the 8th status poll in
  the frame), `gdFsCancel`, `gdFsGetStat` (−10 bad handle), `gdFs_Api34` (driver slot 0x34: the drive handle)
  and `gdFs_Api58` (slot 0x58: drive-status request, the drive handle busy until the next frame: the game's
  10-frame drive watch, 0x8C012DC4), `gdFsGetDrvStat` (1 PAUSE, 3 PLAY while CDDA plays: the game waits for
  "not 3" after pausing), `gdFsChangeDir` (root only), CDDA play/pause/stop/release logged with the frame.
  Opened files are logged as directory index + 4-digit group (PLAN 2) + size, not by name.
- **Pads**: `pdGetPeripheral(port)` returns PDS_PERIPHERAL structures in the BIOS work area (on/off/press/
  release, r/l, sticks, info → {type}); port A holds a standard controller (support 0x000F06FE, which the
  game's reader 0x8C01087C requires: analog X and Y), the others are empty. The game reads +0x30 info type
  (bit 0 controller, bit 7 skipped), +4 support, +8 on, +0x1C/+0x1E sticks (thresholded at ±64).

**Test mode** (native argv; the wasm exports `rt_set_*`, `rt_arg_buf`/`rt_set_pad`, passed by
`node/run.mjs`): `--frames N`, `--pad "F[+N]:BTN|BTN,..."` (port A holds the buttons for frames F..F+N−1,
N = 4 by default; A B C X Y Z D START UP DOWN LEFT RIGHT LT RT), `--digest N` (64-bit digest of the 16 MiB of
guest RAM every N frames), `--frame-log 0|1`, `--watch A,B` (guest words on the frame line), `--verbose N`.
Per frame: `frame F hle H guest G interp I files N heap FREE/BLOCKS pad BITS ram DIGEST`. `hle new` lines
give each SDK function's first frame; `hle screen` lines mark a new set of SDK functions per frame held for
8 frames (the busiest listed): with no renderer yet, screens are told apart by their call pattern and files.

**Results (S4)**, native and wasm32 (node) **identical frame by frame, RAM digests included** (6 000 frames of
the race script below; 1 600 frames of a menu script), deterministic run to run:

| Frames | What (no input) |
|---|---|
| 0 | sound driver (group 9900), container 2000, PVMs 2001/2002, logo 7999: first logo |
| 211 | logo 8000 |
| 427–429 | a picture 80xx (random: timer + float), title data 3000–3280, CDDA track 5 |
| 2 228 | no input for 30 s: attract demo (picture, game container 5000, a course and characters 5xxx; CDDA 11, 12, 13) |
| 5 828 | back to the title |

With `--pad 600:START,700:A,1100:START,1200:A,1500:A,1700:A,1760:A`: title → Start (607: the menu) → A
(706: a screen of 167 sprites) → Start (1 102: a picture 80xx, **character select** 5100–5111, CDDA 8) → A
(1 203) → A (1 563: next select screen, CDDA 9) → A, A (1 762: **a race loads**: 19 files, game container 5000,
course 58xx/5300/54xx, characters 52xx; CDDA 11 at 1 854, race music CDDA 12 at 2 244) → racing at 6 000
frames. 42 files opened, 0 interpreter-fallback calls; 150 SDK functions used (39 of them as guest code), 9.5 M SDK calls, 0.95 M of them HLE. Speed: about 300 frames/s
native and in node (wasm 45 MB: the whole image is translated now; S5+ can translate only what the guest SDK
functions reach).

## 8. S6: resumable frames

The generated C is resumable so the retrowser host can step it (`rtw_frame`): every call statement is a site
`R_n`; after a call `RT_UNW(n)` saves (function, n, target) and returns when `rt_unw` is set; on entry with
`rt_rew` set the function pops its site and jumps back into that call. Functions with sites and back edges are
emitted in loop mode (entry and back edges through `D_`), keeping the control flow reducible; the resume switch
is separate from the label switch. The interpreter fallback saves and restores its own state the same way. The
harness builds the same C with `rt_unw` always 0. Design, checks and numbers: `docs/port/RTW.md` 2.

## 9. S8: the translation trimmed to the code that can run

`recomp.py --trim port/runtime/hle.c` (the default of `tools/s3.sh gen` since S8; `NOTRIM=1` translates everything)
keeps only functions that can execute. Roots: every function of the game range (game + SHC runtime), the entries,
the SDK functions the runtime runs as guest code (`hle.c` `guest[]`, 53 found by name), and every function whose
start address is stored as a word in the data ranges (outside the three code ranges: callbacks and function
tables, conservatively). Edges: `bsr`/`bra` targets and literal-pool constants that are function starts. An SDK
entry point the runtime takes over (HLE) is never entered, so it is not translated and its callees are not
followed through it. Anything dropped that still runs goes through `rt_call` to the interpreter fallback (and
`rt interp` says so).

Result: **5 943 of 8 068 functions kept** (2 125 dropped, 172 of them HLE targets; 306 836 of 526 470 labels), the
wasm **47.1 MB → 24.6 MB**. The trimmed and the full wasm give **the same frame lines and RAM digests** over 6 000
frames of the S4 route and 8 000 frames of the no-input boot (attract demo), both with **0 interpreter calls**.
The differential harness runs on the trimmed translation by default (`GEN=genfull` after
`NOTRIM=1 tools/s3.sh gen --out /build/genfull` for every function).
