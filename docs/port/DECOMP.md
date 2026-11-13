# Pen Pen TriIcelon: hand-written C replacing the translation (S11-S19)

PLAN 7.2's end state is a build of hand-written C only. S11 (started before S9/S10 at the user's request) sets up the
method and does a first region. The hand-written C lives in `src/` (committed); the machine translation stays the
gitignored, local-only `port/gen/` and the volume `penpen-build` (PLAN 9). Hand-written C is not a renamed
translation: it is written from the function's behaviour (Ghidra's pseudo-C and the disassembly, read in the terminal
only, PLAN 9), with types, structs, loops and comments.

## 1. Status

| | S11 | S12 | S13 | S14 | S15 | S16 | S17 | S18 | S19 |
|---|---|---|---|---|---|---|---|---|---|
| Game functions replaced (`docs/progress/decomp_functions.tsv`) | 47 (section 9) | 70 (+23) (section 11) | 2 468 (+2 398: ~2 000 identical copies, section 13) | 2 938 (+470: copy-group leaders and their copies, scene constructors, the RaceSetup update, the loading screen, 7 delegated batches; section 15) | 3 079 (+141: lead resumable scenes and destructors, 9 delegated batches; section 16) | 3 117 (+38: the SetupMenu construction, 4 delegated writers: menus, racer, course, actors, text, contacts; section 17) | 3 806 (+689: the rest of S15's b2 list, nine delegated batches of mostly unreached functions; section 18) | 4 005 (+199: the last batches b01-b06, single ones, 50 fragments; section 19) | 4 046 of 4 046 (+41: type 0x31 x3, mode 11's start, func_8C042000 as a fragment, and the 36 game functions functions.tsv missed, now listed: batches u / u2; section 20) |
| Region | the game's core: main loop, start-up, callbacks, drive watch, task tree, Game modes, pads, views; the sound front end | + the GD-ROM file loader, the data-container fix-ups (Ninja models, motions, texture lists), CDDA pause and play, the title scene's constructor and destructor | + float helpers; the vector / Body / Ninja / accessor inline methods, the title scene, 231 delegated leaves (engine, save, VMU, race scene, racers, course, camera, actors) | + typed classes for S13's code (407 raw offsets -> 47); the race scene's HUD / pause / results, the racer's body, controls and effects, the motion player, the computer racer, the course construction, PenPen House / GOOD NIGHT / menus, the other cups' scenery actors | + the GOOD NIGHT and movie scenes' construction and playback (resumable), the RaceScene's construction / destructor, the actor group and the factory, actor types 1 2 3 0x20 0x2D, the course's ground search and environment (fog, lights, weather, sky, sounds), the racer's actions (dash, hits, states 2-4) and base construction, the race views' drawing and start per mode, the menus' screens, logos, save progress, the RTC, the motion player's draw, effect pushes | + the SetupMenu (construction, states 1-3, screens), the racer's run / ground / reset / update, the course's draw and weather, actor types 1 2 0x19 0x24 0x2B, the text drawer, the camera upload, the sphere-triangle test | + the racer's effects / sounds, the computer racer's states 3 and 4, the race step, the title's intro; task trees, motion drawers, container fix-ups, CD, save records, VMU drive, card messages, the staff roll, gallery and options scenes, racer swimming, race sound ports, the owner scene, the replay racer, actor types 0x00 0x06 0x09 0x0A 0x0B 0x0F 0x10 0x1B 0x26-0x28 0x2F-0x32 | + the cast viewer (func_8C0200E8), the race ending (func_8C02AF9C), type 0x32's update (func_8C060706), the last actor types, contacts' model builders, sbInitSystem, text drawers decoded from raw bytes, the owner scene and cast viewer construction (resumable); 50 fragments | + the gap functions (vtable-only init / update / draw methods of a dozen actor types, the title menu's and logo scene's draws, out-of-line setters); outside the game: the C runtime's atan2f / asinf / acosf / floorf (12 functions, src/lib) |
| Routes (`tools/routes.tsv`) | boot 8 000, route 6 000 | + explore 7 000, vs2 4 000, vs4 4 000, demo 3 400, reset 3 600 (section 5.1), inputs 1 000 (section 10) | the same 8 | + cup2, cup3, cup4 (unlock flags poked: the other cups), goodnight (mode 4): 12 routes | the same 12 (explore and goodnight also poke the score tables; goodnight runs on 2000-06-15) | the same 12 | the same 12 | + options (mode 12), gallery (mode 11), staffroll (mode 9), cup1b, cup2b, cup3b: 18 routes | + cup4b (CSD): 19 routes |
| Differential test (`tools/s11.sh test`) | every export but main passes, 0 fail | 8 routes, 5 792 samples: **69 of 70 pass, 0 FAIL** (main: never returns, whole-run comparison only) | 8 routes: 0 FAIL; every export passes or is a copy of a passing one | 12 routes, 135 382 samples: **0 FAIL**; every export passes or is a copy of a passing one, but main, cd_play (inside, S12) and task_destroy_tree (inside, a targeted run: 15.5) | 12 routes, 114 757 samples: **0 FAIL** (rare paths checked by targeted runs: 16.3) | 12 routes (the corpus run), 63 570 samples: **0 FAIL**; the corpora replay with 0 FAIL | 12 routes, 39 835 samples: **0 FAIL**; the corpora replay with 0 FAIL (c5: 8 174, syn: 4 128); **synthetic samples** (section 18): 2 540 functions with passing ones (2 043 that no route runs) | 18 routes, 52 622 samples: **0 FAIL**; corpora replay 0 FAIL (c5 + syn: 19 157); synthetic: 2 702 functions with passing samples  | 19 routes (the c7 corpus run), 57 489 samples: **0 FAIL**; corpora replay (c7 + syn, 15 819 samples): c7 0 FAIL; syn: 14 S17 / S18 samples FAIL under the defined stack (fresh ones of the same functions pass, 20.6); synthetic: + the maths, type 0x31, batches u / u2 |
| Whole-run comparison (`tools/s11.sh compare`) | route + boot: every frame line identical, native and wasm | all 8 routes: every frame line identical (RAM digest every frame), native and wasm; native = wasm | all 8 routes identical, native and wasm; native = wasm | all 12 routes identical (the interpreter-call count left out), native and wasm; native = wasm | all 12 routes identical, native and wasm; native = wasm | all 12 routes identical, native and wasm; native = wasm | all 12 routes identical, native and wasm; native = wasm | all 18 routes identical, native and wasm; native = wasm | all 19 routes identical, native and wasm; native = wasm |
| Mutation score (section 10) | 63.6 % (upper bound: see section 10) | Mull: **90.4 %** (776 mutants: 672 killed, 33 equivalent, 71 unreached, 0 survived; reach score 100 %) | fpu.h 118/118; lead code 355 mutants: 90.9 % (reach 100 %); delegated code not yet | S13 files the refactor left unchanged: 583 mutants, **91.6 %** (reach 100 %, 0 survived); refactored / new files: partial or not yet (15.3) | Mull m2 / m3 / m4 / m6 (59 files with S14's row): 4 703 mutants, 3 813 killed, 26 equivalent, 609 unreached, **255 survived (open)**: 81.5 % (reach 93.7 %): 16.1 | **every file**: 121 files, 12 714 mutants, 9 110 killed, 40 equivalent, 2 007 unreached, **1 557 survived (open)**: 71.9 % (reach 85.4 %); the replay gate: ~5 s per mutant (17) | 193 files, 16 960 mutants, 12 496 killed, 38 equivalent, 3 163 unreached, **1 263 survived (open)**: 73.8 % (reach 90.8 %); synthetic samples in the replay gate (18.5) | S18 code (n1, partial: 6 340 mutants of 73 files): 3 743 killed, 363 survived, 2 234 uncovered, 49 equivalent: 59.5 % (reach 91.2 %); the S17 combined figures stand for the files S18 did not change | combined (S17 + S18 n1 + S19 n2 / n3b / n4): 22 991 mutants, 16 753 killed, 144 equivalent, 4 510 unreached, 1 584 survived (each with a reason): 73.3 %, reach 91.4 %; S19's new files 81.5 % |
| Functions / bytes | 47 of 4 010 (1.17 %), 4 728 of 301 130 bytes (1.57 %) | **70 of 4 010 (1.75 %), 7 474 of 301 130 bytes (2.48 %)** | **2 468 of 4 010 (61.55 %), 57 508 of 301 130 bytes (19.10 %)** | **2 938 of 4 010 (73.27 %), 104 420 of 301 130 bytes (34.68 %)** | **3 079 of 4 010 (76.78 %), 135 372 of 301 130 bytes (44.95 %)** | **3 117 of 4 010 (77.73 %), 177 706 of 301 130 bytes (59.01 %)** | **3 806 of 4 010 (94.91 %), 233 516 of 301 130 bytes (77.55 %)** | **4 005 of 4 010 (99.88 %), 300 222 of 301 130 bytes (99.70 %)** | **4 046 of 4 046 (100 %), 312 174 of 312 174 bytes (100 %)** (functions.tsv + 36 gap functions) |

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
|---|---|---|---|---|---|---|
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

## 15. S14: typed classes, Mull in two passes, unlock routes, more functions

### 15.1 Tools (S14)

- **Mull gate, two passes** (`tools/mull_gate.py`, `tools/mull_run.py`, `port/runtime/difftest.c`): with ~2 500
  exports the full difftest makes every route ~5x slower (the control: 209 s against 42 s in S12). A mutant now runs
  first with the difftest sampling only the exports of its own file and of that file's copies in `src/copies/`
  (`--hwtest B:50:A1,A2,...`: the difftest's single-function filter became a list); a mutant that survives this pass
  runs the full gate (every export sampled), so a survivor's verdict is the full gate's and a kill in the first pass is
  a real kill (gate names `difftest-file` / `frame-file`). Killed mutants: mean 4 s. `mull_run.py` also snapshots
  `tools/routes.tsv` (the gate read it live: an edit during a run would have broken every later mutant).
- **One score from several runs** (`tools/mull_combine.py OUT RUN...`): Mull runs one or two files at a time; the
  combiner keeps each file's last run, and `tools/mutate.py report-mull` reads the combined file.
- **Readability debt** (`tools/rawoff.py [SRC] [-v]`): counts raw-offset accesses (`U32_AT(x + 0x..)`, `ARGn + 0x..`,
  `field_*(.., 0x..)`).
- **`--poke ADDR=VALUE@FRAME,...`** (runtime test input, native and the node host): writes a guest word at the end of
  frame FRAME, after its frame line. Routes use it for progress no route can play through: the save record's unlock
  flags (0x8C1B7E54-0x8C1B7E63 = all ones, written at frame 1100 in PenPen House, after the house has loaded the
  record) open the other three cups and the VS entries of the course map. Pure and hand runs get the same writes.

### 15.2 Typed structs for the S13 code (readability debt)

S13's delegated batches reached fields by raw offsets (`U32_AT(obj + 0x38)`, `field_get_f32(ARG0, 0x74)`): 407 such
accesses (`tools/rawoff.py`). S14 declared the classes behind them, with the guest layout checked by `HW_OFFSET` /
`HW_SIZE`, from the constructors, allocation sizes, vtables and what the still-translated code (the race update, the
camera, the course) does with each field (one delegated writer on a copy of `src/`, merged by the lead):

- `game/racer.h`: **Racer** (0x5A8, constructor func_8C03755C, vtable 0x8C23DACC): character and motion player at
  0x24, the Contacts at 0x1B8, the moving frame (**FrameBody**) at 0x2B8, heading Body1 at 0x32C, the character's
  **CharacterParams** (0x434-byte records) at 0x43C, the follow camera from 0x498 (springs, the eye Body at 0x4EC).
- `game/race_scene.h`: **RaceScene** (constructor func_8C0293F0; the "panel" of game.h) with its **RaceSetup** (mode
  7's scene, 0xA8), course at 0x38, actors at 0x48, players at 0x4C, views and the watched player; **SlotList**.
- `game/course.h`: **Course** (0x5790, constructor func_8C041114, vtable 0x8C23DB5C): cell attributes, segments,
  actor placements, tracks, four **CourseEnv** (0x1BC: fog track, light rig, weather particles per view) at 0x4EE4,
  the per-view bit sets at 0x55D4; record types (CourseSegment, FogKey, LightKey, CellKey).
- `game/actor.h`: the **Actor** base (0x28, vtable at 0x24), **ActorGroup** (0x554: the course's placements built by
  the factory func_8C043D4C, 0x34 types), and the actor classes whose methods are written (ActorPair, Strip, Pieces,
  Chain, Effect, ...).
- `game/motion.h` (**MotionPlayer**), `game/effects.h` (the racer's ribbons and effect sets), `game/menu.h`
  (**SetupMenu**, the menu model drawer), `core/contact.h` (**Contacts**), View fields 0x64-0x15C in `core/view.h`,
  **NjSprite** in `core/ninja.h`. `member_addr(a, Type, member)` (core/vec.h) is a member's guest address with the
  original's arithmetic.

Accessor copies (`core/inline.c`) whose whole copy group is used on one class say that class's field
(`racer_heading`, `race_scene_cell_point`, ...); groups used on several classes stay class-neutral, which is what the
44 raw offsets left are (the generic helpers' bodies and multi-class copies). Every changed file was compared before
the gates at the object-code level (the old and new C compiled with a flat `hw_ptr`: the same loads, stores and calls
in every function; differences only in register allocation and addressing form), then the difftest and the
whole-run comparison. Reading the code with its types found two S13 bugs: func_8C044EEA added to `0xAC` instead of
`Course.envs` (0x4EE4) and func_8C0417D2 read `0x1BC` instead of `Course.view_bits` (0x55D4): both took the wrong
half of a literal-pool word that holds two 16-bit constants. The first passed S13's difftest (the routes push by
zero vectors, so the wrong place was left unchanged); the second was S13's "to redo" failure. Both fixed.

### 15.4 Routes (S14)

What decides which code runs is mostly data: the course's actor placements pick among the actor factory's 0x34
classes (func_8C043D4C), so the other cups' courses reach code no route ran. They are locked on a new save; the save
record's unlock flags (game/save.c: `flags` and `course_flags`, 0x8C1B7E54-0x8C1B7E63) set by `--poke` in PenPen
House (frame 1100: the house has loaded the record at ~700, which overwrote an earlier poke) open them, and RIGHT on
the cup select (~1640, before the A at 1700) picks one. Measured with `fncover` on candidate routes
(`tools/routes_try.tsv`, local) before adding them to `tools/routes.tsv` (cup2 also pokes the three score tables
and the per-character bytes to small distinct values: S13's save code had only seen zero scores):

| route | what | new game functions (beyond the 8 routes) |
|---|---|---|
| cup2 / cup3 / cup4 | the 2nd / 3rd / 4th cup's first course, raced from ~1830 | 153 / 142 / 111 (383 together, 22.6 KB) |
| goodnight | PenPen House's GOOD NIGHT: Game mode 4 (never visited before), back to the title | 29 (2.9 KB) |
| middle, long | Sweets' other two courses | 2 (same actors as Easy) |
| vssame, vshana | the unlocked VS entries of the course map | 2 |
| vs3 | VS with three controllers | 0 |

Mode transitions found reading the menus' code: the title's choices 0 / 1 / 10-12 start mode 7 with argument 0 /
4 / 1-3 (TRICELON, TIME TRIAL, VS 2-4 Pen); the RaceSetup's update (func_8C01A950) builds the SetupMenu (state 0,
0x424 bytes) or the RaceScene (1, 0xD2C bytes) and leaves for mode 4 (state 3: GOOD NIGHT), 9 (state 4), 1 (5) or 2
(6); mode 3's scene (func_8C01DE32) plays a Sofdec movie and leaves for mode 1 or 2 when it ends. Mode 12 needs the title's choice 3, which the title's menu never sets.

**Frame depth (a difftest rule found in S14).** The difftest replays what an HLE call wrote into the live stack as
offsets from r15 at the call (section 5). That covers out-parameters in the function's own frame, but also writes
into a *caller's* stack data (a matrix the caller keeps on its stack, which njRotate rewrites): those land on the
same words only when the hand-written function's r15 at the call is as deep as the original's. A function that
calls the SDK with pointers into stack data therefore allocates the original's whole frame (its locals and its saved
registers: `hw_stack_alloc(depth)` with a struct that names the slots), not just its out-parameters
(func_8C0120CA, S13's "to redo": 0x20 bytes; it passes with them). `tools/framecheck.py` lists every hand-written
function's allocation next to the original's frame depth (from the dumps) for review; a smaller allocation is only
a problem when the SDK writes stack data outside it.

### 15.3 Mull on S13's code

Mull ran per file group (`tools/s11.sh mutate --budget 60 --only ...`, the two-pass gate of 15.1):

- **m1** (13 S13 files, the 8 routes of the time, 6 796 s): 856 mutants, 770 killed, 1 equivalent, 64 unreached,
  21 survived. 18 of the survivors were in game/save.c, where every route had zero scores (sums, bests, the
  course-flag index all read zeros): cup2 now pokes small distinct scores; one in core/task.c is equivalent (the root
  tree's updates_per_frame is rewritten by game_frame before it is read) and one in game/vmu.c unreached (a second
  memory card), both noted by id in `mutation_notes.tsv`. contact.c, sprite_text.c, task.c, save.c and vmu.c changed
  after m1 (the refactor), so their m1 results do not count.
- **Recorded score (S14 row of `mutation_sessions.tsv`)**: the files whose m0 / m1 results still match the source:
  core/{random,pad_analog,gfx_init,motion,model_build,pad,file}.c, game/{main,sound}.c: **583 mutants, 533 killed,
  1 equivalent, 49 unreached, 0 survived: 91.6 %, reach score 100 %**.
- **m2** (12 refactored files: game/{stage,menu_sprites,save,vmu}.c, core/{accessors,view_setup,system,texture,
  sprite_text,contact,transform,task}.c, 12 routes): stopped part way to free the machine for the end-of-session gates: 55 mutants run (accessors, contact, sprite_text, system, transform): 51 killed, 4 on lines no route runs, 0 survived; not a score (`port_work/s14/mull/m2_partial_gate.log`)
- Not mutation-tested yet: the refactored game files actor.c, camera.c, course.c, racer.c, model.c, race_scene.c,
  title.c, core/{vec,inline,view}.c, and everything written in S14 (lead and batches).

Two things the first m2 attempts found: the frame lines differed only in the interpreter-call count where hand-written
C replaces a cut-short function's interpreted tail (vs2, vs4, cup4), so the gate stopped at its control; the count is
now left out (15.1). And Mull's coverage step now survives a route that writes no profile.

### 15.5 New functions

- **Lead-written** (S14): the leaders of copy groups that no export covered yet (found by listing the groups with a
  reached member and no export: the destructor bodies of the 63 classes that keep Contacts after two words, twice;
  the race scene's slot list, 23 copies; the RaceSetup getter, 14; the current view's light colour and direction,
  the motion player's deleting destructor, the ribbon reset, the actor base construction, the scene's view bit and
  start value, float accessors), the scene constructors of modes 0, 3 and 7, of the SetupMenu and the RaceScene, the
  RaceSetup's update (it switches between the menu and the race and leaves mode 7) and the loading screen
  (func_8C01630E with the tail Ghidra split off at a literal pool): resumable, written by the lead (13.5's rule).
  `tools/copies.py` had never seen an export with a trailing comment: vec3_set's group of 11 got its copies.
  The RaceSetup's update now builds the menu and the race scene from C, so the constructors and the destructor of
  the scene it replaces (task_destroy_tree) are entered only from C: a targeted run (`--hwtest 3000:1:8c01a950` on
  the route's first 2 000 frames, every call sampled) covers the state changes: 1 295 samples pass, with
  task_destroy_tree, the SetupMenu's and the RaceScene's constructors passing inside them.
- **Delegated** (13.5's workflow, `tools/batches.py` batches of ~12 KB in `port_work/s14/b2/`, brief
  `port_work/s14/WRITER_BRIEF.md`: typed classes required, `rawoff.py` must report 0): batch b05 (the race scene's
  panel, pause screen, results and ending sprites, item digits, sound-by-mode helpers: `game/race_hud.c`; the
  racer's collider, reset, state, destructor body and effect thunks: `game/racer_body.c`): 30 of 39; batch b02 (the
  menus' penguins, GOOD NIGHT = mode 4's scene, the movie scene's set-up, the SetupMenu = PenPen House and the
  selection screens: `game/menu_penguin.c`, `game/good_night.c`, `game/house.c`): 29 of 38; batch b10 (the other cups' scenery actors: the
  factory's types 7, 8, 0x19, 0x1E, 0x23, 0x24, 0x2B, 0x2E, 0x33 declared as classes in `game/actor.h`, pieces, chains,
  bubbles, motes, springs, posts, balls, wobblers, effects, bursts: `game/actor_*.c`, `game/burst.c`): 66 of 70;
  batch b08 (the computer racer
  LaneRacer, the Course's construction, three of the racer's camera modes, the race scene's contact pass:
  `game/lane_racer.c`, `game/course_build.c`, `game/racer_camera.c`, `game/slot_collide.c`): 17 of 20; batch b07 (the
  racer's effects task and its ribbon and particle sets, the motion player's build / play / advance, the racer's
  construction and controls: `game/effects.c`, `game/motion_player.c`, `game/racer_control.c`): 26 of 37;
  batch b11 (more of the cups' scenery: bumpers,
  flaps, flocks, turning gates, geysers, swimmers, signs, buoys, flyers, fences, wave fields, ornaments, swingers,
  sparkles, rings: `game/actor_*.c`): 65 of 69. The difftest caught one more literal-pool half (func_8C05045A read RaceScene.view_cell instead of the
  per-view `focus` array at 0xDC: fixed, `focus` became an array). The cup4 route stopped in the hand build where
  Ghidra had cut func_8C059CAC short: the recompiler translated its first part and let the interpreter run the rest,
  entered with the return address of the last call; with a hand-written caller that address was hw_call's marker.
  The function is now written whole (and the interpreter treats a return to the marker as a return). Skipped
  with reasons: fragments Ghidra split off a function that the writer wrote whole, functions that end frames
  (the Sofdec loop, constructions that show the loading screen: lead work), a dump that loses sync.

### 15.6 Next

1. Finish the Mull runs this session started or left (`tools/s11.sh mutate --budget 60 --only ...`, one or two
   files per run; `tools/mull_combine.py` then `tools/s11.sh report`): m2's 12 files again (it was stopped part way), then game/{actor,camera,course,racer,model,race_scene,title}.c, core/{vec,inline,view}.c and every file written in S14 (`src/game/{race_hud,racer_body,house,good_night,menu_penguin,loading,lane_racer,course_build,racer_camera,slot_collide,effects,motion_player,racer_control,burst,actor_*}.c`); classify every survivor.
2. Merge and gate the staged batches left (`port_work/s14/stage/<batch>/`, read their reports' header notes), then
   write the remaining batches of `port_work/s14/b2/` (re-cut with `tools/batches.py` after `tools/s11.sh cover`:
   the cup routes reach ~380 more functions) with the brief `port_work/s14/WRITER_BRIEF.md`.
3. Lead work (resumable / yield chain): the SetupMenu's construction func_8C022D80, the RaceScene's func_8C0293F0,
   GOOD NIGHT's func_8C01D2C4 / func_8C01D4CA, the movie scene func_8C01DE32 / func_8C01DEF8 (Sofdec loop),
   func_8C0100A0 / func_8C010000 (start-up); func_8C0623B4 (needs a way to test inside game_init).
4. More routes: the cups' other courses, the ending (Game mode 9: RaceSetup state 4, after winning a cup), the VMU
   menus (no VMU, a full one), the house's DRESS UP / RESULTS with unlocked flags.


## 16. S15: the Mull debt, the lead's resumable scenes, nine delegated batches, a cheaper Mull gate

### 16.1 Mull runs (the debt first)

`tools/s11.sh mutate --budget 60 --workers 6 --only ...` per file group, combined with `tools/mull_combine.py` into
`port_work/s15/mull/s15_combined.out` (S14's row + m2 + m3 + m4), reported by `tools/mutate.py report-mull`:

| run | files | mutants | killed | equivalent | unreached | survived | wall time |
|---|---|---|---|---|---|---|---|
| m2 | S14's refactored set again: game/{stage,menu_sprites,save,vmu}.c, core/{accessors,view_setup,system,texture,sprite_text,contact,transform,task}.c | 506 | 455 | 1 + 10 classified | 29 + 6 classified | 21 -> 0 open (save.c / system.c re-run in m3) | 2 h 02 |
| m3 | the S15 code merged first (good_night, game, race_scene, race_scene_methods, actor_group, actor_types, course_ground, course_env, actor_factory, racer_actions, menu_screens, accessors) + system.c, save.c with the new route inputs | 1 894 | 1 547 | 19 classified | 263 + 20 classified | 84 -> 45 open | 4 h 41 |
| m4 | the S15 code merged later (race_draw, race_players, racer_base, save_progress, logos, item_pen, text_view, race_setup, actor_type20, scene_misc, motion_draw, racer_camera_update, racer_squash, effects_push(_b), course_camera, core/{view_layout,model_free,cnk_walk,sprite_pointer,rtc,video_mode}) + the S14 files S15 changed (lane_racer, menu_penguin, racer_body, actor_flyer, house) | 1 870 | 1 421 | 0 | 258 | 191 open | 6 h 38 |

**Combined (59 files, actor_types.c from m6): 4 703 mutants run (+29 skipped as duplicates), 3 813 killed, 26
equivalent, 609 unreached, 255 survived: score 81.5 %, reach score 93.7 %** (the S15 row of
`docs/progress/mutation_sessions.tsv`). The 255 open survivors are listed in `docs/progress/mutation_survivors.tsv` (class `survived`); the
largest groups: core/rtc.c 40, actor_types.c 35 (m6) (the clock's date and second conversions: the routes run on one fixed RTC date and no
gate reads the save's time stamp, which goes to the VMU file), race_draw.c 34, house.c 19,
racer_actions.c 15 (cup3's six dashes and one hit: rare paths). Still without a Mull result in the combined file (30 files): the S14 files S15 did not touch (game/{race_hud,
effects,motion_player,racer_control,burst,course_build,racer_camera,slot_collide,loading}.c and the 14 other
game/actor_*.c) and the S13 files the S14 refactor changed (game/{actor,camera,course,racer,model,title}.c,
core/{vec,inline,view}.c); core/fixup.c and core/ninja.c have only their S12 / S13 results; game/racer_camera_start.c
is not called (16.3).

Classified this session: 50 survivors, by id in `docs/progress/mutation_notes.tsv`, each with its reason: equivalent (empty
methods and constructors, values overwritten before use, `game_language()` being 1 with languages 0 and 1 sharing
their data, the never-set-up debug log, the HLE's Sofdec ignoring the frame-count mode), or unreachable with the input
that would reach them (no LEFT / RIGHT on GOOD NIGHT's choice, a second VMU, a rolled camera, a negative slot, a
failed allocation, a locked character, a non-zero record reset, ...). Route inputs added to kill survivors: explore
and goodnight poke the score tables (save_total_* / sum8 / save_best_a now see non-zero scores), goodnight runs on
2000-06-15 (`--rtc 1592179200`: the save's weekday from a month other than January: date_weekday's survivor was
equivalent on 2000-01-01, the HLE's fixed date, because (13*13+8)/5 = 35 is a multiple of 7).

### 16.2 A cheaper gate (the lead's request after m4)

m2-m4 took 2 h, 4 h 41 and 6 h 38: **survivors cost ~82 % of the time** (m4: 191 survivors, mean 571 s each, against
7.9 s per killed mutant): a survivor ran both passes (the file's exports, then every export) on every covering route.
`tools/mull_gate.py` / `tools/mull_run.py` now:

1. order a mutant's covering routes by expected kills per second (the line's execution count over the route's
   control time, highest first);
2. run Mull's pass with the file-exports difftest on at most **3** routes; a mutant that survives them is
   `survived-partial`; after mull-runner, `mull_run.py`'s **final pass** runs only those through both passes on every
   covering route (the mutant switched on through its environment variable, as Mull does) and only that verdict counts
   (a kill there turns Mull's "Survived" into "Killed" in the report);
3. a partial-pass run has a time-out of 2x the control + 20 s and a 10 s stall, **neither a kill** (the mutant goes to
   the final pass); final and control runs keep 3x + 30 s and 20 s (a loaded machine must not kill a slow good run:
   a first try with 1.5x + 15 s in the final pass gave 35 false time-out kills on actor_types.c);
4. of the two call-replacement mutants of one call (`cxx_replace_scalar_call_zr` / `_nz`), the second is skipped once
   the first is killed: logged `skipped-dup`, never counted as killed (`mutate.py report-mull` prints the count).

Not done: **starting a mutant from a snapshot** a few frames before its function first runs (the lead's first item).
The runtime's state is not only guest RAM and the CPU: the HLE keeps file positions and pending GD-ROM reads, the
audio mixer and CDDA, timers, the pads' edges and the render state outside guest RAM; a snapshot needs all of it
serialised (native and wasm), which is a runtime change of its own (next step 1).

**Measured on one file (game/actor_types.c, 209 mutants, 6 workers):** before (m3's gate): 12 406 gate-seconds,
**59.4 s per mutant**, 147 killed (some by time-outs under load), 16 survived; after (m6, the new gate): 12 380
gate-seconds, **59.2 s per mutant** (47 min 56 s wall for the file alone), 99 killed, 29 skipped as duplicates, 35
survived. **No gain**: the true survivors still pay the full final pass (36 survivors: 11 838 s), and the old gate's
cheaper "kills" on this file were largely time-outs under load: with time-outs no longer counted in the partial pass
and the final pass kept generous, ~19 of m3's kills on this file turn out to be survivors (the honest number is the
higher one; actor_types.c now counts m6's result). A first try with a 1.5x + 15 s final time-out gave 20.5 s per
mutant, but 35 of its kills were false time-outs: rejected. The cost is the survivors' full gate; only the snapshot
start (a window of frames instead of whole routes) or a final pass on fewer routes can cut it.

### 16.3 New functions (141: 2 938 -> 3 079)

- **Lead** (resumable, DECOMP.md 4): GOOD NIGHT's construction and constructor (func_8C01D2C4 / func_8C01D4CA) and
  mode 4's start routine (func_8C01A1B8), the movie scene's update and Sofdec playback loop (func_8C01DE32 /
  func_8C01DEF8: create, play until the end / Start / an error, one more frame, destroy), the RaceScene's construction
  (func_8C0293F0: 8 racer slots, mark tables, per-slot lists, the labels by language) and destructor body
  (func_8C0297D4); RaceScene's tail (to 0xD2C) and its 8-wide racer arrays declared. Not done: the SetupMenu's
  construction func_8C022D80 (1 350 bytes, next step), the start-up func_8C010000 / func_8C0100A0 (Shinobi region,
  not game functions).
- **Delegated** (brief `port_work/s15/WRITER_BRIEF.md`, batches re-cut by `tools/batches.py` into `port_work/s15/b/`,
  then `b2/`; every batch merged, difftested on 12 routes and compared by the lead): b08 26 (the actor group, types 1,
  2, 3, 0x20, 0x2D), b07 21 (the course's ground search, the environment: fog, lights, weather, sky, sounds; the actor
  factory), b05 13 (the racer's actions), b03 16 (the menus' screens, small RaceScene methods), b04 15 (the race
  views' drawing, the race start per mode, the racer's base construction), b01 19 (view layout, model free, chunk
  walk, the pointer sprite, save progress, the logos scene, item pen, text in a view, the RaceSetup's range), b09 14
  (the RTC module, the video mode, actor type 0x20's update, two small methods), b06 6 (the motion player's draw,
  the racer's draw, squash, camera update, the ribbon push), c05 2 (effect set B's push, the course's TV camera).
- **Found by the difftest on merging** (each fixed before the gates): b08's group attach wrote the blocks to
  Actor+0x1C instead of Actor.models (+0x20: two setter thunks mapped to one field); b03's held-motion draw
  dereferenced the object once too often; b09's video mode called core/system.c's `system_language` (S13, never
  tested): game_init's sample then differed: it calls the original func_8C0623B4 now and `system_language` stays
  unexported; b06's racer_camera_start (and c05's rewrite of it) make racer_camera_update fail at a race's first frame
  (the eye's position): racer_camera_update calls the original func_8C03C804, the C version is kept unexported (to
  find); racer_draw returns nothing its callers read, so the LaneRacer's thunk to it (func_8C03E14A) is a plain call.
- Readability: the four S14 "SetupMenu" setters are the **RaceSetup's** (`race_setup_set_variant / _course / _word68 /
  _word88`, core/accessors.c; game/house.c calls them where it leaves for the race; their exports stay commented out:
  untested, C callers only); flyer_pose's parent stack has the original's 5 slots (it had 64 host slots; a deeper
  tree ends the walk instead of overwriting memory). Raw offsets: still **47** (the class-neutral accessors).
- Rare paths checked by targeted runs (`--hwtest 20000:0:A,B`, every call of the listed functions sampled): cup3
  (racer_update_state3 5 280 samples: the dash / hit functions pass inside), boot (racer_update_state4 14 256
  samples: action 4's start passes inside).

### 16.4 Next

1. Mull: the snapshot start (16.2's missing item: serialise the HLE state), then the files still not mutation-tested
   (16.1), then the 236 open survivors (kill with input, or classify by id).
2. The 82 reached functions left (`tools/batches.py` into `port_work/s15/b2/`: big ones: func_8C029EFA 4 226 bytes,
   func_8C035FA2 4 042, func_8C0351C8 3 546, func_8C025CEC / func_8C0250E4 / func_8C023D16 (menus), func_8C03E150,
   func_8C03FC68, func_8C0433D6, func_8C030F00 (the racer's reset), func_8C0317D4, fragments to fold into their
   owners), the SetupMenu's construction func_8C022D80 (lead), racer_camera_start's difference.
3. Routes for the 2 370 unreached functions: none added this session (the other characters, multiplayer modes,
   options, VMU menus, the ending: 15.6 step 4).


## 17. S16: the offline sample-replay Mull gate, the Mull debt, more functions

### 17.1 Why Mull was slow, and the replay gate

A Mull mutant was judged by whole routes from boot: the mutated build ran the routes that execute the mutated line
(up to 3 in S15's partial pass, then every covering route for the partial survivors), each 5-50 s, with the
difftest sampling the file's exports. A kill usually came early (the first sample of the function), but a survivor
had to run every covering route to its end: survivors were ~82 % of Mull's time (16.2), and S15's re-ordering gave
no gain (59.4 -> 59.2 s per mutant on actor_types.c).

The difftest (section 5) already holds everything a sample needs to be judged again without the run: the snapshot
(CPU + 16 MiB of guest RAM), the HLE results of the live call (r0/fr0, the argument registers read, the stack
out-parameters, the RAM the HLE filled), the hardware reads, and side A's outcome (the original code, interpreted).
S16 keeps them:

- **Corpus** (`--hwcorpus DIR:K` with `--hwtest`, `tools/s11.sh corpus TAG K`): every route of the hand build runs
  the full difftest and keeps up to K passing samples per function: the first, then any whose side A entered a new
  set of functions (with the number of HLE calls: the "path signature"), then one in 8 at random. Guest RAM is
  stored as 4 KiB pages in one content-addressed store per route (`pages.bin`: the samples of one run share most
  pages), side A's final RAM as the pages that differ from the snapshot, plus the HLE / stack / RAM-fill /
  hardware-read records, side A's CPU, end and SDK-call log, and the list of every function side A entered
  (`index.tsv`). A second pass samples alone the functions the first pass entered but never sampled (their calls
  fell inside another function's sample: one sample at a time). The corpus holds guest RAM: it stays in the Docker
  volume `penpen-corpus` (never in git).
- **Replay** (`--hwreplay LIST`): the runtime starts (no frame runs), and for each listed sample side B runs the
  hand-written function of THIS build from the snapshot against the recorded side A, with the same comparison as
  the live difftest (difftest.c `side_b_compare`, now shared). `PENPEN_REPLAY_DIFFS=N` prints the first N differing
  words (above 100: every replayed HLE call and hardware write).
- **Gate** (`tools/mull_gate.py`, `tools/replay_corpus.py`, `tools/mull_run.py`): for a mutant, the samples that
  ran the mutated function's original code: its own samples, and the samples of other functions whose side A
  entered one of its addresses (its HW_EXPORT addresses and the "was func_" addresses of its comment; a line outside
  any addressed function uses all of its file's), at most 160 samples / 4 s of side B. FAIL or a crash = killed
  (gates `replay`, `replay-crash`); a replay time-out (120 s) is not a kill. Only a mutant the replay does not kill
  (or whose function has no samples) runs the whole-route gate: the file's exports sampled by the difftest, frame
  lines with the RAM digest, audio lines, the run's end, on the 1-3 routes that run the line most per second
  (a stall = kill, the overall time-out is not). A survivor of both stays "survived". Before Mull runs, every
  selected sample is replayed on the unmutated build (the control: a sample that fails there is left out and
  counted; none did).

**Measured** (6 workers, one container, the same two files as the "before" runs; seconds per mutant = the gates'
own time summed over the file's mutants / its mutants, as 16.2 measured it):

| file | before (gate, run) | after (S16 gate, run v2) | kills before / after (same 209 / 217 mutants) |
|---|---|---|---|
| game/actor_types.c | 59.2 s (S15 gate, m6) | **4.6 s** | 99 / 126: every old kill kept; the 27 more are S15's skipped call duplicates, now run (3 of them checked by hand: syMalloc / panel_get / actor_model replaced by 0: a NULL block, the slot list of scene 0, the other model: real) |
| game/race_draw.c | 52.8 s (S14 two-pass gate, m4) | **4.1 s** | 158 / 158: the same set |

The two files' run took 940 s of wall time with Mull's set-up (3 min: references, coverage, the replay control):
before, actor_types.c alone took 48 min. Of 426 mutants, 210 were killed by the replay (median 0 s), 74 by the
routes after the replay, 72 survived both (15.3 s mean: the replay plus up to 3 routes), 70 are on lines no route
runs. A first run (v1) took the 3 cheapest covering routes for the fallback and lost 6 old kills (the routes that
run the line most per second find them: kept); its corpus also lacked the functions only ever entered inside other
samples (e.g. ActorType01's constructor, called from C), hence the corpus's second pass.

### 17.2 The two unexported S13 / S15 functions (found with the replay)

The replay made both quick to find: the caller's corpus samples were replayed with the C version swapped in (an
edit to the caller, no new translation), `PENPEN_REPLAY_DIFFS=N` showing the differing words.

- **racer_camera_start** (func_8C03C804): the far point's test was reversed. The original takes the far vector
  when the RaceSetup's count (0x64) is **at most** 4 (or the course starts past segment 0); S15's two versions took
  it above 4. Only the time trial's intro (explore, frame 5604) took the other branch, hence "the eye's position"
  at a race's first frame. Exported; racer_camera_update calls the C.
- **system_language** (func_8C0623B4): the BIOS HLE returns no configuration block (0), and the original then reads
  the language byte at address 0x0C, outside main RAM (the I/O model: a replayed hardware read); the C read it
  through `hw_ptr` (main RAM's byte 0x0C), so game_init stored another language and its sample differed later (two
  extra hardware writes through a stale pointer). It now reads through `hw_read8` (new in hw.h: a byte through the
  I/O model). Exported; video_mode and video_init call the C.

### 17.3 New functions (38: 3 079 -> 3 117; bytes 44.95 % -> 59.01 %)

- **Lead**: the SetupMenu's construction func_8C022D80 (resumable: the loading screen; the Task, the Bodies, the
  files select.dat / the course map's textures by language / penhouse.pvm / house_pen.bin / Owner.bin and
  Owner.pvm, the penguins and the mascot reset, the menu sprite, the first state, the players' words, the springs'
  damping, the house's choices, the camera from the original's 0x40-byte frame, the sound set; tested inside the
  RaceSetup update's 9 samples that build the menu, then by its own); racer_camera_start (func_8C03C804) and
  system_language (func_8C0623B4) exported after their fixes (17.2); video_init / video_mode call the C.
- **Delegated** (brief `port_work/s16/WRITER_BRIEF.md`, four writers, every file merged, difftested and replayed
  by the lead): w1 (b01/b03): the save load, the pause menu's two draws, the RaceSetup's construction and racer
  picks, the RaceScene's build and update; w2 (b04-b06): the racer's contact sort, run (state 3 / state 4), ground
  and walls, the course's draw and weather, the computer racer's look-ahead, actor types 1 (setup, update), 2, 0x19,
  0x24, 0x2B, the actor pair's setup, the pieces' update, the sign's draw; w3: the SetupMenu's states 1-3 and the
  screens of states 2, 3 and 4 (penguin and course selection, costumes, records), the racer's reset; w4: the
  Racer's update after its controls (func_8C0317D4 with the half Ghidra split off as func_8C0319F4), the text
  drawer (Shift-JIS, func_8C01529A), the view's camera upload (func_8C011C7C), the Contacts' sphere-triangle test
  (func_8C013E1C), mode 4's record panel (func_8C028882). Their callers' CALL wrappers were removed (house.c,
  good_night.c, menu_screens.c, sign_draw.c, text_view.c, lane_racer.c, racer_control.c, view.c, course_env.c).
- **Found on merging** (each by the replay of the existing corpus before any route ran): racer_ground (w2) read
  `ground` where the original reads word17c (the low half of a literal-pool word again): the racer updates'
  samples differed in the ground word and the wall push; fixed. The writers' "unsure" notes kept for review:
  text_draw takes the cell of an unknown first character from the caller's r14 (as the original does); the
  record screen divides through the SHC helper (a divisor of 0 is possible); records_draw's tail func_8C027628 is
  a fragment of it.

### 17.4 The Mull debt

Runs (`port_work/s16/mull/run.sh NAME REGEX`: `tools/s11.sh mutate --budget 60 --workers 6 --only REGEX --corpus
/corpus/TAG`), combined with `tools/mull_combine.py` into `port_work/s16/mull/s16_combined.out` (S15's combined
file, then v2, n1, n2: a later run of a file replaces the earlier one) and reported with `tools/mutate.py report-mull
. ... S16`:

| run | files | mutants | killed (replay) | survived | unreached | wall |
|---|---|---|---|---|---|---|
| v2 | actor_types.c, race_draw.c (the measurement) | 426 | 284 (212) | 72 | 70 | 16 min |
| n1 | the 30 files without a result + the files changed or added before it (main, system, video_mode, racer_camera_*, save_load, pause_menu, race_setup_init, race_scene_build / _update, house) | 5 404 | 4 002 (3 394) | 778 -> 745 open | 624 | 99 min |
| n2 | the files with S15 survivors + everything merged after n1 (w2-w4) | 6 249 | 4 198 (3 177) | 809 -> 776 open | 1 242 | 78 min |

**Combined (all 121 files of src/core and src/game): 12 714 mutants, 9 110 killed, 40 equivalent, 2 007 unreached,
1 557 survived: score 71.9 %, reach score 85.4 %** (S16 row of `mutation_sessions.tsv`; S15: 59 files, 4 703
mutants, 81.5 %). The score is lower than S15's because 62 more files were mutated, most for the first time, and the
fallback runs at most 3 routes (S15's final pass ran every covering route; on the two measured files that lost no
kill, but it may on others). Of S15's 255 open survivors: 5 killed, 212 survive again, 38 are on lines that changed.

Largest survivor groups: menu_costume.c 93, effects.c 76, menu_selection.c 71, actor.c 63, actor_bodies.c 58,
actor_misc.c 57, title.c 39, actor_types.c 37, rtc.c 35, race_draw.c 34, actor_type19.c 33, race_scene.c 32,
race_hud.c 32, house.c 32, menu_times.c 30. A first look: many are on paths whose data never varies on the routes
(effects.c's set A walks arrays whose entries are all equal, so walking them backwards reads the same values),
calls of empty or pure methods whose result is unused (title.c's Vec3 constructor), or stores into a scene that is
destroyed the next frame (title.c's state 3 before the mode change): equivalents to note by id; others need inputs
(the RTC's date conversions: one fixed date per route). Not classified this session (the notes file is unchanged).

### 17.5 Routes

Candidates measured with `tools/s3.sh fncover` (temporary rows in `tools/routes.tsv`, removed): the house's fifth
choice with the unlock flags poked (DOWN x4, UP, DOWN x1, then A presses: 0 new functions), the title menu's other
entries (DOWN x3, DOWN x4, UP: 1 new function). None kept; the ending (Game mode 9: RaceSetup state 4) and the
options still need their inputs found by reading the menus' code.

### 17.6 Next

1. The open survivors, with the replay's help: per-sample line coverage (gcov on the replay) would say which
   survivors no sample reaches (unreached, with the input) and which are reached but invisible (equivalent, or a
   comparison gap); S15's 212 first.
2. The functions left in `port_work/s15/b2/`: 8c01bef8, 8c0351c8 (3.5 KB), 8c035fa2 (4 KB), 8c03e150, 8c03ed24,
   8c029efa (4.2 KB), 8c01c668, state 4's update 8c0270d2; then `tools/batches.py` again.
3. Routes for the 2 370 unreached functions (the ending, options, VMU menus, other characters).

## 18. S17: synthetic samples, the draw context, per-sample line coverage, the survivors, more functions

### 18.1 Synthetic samples (`--hwsynth`, `tools/synth.py`)

The routes run 1 640 of the 4 010 game functions; a hand-written function no route runs had no live sample, so no
difftest and no Mull kill (its mutants were "unreached"). S16 measured that new routes add almost nothing. S17 makes
samples without a route, with the live difftest's own machinery (difftest.c, section "S17: synthetic samples"):

1. **A base**: the snapshot (CPU + 16 MiB of guest RAM) of a corpus sample of `c5`: by default the title (boot
   f600), PenPen House (route f1300), the setup menu (route f1750), a 1P race (route f3000), a 4P race (vs4 f3000),
   the third cup's race (cup3 f2700), explore f3700, GOOD NIGHT (f1500); plus up to 4 related samples: the function's
   own, else the samples of its direct callers, else of functions whose side A entered it (their snapshots hold its
   objects).
2. **Prelude calls** (optional, per job: `pre FN A0 A1 A2 A3`): the ORIGINAL code, interpreted on the base with the
   HLE live (a constructor building an object of the class under test, the seconds-to-date making a valid date);
   `$k` names the k-th call's result.
3. **Arguments**: the spec's candidates (`arg I ...`, `farg I ...`: exclusive when given), else values from the base:
   its registers, the pointers on its stack (the callers' live objects), **the objects of the function's class**
   (words of RAM equal to the function's address are vtable slots; RAM words pointing to the start of such a
   vtable are an object's vtable field; `this` = field - its offset + the slot's adjustment, for the usual field
   offsets), small integers, the constants of its own instructions (`cmp/eq #imm`, `mov #imm`, `mov.w` literals),
   numbers of any size. **Pokes**: a word of an argument's object (at an offset the function's own displacements
   use, or any of the first 512 bytes) or of a global its literal pool names, set to a small value, a constant of
   the function, a neighbour, a bit flip, a pool pointer or a float (`nopoke` turns them off).
4. **The recording pass**: the ORIGINAL code interpreted from that state with the HLE live; its HLE results (r0 /
   fr0, the argument registers it read, the stack out-parameters, the RAM it filled) and hardware reads are recorded
   exactly as for a live sample (rt_call's hooks).
5. **The judgement**: the live difftest's `sample_judge` (shared with hwt_end since S17): side A (the original,
   interpreted, HLE replayed) and side B (the hand-written function, HLE replayed) from the snapshot, compared as in
   section 5; a passing sample joins the corpus (`/corpus/syn/NAME_<shard>_<restart>`, volume `penpen-corpus`), a
   FAIL is kept apart (`fail.tsv` beside, for `PENPEN_REPLAY_DIFFS`).

The search is coverage-guided: an input is judged only when its recording pass ran an instruction of the function's
original code that no kept input ran (the extent's halfwords, delay slots included); the kept inputs form a queue
that is mutated (an argument, a float, a poke added or removed). A job ends after ITERS inputs, KEEP kept samples,
600 inputs in a row with nothing new, or 20 s.

**Refused inputs** (counted, never judged): the recording pass ends otherwise than by the return (a fault, the step
budget, njWaitVSync: a frame would end inside the call, a 2 s alarm: an HLE function looping on a count read
through the wrong object, a host fault inside an HLE function); the guest code (not an HLE function) touched memory
no real pointer does (`odd`: any write outside RAM; a read outside the Holly / G2 / AICA / RTC registers, the AICA's
RAM, the store queues and the SH-4's register blocks; code run outside the code ranges: a call through a word that
is no method; a job may allow hardware accesses with `hw`: the RTC, the BIOS block); side B left guest RAM
(`domain`: a host fault in the hand-written C, an index or pointer value the C indexes past the 16 MiB with, where
the original's 32-bit address arithmetic lands somewhere). Argument and poke values that would be pointers only
through a RAM mirror, unaligned, into the SH-4's area or into the code ranges are replaced by small numbers; pokes
never touch the code ranges (side A interprets RAM, side B runs translated code).

**Hints** (`tools/synth/hints.txt`, `--spec-extra`): per function (`for ADDR`, or `for name:C_FUNCTION` for every
export of a C function, its identical copies included): `arg I V ...` (exclusive candidates: an index's range, a
racer `[[8c096230]+4c]`: `[X]` is the word at X of the base, here g_panel's RaceScene's player 0), `farg`, `pre`
(a constructor: `pre 8c043d4c 0 TYPE 0 0` builds an actor of the factory's type, then `arg 0 $0`), `nopoke`, `hw`.
The writers of the S17 batches wrote hints for their functions (class of `this`, index ranges).

**What a synthetic pass proves**: the hand-written function equals the original, bit for bit in the compared state,
on these states: real scenes' objects, with the arguments and pokes above. **What it does not**: that the game ever
calls it with these values (a poke can make a combination the game never makes; a FAIL there may be outside the
function's domain: 18.2); that the paths no input reached are right (the instruction coverage says how much ran);
anything about a call that ends a frame (refused: those stay route-only).

### 18.2 Results (`docs/progress/synth_coverage.tsv`, `tools/synth/notes.tsv`)

Runs (`tools/synth.py run NAME LIST --iters 3000-4000 --keep 12 --par 1-2 --spec-extra tools/synth/hints.txt`): h1
(every hand-written export no route runs, 1 519, and the 530 reached ones that own open Mull mutants), h3/h4 (h1's
FAILs again, after the domain rules and hints), n1-n12 (each merged batch's exports), t8-t11 (the RTC with `hw`,
spread seconds; sound_select_set), d1 (`--deep 40`: the 136 reached functions whose open mutants no sample reached).
The newest row per function:

| | functions | with passing synthetic samples | samples | original's instruction bytes run (of those with samples) |
|---|---|---|---|---|
| no route runs them | 2 093 | 2 043 | 2 308 | 48 542 of 51 812 (93.7 %) |
| a route runs them (owners of open mutants) | 546 | 497 | 1 278 | 87 868 of 119 546 (73.5 %) |

Without a sample (99): the functions whose every input ends a frame (the loaders, the scene constructors: route-only),
those whose prelude cannot run on the bases (the gallery / options constructors load files that are not there),
and a few whose inputs the domain rules refuse. Six samples kept earlier were removed from the corpus when later
merges made the C fault on their out-of-domain inputs (an index taken from a pointer: notes). Real differences found: **matrix_axis_angle** (b01) stored the axis
before reading the matrix (an aliased axis: fixed, 18.6); the **tasktree copies** (b02) and **race_draw_view_three**
(b04) and **sound_select_set** first FAILed through three test gaps, fixed: a P4 "pointer" read as a register
(refused now), the HW_RETURN marker of a function Ghidra cut short (sbx_steps), and an HLE out-parameter in the
caller's frame written with the value already there (the frame diff missed it; syCfgGetSoundMode now reports it
through `hwt_hle_wrote`, kept as a stack word). The **8 FAILs left** are judged outside the functions' domain
(`tools/synth/notes.tsv`): the container fix-ups on arbitrary RAM (5: the "file" aliases itself and the base),
mascot_update with SetupMenu.course / motion poked past their tables, type19_step2 on an object of another class,
race_scene_env_push on a "scene" that is no RaceScene; each passes on in-domain inputs. All passing samples replay
with 0 FAIL on the final build.

### 18.3 The draw context in the HLE comparison

S16's Mull survivors in race_draw.c (time_draw's `x + 0x2C` mutated to `- `, a removed matrix push) were
invisible: the difftest compared an HLE call's registers only, and a sprite whose position is set, drawn, then set
for the next digit leaves no trace in the final RAM. difftest.c's `ctx_of` now records, per Ninja draw call (njDraw*,
nj_Model*, nj_Object*, nj_Motion*, nj_ShapeMotion*, njCnkModDrawModel), the current matrix (3D draws only), the
render state the HLE reads (njControl3D, the constant attribute masks and material, the texture list: a texture
list on the stack counts as one token, as in section 6), 32 bytes at each argument register that points into RAM
outside the stack (compared for the registers the call uses), and FR4 for njDrawSprite2D / the motion draws (Ninja's
njDrawPolygon takes no float: its FR4 is the caller's leftover). Corpus samples carry it (header flag; older samples
are compared without it). The 12 routes' difftest passes with it (0 FAIL).

### 18.4 Per-sample line coverage (`tools/linecov.py`, `tools/s3.sh linecov ROOTS P`)

src/ compiled with gcc --coverage, linked with the hand build's other objects; every sample of the corpora replayed
(`#all`), one gcov profile per process; "lc FILE LINE COUNT" per line some sample's side B ran. c5 alone runs 9 466
lines of src/; c5 + syn runs 15 092 (S17's code included). It splits the open mutants (18.5) into "no sample reaches
the line" (the replay gate cannot see it) and "reached but invisible".

### 18.5 The survivors

Runs (`port_work/s17/mull/*.out`; `--corpus /corpus/c5,/corpus/syn`): **r** (the r1-r3 files and their callers:
1 741 mutants, 1 160 killed, 242 survived), **m2** (b01-b03, b05 and the files they touched: 1 380, 909 killed: 861 by
the replay, nearly all synthetic; 6 survived), **m3** (b04, b06-b08: 1 932, 1 053 killed, 11 survived), **m4** (b09,
lane_markers.c: 645, 383 killed, 27 survived); **s** (S16's open mutants of the files S17 did not change again, by id:
`mull_run.py --ids`, 3 059 mutants: 808 killed, 783 of them by the replay of synthetic samples), **s2 / s3** (rtc.c's
45 again after a hint spread the seconds over 1950-2018: 7 more killed). Combined (`tools/mull_combine.py`: S16's
combined file, r / m2-m4 by file, s / s2 / s3 by mutant: `--per-mutant`):

**16 960 mutants, 12 496 killed (9 365 + 307 by the replay), 38 equivalent, 3 163 unreached, 1 263 survived (open):
73.8 %, reach 90.8 %** (S16: 12 714, 71.9 %, reach 85.4 %, 1 557 open). Of S16's 1 557 open survivors: 457 killed,
2 equivalent, 108 on lines S17 changed (re-mutated with their files), 990 still open. Of S15's 212: 56 killed, 2
equivalent (rtc_read_seconds' debug_log_word calls: the debug log's flag is never set), 1 on a changed line, 153 open.

Per-sample reach of the open ones (`docs/progress/mutation_sample_reach.tsv`, from `tools/linecov.py`): survived
1 263 = 1 059 on lines some sample runs (reached but invisible: equivalents or comparison gaps) + 204 on lines only
the routes run; unreached 3 163 = 936 on lines synthetic samples run without seeing the change (in substance open)
+ 2 227 that no sample and no route runs. S15's 153: 130 reached-invisible, 23 no sample. **Not done**: the per-id
classification of the reached-invisible ones (equivalent with a reason, or a comparison gap to close: the S16 look
found dead stores before a scene's destruction, calls of empty methods, arrays of equal entries walked backwards).

### 18.6 New functions (689: 3 117 -> 3 806; bytes 59.01 % -> 77.55 %)

Delegated (brief `port_work/s17/WRITER_BRIEF.md`: S16's with S17 additions: the batches of `tools/batches.py`'s new
`ALL=1` mode, the dumps of the 547 left functions that the S14 dumps lacked (`port_work/s17/dump`, Ghidra decomp2 /
asm in the sandbox), synth hints), every file merged, compiled together (duplicate symbols / exports checked),
difftested and replayed by the lead:

- **r1-r3** (S15's b2 list, route-backed): the racer's effects (func_8C0351C8) and sounds (func_8C035FA2), the
  computer racer's state 3 (func_8C03E150: lane_racer_drive) and state 4 (func_8C03ED24: lane_racer_steer), the
  race scene's race step (func_8C029EFA, called by race_scene_update: state 3 is the race itself), the title's
  intro (func_8C01BEF8) and logo draw (func_8C01C668); new title.h, lane_markers.h. func_8C0270D2 was S16's
  (penguin_look_update). **Found by the route difftest**: lane_racer_steer's stroke clock read range2_min (0x658)
  where the original reads range1_max (0x654) (the lane racers' samples differed in two clocks): fixed.
- **b01-b09** (mostly unreached; synthetic): task trees, pad axes, the 12 motion / shape-motion drawers, the container
  fix-ups no route loads, the CD (directory change, stop, release, the error callback), 2D lines / triangles,
  planes, big text, texture lists, contacts trees; the save records (rewards, flags, times, ranks), the VMU drive
  (callbacks, progress), the memory-card messages, the motion player's accessors, the title fade, the staff roll
  (mode 9: the credits' scene update / release; its construction loads files: not written), the gallery (mode 11)
  and options (mode 12) scenes, the trap set and cast scene, racer / RaceSetup field accessors, the race sound
  ports, the three-player views, the after-race menu, the racer's swimming (state 5) and action 4, the replay racer
  and the plain racer, the owner scene, motion attachment; actor types 0x00, 0x06, 0x09, 0x0A, 0x0B, 0x0F, 0x10,
  0x1B, 0x20's draw, 0x24's draw, 0x26, 0x27, 0x28, 0x2F, 0x30, 0x31, 0x32 and the lifetimes / draws of many more
  (post, balls, wobbler, flap, flock, turning gate, geyser, swimmer, buoy, rings, ornaments, swingers), the RTC's
  setters and compare. Merging removed the callers' CALL wrappers it could (fixup.c, file.c, texture.c, vmu.c,
  sign_draw.c, race_step.c) and the commented-out exports it superseded (model_build.c's 8c01509a, menu_penguin.c's
  8c022c70). **Found by a synthetic sample**: matrix_axis_angle (b01) stored the axis before reading the matrix's
  last differences (the original reads all three first; a caller passing the matrix as the axis): fixed.
- Left (204 functions, 67.6 KB): the large ones (func_8C0200E8 4.6 KB, func_8C02AF9C, the type 0x32 update, ...),
  those the dumps cut short, the frame-ending loaders and constructors, fragments (tails of other functions).

### 18.7 Next

1. The 1 059 + 936 reached-invisible mutants, by id (`mutation_sample_reach.tsv`): the equivalents with reasons; the
   gaps the S17 draw context does not close (an HLE call reading a stack structure: 32 bytes are hashed only off the
   stack; HLE functions that write out-parameters without `hwt_hle_wrote`; `diverse` samples for arithmetic).
2. The 204 functions left (67.6 KB): the large ones first (func_8C0200E8, func_8C02AF9C, the type 0x32 update
   func_8C060706, func_8C0413FA, func_8C03F470 ...), those the dumps cut short (decode the tails), the loaders.
3. Synthetic samples for the 99 functions without one: preludes for the gallery / options scenes (their
   constructors load files: a base whose scene is built), racer methods through `[[8c096230]+4c]` hints,
   `diverse` for the remaining arithmetic.
4. Routes for the options (mode 12), gallery (mode 11) and the staff roll (mode 9): their scenes are now C; their
   inputs are in the title / house menus' code (S16 tried the title's DOWN x3/x4: 1 new function).

## 19. S18: the last functions, fragments, routes for modes 9 / 11 / 12, frame-passing preludes, test gaps

### 19.1 Functions (199: 3 806 -> 4 005 of 4 010 (99.88 %); bytes 233 516 -> 300 222 of 301 130 (77.55 % -> 99.70 %))

Six delegated batches cut by `tools/batches.py` (`port_work/s18/b/b01-b06`, brief `port_work/s18/WRITER_BRIEF.md`:
the S17 brief with S18 additions: a linear disassembly of the functions the batch dumps cut short, `switch` tables,
phase helpers for the big ones, the callers' wrappers to replace) and two single ones (func_8C016C44, whose
literal pool no dump had: fetched with `tools/ghidra.sh asm`; the owner scene's construction func_8C02F850,
resumable); the lead wrote the cast viewer's construction (func_8C02004C, func_8C01FE58: resumable). Merged by the
lead (headers patched against the writer's base, the callers' CALL wrappers replaced, compiled together), then
route difftest, synthetic samples, corpus replay, pure vs hand.

- **b02**: the cast viewer (a debug scene no code builds: CastScene, TrapSet and its objects): its update
  **func_8C0200E8** (4.6 KB, in phase helpers), its draw, the TrapSet's population, draws, attach; the config
  scene's entry draw; the staff roll's construction, draw and penguin.
- **b03**: the race's ending **func_8C02AF9C** (outcome, slide, finish, the results menu), drops / trails / ribbon
  effects, the lane racer's debug draws, mode 2's start, the owner scene's constructor (resumable).
- **b04**: actor types 0x06, 0x0F, 0x19 (steps 0, 1, 3), 0x28, 0x2F, 0x0B's response, the effect actor's draw /
  burst / free, contact responses, the course's ground search below a point, the start position.
- **b05**: actor type 0x32's update **func_8C060706**, types 0x10, 0x25-0x27, 0x30, 0x31, the buoy / geyser / swimmer
  / rings draws, two mesh responses.
- **b01**: the contacts' model builders, sphere / segment / triangle geometry, sbInitSystem / sbExitSystem (the
  interrupt mask through SR, as the original's ldc), the number sprite, the staff roll's build, results / ending
  text, the pause and ending menus' lines, the save write, the VMU info, the game's shutdown, the start routines of
  modes 1, 9, 12 (11: see 19.5).
- **b06**: the results' pulse and banners (the code after njSin that every dump lost, decoded from the raw bytes),
  the config scene's leave entry, the lane racer's state 5 (its real extent runs past functions.tsv's size), effect
  set A's push, the racer camera's states 1 and 5, the lap times, the racer's state 1 and swim strokes.

**The writers' "unsure" notes** (their reports in port_work/s18; each function was then judged by the difftest): the
race ending's untested branches (the VMU / progress unlock path, record times for mode != 0) and the type 0x30 /
finish-camera reads of stale stack slots past a step of 2 / a part above 4 (the C reads the same guest frame slots:
equal only when the frames are, see 19.4's cup4b note); calls through game thunks written as calls of the SDK
function (njCalcPoint via 0x8C03880E, floorf via 0x8C028E32); inlined style resets written as calls of the helpers
(text_style_reset, text_set_scale: the same final stores, another order, no aliasing); the trail object (no
constructor: untested); 8c016E98 / 8c016FD4's lost pool words inferred from their siblings (route-tested since: the
race results); sbInitSystem's SR access (the interpreter's own sh4_set_sr).

**Fragments** (`HW_EXPORT_FRAGMENT(addr, parent)`, hw.h, hw.c): 50 "functions" of functions.tsv are pieces of
another function: Ghidra split the function at a literal pool, at an epilogue its branches reach, at a case tail, or
after a call it took for one that never returns (njSin); four are literal-pool data. Each parent is hand-written C
doing the piece's work; checked per piece: no call to it but from its parent, no entry in entries.tsv, no pointer to
it in the binary but in its parent's pool. recomp.py leaves its translation out (it is an HW_EXPORT*), and entering
it alone stops the run with a message. `decomp_progress.py` counts them (kind `fragment`).

### 19.2 Routes (`tools/routes.tsv`: 18 routes)

- **options** (mode 12, the language set-up): the title menu never sets choice 3, so Game.next_mode (Game at
  0x8CE16B00 + 0x10) is poked to 12 on the title; the cursor moved every way, B leaves; again, A sets a language and
  saves (no VMU).
- **gallery** (mode 11): its files (8098gallery.pvm, the pictures' PVMs) are not on this disc (the US release): the
  original's loader retries for ever. The route pokes the scene's file name and two pictures' names to
  4000config.pvm and unlocks two pictures, then next_mode = 11: every direction, A twice, Start.
- **staffroll** (mode 9, reached on hardware after a cup is won): next_mode = 9; its texlist loads through njLoadTexture,
  whose SDK code (guest) failed for ever on the HLE's texture manager: an HLE njLoadTexture now reads each texname's
  file and decodes it (nj_draw.c, the PVM loader's decoder); the phases' countdowns are poked short, Start leaves
  for mode 4.
- **cup1b, cup2b, cup3b**: DOWN on the course map picks the cup's other course (the cup routes race CSB2, CSC2, CSD2
  by default): CSA, CSB, CSC. **cup4b** (CSD) is measured but not kept: 19.5.

### 19.3 Test paths

- **Synthetic preludes that pass frames** (`frames`): a constructor that shows a loading screen ran into
  njWaitVSync and was refused; in a prelude the frame now passes (hle.c `hle_frame_pass`: the end-of-render
  callbacks and the V-blank's work, no frame advance, no yield), up to 900 frames. The judged call still refuses a
  frame's end. With it: the cast viewer, the owner scene, the TrapSet are built on any base.
- **Files in synthetic runs** (`slow`): the synthetic process runs no frame, so the game's gdFsInit never ran there
  and every gdFsOpen failed (a loader looped to the step budget): the drive is attached to the game's work area
  (0x8C1AF120, 8 handles) at start; `slow` gives a loader 30 s and 20x the steps.
- **Stack arguments** (`stk K V...`: the 5th.. argument @(4K,r15)); a later hint block replaces an earlier one (S18's
  hints after S17's); `synth.py table` keeps a row with samples over a newer one without.
- **Replay**: side B's runaway (HLE calls or hardware reads 100 000 past side A's) is a FAIL: a mutant looping on a
  replaced status (`while (gdFsDaStop() != 0)`) ran into the 120 s time-out, which is not a kill. Corpus directories
  64 -> 1024: past 64 the samples were "unreadable", not judged, silently (S18's routes and synthetic runs passed it).
  Side A notes a function it runs into by falling through (a fragment), not only by a branch.
- **Mull gate**: a static helper with no address took every sample of its file, cut at 160: the samples that run it
  with the input a mutant needs were often cut off (menu_costume.c's grid moves); it now takes its callers'
  addresses in the file (transitively).
- `PENPEN_REPLAY_DIFFS` in the live difftest too (`tools/s3.sh` passes it), and a FAIL lists the hand-written
  functions side A entered (the candidates).

### 19.4 Bugs found

- **race_scene_ending** (b03, func_8C02AF9C): handed the RaceSetup words44[player] = place where the original stores
  words44[place] = player (the inverse permutation; func_8C029074 is words44[r5] = r6): a synthetic sample, fixed.
- **vec3_blend_lost** (b04, func_8C042D04): exported reading r4 / r5 where the original reads r5 / r6 (r4 unused):
  a synthetic sample with distinct registers, fixed.
- **racer_effects_ribbons_emit** (b02, func_8C01F752): a tail-jump thunk to a void method exported with RET (r0
  was the callee's leftover): a synthetic sample, fixed (void).
- **results_draw_banner_top** (c44, func_8C016C44): the pulse step written 0x44888888 where the pool holds
  0x44088888: found reading the writer's file against the asm, fixed before the difftest.
- **HLE**: the SDK's njLoadTexture (guest code) never succeeds on the HLE texture manager: texlist_load (S13 C)
  and the staff roll looped for ever: HLE njLoadTexture (19.2).
- **Test gaps** (19.3): synthetic gdFsOpen always failed (no gdFsInit in the synthetic process: every loader input
  refused); corpus directories past 64 silently "unreadable" (in replays and the Mull gate); a side-B runaway was
  a time-out, not a kill; a static helper's Mull samples were cut at 160 among its whole file's.
- **Open** (19.5): on the fourth cup's other course (CSD, route cup4b) pure and hand differ from frame 1764 (the
  Rings actors' sizes and turns, and errno 0x8C1BAC68 set to EDOM 0x44D by one side's atan2f); no single group of
  exports (S18's 97, old actors, course / race scene, all resumables, all copies) brought it back translated: not
  found. Narrowed: at the end of frame 1764 (the course built) only 0x8C1823F0 differs (a pointer into the stack: a
  token for the digest); from frame 1765 the Rings (type 0x2C) sizes / turns and errno; every function passes its own
  samples. A guess to test first: original code reading stale stack slots (the writers flagged such reads in type
  0x30's part draw and the finish camera), whose contents differ because C callers do not write the saved-register
  words the translation writes. Also measured: at frame 1765 the hand build's errno (0x8C1BAC68) is EDOM (0x44D)
  and the pure build's 0: a math call gets an out-of-domain input in the hand build only; translating every export
  of the 37 files that call the runtime's math functions (276, minus fragments' parents) does not remove it, so a
  translated caller gets a different input from C code. type 0x31's update, face_player and hold (b05) differ on that route and in synthetic samples.

### 19.5 Not ported, with reasons (5)

5 of 4 010:

- **func_8C05F380, func_8C05F642, func_8C05F7B0** (actor type 0x31's update, its turn toward the player, its hold;
  C written by b05, commented out with the evidence in game/actor_type31.c): on cup4b the face-player sample differs
  (the original's direction to the player is the unit vector njUnitVector makes inside race_scene_nearest_player,
  side B's is not: atan2 then differs) and synthetic samples of the hold / update differ; not understood. What it
  would take: find the cup4b divergence (above), then difftest these three on that route.
- **func_8C01A2AE** (mode 11's start routine; C written, b01): the gallery route's whole run equals the pure one with
  it, but the difftest's gallery_update sample in the mode's first frame differs in a gdFsRead sector count (a stack
  out-parameter of the file load replayed at another depth). Measured at the end of S18: the sample's HLE sequence is the
  same on both sides (njReleaseTexture, gdFsOpen, gdFsGetFileSctSize, gdFsRead ...); side A (the original texture_load,
  which makes the file calls itself: it entered no file_load) reads the sector count from a slot the replay did not
  write, while the C texture_load goes through file_load's frame: the out-parameter sits at another r15 offset on the
  two sides, and with the translated start routine the stale slot happened to hold the right count (the frame's first
  load). The fix: texture_load's C frame laid out as the original's (DECOMP 15 frame rule), then export.
- **func_8C042000** (the second half of func_8C041F9C, the start / finish position body, which stays translated:
  course_positions.c computes it in C for its two callers): a fragment cannot be declared while its parent's
  translation can run into it; it goes with func_8C041F9C (not in functions.tsv's game list: Ghidra put the
  boundary at 8C042000).
- (Written last and kept: func_8C04CC88 and func_8C055544, the tail of a vec3 difference copy whose true entry
  0x8C04CC84 / 0x8C055540 is not a function of functions.tsv: the translation of that entry runs on into the piece
  with FR7 / FR8 / r5 set; func_8C057698, the last two stores of a four-float setter entered the same way, r0 = 12:
  game/pieces_s18.c, written over the registers they inherit (hw.h INHERITED_R0). The stub func_8C023858, a
  fragment of the SetupMenu construction: no call, no pointer.)

### 19.6 Mutants

S17 left 1 995 open mutants on lines some sample runs (1 059 survived + 936 "unreached" by the routes; the per-sample
reach of `mutation_sample_reach.tsv`). S18 worked on them in three ways:

- **Gaps closed** (19.3), each of which made mutants survive whatever their code did: a side-B loop that never ends
  was a replay time-out (not a kill); past 64 corpus directories, samples were "unreadable", not judged; a static
  helper's samples were its whole file's, cut at 160 (the mutants of menu_costume.c's grid moves never met a sample
  that pressed a direction). The re-run by id with the closed gaps (`port_work/s18/mull_ids2.txt`: the 1 694 ids not noted and not in a file S18 changed) was not run this session: the first step of the next.
- **Equivalents by rule, checked per site** (`port_work/s18/mut/classify.py`; a rule applies only when the line is
  unchanged since the mutant was made): 75 written to `mutation_notes.tsv`: removed calls of `method_empty()` (an
  empty C function: 11); `method_self(x)` statements whose result is dropped (it returns x: 48); the Matrix
  constructor's identity (`matrix_identity(m)`) that `njUnitMatrix(m)` on the next line writes again (12); removed
  `array_construct(n, size, p, ctor)` where ctor is the empty method (the runtime helper 0x8C062800 stores only
  into its own frame and calls the constructor per element: 4).
- **Clusters that are not equivalents** (left open, by reason): replaced comparisons (`fgt`, 65), replaced cell /
  view queries (`race_player_cell`, `race_scene_view_bit`, `race_scene_cell_attr`, `save_flag`, ...): inputs the
  samples do not vary (a synthetic `diverse` run per function, or the routes of the new courses, would kill them);
  removed `vec3_zero` / `body_reset` in constructors whose fields are then rewritten (most are equivalent in
  practice, but the proof needs each constructor's later stores: not written as a rule).

Mull on the S18 code (run n1: the 74 files S18 added or changed, corpora c5 + syn): **6 340 mutants of 73 files (stopped at the end of S18 in core/contact_models.c line 208; core/inline_b01.c, sb_system.c, sphere_geometry.c and sprite_number.c not reached), 3 743 killed (3 475 by the replay), 363 survived, 2 234 "uncovered" (no route runs the line and the replay of its samples saw no difference: mostly the dead cast viewer, type 0x30 / 0x31, contact_models.c, the owner scene's construction), 49 equivalent by the rules: 59.5 %, reach score 91.2 %** (port_work/s18/mull/n1_gate.log; not combined into mutation.tsv: a partial run). The survivors are not classified one by one beyond the rules; the clusters: the soft-reset combination's pad tests (game.c: the fallback routes chosen for the line do not press A+B+X+Y+Start), type 0x28 / 0x19 / 0x10 / 0x26 updates (inputs the samples do not vary), the staff roll's draw and build (one route).

### 19.7 Results and next

**4 005 of 4 010 game functions (99.88 %), 300 222 of 301 130 bytes (99.70 %)** (S17: 3 806, 77.55 %); 50 are
fragments. Routes: 18, running 1 960 game functions (S17: 1 640). Difftest: 18 routes, 52 622 samples, 0 FAIL; the
corpora replay 0 FAIL (c5 + syn, 19 157 samples); synthetic samples: 2 702 functions with passing ones. Pure vs hand:
identical frame lines on all 18 routes, native and wasm; native = wasm. sh4 257 / 0, mixer 26 / 0, harness 5 736 / 0 / 207, `GEN=genhand tools/rtw.sh build` + check OK, the browser
e2e passed (the first since S10). Never executed by a test: 13 own functions (+ 93 copies whose group has an executed
member).

Next:

1. The cup4b divergence (19.4 / 19.5): a RAM diff per frame from 1765 on, the translated math caller that gets an
   out-of-domain input in the hand build; then type 0x31's three functions on that route, and route cup4b kept.
2. Mode 11's start: why the difftest's replay of gallery_update's first-frame sample differs (the stack out-parameter
   of the file load, at another depth when the start routine is C).
3. The open mutants by id with the closed gaps (`port_work/s18/mull_ids2.txt`), then the rules of 19.6 for more
   clusters.
4. func_8C041F9C + func_8C042000 (the position body) written together; the 13 never-executed functions (routes on the
   CSD course once it is equal; the cast viewer and the owner scene are dead or unrouted: a resumable judged sample).

## 20. S19: cup4b's root cause (fresh frames read as zero), the game region's last functions, the code outside it

### 20.1 cup4b: the root cause

S18 left the fourth cup's other course (CSD) out of the routes: pure and hand differed from frame 1764 (S18 wrote
1765; the frame-log digests show 1764: the course's build frame). Found this way:

- the difftest restricted to that frame and every outermost call sampled (`PENPEN_HWT_FRAMES=1764`, `--hwtest
  1000000:0`): one FAIL, actor_group_draw (errno 0x8C1BAC68 = EDOM on side B); with that function skipped
  (`PENPEN_HWT_SKIP`: its callees sampled instead) every callee passed. So the difference came from something the
  per-function comparison does not see.
- `PENPEN_REPLAY_DIFFS` listed the sample's differing words: type 0x31's actors, one holding a code address
  (0x8C05F4E4, a return address inside type 0x31's update) on side A: a stack word copied into an object.
- a debug build (`-DPENPEN_DBG`: `PENPEN_DBG_PC` reports an op address's registers and the words under r15,
  `PENPEN_DBG_W` every write into a RAM range with the writer's op address): race_scene_nearest_player
  (func_8C02C922) is called from type 0x31's face_player with every racer about 10 300 away; nothing is nearer than
  its 10000 limit, so its "best" Vec3 (frame + 0x18) is never written and is copied out as the direction: the actor
  turns toward stack garbage every frame until a racer comes near (atan2f(0, 0) set EDOM in the hand build). The
  write watch on that slot in the pure build: its words came from func_8C05E440's register pushes, func_8C049A48's
  locals and func_8C0121FE's argument spill, all hand-written C in the hand build (no guest frames, so the hand
  build's r15 there is 0x12C higher and the slot holds other words).

The original reads uninitialised stack; what it reads on hardware is whatever earlier code left at that depth,
which readable C cannot reproduce. **The fix defines it for both builds**: lowering r15 clears the words it uncovers
(`sh4_stack_lowered` in sh4_ops.h: `add #-n,r15`, `add Rm,r15`, `sub Rm,r15` in the translation and the
interpreter; `hw_stack_alloc` in C). A fresh frame reads as zero; the pure build differs from hardware only where
the original reads a word it never wrote. With it cup4b's pure and hand runs are identical over all 3 300 frames,
the 18 other routes stay identical, the S18 exceptions pass (type 0x31's update / face_player / hold: S18's
"to_player not normalised" was this garbage; mode 11's start: S18's stale sector-count slot), and cup4b is route 19.
Other stale reads the writers flagged (type 0x30's part draw past part 4, the finish camera) are defined the same way.

### 20.2 The game region

- **S18's five exceptions**: type 0x31's update / face_player / hold exported (cup4b's difftest; synthetic samples
  of all three pass, the hold's 104 of 204 bytes: no route holds a racer); mode 11's start exported (the gallery
  route's difftest passes and pure = hand on it); func_8C042000 declared `HW_EXPORT_FRAGMENT(8c042000, 8c041f8c)`
  (the second half of func_8C041F8C after its njSin call; func_8C041F70 jumps into the shared body; both callers are
  C, so nothing else enters it). functions.tsv's 4 010: **4 010 / 4 010, 0 exceptions**.
- **functions.tsv missed game code** (20.3): 36 functions the routes run (26 translated through entries.tsv, 10 in
  the interpreter) are added to functions.tsv (region game, evidence `s19-gap`, extent to the next start, so their
  byte counts include a trailing literal pool): the game counts **4 046** functions. Batch u (a delegated writer:
  `port_work/s19/stage/u`, merged and tested by the lead) wrote 30: the title's draw and menu draw, the logo scene's
  draw, the init methods of the pieces, spinner, types 0x1C / 0x25 / 0x06 / 0x26, the rings and the flock, the
  chain's view draw and 18 out-of-line setters (`core/inline_u.c`). Batch u2 (a second writer, `port_work/s19/stage/u2`) wrote the last six (1.2 KB each: type 0x2B's init, the balls' and the ornament's update methods, the wobbler's, type 0x19's and the sign's init). Each of the 36 passes its route difftest (own samples or inside its caller's) and 33 have passing synthetic samples. **4 046 / 4 046 game functions, 0 exceptions.**
- **Never executed by a test** (a route runs it, a corpus sample tests it or its side A enters it, or a sample runs its C lines: c5 / c6 / c7 / syn, `port_work/s19/never.txt`): **13 own functions**, the same 13 as S18 (cup4b reached no new one): the cast viewer's and the owner scene's construction (dead / unrouted, resumable), the contacts' posed builders (contacts_add_cnk_model_posed, contacts_add_object_posed), effect_trails_push (the trail object has no constructor in the binary), four contact responses (post, type 0x0B, the turning gate, type 0x2D) and types 0x30 / 0x32's draws (no course of the routes places them in view); 15 fragments (no code of their own); every one of the 52 copies has an executed member in its copy group.
- **The writers' "unsure" notes (19.1)**, checked: the stale-slot reads (type 0x30's part draw past part 4, the
  finish camera) are now defined (20.1) and their samples pass under it; the thunk calls written as SDK calls
  (njCalcPoint via 0x8C03880E: the thunk pushes r4 under r15 and jumps, a word no later code reads before a fresh
  frame clears it; floorf via 0x8C028E32: a plain jmp) are equivalent; the race ending's VMU / unlock branches and
  the trail object stay without a route (synthetic only); sbInitSystem's SR access goes through the interpreter's
  sh4_set_sr like the original's ldc.

### 20.3 The code outside the game region: measured (`tools/outside_measure.py`, `docs/progress/outside*.tsv`)

The hand build's translation (port/gen of `GEN=genhand`) holds every function the trim keeps that src/ does not
export. `tools/s3.sh fncover` with `FNCOVER_HAND=1` compiles that translation with gcc --coverage, links it with the
hand-written objects and runs the 19 routes: the translated functions the hand build still runs, and (from the run
logs) every address the interpreter fallback was entered for. `tools/outside_measure.py` tabulates both by library
(functions.tsv's region; addresses functions.tsv does not list are `*_unlisted`, below 0x8C062440 `game_unlisted`).

Measured on the final hand build (S19's end; `docs/progress/outside.tsv`): **1 895 translated functions (405 KB)
remain in the hand build's translation; the 19 routes run 96 of them (12 KB); the interpreter is entered nowhere**
(before batch u: 133 functions, 22 KB, and 10 interpreter entries). By library, executed / translated:

| library | executed (bytes) | translated (bytes) | hand-written |
|---|---|---|---|
| game (functions.tsv + S19's 36) | 0 | 0 | 4 046 |
| game_unlisted (gap code with an entry or a branch, never run by a route) | 0 | 54 (10 052) | 0 |
| Ninja (matrix stack, transforms, trig, view / camera / screen setters, chunk read, fog table) | 49 (8 800) | 527 (250 140) | 0 |
| shc_rt (the SHC C runtime) | 21 (1 616) | 98 (14 614) | 12 |
| sy (heap, boot pieces) | 7 (394) | 71 (7 274) | 0 |
| bu (buMakeBackupFileImage, thunks) | 6 (596) | 84 (12 712) | 0 |
| sy_start (boot) | 4 (170) | 6 (182) | 0 |
| unknown | 4 (362) | 30 (1 260) | 0 |
| Kamui | 2 (34) | 69 (13 754) | 0 |
| bios_stub, gdFs, pd (thunks) | 3 (22) | 131 (9 768) | 0 |
| CRI (Sofdec / ADX / mw / SJ), sd (Manatee), kd, and the unlisted SDK entries | 0 | 825 (84 874) | 0 |


Two findings:

- **functions.tsv's game region is not the whole game.** Ghidra never disassembled code reached only through
  pointers: actor types' init methods in vtables (type 0x19's init at 0x8C0505A4, 1 232 bytes; the wobbler's at
  0x8C04FB4E; ...), and the tiny inline setters SHC emitted out of line in each unit (Vec3 from fr4-fr6, a Body's
  damping, single fields). entries.tsv (S4) gave 53 of them a translation; ten more ran in the interpreter.
  `tools/gaps.py` lists the gaps (579; 27 KB that is not a literal pool), the pointers into them and the
  interpreter's entries. They are game code: S19 adds them to the game's inventory (20.2).
- **The executed rest is small**: the routes run 96 translated functions (12 KB) of 1 895 (405 KB); the other
  1 799 (the CRI Sofdec / ADX player, Manatee's guest code, the GD-ROM / pad / VMU drivers the HLE replaces, most
  of Kamui and Ninja's draw paths the HLE replaces) are translated but never entered: they can go once the
  executed ones are C and the trim follows the C's calls.

### 20.4 Plan: no executed code in port/gen, the interpreter never entered

1. **game_unlisted**: the 36 the routes run are done (S19 batches u / u2, now in functions.tsv). Left: 54 translated
   gap entries (10 KB) no route runs (most are static bsr targets inside other gap code, some pointer-only entries)
   and the gaps `tools/gaps.py` flags as code with no entry: each goes to functions.tsv when it is confirmed as code
   (a pointer to it, a branch into it), then C with synthetic samples, like the 36.
2. **shc_rt** (the SHC C runtime, 29 executed): the float maths are C (S19: atan2f, asinf, acosf, floorf and their
   kernels, 12 functions); next the integer division helpers (0x8C062464 / 0x8C062518: r1 / r0 with SHC's register
   convention: an adapter that reads r0 / r1 and leaves r1 as the original does), the block copies (0x8C0625E0 /
   0x8C0625EC and the unrolled entries 0x8C0625FE..0x8C062636 that SDK code jumps into: one C function per entry
   count), the array constructor 0x8C062800, rand (0x8C0638F4 / 0x8C064B58), the stream-table helpers
   0x8C065538 / 0x8C064BF8 / 0x8C064C9C (1.1 M calls: the stdio table at *0x8C0998A8), 0x8C062B70 and its four
   helpers. Gates: route difftest + synthetic samples with every input class (hints in tools/synth/hints.txt).
3. **Ninja** (49 executed: the matrix stack, njCalcPoint / Vector, njInvertMatrix, njMultiMatrix, the rotations,
   njUnitVector, njSin / njCos / njSqrt / njRandom, the view / camera / screen setters, nj_ReadChunkFile, the fog
   table): hand-written C in src/sdk/ninja_*.c over the same guest globals (the current matrix and the stack S5's
   draw HLE reads), with fpu.h for every float operation (fipr / ftrv / fsca / fsrra as the SH-4 computes them:
   ftrv needs a helper in fpu.h, tested by test_fpu.c). These are the hottest functions (njInnerProduct 17 M
   calls on the routes): the C also makes the hand build faster.
4. **sy** (syMalloc / syFree / syMallocInit: the allocator over guest RAM, kept bit-exact because the game's
   pointers come from it), **sy_start** (the boot code: resumable, on the yield chain like main), **bu**
   (buMakeBackupFileImage and four thunks), **Kamui** (2 fog leaf calls), **bios_stub**, the gdFs / pd thunks and the
   four `unknown` functions: C, or the HLE where it already does the work (a thunk into an HLE function becomes a
   direct call).
5. **The interpreter**: since batch u the 19 routes enter it nowhere (`outside.tsv`); after 1-4 `rt_call`'s fallback is made a stop in a
   build option (an entry no test reached would stop the run instead of interpreting), and recomp.py's trim follows
   the C's CALLs instead of the translation's, so port/gen shrinks to the code no route runs.
6. Each step keeps the gates: the route difftest (19 routes), synthetic samples for what no route runs, pure = hand
   (native and wasm), the corpora replays, Mull on the new C.

### 20.5 Mutants

- **n2** (S18's n1 stopped before these: contact_models.c from line 208 on, sb_system.c, sphere_geometry.c,
  sprite_number.c, inline_b01.c (no mutants); with S19's actor_type31.c and lib/shc_math.c; corpora c6 + syn):
  1 150 mutants, 798 killed, 233 uncovered, 119 survived. The 119 by cluster (`mutation_notes.tsv`, one reason per
  file that holds for each id): sb_system.c 13 *accepted* (SR's interrupt-mask / bank bits: no interrupt is
  modelled and nothing reads them later); contact_models.c 39, actor_type31.c 45, shc_math.c 22 *open* (the
  inputs that would show them never occur on the routes or in the kept synthetic samples: chunk types and posed
  models no course holds; a racer near a type 0x31 actor; boundary arguments of the maths).
- **n3b** (S18's 1 694 open ids by id, with the gaps S18 closed; corpora c6 + syn; 24 ids no longer exist): 1 670 mutants, **531 killed** (473 by the replay), 367 uncovered, 772 survived. (A first n3 skipped every mutant: the gate matches ids with Mull's `/tmp/mull/src` prefix; the list was given without it.)
- **n4** (S19's new files: batches u / u2, 18 files): 844 mutants, 688 killed, 15 unreached, 141 survived: **81.5 %** (reach 83.0 %).
- **Combined** (`tools/mull_combine.py`: S17's combined file, S18's n1, S19's n2 / n4 by file, n3 by mutant):
  **22 991 mutants, 16 753 killed, 144 equivalent, 4 510 unreached, 1 584 survived (each with a reason): 73.3 %, reach 91.4 %** (S17: 16 960 mutants, 73.8 %, reach 90.8 %, 1 263 open; S18's n1 was not combined). Every survivor has
  a note (`mutation_notes.tsv` -> `mutation_survivors.tsv`): 13 *accepted* (sbInitSystem's SR bits), the S19 clusters
  of n2 (106), and the rest *open* by mutator class with the reason the samples cannot show it (a boundary input
  never sampled, a call result that is 0 / non-zero on every sampled path, a value rewritten before it is read, a
  removed call whose writes are rewritten, an operand that is 0): 1 571 open in all. They are test gaps, not proven equivalents.

### 20.6 Results and next

**4 046 of 4 046 game functions (100 %), 312 174 of 312 174 bytes** (functions.tsv's 4 010 and S19's 36), 0
exceptions, 51 fragments; outside the game 12 hand-written functions (src/lib/shc_math.c). Routes: 19. Difftest: the
c7 corpus run, 19 routes, 57 489 samples, 0 FAIL. Corpora replay (c7 + syn, 15 819 samples): c7 0 FAIL; syn: 14 of its S17 / S18
samples fail (racer_camera_intro, title_intro, actor_type30_pose_node, racer_camera_finish, racer_camera_start):
they were recorded while the stack was undefined; the five functions' fresh synthetic samples (s19re, 32) pass and
Mull's gate leaves the 14 out as failing without a mutant; why the old ones fail was not traced. Pure vs hand:
identical frame lines on all 19 routes, native and wasm; native = wasm. sh4 257 / 0, fpu 8 999 782 / 0, mixer
26 / 0, harness 5 953 functions: 5 747 pass, 0 fail, 206 untested (fixed: the fresh-frame clearing now goes through
the harness's memory hooks); `GEN=genhand tools/rtw.sh build` + check OK; the browser e2e passed (49.7 game
frames/s in the race), its title / PenPen House / race screenshots look right. Never executed by a test: 13 own (as
S18), 0 copies without an executed group member. The hand build runs 96 translated library functions (12 KB) and
enters the interpreter nowhere on the 19 routes.

Next:

1. The outside region by 20.4: the C runtime's rest (division and copy helpers with SHC's register conventions, rand,
   the stream-table helpers, 0x8C062B70), then Ninja's 49 executed functions (an ftrv helper in fpu.h first), sy's
   heap, sy_start, bu, the thunks. Delegated batches work as for the game (the brief plus "SDK function: keep the
   guest globals the HLE reads").
2. The 54 translated gap entries no route runs: confirm each as code, add to functions.tsv, write, synthetic samples.
3. The open survivors (20.5) by input class: synthetic hints at the boundaries (maths), a posed model and the other
   chunk types (contacts), a racer near type 0x31.
4. The 14 stale synthetic samples: trace one (a -DPENPEN_DBG replay with PENPEN_DBG_PC at the camera's njCalcPoint),
   then re-record or retire them (the user decides: removing failing samples needs their say).
