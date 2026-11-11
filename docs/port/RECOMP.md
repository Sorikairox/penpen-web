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
| `port/recomp/recomp.py` | the translator: 1ST_READ.BIN + `functions.tsv` → one C function per guest function, `table.c` (address → function), `meta.tsv` |
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

Deliberate deviations: host IEEE round-to-nearest (FPSCR.RM round-to-zero and DN not modelled; wasm has no
rounding modes), no FPU exception flags; NaN results are the SH-4's own qNaN (0x7FBFFFFF / 0x7FF7FFFF_FFFFFFFF),
which also makes interpreter and recompiled C bit-identical; `fsca`/`fsrra` exact then rounded (hardware:
table approximations); `fmac` and `fipr`/`ftrv` without fused multiply-add (`-ffp-contract=off` everywhere,
or GCC fuses differently in each inlining context); cache ops no-ops; `pref` of a store-queue address calls
`sh4_io_pref` (no-op in the boot); `rte`, `trapa`, `sleep`, `ldtlb` stop the run (never reached).

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
functions, 131 392 instructions; `--ranges all`: 7 925 functions):

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
the stack at or above its entry value; calls and jumps that reach HLE or translated code go native. In the boot
the fallback ran 199 times / 5 674 instructions in 60 frames, at 8 entry points: 3 in the SDK range that are not
in the census (called through pointers or thunks: 0x8C09CD78, 0x8C09D4F2, 0x8C0A2544) and 5 entries into the
middle of functions (C-runtime helpers 0x8C06261E–0x8C062636, game 0x8C01A4FE).

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

## 6. Boot (S3 result)

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

The run stops at the frame limit (`--frames N`); in 60 frames the first screen's loop does not move on (not
yet investigated: likely waiting on stubbed state such as the timer, the pads or SDK maths results). S4
makes those real.
