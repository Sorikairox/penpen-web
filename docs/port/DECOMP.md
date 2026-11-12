# Pen Pen TriIcelon: hand-written C replacing the translation (S11, S12, S13)

PLAN 7.2's end state is a build of hand-written C only. S11 (started before S9/S10 at the user's request) sets up the
method and does a first region. The hand-written C lives in `src/` (committed); the machine translation stays the
gitignored, local-only `port/gen/` and the volume `penpen-build` (PLAN 9). Hand-written C is not a renamed
translation: it is written from the function's behaviour (Ghidra's pseudo-C and the disassembly, read in the terminal
only, PLAN 9), with types, structs, loops and comments.

## 1. Status

| | S11 | S12 | S13 |
|---|---|---|---|
| Game functions replaced (`docs/progress/decomp_functions.tsv`) | 47 (section 9) | 70 (+23) (section 11) | 2 468 (+2 398: ~2 000 identical copies, section 13) |
| Region | the game's core: main loop, start-up, callbacks, drive watch, task tree, Game modes, pads, views; the sound front end | + the GD-ROM file loader, the data-container fix-ups (Ninja models, motions, texture lists), CDDA pause and play, the title scene's constructor and destructor | + float helpers; the vector / Body / Ninja / accessor inline methods, the title scene, 231 delegated leaves (engine, save, VMU, race scene, racers, course, camera, actors) |
| Routes (`tools/routes.tsv`) | boot 8 000, route 6 000 | + explore 7 000, vs2 4 000, vs4 4 000, demo 3 400, reset 3 600 (section 5.1), inputs 1 000 (section 10) | the same 8 |
| Differential test (`tools/s11.sh test`) | every export but main passes, 0 fail | 8 routes, 5 792 samples: **69 of 70 pass, 0 FAIL** (main: never returns, whole-run comparison only) | 8 routes: 0 FAIL; every export passes or is a copy of a passing one |
| Whole-run comparison (`tools/s11.sh compare`) | route + boot: every frame line identical, native and wasm | all 8 routes: every frame line identical (RAM digest every frame), native and wasm; native = wasm | all 8 routes identical, native and wasm; native = wasm |
| Mutation score (section 10) | 63.6 % (upper bound: see section 10) | Mull: **90.4 %** (776 mutants: 672 killed, 33 equivalent, 71 unreached, 0 survived; reach score 100 %) | fpu.h 118/118; lead code 355 mutants: 90.9 % (reach 100 %); delegated code not yet |
| Functions / bytes | 47 of 4 010 (1.17 %), 4 728 of 301 130 bytes (1.57 %) | **70 of 4 010 (1.75 %), 7 474 of 301 130 bytes (2.48 %)** | **2 468 of 4 010 (61.55 %), 57 508 of 301 130 bytes (19.10 %)** |

## 2. How a hand-written function replaces the translated one (link-time selection)

- A hand-written function is exported with `HW_EXPORT(8c0102c0, task_add_children(task_at(ARG0), task_at(ARG1)))`
  at the start of a line (`src/include/hw.h`). The macro defines the C symbol `f_8c0102c0(SH4 *c)`: the name the
  translated code calls (direct calls `CALL_D` and the address table `rt_fn_table`).
- `recomp.py --hand src/` (S11) finds every `HW_EXPORT...(<addr>,` line, checks the address is a translated
  function, and **leaves its body out** of `g_*.c`; `funcs.h` still declares `f_<addr>` and `table.c` still lists it
  (flag `hand` = 1), so the linker takes the hand-written definition. Its extent is written to `hand.tsv`; `meta.tsv`
  (the random harness's list) leaves it out. Analysis (reachability, trimming, extra entries) is unchanged.
- `build.sh` compiles `src/**/*.c` (with `-Wextra`) only when the translation it builds has a non-empty `hand.tsv`.
  Two translations live side by side: `/build/gen` (pure: the reference) and `/build/genhand` (`HAND=1 GEN=genhand
  tools/s3.sh gen`, or `tools/s11.sh gen`); `GEN=genhand` makes `tools/s3.sh native|wasm` build `/build/native-genhand`
  / `/build/wasm-genhand`, and `tools/rtw.sh build` the module from it.
- A mid-function entry point into a replaced function (`entries.tsv`, static finds) keeps its own translated copy;
  avoid replacing such functions (none in S11).

## 3. Writing a function

**Guest memory.** All game state stays in guest RAM with the original layout, because translated code still reads and
writes it. `gaddr` is a 32-bit guest address; `hw_ptr(a)` gives a host pointer (main RAM only, mirrors folded);
`hw_addr(p)` goes back. Structs are declared with the original's offsets and checked with `HW_OFFSET` / `HW_SIZE`;
pointer fields are `gaddr` (the host may be 64-bit). Globals at fixed addresses are named macros over `GUEST_VAR(type,
addr)` (`g_game`, `g_vblanks_pending`, ...). Hardware (non-RAM) addresses go through `hw_read32`/`hw_write32`, which
use the runtime's I/O model; so does a store through a pointer that may be 0 when the original wrote through it
anyway (`hw_write32(link, ...)` in task_update_tree). A NULL object passed to a readable function maps to RAM offset 0
instead of the original's hardware space: only on paths the original could not survive either (a failed `syMalloc`),
noted where it applies.

**Calls.** Code still translated, the SDK (translated guest code or HLE) and C++ virtual methods are called with
`hw_call(addr, nargs, a0..a3)` / `CALL0..CALL4`: arguments in r4-r7, PR = a marker address (0x8C000F08) that the
callee must return to (checked), result in r0. `nargs` is the callee's real argument count (the difftest compares
exactly those). A callee with more than 4 arguments: `CALL5` / `hw_call_stack` (the rest on the guest stack, as SHC
passes them); a hand-written function with more than 4 reads them with `ARG4`..`ARG6` (@r15 at entry). A local whose
address is handed to guest code (an SDK function writes a handle or a value through it) lives on the guest stack:
`hw_stack_alloc(bytes)` / `hw_stack_free(bytes)`. The SHC runtime helpers with their own register conventions
(division 0x8C062464: r1 / r0; block copy 0x8C0625E0: r0 bytes from r2 to r1) are written as C (`/`, `memcpy`). SDK functions have named wrappers in `src/include/sdk.h`; still-translated game functions get a
`static` wrapper named for what they do, or `func_8C0xxxxx` when unknown. Hand-written functions call each other as
plain C. Floats: the game runs the FPU in round-to-zero / denormals-zero (RECOMP.md 2); host float arithmetic rounds to
nearest, so hand-written code must not compute floats yet (S11's region has none: the pad reader's float compares of
16-bit stick values are exact as integer compares; stored float constants are written as their bit patterns). A later
session adds RZ helpers (from `sh4_ops.h`) before taking float code.

**Naming.** Files by subsystem: `src/core/` (engine: tasks, pads, views), `src/game/` (main loop, Game modes),
`src/include/` (hw.h, sdk.h). Functions `module_verb_object` in snake_case; structs CamelCase (`Task`, `TaskVtbl`,
`TaskTree`, `Game`, `Pad`, `View`, `PdPeripheral`); globals `g_name`; guest addresses of data `UPPER_CASE` macros.
Every hand-written function says what it was: `/* was func_8C0102C0: ... */` above the definition (`as
func_8C0xxxxx` for a C helper that mirrors a function it does not replace). Identical copies in the binary (C++
inline/thunk duplicates) are exported once each from the same C function.

## 4. Resumable functions (functions a frame can end inside)

The game runs its own loop and waits in `njWaitVSync`; the runtime ends a frame there by unwinding the C call chain
and rewinds into it on the next frame (RECOMP.md 8). The translated code keeps no C locals across calls; hand-written
code does, so:

- `rt_summary` lists the **yield-chain functions** (every function that ever saved an unwind site) after a run
  (`rt yield-chain ...`): in S11's runs main (0x8C0164B4), game_frame, tasktree_update, task_update_tree, game_update,
  the title's mode start and scene code below it, and the start-up code above main.
- A function on such a chain is exported with `HW_EXPORT_RESUMABLE` and written with `HW_RESUMABLE(addr, locals...)`
  (the locals live across a yielding call, restored on a rewind), `HW_RESUME_AT(sites...)` (jumps back to the call that
  yielded), and `HW_YIELDING(n, statement)` around each call that can yield. On the way out the locals go into the
  runtime's unwind record (`rt_unw_save_blob`, up to 64 bytes); the adapter saves the entry PR the same way. On the way
  back in, the statement runs again (the callee rewinds too); its expressions may only use the saved locals and what is
  computed from them before `HW_RESUME_AT`. `hw_call` checks that a rewind repeats the call that yielded.
- A function not written resumable stops the run with a message if a call in it yields (`hw ...: not written
  resumable`); a call made while the frame unwinds stops it too (an unmarked yielding call). Neither happened in S11.

## 5. The differential test: in the real run, from real states (`tools/s11.sh test`, `port/runtime/difftest.c`)

The S3 harness feeds random states to translated code; hand-written C dereferences guest pointers as structs, so it is
tested on the states the game really produces instead, inside the native runtime (`--hwtest BUDGET[:SPREAD[:ADDR]]`):

1. a **sampled call** (the first 8 calls of each function, every power of two, then one per 50 frames, 160 samples
   per function) copies the CPU state and all 16 MiB of guest RAM;
2. the call runs **live** (the run continues with its result) while the results of its HLE calls are recorded, with
   the argument registers each HLE function actually read (the HLE's `R(i)` marks them), the words each HLE call
   wrote into the live stack above r15 (its out-parameters: `gdFsGetFileSize(&size)`, `syCfgGetSoundMode(&mode)`;
   kept as offsets from r15 at the call), and the values of its hardware reads; a call that yields is followed across
   frames until it returns (its sample id travels in the unwind record; given up after 600 frames);
3. **side A** runs the **original code in the SH-4 interpreter** from the snapshot; **side B** runs the
   **hand-written function** from the snapshot (still-translated callees run as translated code); both in a sandbox:
   HLE calls are replayed in order from the live record (target checked; a mismatch falls back to fixed values and is
   counted as `replay-diverged`), with their stack out-parameters written at the side's own r15 + offset (each side
   has its own stack depth; the code still translated keeps its frame layout, and a hand-written caller's
   `hw_stack_alloc` slot sits at the same offset as the original's, checked by the test), hardware reads are
   replayed, hardware writes are logged, nothing is counted;
4. compared: how both ended; r8-r15, GBR, FPSCR, FR12-FR15 (callee-saved; MACH/MACL are caller-saved in this
   compiler: func_8C010DDE uses `mul.l` without saving them); PC and PR on return; r0 when the function returns a
   value (`RET`); every HLE call (target, and the arguments the callee takes: the count given to `hw_call`, else the
   registers the live HLE read); the hardware writes; **all of guest RAM** except the dead stack below the entry r15.
   Words pointing into the stack area count as one token (section 6).

Functions reached only from hand-written C (no adapter call to sample) are tested **inside** their callers' samples:
side A records the hand-written functions whose original code it entered; each one is counted under `inside` when the
sample passes. This relies on the hand-written caller calling the same hand-written functions as the original (true
in S11: where the original inlines code that the C reuses as a helper, e.g. Game's constructor and `task_init`, the
helper is not exported). `game_main` is never sampled (it never returns): it calls its three stages (static constructors,
game_init, game_frame) through their entry points, as the original does, so each is sampled there.

Results (end of S11, `port_work/s11/test_{boot,route}.log`): boot 8 000 frames: 991 samples, route 6 000 frames:
1 046 samples; **every export passes in at least one of the two runs** (own samples or inside others), **0
failures**; main is not sampled (section 5 above). Replay divergences: only the title's mode start (1 of 1 sample in
the boot, 3 of 3 in the route: an HLE call sequence that differs from the live one during the title's loading;
both sides still agree). **Mutants** (one deliberate change each in the hand-written code): the difftest flags 4 of 12
(missing njInvertMatrix in view_select, mode_frames starting at 1, a child's owner set to its parent, njSetBorderColor
argument); the whole-run comparison flags "pressed = held" at frame 601 (no sample fell on frames 601-603); the other
7 change paths the two runs never take (stick input, task deactivation (twice), the demo's Start, the catch-up cap,
inactive-task drawing, split-screen view index): coverage is what the boot and the route exercise.

What it does not cover: paths the runs do not take (single view, no pause, no stick, no task deactivation, modes 1, 4,
5, 6, 9, 11, 12 not visited: their start routines stay translated); HLE writes outside the live stack (file data read
into a buffer, structures filled in the heap: both sides see the snapshot's bytes, the same on both sides but not the
live ones); stack arguments of HLE calls (they sit below the sampled function's entry r15, outside the comparison);
the browser build (the difftest is native).

### 5.1 S12: more routes, HLE memory replayed, a merged verdict

- **Routes** (`tools/routes.tsv`; every gate runs all of them): `boot` and `route` (S11), and five S12 routes driven
  with an extended pad script (`hle_io.c`: `F[+N][@P]:BTN|...`, port P = B-D; a port the script names holds a
  controller for the whole run): **explore** (the stick in the character select and the race, the pause menu twice,
  back to PenPen House, DRESS UP, RESULTS, back to the title, TIME TRIAL and its race), **vs2** (VS ICELON with two
  controllers: two-player race, split mode 2 = two views + panel, player 2 pauses), **vs4** (four controllers: split
  mode 3 = four views + panel), **demo** (Start during the attract demo: back to the title), **reset** (the soft reset
  A+B+X+Y+Start on controller B during a race). Found while writing them: `tools/s3.sh` put the run arguments into a
  `sh -c` string unquoted, so a pad script with `|` was cut into a pipe (the S5 screenshots of such scripts were
  wrong); the arguments are quoted now (`qargs`), and a failed build says so (`build FAILED`) instead of printing
  nothing.
- **HLE memory replay**: the S11 difftest replayed HLE results, stack out-parameters and hardware reads, but not what an
  HLE call writes into guest RAM elsewhere: both sandbox sides saw the snapshot's bytes where the live run had the
  file `gdFsRead` had just read. A loader that then works on the data (the container fix-ups, section 11) was compared
  on stale bytes: its first samples passed with every fix-up function "untested" (the table looked empty). Now the HLE
  reports what it wrote (`hwt_hle_wrote`, called by the GD-ROM read completion), the difftest keeps those bytes (up
  to 12 MiB per sample) and writes them into each side's RAM when the call is replayed. With it the fix-ups are
  tested inside the loader's samples. Other HLE writes into guest RAM (the heap's own bookkeeping, VMU data) are still
  not replayed: both sides keep the snapshot's bytes there.
- **Merged verdict** (`tools/hwtest_merge.py`, printed by `tools/s11.sh test`): a function passes if it passes in at
  least one route (own samples or inside a passing sample), FAILs if any sample failed anywhere.

## 6. Whole-run comparison (`tools/s11.sh compare [native|wasm]`)

The pure and the hand build run every route of `tools/routes.tsv` (S11: the S4 route and the boot) with a RAM digest
every frame and `--digest-nostack 1`; the frame lines (SDK call counts, guest-SDK calls, interpreter calls, files,
heap, pads, digest) must be identical. **Why `--digest-nostack`**: hand-written functions keep no frames on the guest
stack (saved registers, locals), so the stack area holds different bytes, and code still translated, called from
hand-written code, runs at a different stack depth. The first S11 comparison showed it: an SDK variable at
0x8C1823F0 held a pointer to a translated caller's stack struct (0x8C00F31C pure, 0x8C00F390 hand) during frames
427-428, same data behind it. The digest therefore leaves out the stack area 0x8C00C000-0x8C00F3FF (the stack grows
down from the BIOS's 0x8C00F400; the deepest r15 seen at an `rt_call` is 0x8C00F188) and counts every word whose
value points into that area as one token. Without the option, digests are the same as before S11.

## 7. Workflow (adding functions)

1. Pick functions: `tools/ghidra.sh decomp ADDR...` and `asm ADDR:LEN` (terminal only); check the yield-chain list
   (section 4) and whether the runs call them at all (else they cannot be tested: leave them translated).
2. Write them in `src/<subsystem>/`, structs in the subsystem's header with layout checks; export with `HW_EXPORT*`.
3. `tools/s11.sh gen` (both translations), `tools/s11.sh test` (difftest over every route: 0 FAIL, every export pass
   or inside), `tools/s11.sh compare native` and `wasm` (identical frame lines), then `tools/s3.sh test`,
   `tools/s7.sh test`, `tools/s3.sh diff` (pure translation unchanged), `GEN=genhand tools/rtw.sh build` +
   `tools/rtw.sh check`.
3b. (S12) `SESSION=Snn tools/s11.sh mutate` (section 10): every new function goes through the mutation run; classify
   its survivors in `docs/progress/mutation_notes.tsv` (or add a route that kills them), `tools/s11.sh report`.
4. Un-export anything the runs do not reach (keep it as a C helper only if hand-written code uses it, commented "as
   func_...: stays translated").
5. `tools/s11.sh progress SESSION` writes `docs/progress/decomp.tsv` and `decomp_functions.tsv`.

## 8. Progress metric

`tools/decomp_progress.py`: functions and bytes of the **game region** (`functions.tsv` region `game`: 4 010 functions,
301 130 bytes; the SDK and the SHC runtime are not counted: they are HLE or SDK code) replaced by an `HW_EXPORT`,
per session in `docs/progress/decomp.tsv`, the list in `docs/progress/decomp_functions.tsv` (address, size, C name,
file, plain/resumable/main).

## 9. S11's region and structs

- `src/core/task.{h,c}` — **Task** (0x24 bytes: linger countdown, active, stops_on_pause, paused, child, next, parent,
  owner, vtable), **TaskVtbl** (SHC C++ vtable: {function, this-adjustment} pairs: destroy, update, draw), **TaskTree**
  (root task + update passes per frame). The game is C++; every on-screen object derives from Task. Update pass (with
  the cascade that removes deactivated tasks), draw pass, destroy, add children, root attach.
- `src/core/pad.{h,c}` — the per-port button words (held / last / pressed / released, the stick as a d-pad past 64),
  **Pad** (a port number), the SDK's **PdPeripheral** fields the game reads. Note the original's quirk: a controller
  without both analog axes stops the scan of the following ports.
- `src/core/view.{h,c}` — **View** (0x18C bytes, layout + camera data still handled by translated code), split modes
  (1, 2, 2 + panel, 4 views; 4 views are drawn in the order 0, 2, 1, 3), view selection.
- `src/game/game.{h,c}` — **Game** (0x48: TaskTree + Pad + mode, next_mode, mode_arg, mode_frames, reset_exits, root
  task at 0x24 with Game's vtable adjusting `this` by -0x24): the mode state machine (13 modes, each a scene object
  created by a start routine; the demo's 3 600-frame time-out), the soft reset (A+B+X+Y + Start on any controller:
  to the title, or out of the game from the title/menus), the sound driver's frame call.
- `src/game/main.c` — main, start-up (game_init, video_init, static constructors: the table holds pairs and only the
  first word of each is called), game_frame (update passes = V-blanks since the last frame, up to 5; drawing per view;
  drive watch; exit request), the V-sync and end-of-render callbacks, the GD-ROM drive watch (lid open / no disc /
  error leave the game).

- `src/game/sound.{h,c}` — the sound front end between the game and the Manatee driver: per-frame requests for 8
  one-shot ports (priority, sound, volume, pan, pitch; the highest priority of the frame wins, priority 0 always
  replaces; an adjust request without a sound) and 8 MIDI ports, sent once per frame by `sound_update` (volume, pan,
  pitch only when changed); the CDDA music (a requested track fades the current one out over N frames, the CDDA
  volume follows the drive: up while it reports PLAY); start-up (driver from the disc, 8 + 8 ports), bank download,
  sound-set switch (stereo/mono from the console's settings).

## 10. Mutation testing (S12: Mull, `tools/mull_run.py`, `tools/mull_gate.py`, `tools/s11.sh mutate`)

The user's rule for S12: mutation testing is a gate, not an experiment. A **mutant** is one small change at one place
of `src/`; it is **killed** when a gate sees a difference. S11 had planted 12 mutants by hand (4 flagged by the
difftest, 1 by the digest, 7 on paths no run took); S12 makes it systematic and re-runnable.

**Mull** (S12, at the user's request; [mull-project/mull](https://github.com/mull-project/mull) 0.37.0, LLVM 18.1.3).
Mull is a clang pass plugin: every mutant is compiled into **one** build, each guarded by a check of an environment
variable named after it; `mull-runner` runs the test program once per mutant with that variable set. No rebuild per
mutant. Mull and its matching clang live only in the image `penpen-mull:1` (`docker/mull/Dockerfile`: Ubuntu 24.04,
clang-18 from Ubuntu, the two Mull `.deb` files from the GitHub release, SHA-256 printed at build; the network is used
only by `docker build`); runs use the usual sandbox (`tools/s3.sh mull`: `--network none --read-only --cap-drop ALL
--security-opt no-new-privileges`).

- **Build** (`tools/mull_run.py`): `src/` (only the hand-written C) compiled with `clang-18 -fpass-plugin=
  /usr/lib/mull-ir-frontend-18 -g -grecord-command-line` and the build's flags (-O2 ...); `mull.yml` limits mutation
  to `src/` and excludes `src/include/hw.h` (the runtime interface); linked with gcc against the hand build's other
  objects (translation and runtime, unmutated, compiled by gcc in the emsdk image). The native build is used: native
  and wasm are proven identical (section 6).
- **Gates stay ours** (`tools/mull_gate.py`, Mull's `--test-program`): it reads which mutant Mull switched on, takes
  the routes whose gcov coverage (computed once, `src/` built with `--coverage`, every route with the difftest on)
  includes the mutant's line, cheapest first, and runs each with the difftest (`--hwtest 160`), the frame lines with
  the RAM digest every 16 frames and the audio lines compared to the pure build's, the run's end, and stall
  detection (no output for 60 s); exit 1 at the first failure = killed. A mutant on a line no route runs exits 0 at
  once: Mull reports it survived, the report classes it **unreached** from the coverage. Mull's own baseline run (no
  mutant) is our control: every route must pass, else Mull stops.
- **Operators**: Mull's `cxx_all` group as it applies to this C: `cxx_all` plus `experimental` (`negate_mutator`, `cxx_remove_negation`): arithmetic and compound assignments, comparisons and boundaries, bitwise operators, ++/--, void calls removed, scalar calls replaced by 0 / 42, constants in assignments and initialisers changed (a constant's lowest bit flipped, anything else 42), conditions negated. 776 mutants on the 70 hand-written functions (Mull's junk detection drops those that would not compile); the report spells each operator out (`tools/mutate.py` MULL_OPS).
- **Report** (`tools/mutate.py report-mull`, `tools/s11.sh report`): each Mull mutant (id
  `mutator:file:line:col:line:col`) is mapped to the hand-written function whose body or `HW_EXPORT` adapter holds
  the line; the classes and files are the same as before: `docs/progress/mutation.tsv` (per function),
  `mutation_survivors.tsv`, `mutation_sessions.tsv`, notes by Mull id in `mutation_notes.tsv`, input-needed rules in
  `mutation_reach.tsv`.

**Classification.** Not killed = one of: **equivalent** (it cannot change what the program does, or not in this
runtime: the reason is written down, per id), **unreached** (coverage says no route runs the line, or the note says
which input would reach the changed behaviour), **survived** (a test gap). Mutants are never weakened to raise the
score. **score** = killed / (mutants − equivalent); **reach score** = killed / (the same − unreached).

**S12 results (Mull, final run: 70 functions, 8 routes).** 776 mutants: **672 killed** (difftest 425, frame line
157, early stop 44, stall 39, crash 7), **33 equivalent**, **71 unreached**, **0 survived**: **score 90.4 %**, reach
score 100 % (every mutant a route reaches is killed or explained). Per file (score / reach score): file.c 97.8 / 100,
fixup.c 84.5 / 100 (32 unreached: container types and data shapes no route loads), pad.c 98.4 / 100, task.c
90.7 / 100, view.c 88.2 / 100, game.c 91.3 / 100, main.c 89.0 / 100, sound.c 92.1 / 100, title.c 100 / 100.
The first Mull run (69 functions, the 7 S11/S12 routes) gave 773 mutants, 645 killed, 50 unreached, 78 survived:
83.4 %. The 78 were then worked through: **19 killed by new input** (route `inputs` and the controller-A soft reset in
`route`, below; 5 of the 50 unreached lines are now run and killed too, and cd_play's 3 mutants are new), **33
equivalent** with the reason per id in `mutation_notes.tsv` (offsets of 0, dead initial values,
empty hooks, values overwritten before use, the original's zero tables), the other 26 **unreached with the input that
would reach them** (mesh sets with attribute pointers, models with two mesh sets, 1- or 5-element motions, a type-20
container entry, two sound requests on one port in one frame, music track 0, MIDI sequence 0, a reset while the tree
is paused). 45 of the unreached are lines no route runs (coverage), each with its reason from `mutation_reach.tsv`.

**Routes added for survivors** (`tools/routes.tsv`, pad script tokens in `hle_io.c`): `HL/HR/HU/HD` push the stick
exactly to 64 (the d-pad threshold: not a direction), `NOAX` / `ONLYX` / `ONLYY` make a controller without both analog
axes, `OUT` reads a port as empty (pulled out). Route **inputs** (1 000 frames, 1.1 s): the four thresholds on A,
partial soft-reset combos (one of A/B/X/Y/Start missing) on B then A, B pulled out while holding A+B, B without axes,
with only X, with only Y. **route** now pauses the race at 5850 and soft-resets from controller A at 5900.

**Timings** (MacBook, 10 cores, one container, 6 Mull workers; the control alone, unloaded: boot 8.3 s, explore 7.5,
route 7.2, vs4 6.0, vs2 5.0, reset 4.1, demo 3.7, inputs 1.1 = 42 s): **build of all mutants 3 s** (one clang
compile of `src/` with the plugin, one link), setup 23 s (pure references + gcov coverage of 8 routes), runner 1 549 s
(**26 min**): killed mutants 3.7 s mean (median 1 s: most die in the first frames of the cheapest covering route),
survivors 80 s mean (every covering route to its end: the cost of a survivor is the whole set of routes, ~2x the
unloaded control under 6 workers), unreached 0 s. Total 1 577 s. The first pipeline (rebuild + link per mutant, 8
jobs) had needed hours for 1 033 of 2 015 mutants. Pilot first (`--sample 40`: 14 mutants, 2 min) to check the gates
and the timing. Time-outs: per route 3x the control + 30 s, a stall after 20 s without a frame line; every one of
the 39 stall kills is a loop a mutant made endless (request retry loops, loop counters run backwards, the title's
drive-status wait), not re-run one by one.

**First pipeline (rebuild per mutant), results kept as history.** S11's code with S11's two routes (the S11
row of `mutation_sessions.tsv`): 1 302 mutants, 823 killed, 236 unreached, 235 survived, 8 stillborn (63.6 %; an
upper bound: 44 time-out kills were made under load with a 3x time-out; 15 re-checked: 11 real, 1 stop, 3 survive).
S12's code with the seven routes: **stopped at the user's request** after 1 033 of 2 015 mutants (plus 774 S11 kills
carried over by id); not a score. Cost: a survivor runs all seven routes (~90 s each), the run would have taken
hours. Lessons kept: snapshot `src/` at the start; never kill on wall time under load (stall detection); a digest
every 16 frames can miss a transient difference (the few such survivors are listed in the notes).
The survivor notes written then (`docs/progress/mutation_notes.tsv`, `mutation_reach.tsv`) are keyed by the first
pipeline's ids; they are the starting point for the Mull run's classification.



## 11. S12's region and structs

- `src/core/file.{h,c}` — the GD-ROM file layer: `file_init` (no exit request, no drive status, the error callback for
  every request), `file_load` (a whole file into a new `syMalloc` block of whole sectors; the SDK writes the size and
  the sector count into the caller's frame: those two words live on the guest stack, where the original keeps them),
  `file_load_fixed` (into the fixed area at 0x8CE40000: the sound driver), `file_free` (and its thunk), `cd_pause`.
  Every SDK request is repeated until the drive takes it. `cd_play` (func_8C012D38, S12 late: gdFsDaPlay
  until the drive takes it; tested inside music_update's samples on the route and by its 3 Mull mutants). Still
  translated (no route reaches them, so the difftest could not test them): the directory change, the error callback,
  CDDA stop / release (the music's track
  change happens on frames the game_update samples never fell on). Note: S11's `sound.c` called func_8C012D5C
  `cdda_stop`; it is gdFsDaPause (`cd_pause`).
- `src/core/fixup.c` — **data containers** (`container_load`, was func_8C012EBE): a file that starts with 16-byte
  entries {type, offset, a, b}, ended by type 0; offsets inside the file (0 or negative = no pointer, `reloc`) become
  addresses, following the Ninja structures each type names (**NjObject**, **NjModel**, **NjMeshset**,
  **NjCnkModel**, **NjTexList**/**NjTexName**, **NjMotion**: `file.h`). The motion fix-up reads the motion's element
  bits (position, angle, scale, vector = plain keys; shape, vertex = keys holding a pointer; the original starts from
  a 5-word table at 0x8C08AD54) and fixes 1 to 5 elements per node. The fix-ups that ran inside passing loader samples
  cover types 1 (model tree), 2 (motion), 3 (texture list), 5 (motions), 14 (motion pair list), 15/16 (chunk trees)
  and 19 (chunk action list); types 7 and 8 are handled inside `container_load`. The fix-ups of types 4, 6, 9-13, 17, 18 stay translated (called by address from
  `container_load`) until a route loads one; the original's func_8C0139D6 (type 18) takes its count from its fourth
  argument and passes the same value on as the node count (kept, noted).
- `src/game/title.c` — the **title scene** (0x1C4 bytes, mode 2; mode 1 builds it with variant 0): the constructor
  (vtable, then the scene base constructor that loads and shows frames: resumable), the destructor body (frees the
  eight blocks it loaded, releases its textures, stops the music, waits while the drive still plays) and the virtual
  destroy. Update and draw are float code: they stay translated (no FPU helpers yet, section 3).


## 12. Next (as S12 left it)

1. **Float helpers** before any float code: the scenes' update and draw methods (the title's 0x8C01BB52..., the house,
   the course map, the race) compute in round-to-zero / denormals-are-zero single precision; hand-written C needs
   helpers built on `sh4_ops.h` (fadd/fmul/fdiv/fsqrt/ftrc/float under FPSCR RZ+DN), tested against the interpreter
   on random and boundary operands, and a difftest that compares FR registers it does not yet (only FR12-FR15).
2. **Routes for what is still unreached** (`docs/progress/mutation_survivors.tsv`, class `unreached`): a controller
   unplugged mid-run and a device without analog axes (the pad reader's special cases: needs hle_io options), the
   drive lid / no disc (an HLE switch), a run long enough for a CDDA track to end, containers with the types no route
   loads (other modes: 1, 4, 9, 11, 12 are never visited), more than one V-blank per frame (a slow-frame option).
3. **Mutation run time** (section 10): 26 min for 776 mutants with 6 workers; 75 % of it is the 59 mutants that
   are not killed running every covering route to the end. Mutants already noted equivalent could be skipped in the
   gate (exit 0 without running, still reported from the note) to save ~45 min of CPU; any new function should go
   through `tools/s3.sh mull --only <its file>` first (5 min for pad.c + game.c).
4. The race scene and the menus (house, course map) once floats are possible; the remaining translated pieces of the
   loader (directory change, error callback, CDDA play/stop/release) when a route reaches them.


## 13. S13: float helpers, identical copies, function coverage, the title scene

### 13.1 Float helpers (`src/include/fpu.h`)

The game runs the FPU with FPSCR = 0x00040001 (round toward zero, denormals are zero, single precision); the host
rounds to nearest. Hand-written float code therefore never uses host float arithmetic: its floats have the type
**`f32`**, a struct holding the bits, so `a + b` does not compile, and every operation is a function that is exactly
one SH-4 instruction as the interpreter and the translation compute it (built on the same `sh4_ops.h` primitives
`rz_fadd`, `rz_fmul`, `rz_fdiv`, `rz_fsqrt`, `f_rz`, `f_dz`, `fq`): `fadd fsub fmul fdiv fsqrt fmac fneg fabs_ fgt feq
f32_from_int f32_from_uint f32_to_int fsca fsrra fipr`. Denormal operands and results are flushed (DN), NaN results
are the SH-4's own qNaN, `fmac` rounds the product then the sum (as `sh4_ops.h` models it), `f32_from_uint` is SHC's
`float` + 2^32 fix-up, `f32_to_int` is `ftrc` (saturating). Constants: `F32(1.5f)` for exact values, `F32_BITS(...)`
for the binary's patterns. Comparisons the compiler builds: a < b is `fgt(b, a)`, a <= b is `!fgt(a, b)`.

- **Interface** (`hw.h`): float arguments in FR4, FR5, ... (SHC's convention, independent of r4-r7): `FARG0..FARG5`
  in an adapter, `RETF(v)` for a result in FR0, `FARGS1..FARGS4` before a `CALLn` and `hw_fret()` after it for calls
  out. `U32_AT` / `F32_AT` reach one field of an object whose class is not declared yet.
- **Difftest** (`difftest.c`): with `RETF` the sample compares FR0 (as `RET` compares r0); the float arguments a
  hand-written call declares with `FARGS` are compared for HLE calls (FR4-FR7, bit for bit).
- **Tests** (`port/sh4/tests/test_fpu.c`, `tools/s3.sh fputest [N]`): every operation against the instruction it stands
  for, executed by the interpreter (`sh4_step` on the instruction word) with FPSCR 0x00040001: 49 edge operands
  (signed zeros, denormals, the smallest and largest normals, infinities, quiet and signalling NaNs, one ulp around
  1 and 2^24, 2^-24, 1/3, 0.1, pi, 2^32, products near both ends) crossed with each other, 200 000 random operand
  pairs per operation (random bits, nearby exponents where sums cancel, ordinary game magnitudes), edge and random
  integers for the conversions, all 65 536 FSCA angles, random FIPR vectors: **8 999 782 checks, 0 failures** (0.9 s).
- **Mull** (`tools/s3.sh fpu-mull`, `tools/mull_fpu.py`, image `penpen-mull:1`): `fpu.h` mutated, `test_fpu.c` as
  the test program (the interpreter and `sh4_ops.h` are the reference, not mutated): **118 mutants, 118 killed (100 %)**;
  the one first survivor (`i < 4` -> `i <= 4` in `fipr`) was killed by giving the test vectors a fifth element that
  must not count. The game-route Mull run excludes `fpu.h` (and `hw.h`).

### 13.2 Identical copies (`port/symbols/copies.tsv`, `tools/dupes.py`, `tools/copies.py`, `src/copies/`)

The game is C++: SHC compiled each inline method (vector helpers, accessors, constructors, thunks to the SDK) into
every unit that uses it. `tools/s3.sh dupes` (`tools/dupes.py`, in the sandbox: it reads 1ST_READ.BIN, prints
addresses only) groups the game's functions whose bytes are the same once outside branches are absolute and
literal-pool loads are their values: **204 groups, 2 606 functions** (65 % of the game's functions, 12 % of its bytes);
the groups are `port/symbols/copies.tsv`. One hand-written function stands for its whole group: `tools/copies.py`
(`tools/s11.sh copies`) writes an `HW_EXPORT` line with the same statement for every other copy that the translation
has (the pure translation's `meta.tsv`; a copy the trim dropped can never run) into `src/copies/<path of the source
file>`, which includes that file's headers. The statement repeated is the first export of the group in address order,
except that one in `core/inline.c` wins (class-neutral accessor names: two classes' getters of the same offset compile
to the same bytes).

**Rule for testing copies**: identical machine code behaves identically for every input, so a group is tested when
one of its copies passes the difftest (own samples or inside a passing caller); its unreached copies count as tested
by identity. `tools/s11.sh status` (`tools/hand_status.py`) lists, after a test, the groups with no passing copy and
the single exports that are untested: they are removed (S13: 14 groups and the title's fade-in, never reached). Mull
excludes `src/copies/` (their adapters are the same statement; the function bodies are mutated where they are
written). Fragments (the shared epilogue tails that SHC jumps into: `jsr @r3` on a frame the caller built) are not
functions: never hand-written.

### 13.3 Function coverage, call graph, Ghidra dumps

- `tools/s3.sh fncover` / `tools/s11.sh cover` (`tools/fncover.py`): the pure translation compiled with gcc
  `--coverage`, every route run once with its own profile directory, gcov's JSON call counts per translated function:
  `docs/progress/fn_coverage.tsv` (address, route:calls). S13: **1 228 game functions (155 780 bytes) run on the 8
  routes**; the other 2 782 game functions are reached by no route (other modes, menus, error paths).
- `tools/ghidra.sh decomp2 ADDR...` (`S13Decomp.java`): the pseudo-C with the code range read-only (the
  decompiler folds literal-pool constants: `0xbc` instead of `DAT_8c01b2d8`) and FPSCR.SZ/PR known as 0 (the 64-bit
  fmov ghosts become dead `cVar == '\0'` branches: take that side). Float arguments still show as CONCAT44 pairs:
  read the disassembly (`tools/ghidra.sh asm`) for anything with floats.
- `tools/ghidra.sh post S13Calls.java LO HI`: the direct call graph ("call FROM TO..."), used to order the work
  leaves first. Both dumps are disc-derived: they stay in `port_work/` (gitignored), never in docs or commits.
- A host crash in hand-written C prints the native call stack as `rt crash` lines (`port/runtime/main.c`; S13 found
  a method that takes its port in r5, not r4, this way).

### 13.4 The motion classes and the title scene

- `src/core/vec.{h,c}`: **Vec3**, **Quat** (0, 0, 0, 1), **Matrix** (4x4), and the game's damped-motion model:
  **Body** (position, velocity, force, damping; `body_step`: velocity += force, velocity *= damping, position +=
  velocity, force = 0; `body_pull`: force += (target - position) * k) and **Body1**, its one-dimensional twin
  (`body1_step`, `body1_pull`, `body1_pull_angle` for an angle taken the short way round as a 16-bit Ninja angle).
  The title's camera eye, target, logo and the "mover" are Bodies; its three angles are Body1s.
- `src/core/ninja.{h,c}`: the game's stubs to Ninja (jumps with every register passed on), matrix helpers on the
  current matrix (float angles truncated to Ninja units), the njControl3D flags stack, fog on/off.
- `src/core/inline.{h,c}`: class-neutral accessors (`field_get`, `field_set_f32`, ...), the empty method, `return
  this`.
- `src/core/view.{h,c}`: View fields 0xF0-0x11B (fog, light direction, intensity, colour) and their setters.
- `src/game/title.c`: **TitleScene** (0x1C4) declared field by field: the pad, the state (0 fade-in, 1 logo, 2 the
  intro of variant 0, 3 the VMU save check, 4 leaving), the buttons pressed with a 15-frame lock, the logo phases
  (wait 300 frames, the logo's motion with a 50-frame fade-in, then two layers that scroll and cross-fade along a sine
  every 16 frames), the attract time-out (1 800 frames: the demo and mode 3 alternate), the texture archive per
  language, seven model files read with nj_ReadChunkFile, the camera bodies and goals. Hand-written: the
  construction (`title_init`, was func_8C01B23C, resumable: the fade it starts ends frames), the update (state
  machine and the camera pulls), the logo. Still translated: variant 0's intro (func_8C01BEF8, 1 714 bytes), the draw
  (func_8C01BB0C: not in functions.tsv, only the vtable points to it), the VMU save check (func_8C018E10 /
  func_8C018E7C); the fade-in (func_8C01BB52) is written but not exported: only mode 1 (never visited) starts at
  state 0.

### 13.5 Delegated batches (the S13 workflow for throughput)

`port_work/s13/regions/BRIEF.md` (gitignored, with the dumps) is the brief: the covered, not-yet-written functions
whose direct callees are all written or outside the game ("leaves", from `S13Calls`), split by address into batches;
for each batch the `decomp2` pseudo-C and the disassembly are dumped once into `port_work/`; a writer (S13: two
subagents in parallel, no Docker, no git) writes C files into `port_work/s13/stage/<batch>/` (never `src/`: a build
compiles every file of `src/`, and a half-written file breaks the gates running meanwhile) and syntax-checks them on
the host (`clang -fsyntax-only` with `-Isrc/include`: `src/` is our code, no disc data); the lead copies them into
`src/`, runs `tools/s11.sh copies`, `gen`, the difftest route by route, `tools/s11.sh status`, and comments out
(`/* not exported (S13 difftest: reason): HW_EXPORT(...) */`) every export that fails or is unreached; then the
whole-run comparison. S13: batch a 87 of 99 written (12 skipped: fragments, functions cut short, one that waits for
a V-blank, one that changes SR), batch b 144 of 163 (19 skipped); after the difftest, 7 exports commented out
(different stack or float behaviour, different HLE calls, unreached) and 1 (a read of the caller's r13) never
exported. **Never edit `src/` while a gate runs** (S13 lost one comparison that way).

## 14. The plan for the whole game (S13)

Every game function becomes hand-written C. 4 010 functions, 301 130 bytes; done after S13: see section 1 and
`docs/progress/decomp.tsv`. By region (address ranges of the game code; "left, reached" = no export yet but some route
runs it, from `docs/progress/fn_coverage.tsv`; "left, unreached" = no route runs it yet):

| range | what lives there | functions | done | left, reached | left, unreached |
|---|---|---|---|---|---|
| 8C010000-8C012FFF | engine core: tasks, pads, views, files | 113 | 61 | 6 (1 582 B) | 46 (5 704 B) |
| 8C013000-8C015FFF | data fix-ups, contacts, model builders, text | 92 | 60 | 8 (3 358 B) | 24 (3 626 B) |
| 8C016000-8C018FFF | main loop, start-up, sound, VMU | 101 | 52 | 11 (2 578 B) | 38 (5 778 B) |
| 8C019000-8C01A3FF | Game modes, save data | 96 | 52 | 9 (464 B) | 35 (1 540 B) |
| 8C01A400-8C01B1FF | mode 0 (logos), race-scene wrapper | 69 | 55 | 9 (1 924 B) | 5 (80 B) |
| 8C01B200-8C01CFFF | title | 19 | 13 | 2 (2 586 B) | 4 (140 B) |
| 8C01D000-8C021FFF | menus: modes 3, 4, 9, 11, 12 | 201 | 100 | 4 (512 B) | 97 (17 184 B) |
| 8C022000-8C02FFFF | PenPen House, course select, race scene | 405 | 206 | 101 (39 478 B) | 98 (9 240 B) |
| 8C030000-8C03FFFF | racers, motion, stage objects | 460 | 277 | 87 (42 322 B) | 96 (11 498 B) |
| 8C040000-8C04CFFF | course, camera, lights, actors, effects | 914 | 593 | 104 (21 978 B) | 217 (11 312 B) |
| 8C04D000-8C06243F | other modes and objects (mostly unreached) | 1 540 | 999 | 22 (3 490 B) | 519 (57 248 B) |

(The "done" column counts copies: most of the bytes are left.) The order, each step leaves first (`S13Calls`):

0. **Start of S14: close S13's open gate**: Mull on the delegated files, one or two files per run with `--only` and
   `--budget 60` (the full-set control alone takes 3-5 min now; the title / vector run took 62 min for 355 mutants),
   classify every survivor, record the score; rewrite `sprite_text.c`'s register-level call of 0x8C062518 as C.

1. **Reached code, by region, in delegated batches** (13.5): the 343 reached functions left (~120 KB): the race
   scene, racers, course, camera and actors (8C022000-8C04CFFF) are 85 % of it; their big functions (500-2 600
   bytes: motion players, the race update, collision) need the asm, not the pseudo-C. ~3-4 sessions at S13's rate
   (231 written by two batches in ~25 minutes each, ~10 % failing the first difftest).
2. **Routes before unreached code**: no function is exported untested, so the 1 199 unreached functions (~180 KB)
   wait for routes: the title's other menu entries (OPTIONS: up then A from the menu; VS with 3 players), the
   other characters and courses in the race select, time trial, the house's other rooms, results/rankings, the
   game's ending (mode 9), modes 1, 4, 11, 12, the VMU menus (save/load/delete with and without a VMU), an
   unplugged controller, the drive lid. The `--watch` frame column on the Game's mode (`Game` at 0x8CE16B00 in the
   pure run: mode +0x0C, next_mode +0x10, mode_arg +0x14) finds the inputs cheaply; screenshots confirm them.
   Then `tools/s11.sh cover` again, and step 1 on what the new routes reach. ~4-6 sessions.
3. **Resumable chain and yields**: the functions on the yield chain (`rt yield-chain`) and those that wait for a
   V-blank are written with `HW_RESUMABLE` by hand (not delegated).
4. **Mutation** on every batch (`tools/s11.sh mutate --only`), the report classified, then the final gates.
5. The translated SDK and HLE stay as they are (PLAN 7.2); the end state is a build where every game function in
   `functions.tsv` (and the vtable-only ones like the title's draw at 0x8C01BB0C) is hand-written.
