# HANDOFF

Newest session first. Read `docs/port/PLAN.md` (disc facts, survey, approach, phases, rules) before working.

## S24 — 2026-11-14: the foam glitch: nj_ModelL4 is the basic-model drawer; the draw check compares vertices

All guest code ran in `--network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp`
containers; `git log --all -- port/gen` stays empty. Design and numbers: `docs/port/DECOMP.md` 25, `RENDER.md` 5.

1. **The user's report** (public genlib build, iPhone): on Sweets' running section, from the moment the penguin hit the
   pink slow foam, slabs across the screen converging on the penguin. **Reproduced** (route **foam**: the attract demo,
   which races course 0 = Sweets' Easy course, kept past its time-out by poking Game.mode_frames; its computer player
   reaches the foam at ~6049): native (trace) and wasm (screenshots), from frame 6050.
2. **Root cause** (DECOMP 25.2): the foam is the course's type-4 actor (the scrolling strip), drawn with
   **nj_ModelL4** (8C0AB8C8, its only caller). The census's "L4" family is Ninja's BASIC-model family (NJS_OBJECT /
   NJS_MODEL); the HLE read it as a chunk object since S5, the NJS_MODEL's normals as strip chunks indexing whatever the
   vertex buffer held (the penguin just drawn): 168 084 stale-vertex references in one frame, and the strip itself never
   drawn. Old, not a regression; no route had reached a type-4 actor.
3. **The fix** (port/render/nj_model.c): the object walk takes its model drawer; nj_ModelL4 walks with draw_basic in the
   L4 light. The pink foam now shows, no slabs.
4. **The test that would have caught it** (`tools/s11.sh drawcheck`, DECOMP 25.4): S23's (model, XMTRX) check passed
   the old handler (same walk and matrices). S24 lets the original L1 and basic per-model functions run to their return
   in the interpreter (the SDK's vertex buffer set up in a scratch window, rt.c; store queues routed by QACR) and
   compares each model's transformed vertices (index, x, y, 1/z) with the HLE's. With the old handler put back:
   `vertices 54/0` on every nj_ModelL4 call.

Tests (end of S24): drawcheck 22 routes, every 10th frame: **194 383 drawer calls, (model, XMTRX) 0 differ;
148 284 calls / 12 943 290 vertices compared, 0 differ** (not compared, counted: shape motions, links, L2, L3);
pure vs genlib (`--no-interp`) **22 / 22 identical, native and wasm, both digests; native = wasm on all 22**;
route difftest (22 routes) **84 397 samples, 0 FAIL**; rtw build (penpen.wasm 1 293 592 bytes) + check OK; browser e2e
done (the save found after the reload; its frames/s line was not captured). Not rerun: corpora replay, sh4, fpu,
ninjatest (no change to the code they cover).

Screenshots (`/private/tmp/claude-501/-Users-sorikairo-Code-pokemonsnap/f6d4a533-34b6-4407-a635-03b29c8ddb2f/scratchpad/s24/`, PNG, not in git): before_foam_f06340 /
f06440 / f06460 (the old build: slabs, a brown screen), after_foam_f06120 / f06240 (the penguin on the pink foam) /
f06340 / f06440 / f06460 (the corridor); e2e/b1-house, b1-race, b1-race-late (the browser).

Not verified: on hardware / the user's own race (a time trial or cup race at 1'18": the route is the demo's computer
player on the same course); the strip's light and specular (draw_basic as nj_ObjectB's; 8C0AB13C not read in full)
and its bounding-sphere clip (not modelled); nj_MotionL4 / nj_ShapeMotionL4 (no caller). Seen, not changed: Ninja's
L1 strip writer drops triangles nearer than z = 1, the HLE clips them (RENDER.md 7); the slide trail's white
rectangles in the demo. Next: the vertex check for the L2 / L3 families and the shape motions (DECOMP 25.4), the near
plane as the original.

## S23 — 2026-11-14: the characters' bind pose: the draw HLE checked against the original Ninja drawers

All guest code ran in `--network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp`
containers; `git log --all -- port/gen` stays empty. Design and numbers: `docs/port/DECOMP.md` 24, `RENDER.md` 5.

1. **The user's report** (public genlib build): every character in PenPen House and the races stood in its bind pose.
   **Root cause** (DECOMP 24.2): Ninja's current matrix is XMTRX; the HLE's call helper restored XMTRX after each SDK
   call, so since S8 the HLE transformed the matrix stack's RAM slot, which njPushMatrix overwrites with XMTRX for each
   child: every node was drawn at the object's matrix times its own transform only. Also wrong: the node walks (BREAK
   dropped siblings; SKIP, the root ZXY flag, the node callback), the key interpolation, the link drawers' arguments,
   nj_ObjectL1 (a bare model), nj_SetCallback (a stub: the racers' hats / parts misplaced). **Old, not a
   regression**: the S8 tree (d6f380b, built in a worktree into a scratch volume) draws the same picture.
2. **The fix** (port/render/nj_model.c, nj_SetCallback in src/lib/ninja/render_state.c): transforms on XMTRX, the
   original walks (8C0D023C, 8C0CC6F4), keys bit-exact with fpu.h, links blended as 8C112BFE.
3. **The test that would have caught it: `tools/s11.sh drawcheck [N]`** (PENPEN_DRAWCHECK): each drawer call of every
   N-th frame runs first as the ORIGINAL code in the interpreter, its per-model draws trapped, and its (model, XMTRX)
   list is compared bit for bit with the HLE's. Before the key / signature fixes it found 176 of 1 143 calls different
   (route); **final: 21 routes, every 10th frame, 178 327 drawer calls, all bit-exact**.
4. **483 stale samples** (recorded with nj_SetCallback as an HLE call: "hle calls N/M"; a build with it HLE again
   passes all 483) excluded with that reason in tools/replay_exclude.tsv; nothing deleted.

Tests (end of S23): drawcheck **178 327 / 0**; pure vs genlib (`--no-interp`) **21 / 21 identical, native and wasm,
both digests; native = wasm on all 21**; route difftest (21 routes) **76 323 samples, 0 FAIL**; corpora replay (c8 + c9 +
syn) **17 878, 0 FAIL, 520 excluded**; sh4 257 / 0; ninjatest 80 103 / 0, motion 220 436 / 0; genlib 0 translated
functions; outside the game 140 hand-written, 166 HLE; rtw build (penpen.wasm 1 291 816 bytes) + check OK; browser e2e
done, 58.8 game frames/s.

Screenshots (`/private/tmp/claude-501/-Users-sorikairo-Code-pokemonsnap/f6d4a533-34b6-4407-a635-03b29c8ddb2f/scratchpad/s23/`, PNG, not in git): before_house_f01150 /
before_select_f01450 / before_race_f02300 (the pre-fix build), before_S8build_house_f01150 (the S8 tree: the same
picture), after_title_f00500 / after_house_f01150 / after_select_f01450 / after_race_f02300 / after_race_f02500, e2e/b1-house,
b1-select, b1-race (the browser).

Not verified: the shape motions' vertex keys (S8's reading: the check compares matrices, not vertices) and a link's
shape blend (the nearer shape is drawn); quaternion motions (none on the routes); the per-model clip test (8C0D01E0
skips a model whose bounding sphere is off screen when njControl3D has 0x100: the HLE draws it). Next: compare the
vertex buffer as well (trap 8C0CFFE0 / 8C0CFA56), then the shape motions' keys; the cheap-shadow drawer
(njCnkModDrawModel) and njDrawSprite3D are not in the check.

## S22 — 2026-11-14: the motion reader as C, the owner scene routed, a deterministic draw HLE, types 0x30 / 0x32 drawn

All guest code ran in `--network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp`
containers; `git log --all -- port/gen` stays empty; corpora only in the volume `penpen-corpus` (new: `/corpus/c9`
owner / owner_nested / owner_setup / csd30 / csd30_nested, `/corpus/syn/s22*`). Design and numbers: `docs/port/DECOMP.md` 23.

1. **nj_MotionSetup / StepA / B / C / End are C** (`src/lib/ninja/motion.c`, 23.1; HLE stubs before): the reader's
   state, the binary key search, the linear and Catmull-Rom interpolators of the step tables; on the hle.c guest list
   (the original runs on the difftest's side A and in the pure build). **ninjatest motion: 200 000 cases, 2 206 814
   calls, 0 differ.** The pure translation now keeps a function a call runs on into (the interpolators' split parts);
   genlib's C-root scan skips the reader's stored entries: **0 translated functions** in genlib.
2. **The owner-scene hang was a poke artefact** (23.2): with no race run the set-up's penguin slots are -1, the model
   name is read before its table and the loader retries the missing file for ever (**the pure build too**). Route
   **owner** pokes the slots and characters as well: construct / init pass inside race_setup_update's sample, the draw
   25 samples pass.
3. **The draw HLE's powf / sinf / cosf are our own code** (`port/render/det_math.h`, 23.3): **native = wasm for both
   digests on all 21 routes**, pure and hand (boot's frames 3998 / 4102 agree).
4. **Types 0x30 / 0x32** (23.4): only CSD / CSD3 place them (6 + 11, cells 200-203: built on cup4b, never seen).
   Synthetic draws and posed nodes pass (s22t30*, s22t32); route **csd30** moves one type 0x30 actor in front of the
   start: woken, drawn with its motion keys, pure = genlib, 18 live draw samples; screenshots look right (the actor
   rises with its parts posed, puffs beside it).
5. **Mutants** (23.5): the S22 files **1 670 mutants, 1 384 killed, 49 equivalent, 104 unreached, 133 survived (each
   with a reason): 85.4 %, reach 91.2 %** (motion.c: 41 of its 42 survivors killed by ninjatest outside Mull); cheap
   kills: type 0x31 open 49 -> 27, the owner draw 50 -> 11; combined **24 318 mutants, 18 369 killed: 76.2 %, reach
   91.3 %** (S21 74.5 %).

Tests (end of S22): pure vs genlib **21 / 21 native and wasm, both digests; native = wasm everywhere**; difftest (20
routes) **72 151 samples, 0 FAIL**, csd30 **2 033, 0 FAIL**; replay c8 + c9 + syn **17 945, 0 FAIL, 37 excluded**; sh4
257 / 0; fpu 9 249 782 / 0; mixer 26 / 0; ninjatest motion 200 000 / 0; rtw build (1 274 540 bytes) + check OK; e2e
done, **58.8 game frames/s** in the race; screenshots right.

Not verified: genhand compares after the last translation change; the synthetic jobs' first-job failure cause
(self-check, not traced); csd30 is a poked state; the owner scene on other courses / variants. Next: DECOMP 23.6.

## S21 — 2026-11-14: a draw-list digest, the stale samples proven, genlib as the module, samples for what had none

All guest code ran in `--network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp`
containers; `git log --all -- port/gen` stays empty; corpora only in the volume `penpen-corpus` (new: `/corpus/syn/s21*`,
119 samples). Design and numbers: `docs/port/DECOMP.md` 22.

1. **A draw-list digest on the frame line** (`ram R draw D`, port/render/rd.c, 22.1): an XMTRX-only difference that
   reaches a draw now shows in the whole-run comparison (a perturbed racer shadow: RAM equal on all 3 300 frames of
   cup2, draw different on 1 535). Pure vs hand with it: **genlib 19 / 19 native and wasm, genhand 19 / 19 native,
   19 / 19 wasm**; native = wasm for RAM everywhere, for the draw digest on 18 routes (boot: 2 frames, the draw HLE's
   host powf / sinf / cosf). racer_draw now calls the ZXY thunk the original calls (no behaviour change).
2. **The 15 stale samples are a recording artefact, proven** (22.2): with the fresh-frame clearing turned off in a
   debug build all 15 pass; type 0x30's 8 read PoseFrame words that nj_MotionStepA-C (HLE stubs here) never write.
   Reasons written in `tools/replay_exclude.tsv`; nothing deleted. Found: **nj_MotionStepA / B / C are HLE stubs**.
3. **genlib is the retrowser module** (`tools/rtw.sh build`, 22.3): 1 266 041 bytes, rtw check OK, browser e2e passed
   (0 recompiled functions, 4 179 hand-written); title / house / race screenshots right.
4. **Synthetic samples for what had none** (`tools/synth/s21/`, difftest `set ADDR VALUE`, 22.4): the four contact
   responses, both posed contact builders, the trails, type 0x1B's init, type 0x31's hold / update, the G2 DMA block (9
   functions), the start-up's sections and the registry. **Bug found: actor_type31_hold** wrote the held player's
   position into the Body instead of v164 (fixed; s21h3/s000000 catches it). Left without samples, with reasons: the
   start / body (never return), the cast viewer (dead), the owner scene (a poke route hung), types 0x30 / 0x32's draws.
5. **Mutants** (22.5): the files S21 sampled: **1 345 mutants, 962 killed, 25 equivalent, 257 unreached, 101
   survived (10 accepted, 91 open by input class, each with a reason): 72.9 %, reach 90.5 %**; combined **24 181 mutants,
   17 882 killed: 74.5 %, reach 91.3 %** (S20 73.4 %).

Tests (end of S21): corpora replay (c8 + syn) **17 590 samples, 0 FAIL, 15 excluded**; sh4 257 / 0; compares as in 1;
rtw check OK; e2e passed. Not rerun: the 19-route difftest, fputest, ninjatest, mixer (no change to their code).

Not verified: the e2e's frames/s (the log line was cut); why the owner-scene poke hangs; whether the draw digest
differs between pure and hand anywhere outside the 19 routes (it cannot be computed per sample).

Next: (1) nj_MotionStepA / B / C as HLE (type 0x30's poses); (2) the owner scene route (debug the hang at 1740); (3)
types 0x30 / 0x32's draws (a course that places them, or a constructed object tree); (4) the draw HLE's host maths as
our own code so the draw digest is native = wasm; (5) the open Mull survivors (22.5).

## S20 — 2026-11-13/14: the library code as C, every function outside the game accounted for, a build without generated code

The user's goal: "Don't stop until all the code is ported". All guest code ran in `--network none --read-only
--cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log --all -- port/gen` stays empty;
corpora (guest RAM) only in the Docker volume `penpen-corpus` (new: `/corpus/c8`, the 19 routes; synthetic
`/corpus/syn/s20lib*`, `s20re*`). Design and numbers: `docs/port/DECOMP.md` 21.

1. **The library code as C** (`src/lib/`, 130 functions, 21.2): S19's 96 executed translated functions, the 22 the
   C can reach that no route runs, S19's 12 maths: the SHC runtime (division with its r1 / r0 convention, C++ array
   loops, memmove / memset / memcmp, rand, the block copy, frexpf / modff / sinf / cosf), the G2-bus copy, sy's heap
   / start-up / library registry, bu's file image and the HLE thunks, Ninja (batches N1 / N2: the matrix stack and
   transforms on XMTRX, vectors, trig, view / camera / screen, the chunk reader, the fog table, render state), two
   Kamui register writes. fpu.h has `ftrv`; hw.h `hw_xmtrx()` / `REG(n)`; the game's sdk.h wrappers call the C.
2. **Three game functions functions.tsv missed** (vtable-only, in gaps: the owner scene's draw, type 0x1B's init; a
   copy of text_style_reset), written (delegated W3 + lead): **4 049 / 4 049 game functions**.
3. **Every function outside the game region accounted for** (`docs/progress/outside.tsv`, `outside_classes.tsv`,
   21.4): 130 hand-written, 172 HLE, **0 reachable left untranslated**, 1 437 reached only through stored pointers
   nothing that runs reads, 2 296 dead. S19's 1 895 translated: 118 C, 2 game, 1 437 data-only, 338 dead. The 54
   gap entries: all code; 52 covered by their parents' / callers' C or unreferenced, 2 real methods (written).
4. **A build without generated code** (`tools/s11.sh genlib`, 21.5): no translated function at all (table.c only);
   **pure = genlib on all 19 routes, native and wasm, native = wasm, the interpreter never entered**
   (`--no-interp`); wasm **1 264 457 bytes**; rtw check OK and the browser e2e passed with it (0 recompiled
   functions, 4 179 hand-written; 58.3 game frames/s in the race).
5. **The stale synthetic samples** (21.6): kept, listed in `tools/replay_exclude.tsv` with reasons (15: S19's 14 and
   one more of type 0x30's pose), honoured by every replay gate; fresh samples of the five functions recorded and pass.
6. **Bugs found** (21.7): the G2 DMA block through a NULL pointer (lead), the fog table through hw_ptr (N2), the G2
   read order (lead), **func_8C0304CA was njRotateZXY's thunk exported as njRotateXYZ's (S14; invisible until XF was
   compared)**, a difftest over-reach (FR4 at the nj_Motion stubs).
7. **Tests and tools** (21.1): difftest compares XF0-XF15; synthetic hint `reg`; recomp.py `--trim-hand` /
   `--classify`; `--no-interp`; `PENPEN_DIGEST_MAP`; `tools/s3.sh ninjatest` (Ninja's C against the original: 580 000
   cases, 0 differ); `tools/outside_classify.py`, `tools/gap_check.py`; compare `HG=` / `NOINTERP=` / `REUSE=`.

**Progress: 4 049 of 4 049 game functions (100 %), 313 196 of 313 196 bytes** (S19's 4 046 and the 3 S20 found);
outside the game: 130 hand-written (S19: 12), 172 HLE, 0 reachable left untranslated. Routes: 19.

Tests (end of S20): difftest over the **19 routes (the c8 corpus run), 68 626 samples: 0 FAIL** (XF0-XF15 compared);
corpora replay with the final build (c8 + syn, **17 471 samples): 0 FAIL**, 15 excluded with reasons; synthetic
samples of the library C (s20lib*: 128 functions with passing samples after the two fixes) and of the stale samples'
functions (s20re*); **ninjatest 580 000 / 0**; pure vs hand **identical frame lines on all 19 routes, native and wasm,
for genhand and genlib; native = wasm; the interpreter entered nowhere** (`--no-interp`); sh4 **257 / 0**; fpu
**9 249 782 / 0** (+ ftrv); mixer **26 / 0**; harness **5 953 functions: 5 747 pass, 0 fail, 206 untested**
(unchanged); `GEN=genhand tools/rtw.sh build` + `check` **OK** (and with GEN=genlib); browser e2e **passed** with both
(the final hand build: 58.8 game frames/s in the race; the build without generated code, before the last three fixes: 58.3); the title, PenPen House and race screenshots look right. Mull (new code):
**1 373 mutants, 1 035 killed, 25 equivalent, 183 unreached, 130 survived (each with a reason): 76.8 %, reach 88.8 %**;
combined **24 181 mutants, 73.4 %, reach 91.3 %**.

Not verified: why 8 of the old type 0x30 samples end with another XMTRX (the old snapshots; fresh samples and the unit
test pass); the draw HLE reads the matrix through the RAM slot at NJG_CURMATRIX, so a difference left only in XMTRX
is seen by the per-sample XF comparison, not by the whole-run RAM digest (a draw-list digest per frame would close
that); the `data` class (1 437) rests on "no code that runs reads those tables" (the strict build drops them and runs
the 19 routes with the fallback a stop); 12 library functions without samples of their own besides the whole-run
comparison (the start-up chain: start, its body, the sections, the registry); the 13 never-executed game functions
(as S19); the open Mull survivors of earlier sessions.

Next: (1) a draw-list digest in the frame line (XMTRX-only differences in draws); (2) the genlib build as the
default (`tools/rtw.sh build` with GEN=genlib: the module is 1.26 MB, 13x smaller); retire the translation path
(recomp.py stays for the difftest's reference and the pure build); (3) the open Mull survivors by input class; the
13 never-executed game functions; (4) the 8 old type 0x30 samples (a debug replay, with the user's say).

## S19 — 2026-11-13: cup4b's root cause (fresh frames read as zero), the game region's last functions and its missed ones, the code outside it measured and begun

The user's goal: "Don't stop until all the code is ported". All guest code ran in `--network none --read-only
--cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log --all -- port/gen` stays empty;
corpora (guest RAM) only in the Docker volume `penpen-corpus` (new: `/corpus/c6`, the 19 routes). Design and numbers:
`docs/port/DECOMP.md` 20.

1. **cup4b** (20.1): the original race_scene_nearest_player (func_8C02C922) copies out a never-written stack Vec3
   when no racer is within 10000 (CSD's type 0x31 actors, most of the race): stack garbage that hand-written C (no
   guest frames) cannot reproduce. Found with the difftest sampling every outermost call of frame 1764
   (`PENPEN_HWT_FRAMES`, `PENPEN_HWT_SKIP`) and a `-DPENPEN_DBG` build's PC / write watches. Fixed by defining it:
   lowering r15 clears the uncovered words (sh4_ops.h `sh4_stack_lowered`, hw_stack_alloc), on both builds.
   cup4b is route 19; pure = hand on all 19.
2. **Game region** (20.2): S18's five exceptions exported (type 0x31 x3, mode 11's start, func_8C042000 as a
   fragment): 4 010 / 4 010. functions.tsv missed 36 game functions the routes run (vtable-only init / draw methods,
   out-of-line setters; 10 of them ran in the interpreter): added (4 046), all 36 written by two delegated batches (u: 30, u2: 6), merged and tested by the lead: **4 046 / 4 046 game functions, 0 exceptions**. Never executed by a test: 13 own (the same 13 as S18: the cast viewer's and owner scene's construction, the contacts' posed builders, effect_trails_push, four contact responses, types 0x30 / 0x32's draws) + 0 copies without an executed group member.
3. **Outside the game region** (20.3, 20.4): measured (`tools/outside_measure.py`, `docs/progress/outside*.tsv`);
   begun with the C runtime's float maths (`src/lib/shc_math.c`: atan2f, asinf, acosf, floorf, their kernels and
   errno wrappers: 12 functions).
4. **Mull** (20.5): n1's rest (n2: 1 150 mutants, 798 killed), S18's 1 694 open ids again with the closed gaps (n3b: 531 killed), the S19 batches (n4: 844 mutants, 81.5 %). Combined: **22 991 mutants, 16 753 killed, 144 equivalent, 4 510 unreached, 1 584 survived (each with a reason): 73.3 %, reach 91.4 %**; the survivors classified by reason (13 accepted, the rest open with the input class that would kill them).

5. **Bugs found**: the cup4b divergence (the original's uninitialised read; the run time now defines fresh frames);
   a build race of S19's own making (the rtw build and the wasm comparison shared /build/wasm-genhand: one route's
   hand run linked against half the objects; re-run). No hand-written function was found wrong: every difference
   S19 hunted came from the undefined stack.
6. **Tests and tools**: difftest `PENPEN_HWT_FRAMES` / `PENPEN_HWT_SKIP`; `-DPENPEN_DBG` builds
   (`PENPEN_DBG_PC`, `PENPEN_DBG_W`); `FNCOVER_HAND`, `tools/outside_measure.py`, `tools/gaps.py`;
   `SYNTH_CORPUS`; corpora c6 / c7 (the 19 routes).

**Progress: 4 046 of 4 046 game functions (100 %), 312 174 of 312 174 bytes**: functions.tsv's 4 010 (S18: 4 005)
and the 36 game functions it missed (S19, evidence `s19-gap`); 0 exceptions; 51 fragments. Outside the game: 12
hand-written (the C runtime's float maths). Routes: 19 (S18: 18).

Tests (end of S19): difftest over the **19 routes, 57 489 samples: 0 FAIL** (the c7 corpus run, 4 058 exports);
corpora replay (c7 + syn, 15 819 samples): **c7 0 FAIL**; the old synthetic corpus syn: 14 of its S17 / S18 samples (5 functions:
the intro / start / finish cameras, the title intro, type 0x30's pose) FAIL under the defined stack (recorded when
the stack was undefined); fresh synthetic samples of the same five functions pass (s19re), and Mull leaves the 14 out
as failing without a mutant; pure vs hand **identical frame lines on all 19 routes, native and wasm; native = wasm**;
sh4 **257 / 0**; fpu 8 999 782 / 0; mixer **26 / 0**; harness **5 953 functions: 5 747 pass, 0 fail, 206 untested** (it crashed on the fresh-frame clearing until that went through its memory hooks: fixed);
`GEN=genhand tools/rtw.sh build` + `check` **OK**; browser e2e **passed** (4 058 hand-written functions: logos,
title, menu, house, selection, course, race start and race, the VMU save found after the reload; 49.7 game frames/s
in the race); the e2e screenshots (title, PenPen House, race) look right. Never executed by a test: 13 own (as S18).
Translated code the routes still run: 96 library functions (12 KB), the interpreter entered nowhere.

Not verified: the 14 stale synthetic samples (why they fail under the defined stack was not traced: a dbg replay
was not run); the open Mull survivors (20.5: classified by reason, not killed); the 54 translated gap entries no
route runs and the gaps tools/gaps.py flags without an entry (not confirmed as code); the 13 never-executed
functions; the hold of type 0x31 beyond its 104 of 204 bytes.

Next (DECOMP 20.6): (1) the outside region by the plan of 20.4: the C runtime's rest (division, copies, rand, the
stream helpers), then Ninja's 49 executed functions (matrix stack, transforms, trig: an ftrv helper in fpu.h), sy
(heap), sy_start, bu, Kamui / thunks; (2) the 54 unexecuted gap entries confirmed and written; (3) the open survivors
by input class (synthetic hints at the boundaries); (4) re-record or retire the 14 stale synthetic samples with the
user's say.

## S18 — 2026-11-13: the last functions (six delegated batches), fragments, routes for modes 9 / 11 / 12 and the first courses, frame-passing preludes, test gaps closed

The user's goal: "Don't stop until all the code is ported". All guest code ran in `--network none --read-only
--cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log --all -- port/gen` stays empty;
corpora (guest RAM) only in the Docker volume `penpen-corpus`. Design and numbers: `docs/port/DECOMP.md` 19.

1. **Functions** (19.1): six delegated batches (b01-b06, brief `port_work/s18/WRITER_BRIEF.md`: S17's + S18
   additions) and two single ones (c44, the owner scene's construction), the lead's cast viewer construction
   (resumable); every file merged, compiled, difftested (route or synthetic) and replayed by the lead. The three
   named: **func_8C0200E8** (the cast viewer's update, 4.6 KB: synthetic, through a frame-passing prelude that
   builds the scene), **func_8C02AF9C** (the race ending: synthetic), **func_8C060706** (type 0x32's update).
   **Fragments**: Ghidra's splits of a function (literal pools, epilogues reached by branches, tails after a call it
   took for one that does not return, pool data) are declared with the new `HW_EXPORT_FRAGMENT(addr, parent)`
   (hw.h): their translation is left out, the parent's C does the work, entering one alone stops the run.
2. **Routes** (19.2): `options` (mode 12: next_mode poked on the title), `gallery` (mode 11: its files are not on
   this disc: the scene's and two pictures' names poked to 4000config.pvm), `staffroll` (mode 9: phases' countdowns
   poked; needed the new HLE njLoadTexture), `cup1b` `cup2b` `cup3b` (the cups' other course: DOWN on the course
   map); **cup4b** (CSD) is not kept: pure and hand differ there (19.5).
3. **Test paths** (19.3): synthetic preludes that pass frames (`frames`) and load files (`slow`, the drive attached
   to the game's gdFs work area: every synthetic gdFsOpen had failed), stack arguments (`stk`), a later hint block
   replaces an earlier one; the replay: side B's runaway is a FAIL (was a time-out), corpus directories 64 -> 1024
   (later ones were silently "unreadable"), side A notes a function it falls into; the Mull gate gives a static
   helper its callers' samples (was every sample of its file, cut at 160).
4. **Bugs found** (19.4): see DECOMP 19.4.
5. **Mutants** (19.6): see DECOMP 19.6.

Progress / tests / not verified / next: DECOMP.md 19.

**Progress: 4 005 of 4 010 game functions (99.88 %), 300 222 of 301 130 bytes (99.70 %)** (S17: 3 806, 77.55 %);
50 of them are fragments (HW_EXPORT_FRAGMENT). Not ported (5, DECOMP 19.5): type 0x31's update / face_player / hold
(C written, differ on cup4b), mode 11's start (C written: whole run identical, one sandbox sample differs),
func_8C042000 (a piece of a parent that stays translated). Raw offsets: 47. Routes: 18 (S17: 12); the routes run 1 960 game functions (S17: 1 640).

Never executed by a test: S17's 50 own (+ 101 copies) -> **13 own** (+ 93 copies, each with an executed identical
member of its copy group, so their C runs): the cast viewer's and the owner scene's construction (resumable, dead or
unrouted: a frame-passing prelude runs the original only), and code of courses / classes no kept
route reaches (types 0x30 / 0x32's draws, the trail push, four contact responses, the contacts' posed builders). 37 of S17's 50 now run (routes or synthetic samples): the gallery and options scenes, the staff roll, mode 1's start
(options now pokes mode 1 at 1900), the TrapSet, the racer actions and constructors, the owner scene's update.

Tests (end of S18): difftest over the **18 routes, 52 622 samples: 0 FAIL**; corpora replay **0 FAIL** (c5 + syn:
19 157 samples); synthetic: 2 702 functions with passing samples; pure vs hand **identical frame lines on all 18 routes, native and wasm; native = wasm**; sh4 **257 / 0**; mixer
**26 / 0**; harness **5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + `check` **OK**; browser e2e **passed** (first run since S10; the hand build, 4 005 hand-written functions: logos, title, menu, house, selection, course, race start and race seen, the host's AudioWorklet playing, the VMU save found and loaded after the reload; 25.3 game frames/s in the race while Mull and the route comparison ran on the machine);
screenshots from the e2e (title, PenPen House, race) look right. Mull on the S18 code (run n1, stopped at the session's end in its last files): 6 340 mutants of 73 files, 3 743 killed (3 475 by the replay), 363 survived, 2 234 uncovered, 49 equivalent: **59.5 %, reach 91.2 %** (DECOMP 19.6).

Not verified: the cup4b divergence (CSD: pure and hand differ from frame 1765; not found, route not kept); the
type 0x31 differences; mode 11's start (a replay effect: texture_load's out-parameter at another r15 offset than the original's, DECOMP 19.5); the writers' "unsure" notes
(port_work/s18 reports, summarised in DECOMP 19); the 13 never-executed functions; the open mutants not classified
(S17's 1 995 reached-but-invisible: 100 noted, 207 re-mutated in n1, 1 688 for an id run with the closed gaps; n1's 363 survivors beyond the rules).

Next: (1) find the cup4b divergence (frame 1765: the hand build's errno becomes EDOM, the Rings actors' sizes / turns
differ; a translated math caller gets an out-of-domain input from C code; the bisects by export groups were
inconclusive: DECOMP 19.4), then type 0x31; mode 11's start after texture_load's C frame is laid out as the
original's; (2) the open mutants by id with the closed gaps (19.6); (3) func_8C041F9C + 8C042000 together.

## S17 — 2026-11-13: synthetic samples (difftests and Mull kills without a route), the draw context, 689 more functions

The user's goal: "Don't stop until all the code is ported". The functions no route reaches were the obstacle (no
difftest, no Mull kill; S16: new routes add almost nothing). All guest code ran in `--network none --read-only
--cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log --all -- port/gen` stays empty;
the corpora (guest RAM) live only in the Docker volume `penpen-corpus` (`/corpus/c5`, `/corpus/syn`). Design and
numbers: `docs/port/DECOMP.md` 18.

1. **Synthetic samples** (18.1, `--hwsynth`, `tools/synth.py`): a sample made like a live one but from a real scene
   the game did not give the function: a corpus snapshot as the base (title, house, setup menu, 1P / 4P / cup 3
   races, explore, GOOD NIGHT, plus the function's own / its callers' samples), optional prelude calls of the
   original (a constructor, the factory), arguments from the base's own objects (registers, the callers' stack
   pointers, the objects whose vtable holds the function), the function's constants, field / global pokes; the
   original interpreted with the HLE live records the HLE results and hardware reads; then the live difftest's own
   judgement (`sample_judge`, shared with hwt_end). A coverage-guided search keeps the inputs that run new
   instructions of the original. Out-of-domain inputs are refused, not judged (non-RAM accesses of the guest code,
   code outside the code ranges, a frame's end, an HLE loop or fault, side B leaving guest RAM). Passing samples
   are ordinary corpus samples (`/corpus/syn`): `--hwreplay` and the Mull gate use them. Kept apart in
   `docs/progress/synth_coverage.tsv` (route-backed coverage stays `fn_coverage.tsv`).
2. **The draw context** (18.3): the difftest compared an HLE draw call by its registers only; now also the 3D draws'
   current matrix, the render state (njControl3D, constant attribute / material, the texture list: a stack one as a
   token), 32 bytes at each RAM pointer argument (the sprite, the model) and FR4 for the draws that read it.
3. **Per-sample line coverage** (18.4, `tools/linecov.py`, `tools/s3.sh linecov`): src/ with gcc --coverage, every
   corpus sample replayed: c5 runs 9 466 lines of src/, c5 + syn 15 092.
4. **Survivors** (18.5): Mull with the synthetic corpus in the replay gate: the S17 code (runs r, m2-m4: 5 698
   mutants; on the unreached batches the reach score is 93-99 %) and S16's open mutants again by id (`--ids`: 3 059;
   808 killed, 783 by synthetic samples). **Combined: 16 960 mutants, 12 496 killed, 38 equivalent, 3 163
   unreached, 1 263 survived (open): 73.8 %, reach 90.8 %** (S16: 71.9 %, 85.4 %, 1 557 open). S16's 1 557: 457
   killed, 2 equivalent, 108 on changed lines, 990 open; S15's 212: 56 killed, 2 equivalent, 1 changed, 153 open
   (130 on lines samples run: reached but invisible; 23 that no sample reaches). Every open mutant's per-sample
   reach is in `docs/progress/mutation_sample_reach.tsv`. **Not done**: the per-id equivalent / gap classification
   of the reached-invisible ones.
5. **New functions** (18.6): the rest of S15's b2 list (the racer's effects and sounds, the computer racer's states 3
   and 4, the race step, the title's intro and logo draw: route-backed) and nine delegated batches of unreached
   functions (b01-b09, synthetic). Bugs found: lane_racer_steer read range2_min (0x658) where the original reads 0x654
   (route difftest); matrix_axis_angle stored the axis before reading the matrix (a synthetic sample with the
   axis aliasing the matrix).

**Progress: 3 806 of 4 010 game functions (94.91 %), 233 516 of 301 130 bytes (77.55 %)** (S16: 3 117 / 177 706,
59.01 %). Raw offsets: 47. Routes: 12 (unchanged), 1 640 game functions run.

Tests (end of S17): difftest over the **12 routes, 39 835 samples: 0 FAIL** (with the draw context); the corpora
replay with **0 FAIL** (c5: 8 174 route samples; syn: 4 128 synthetic samples: 6 removed whose out-of-domain inputs
made the C fault after later merges, `tools/synth/notes.tsv`); synthetic: **2 540 functions with passing samples**
(2 043 of them run by no route: 93.7 % of their instruction bytes run), 8 FAILs judged outside the functions' domain
(notes); pure vs hand **identical frame lines on all 12 routes, native and wasm; native = wasm**; sh4 **257 / 0**;
mixer **26 / 0**; harness **5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + `check` **OK**; screenshots from
the hand build (title f500, PenPen House f900, race f3000 of the route) look right.

Route-backed vs synthetic: 1 574 hand-written functions pass with route samples (own or inside); the routes still run
1 640 game functions; the synthetic samples test 2 540 (`synth_coverage.tsv`, kept apart: they show the C equals the
original on real objects of real scenes with chosen arguments and pokes, not that the game makes those calls).

Not verified: the 1 263 open survivors (not classified per id); the 99 functions without a synthetic sample (frame-
ending loaders and scene constructors, the gallery / options constructors that load files) that no route runs either:
compiled, exported, unexecuted; the synthetic samples' FAIL classification (8, judged by reading the inputs);
`hle.c`'s syCfgGetSoundMode change (reports its out-parameter) came after the native / wasm compares and the rtw
check (a difftest-only effect); the browser e2e; the writers' "unsure" notes (port_work/s17 reports, 18.6).

Next (DECOMP.md 18.7): (1) classify the reached-invisible mutants by id; (2) the 204 functions left (67.6 KB: the
large ones, the dump-cut ones); (3) synthetic preludes for the 99 without samples; (4) routes for the options,
gallery and staff roll (their scenes are C now).

## S16 — 2026-11-13: an offline sample-replay Mull gate (59 -> ~5 s per mutant), Mull on every file, 38 more functions

The user's goal: "Don't stop until all the code is ported"; the user also asked why Mull is so slow. All guest code ran
in `--network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log
--all -- port/gen` stays empty; the sample corpus (guest RAM) lives only in the Docker volume `penpen-corpus`.
Design and numbers: `docs/port/DECOMP.md` 17.

1. **Why Mull was slow, and the fix** (17.1): a mutant was judged by whole routes from boot; a survivor ran every
   covering route to its end (~82 % of the time). The difftest already records everything a sample needs to be
   judged again without the run, so the routes now keep a **corpus** of up to 8 passing samples per function
   (`--hwcorpus`, `tools/s11.sh corpus TAG K`: snapshot pages in a content-addressed store, HLE / stack / RAM-fill /
   hardware-read records, side A's outcome, the functions side A entered; ~1.3 GB for 12 routes, 4 min), and
   `--hwreplay` runs side B of chosen samples on any build against the recorded side A. The Mull gate replays the
   samples that ran the mutated function's original code first; only what the replay does not kill runs the
   file-export difftest on the 1-3 routes that run the line most per second. **Measured: actor_types.c 59.2 ->
   4.6 s per mutant, race_draw.c 52.8 -> 4.1 s; the kill sets match or exceed the old gate's** (actor_types.c 99 ->
   126: the 27 more are S15's skipped duplicates, 3 checked by hand; race_draw.c the same 158). The whole corpus
   replays in ~20 s: a regression check of every hand-written function after any edit.
2. **Two S13 / S15 differences found with the replay** (17.2): racer_camera_start had the far point's test reversed
   (the time trial's intro); system_language read the BIOS block's byte through `hw_ptr` where the original reads
   address 0x0C through the I/O model (new `hw_read8`). Both exported.
3. **Mull debt** (17.4): every src/ file now has a Mull result (121 files; S15: 59, 30 without): **12 714 mutants,
   9 110 killed, 40 equivalent, 2 007 unreached, 1 557 survived (open): 71.9 %, reach 85.4 %** (S15: 4 703 mutants,
   81.5 %, 255 open). n1 (the 30 files + the changed and new ones: 5 404 mutants) took 99 min, n2 (the files with
   S15 survivors + the later merges: 6 249) 78 min; with the old gate they would have taken ~15 h and ~17 h. Of S15's
   255 open survivors, 5 are killed now, 212 survive again, 38 are on changed lines. **Not done: classifying the
   1 557 survivors** (17.4 says where they are and what the first look shows).
4. **New functions** (17.3): lead: the SetupMenu's construction func_8C022D80 (resumable); four delegated writers
   (brief `port_work/s16/WRITER_BRIEF.md`): the save load, the pause menu's draws, the RaceSetup's construction, the
   RaceScene's build and update, the racer's contact sort / run / ground and walls / reset / update after its
   controls, the course's draw and weather, the computer racer's look-ahead, actor types 1, 2, 0x19, 0x24, 0x2B, the
   sign's draw, the SetupMenu's states 1-3 and screens, the text drawer, the view's camera upload, the Contacts'
   sphere-triangle test, mode 4's record panel. Found on merging: racer_ground read the wrong literal-pool half.
5. **Routes**: none added. Six candidates (the house's fifth choice with the unlock flags, other title-menu
   entries) were measured with `fncover`: at most 1 new function: not kept (17.5).

**Progress: 3 117 of 4 010 game functions (77.73 %), 177 706 of 301 130 bytes (59.01 %)** (S15: 3 079 / 135 372,
44.95 %). Raw offsets: 47. Routes: 12, 1 640 game functions run.

Tests (end of S16): difftest (the corpus run c5: the full difftest on 12 routes + its nested pass) **63 570 samples,
0 FAIL**, the 38 new exports pass (own samples or inside); the corpora c1-c5 replay with 0 FAIL; pure vs hand
**identical frame lines on all 12 routes, native and wasm; native = wasm**; sh4 **257 / 0**; mixer **26 / 0**; harness
**5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + `check` **OK**; screenshots from the hand build (title f500,
PenPen House f900, race f3000 of the route) look right.

Not verified: the 1 557 open survivors (not classified); five exports tested in c4 but not in c5 (the racer's
set-action / state-4 start and two accessors: their samples went inside the new C caller; their C is unchanged); the
browser e2e; the writers' "unsure" notes (17.3).

Next (DECOMP.md 17.6): (1) classify the open survivors with the replay's help (per-sample line coverage would say
which survivors the samples never reach), starting with the S15 ones (212); (2) the functions left in
`port_work/s15/b2/` (8c01bef8, 8c0351c8, 8c035fa2, 8c03e150, 8c03ed24, 8c029efa, 8c01c668, state 4's update
8c0270d2) and a re-cut with `tools/batches.py`; (3) routes for the 2 370 unreached functions (the ending, options,
VMU menus: find the inputs by reading the menus' C).

## S15 — 2026-11-12/13: the Mull debt (m2, m3, m4), the lead's resumable scenes, nine delegated batches, a cheaper Mull gate

The user's goal: "Don't stop until all the code is ported". All guest code ran in `--network none --read-only
--cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log --all -- port/gen` stays empty;
dumps and staging stayed in the gitignored `port_work/`. Design and numbers: `docs/port/DECOMP.md` 16.

1. **Mull** (16.1): m2 (S14's refactored set again), m3 (the S15 code merged first + save.c / system.c with new route
   inputs), m4 (the S15 code merged later + the S14 files S15 changed): combined with S14's row (actor_types.c from the
   re-measuring run m6), **59 files, 4 703 mutants: 3 813 killed, 26 equivalent, 609 unreached, 255 survived (open):
   81.5 %, reach 93.7 %**. 50 survivors
   classified by id this session (`mutation_notes.tsv`); route inputs added to kill others (explore / goodnight poke
   the score tables, goodnight runs on 2000-06-15). Still without a result: 30 files (16.1).
2. **Gate cost** (16.2, the lead's request): survivors were ~82 % of Mull's time; the gate now orders routes by kills
   per second, runs Mull's pass on at most 3 routes (file exports only), and only those partial survivors get the
   full final pass on every covering route; partial-pass time-outs / stalls are not kills; duplicate call-replacement
   mutants are skipped once one is killed (counted apart). Measured on actor_types.c: 59.4 s per mutant before, 59.2 s after: **no gain** (the true survivors still pay the full final pass; the old gate's time-out kills under load were partly false: ~19 on that file). The snapshot start (the first item) is not
   done: the HLE state outside guest RAM must be serialised first.
3. **Lead**: GOOD NIGHT's construction (resumable) and mode 4's start, the movie scene's update and Sofdec loop
   (resumable), the RaceScene's construction (resumable) and destructor; the RaceSetup setters renamed (they were
   S14's "SetupMenu" setters); flyer_pose's parent stack bounded to the original's 5 slots.
4. **Delegated** (brief `port_work/s15/WRITER_BRIEF.md`): b08, b07, b05, b03, b04, b01, b09, b06, c05 merged:
   the actor group and factory, actor types 1 / 2 / 3 / 0x20 / 0x2D, the course's ground and environment, the racer's
   actions / base / draw / squash / camera update, the race views and race start per mode, the menus' screens, logos,
   save progress, the RTC, the motion player's draw, effect pushes. The difftest caught 4 writer bugs on merge (16.3:
   a setter mapped to the wrong field, an extra dereference, an untested S13 helper, racer_camera_start: kept
   unexported, to find).

**Progress: 3 079 of 4 010 game functions (76.78 %), 135 372 of 301 130 bytes (44.95 %)** (S14: 2 938 / 104 420,
34.68 %). Raw offsets: 47. Routes: 12 (unchanged), 1 640 game functions run.

Tests (end of S15): difftest over **12 routes, 114 757 samples: 0 FAIL**; pure vs hand **identical frame lines on all
12 routes, native and wasm; native = wasm**; sh4 **257 / 0**; mixer **26 / 0**; harness **5 736 / 0 / 207**;
`GEN=genhand tools/rtw.sh build` + `check` **OK**; screenshots from the hand build (title f500, PenPen House f900,
race f3000 of the route) look right.

Not verified: Mull on 30 files (16.1) and 255 open survivors; the new gate's snapshot start (not built); the browser
e2e; racer_camera_start (C, unexported) and system_language (S13, unexported) differ from the originals somewhere.

Next (DECOMP.md 16.4): (1) Mull: serialise the HLE state for a snapshot start, then the 30 files, then the open
survivors; (2) the 82 reached functions left and the SetupMenu's construction func_8C022D80; (3) routes for the
2 370 unreached functions.

## S14 — 2026-11-12: typed classes for the S13 code, Mull on S13's code with a two-pass gate, unlock routes, more functions

The user's goal: "Don't stop until all the code is ported" (sessions back to back). All guest code ran in
`--network none --read-only --cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log
--all -- port/gen` stays empty; Ghidra output and disassembly stayed in the gitignored `port_work/`. Design and
numbers: `docs/port/DECOMP.md` 15.

1. **Tools** (15.1): the Mull gate in two passes (the difftest samples only the mutated file's exports first; the
   full gate only for survivors: a kill costs ~4 s instead of minutes), `mull_combine.py` (one score from per-file
   runs), `rawoff.py` (the raw-offset count), `batches.py` (functions left, by call level, cut into batches with
   their dumps), the runtime's `--poke ADDR=VALUE@FRAME` (test input). Found: `tools/copies.py` never saw an export
   with a trailing comment (vec3_set's group of 11 had no copies).
2. **Readability** (15.2): the S13 code's classes declared with checked layouts (Racer, RaceScene / RaceSetup,
   Course / CourseEnv, Actor / ActorGroup and the actor classes, MotionPlayer, RacerEffects, SetupMenu, Contacts,
   View fields, NjSprite: `game/{racer,race_scene,course,actor,motion,effects,menu,scenery}.h`, `core/contact.h`):
   **raw-offset accesses 407 -> 47** (`tools/rawoff.py`; the 47 are the class-neutral copy accessors of
   `core/inline.c` / `core/vec.c`), every changed file compared at the object-code level first, then the gates.
   Everything written this session is typed (the writers' brief requires `rawoff.py` = 0).
3. **Fixes**: three S13 functions had read the wrong half of a literal-pool word holding two 16-bit constants
   (func_8C044EEA: it passed S13's difftest because the routes push by zero vectors; func_8C0417D2 and
   func_8C044F1E: S13's "to redo" failures). The other "to redo" ones: func_8C010610 turns from the model's z axis
   (S13 reused the y-axis helper of func_8C010594) and keeps its translation in its frame; func_8C0120CA's frame now
   has the original's depth (a difftest rule: DECOMP.md 15.1); func_8C0473C2 calls the course's view bit set, which
   S13 had read from the wrong table: all three exported again. func_8C0623B4 stays out (untestable: its caller's
   samples run past the step budget). Later: func_8C05045A (b10 read the other half again: the per-view `focus` array, caught by the
   difftest on cup2); func_8C059CAC written whole (Ghidra cut it short; the interpreter ran its tail with hw_call's
   return marker as the return address: cup4 stopped; the interpreter now treats that marker as a return).
4. **Mull on S13's code** (15.3): m1 on 13 S13 files (856 mutants: 770 killed, 21 survived: 18 in save.c, where
   every route had zero scores: cup2 now pokes scores; task.c / vmu.c survivors classified by id). Recorded for the
   files whose results still match their source (9 files): **583 mutants, 533 killed, 1 equivalent, 49 unreached, 0
   survived: 91.6 %, reach score 100 %**. m2 (12 refactored files): stopped part way to free the machine for the end-of-session gates: 55 mutants run (accessors, contact, sprite_text, system, transform): 51 killed, 4 on lines no route runs, 0 survived; not a score (`port_work/s14/mull/m2_partial_gate.log`)
5. **Routes** (15.4): **cup2, cup3, cup4** (the save record's unlock flags poked in PenPen House, `--poke`: the
   other three cups' first courses; cup2 also pokes the score tables) and **goodnight** (Game mode 4, never
   visited before): 12 routes; game functions run **1 228 -> 1 640** (unreached 2 782 -> 2 370). Found reading the
   menus: the title's choices and the RaceSetup's states (which modes lead where; mode 12 is never chosen).
6. **New functions** (15.5): by the lead: the leaders of 13 copy groups no export covered (~220 functions with
   their copies), the scene constructors of modes 0, 3, 7, the SetupMenu and the RaceScene, the RaceSetup's update
   and the loading screen (resumable), func_8C059CAC; delegated (7 batches of ~12 KB, `tools/batches.py`, brief
   `port_work/s14/WRITER_BRIEF.md`, merged and gated by the lead): b05 30, b02 29, b10 66, b08 17, b07 26, b11 65.
7. **Gate fixes**: the frame-line comparisons (compare, Mull, mut_worker) leave out the interpreter-call count
   (the pure translation interprets the tail of a function Ghidra cut short, which hand-written C replaces: only
   that count differed); Mull's coverage step survives a route without a profile.

**Progress: 2 938 of 4 010 game functions (73.27 %), 104 420 of 301 130 bytes (34.68 %)** (S13: 2 468 / 57 508, 19.10 %).

Tests (end of S14): difftest over **12 routes, 135 382 samples: 0 FAIL** (every export passes or is a copy of a
passing one, but main, cd_play and task_destroy_tree: inside only, the last checked by a targeted run); pure vs hand
**identical frame lines on all 12 routes, native** (the interpreter-call count left out), **wasm identical on all 12 routes too; native = wasm**; sh4 **257 / 0**;
mixer **26 / 0**; harness **5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + `check` **OK**; screenshots from
the hand build (title f500, PenPen House f900, race f3000 of the route) look right.

Not verified: Mull on the refactored game files (actor, camera, course, racer, model, race_scene, title, vec,
inline, view) and on everything written in S14 (the gate is open; next step 1); m2 partial; the browser e2e; the
writers' notes to check (`port_work/s14/stage/*`: the reports' "unsure" lists are summarised in DECOMP.md 15.5 only
in part: e.g. the accessors.c names for the SetupMenu setters that write the RaceSetup, b11's flyer_pose host stack
of 64).

Next (DECOMP.md 15.6):
1. Finish the Mull runs this session started or left (`tools/s11.sh mutate --budget 60 --only ...`, one or two
   files per run; `tools/mull_combine.py` then `tools/s11.sh report`): m2's 12 files again (stopped part way), then game/{actor,camera,course,racer,model,race_scene,title}.c, core/{vec,inline,view}.c and every file written in S14; classify every survivor.
2. Merge and gate the staged batches left (`port_work/s14/stage/<batch>/`, read their reports' header notes), then
   write the remaining batches of `port_work/s14/b2/` (re-cut with `tools/batches.py` after `tools/s11.sh cover`:
   the cup routes reach ~380 more functions) with the brief `port_work/s14/WRITER_BRIEF.md`.
3. Lead work (resumable / yield chain): the SetupMenu's construction func_8C022D80, the RaceScene's func_8C0293F0,
   GOOD NIGHT's func_8C01D2C4 / func_8C01D4CA, the movie scene func_8C01DE32 / func_8C01DEF8 (Sofdec loop),
   func_8C0100A0 / func_8C010000 (start-up); func_8C0623B4 (needs a way to test inside game_init).
4. More routes: the cups' other courses, the ending (Game mode 9: RaceSetup state 4, after winning a cup), the VMU
   menus (no VMU, a full one), the house's DRESS UP / RESULTS with unlocked flags.

## S13 — 2026-11-12: float helpers, identical copies, function coverage, the title scene, two delegated batches

The user's goal: "Don't stop until all the code is ported". All guest code ran in `--network none --read-only
--cap-drop ALL --security-opt no-new-privileges --tmpfs /tmp` containers; `git log --all -- port/gen` stays empty;
Ghidra output and disassembly stayed in the terminal and the gitignored `port_work/`. Design and numbers:
`docs/port/DECOMP.md` 13 (S13) and 14 (the plan for the whole game).

1. **Float helpers** (DECOMP.md 13.1): `src/include/fpu.h` (`f32`, one function per FPU instruction under FPSCR
   0x00040001, built on `sh4_ops.h`), FARG/RETF/FARGS/hw_fret in `hw.h`, the difftest compares FR0 and declared FR
   arguments of HLE calls. `tools/s3.sh fputest`: **8 999 782 checks against the interpreter, 0 failures**;
   `tools/s3.sh fpu-mull`: **118 mutants, 118 killed**.
2. **Tools** (13.2, 13.3): `tools/s3.sh fncover` / `s11.sh cover` (function coverage per route:
   `docs/progress/fn_coverage.tsv`: 1 228 game functions run on the 8 routes), `tools/s3.sh dupes` (groups of
   identical functions: `port/symbols/copies.tsv`, **202 groups, 2 598 functions**), `tools/copies.py` /
   `s11.sh copies` (the copies' exports in `src/copies/`), `s11.sh status` (`tools/hand_status.py`: untested exports
   by copy group), `tools/ghidra.sh decomp2` (pool constants folded; `+ADDR` disassembles code Ghidra left as data,
   `~ADDR` clears a wrong no-return, `!ADDR:LEN` prints instructions), `S13Calls.java` (call graph), `rt crash`
   native stacks.
3. **Hand-written by the lead**: the vector / Body (damped motion) / Ninja stub / accessor inline methods with their
   ~2 000 identical copies, View light/fog, pad/file/sound/video helpers, the title scene's struct, construction
   (resumable), update and logo (13.4). Found: a method that takes its port in r5 (host crash), Ghidra cutting
   functions short (a copy group fell into the next bytes: the frame lines' interpreter count differed; such
   functions are now never grouped), the title's two phase lengths are two globals.
4. **Delegated batches** (13.5): two subagents wrote 231 leaf functions into `port_work/s13/stage/`; after the
   difftest 7 exports were commented out with the reason, one never exported.

**Progress: 2 468 of 4 010 game functions (61.55 %), 57 508 of 301 130 bytes (19.10 %)** (S12: 70 / 7 474).

Tests (end of S13): difftest over 8 routes: **0 FAIL**, every export passes or is a copy of a passing one (main
excepted: whole-run only); pure vs hand **identical frame lines on all 8 routes, native**; **wasm identical on all 8 routes too; native = wasm**; sh4 **257 / 0**;
mixer **26 / 0**; harness (pure translation) **5 736 / 0 / 207**; `GEN=genhand tools/rtw.sh build` + `check` **OK**; screenshots from the hand build (title f500, PenPen House f900, race f3000 of the route) look right. Mutation: `fpu.h` **118 / 118 killed**; Mull on the lead-written S13 code (`core/{vec,inline,ninja,view}.c`,
`game/title.c`, `--only`, 62 min): **355 mutants, 319 killed, 4 equivalent (title_init: values set twice, an unused
result; reasons by id in `mutation_notes.tsv`), 32 unreached, 0 survived: 90.9 %, reach score 100 %**. A Mull run of
all S13 files was stopped after 140 mutants (133 killed, 7 unreached, 0 survived): with 2 468 exports the control is
207-314 s and a mutant costs up to ~5-12 min, so the whole set would take many hours.

Not verified: **Mull on the delegated batches** (`core/{transform,random,pad_analog,gfx_init,motion,view_setup,
texture,contact,model_build,sprite_text,accessors,system}.c`, `game/{save,vmu,menu_sprites,race_scene,racer,model,
stage,course,camera,actor}.c`) and on the S13 additions to pad/file/task/main/sound.c: the gate is open for them
(first step of S14, per file with `--only`, budget 60); the browser e2e; the copies of a group are tested by identity
(13.2), not each by its own samples. Agent-written helpers still call two runtime helpers through registers
(`sprite_text.c`: 0x8C062518 with r0/r1 set by hand): to rewrite as C `%` once its semantics are pinned.

Next: DECOMP.md 14 (step 1: the 343 reached functions left, the race region first, in delegated batches; step 2:
routes for the unreached 1 199 functions).

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
