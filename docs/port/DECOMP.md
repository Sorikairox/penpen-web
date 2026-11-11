# Pen Pen TriIcelon: hand-written C replacing the translation (S11, S12)

PLAN 7.2's end state is a build of hand-written C only. S11 (started before S9/S10 at the user's request) sets up the
method and does a first region. The hand-written C lives in `src/` (committed); the machine translation stays the
gitignored, local-only `port/gen/` and the volume `penpen-build` (PLAN 9). Hand-written C is not a renamed
translation: it is written from the function's behaviour (Ghidra's pseudo-C and the disassembly, read in the terminal
only, PLAN 9), with types, structs, loops and comments.

## 1. Status

| | S11 | S12 |
|---|---|---|
| Game functions replaced (`docs/progress/decomp_functions.tsv`) | 47 (section 9) | 69 (+22) (section 11) |
| Region | the game's core: main loop, start-up, callbacks, drive watch, task tree, Game modes, pads, views; the sound front end | + the GD-ROM file loader, the data-container fix-ups (Ninja models, motions, texture lists), CDDA pause, the title scene's constructor and destructor |
| Routes (`tools/routes.tsv`) | boot 8 000, route 6 000 | + explore 7 000, vs2 4 000, vs4 4 000, demo 3 400, reset 3 600 (section 5.1) |
| Differential test (`tools/s11.sh test`) | every export but main passes, 0 fail | 7 routes, 5 623 samples: **68 of 69 pass, 0 FAIL** (main: never returns, whole-run comparison only) |
| Whole-run comparison (`tools/s11.sh compare`) | route + boot: every frame line identical, native and wasm | all 7 routes: every frame line identical (RAM digest every frame), native and wasm; native = wasm |
| Mutation score (section 10) | 63.6 % (upper bound: see section 10) | pending: the Mull run (section 10) |
| Functions / bytes | 47 of 4 010 (1.17 %), 4 728 of 301 130 bytes (1.57 %) | **69 of 4 010 (1.72 %), 7 438 of 301 130 bytes (2.47 %)** |

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

## 10. Mutation testing (S12: `tools/mutate.py`, `tools/mut_worker.py`, `tools/s11.sh mutate`)

The user's rule for S12: mutation testing is a gate, not an experiment. A **mutant** is one small change at one place
of `src/`; it is **killed** when a gate sees a difference. S11 had planted 12 mutants by hand (4 flagged by the
difftest, 1 by the digest, 7 on paths no run took); S12 makes it systematic and re-runnable.

**Generation** (`tools/mutate.py gen`, host side: it reads only `src/`). A small C lexer finds function bodies,
`HW_EXPORT` adapters, macros and enumerators, and applies, one site at a time: `rel` (`<`→`<=`, `==`→`!=`, ...),
`arith` (`+`↔`-`, `&`↔`|`, `<<`↔`>>`, `++`↔`--`, compound assignments, unary `~`/`-` removed), `logic` (`&&`↔`||`,
`!` removed), `const` (n → n+1, n-1, 0 / 1), `negate` (the condition of if/while/for), `delete` (a one-line
statement, `break`, `continue`, `return;`; inside `HW_YIELDING` the call becomes `(void)0`), `return` (`return E` →
0 / 1, `RET(E)` → `RET(0)`), `args` (two neighbouring call arguments swapped; not a `CALLn` target), `argreg` (`ARGn`
→ `ARGn±1` in the adapters: the register wiring). Not mutated: `hw.h` (the runtime interface), guest addresses
(≥ 0x80000000), the `HW_EXPORT` address and the resumable macros' site numbers (a change only breaks the build), the
zero padding of `hw_call`'s unused argument registers, and macros or enumerators that no function uses (they cannot
change the program). Ids are a hash of (file, function, operator, original line, mutated line, occurrence): stable
across sessions while the line does not change, so notes stay attached.

**Running** (`tools/mut_worker.py`, only inside the sandbox of `tools/s3.sh mutate`: it runs the disc's code). Once:
the hand build's objects without `src/` pre-linked; `src/` snapshotted (later edits do not touch a running job);
reference frame and audio lines from the pure build for every route; the **control** (the unmutated hand build
through every gate: it must pass, else nothing is reported); **coverage** (`src/` built with `--coverage`, every
route run once with the difftest on: the lines each route executes). Then per mutant, 8 in parallel: compile (a
failure = *stillborn*, dropped), link, and run the routes whose coverage includes the mutated line (all routes for a
line that is not executable code: a macro, an enumerator), cheapest first, each run killed at the first difference:
a difftest FAIL (`difftest`), a frame line (`frame`: the SDK call counts, files, heap, pads, and the RAM digest every
16 frames), an audio line (`audio`: the mixer's digest), the run ending before its frame limit (`stop`, `crash`),
or no output for 120 s / 10x the control's time (`timeout`). A mutant on a line no route runs is *uncovered* without
running. Every line of output is our vocabulary (ids, verdicts, gate names, addresses, frame numbers).

**Classification** (`tools/mutate.py report`, `tools/s11.sh report`). Not killed = one of: **equivalent** (it cannot
change what the program does, or not in this runtime: the reason is written down in
`docs/progress/mutation_notes.tsv`, per id), **unreached** (coverage says no route runs the line, or the note says
which input would reach the changed behaviour: `mutation_notes.tsv` / `mutation_reach.tsv`), **survived** (a test
gap: a route that reaches it must be added, or the mutant explained). Mutants are never weakened to raise the score.
**score** = killed / (mutants − stillborn − equivalent); **reach score** = killed / (the same − unreached).
Outputs: `docs/progress/mutation.tsv` (per function), `mutation_survivors.tsv` (every mutant not killed, class and
reason), `mutation_sessions.tsv` (one row per session).

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
  Every SDK request is repeated until the drive takes it. Still translated (no route reaches them, so the difftest
  could not test them): the directory change, the error callback, CDDA play / stop / release (the music's track
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


## 12. Next

1. **Float helpers** before any float code: the scenes' update and draw methods (the title's 0x8C01BB52..., the house,
   the course map, the race) compute in round-to-zero / denormals-are-zero single precision; hand-written C needs
   helpers built on `sh4_ops.h` (fadd/fmul/fdiv/fsqrt/ftrc/float under FPSCR RZ+DN), tested against the interpreter
   on random and boundary operands, and a difftest that compares FR registers it does not yet (only FR12-FR15).
2. **Routes for what is still unreached** (`docs/progress/mutation_survivors.tsv`, class `unreached`): a controller
   unplugged mid-run and a device without analog axes (the pad reader's special cases: needs hle_io options), the
   drive lid / no disc (an HLE switch), a run long enough for a CDDA track to end, containers with the types no route
   loads (other modes: 1, 4, 9, 11, 12 are never visited), more than one V-blank per frame (a slow-frame option).
3. **Mutation run time**: about (see section 10) for the mutants on 10 cores; most of it is survivors running all
   seven routes. Notes (equivalent / unreached) could skip known mutants in later runs; a shorter route subset for
   the cheap first pass.
4. The race scene and the menus (house, course map) once floats are possible; the remaining translated pieces of the
   loader (directory change, error callback, CDDA play/stop/release) when a route reaches them.

