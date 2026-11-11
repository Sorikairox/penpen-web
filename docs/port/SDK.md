# Pen Pen TriIcelon: the SDK surface the game uses (S2 census)

What the game code (0x8C010184–0x8C062434) calls in the Katana SDK and middleware linked into 1ST_READ.BIN, what
an HLE layer has to do for each call, and in which port session it is needed. Everything here comes from static
analysis inside the sandbox (Ghidra 12.2; scripts `tools/ghidra/S2*.java`); the names are the public Katana SDK
API names where the behaviour matches one, otherwise a role name of ours (`lib_Role`, lower-case after the
prefix). Machine-readable list: `port/symbols/sdk_calls.tsv` (address, name, library, confidence, call sites,
callers, evidence). Method and rules: `docs/port/PLAN.md` 9.

## 1. Totals

**224 distinct SDK targets, 2 091 call sites** (direct calls and literal-pool `jsr`; thunks resolved).

| Library | Targets | Call sites | Session |
|---|---|---|---|
| Ninja (incl. 2 Kamui leaf calls) | 126 + 2 | 1 457 | S5 (matrix/maths: S3/S4, pure code) |
| sy (Shinobi: heap, hardware, timer, cache, chain, cfg, boot) | 18 | 511 | S4 |
| sd (Manatee sound) | 28 | 49 | S7 |
| gdFs (GD-ROM file system + CDDA) | 17 | 33 | S4 (CDDA: S7) |
| bu (VMU backup) | 14 | 18 | S6 |
| pd (pads, pdLcd, pdVib) | 9 | 13 | S6 |
| CRI middleware (Sofdec player) | 9 | 9 | later (intro movie) |
| not code (0x8C23D900, section 5) | 1 | 1 | — |

Names: **150 targets carry an SDK API name** (37 high, 69 medium, 44 low confidence); **73 carry a role name**
(behaviour known, SDK name not pinned: mostly the Ninja draw-function families and their light setters, section
2.6, and a few Manatee host commands); 1 is data. Confidence: *high* = several independent signals (an SDK sample
sequence such as `sbInitSystem`/`sbExitSystem`, constant arguments that match the API, a string naming it);
*medium* = one strong signal (data-flow shape + arguments); *low* = plausible name from shape and neighbours.

The most-called: `syMalloc` 266, `syFree` 218, `njDrawSprite2D` 147, `njScalor` 97, `njScalor2` 82, `njRotateY`
75, `njTranslateV` 69, `njPushMatrix`/`njPopMatrix` 64 each, the chunk-model draw `nj_ModelL1` 62,
`njInnerProduct` 61, `njSetTexture` 58, `njColorBlendingMode` 52.

How the game boots (game functions, our names): `0x8C016248` picks the display mode (VGA if the cable check says
VGA, else TV) and calls the game's copy of the SDK sample **`sbInitSystem` (0x8C010184)**: `syHwInit`,
`syMallocInit(heap)`, `nj_SetWorkSize(0x120000)`, `njInitSystem(mode, frame, count)`, `syHwInit2`,
`pdInitPeripheral(0, recv, send)`, `gdFsInit(8, work, dirents, dirbuf)`. **`sbExitSystem` (0x8C01022C)**:
`gdFsFinish`, (game), `pdExitPeripheral`, `njExitSystem`, `syMallocFinish`, `syHwFinish`, then `syBtExit`.
Graphics set-up `0x8C010C28`: two heap blocks, `njInitTextureBuffer(0x8CE40000, 0x1C0000)`,
`njInitVertexBuffer(...)`, `njInitMatrix(stack, 32, 0)`, `njInit3D`, `njInitShape`, `njInitTexture(list, 768)`,
`njCreateLight`, `njTextureFilterMode(1)`, `njControl3D(4)`, `njSetConstantAttr`, `njSetCheapShadowMode(160)`,
`njSetAspect`. Main loop `0x8C0167F8`; per frame `njWaitVSync`; a V-blank callback (`njSetVSyncFunction`) runs
`njSetBackColor`, `njSetBorderColor(0)` and `sdSysServer`.

## 2. By library: what the HLE must do

### 2.1 sy (Shinobi) — S4

| Function | Sites | HLE semantics |
|---|---|---|
| `syMallocInit(heap, size)` / `syMallocFinish` | 1 / 1 | heap over a 32-byte-aligned block; finish is empty |
| `syMalloc(size)` / `syFree(p)` | 266 / 218 | first-fit allocator, 32-byte-aligned blocks, header in front; the game allocates per scene and frees, so a real allocator over a fixed arena is needed (sizes matter: the game checks NULL) |
| `syHwInit` / `syHwInit2` / `syHwFinish` | 1 each | no-ops in the port (cache, interrupt controller, timer set-up) |
| `syTmrGetCount` | 3 | free-running TMU0 count (the game measures time with it): derive from the host clock at the TMU rate |
| `syCacheOCWB(addr, size)` | 1 | no-op (cache write-back) |
| `syChainAddHandler(event, fn, prio, arg)` / `syChainDeleteHandler(h)` | 5 / 1 | interrupt event chains on Holly IRQs: the port calls the registered V-blank / end-of-render handlers from its frame loop |
| `syCfgInit` / `syCfgExit` / `syCfgGetSoundMode(&m)` | 1 each | console settings read from the flash copy in system RAM: sound mode stereo/mono (the game then sends `STRO` or `MONO` to the sound driver) |
| `syBtExit()` | 6 | BIOS vector 0x8C0000E0 with argument 1: return to the console menu. Port: stop / show "exit" |
| `sy_Nop1`, `sy_Nop2` | 1 each | empty functions |

### 2.2 gdFs — S4 (files), S7 (CDDA)

The file API goes through a driver function table (0x8C179DEC) whose entries call the BIOS GD-ROM vector
(0x8C0000BC) via 13 small stubs at 0x8C063024–0x8C063114; the ISO9660 directory code is at 0x8C0DBE14–0x8C0DC6C4.
The HLE replaces the whole stack with the disc reader (track 48 files).

| Function | Sites | HLE semantics |
|---|---|---|
| `gdFsInit(max_open=8, work, max_dirent, dirbuf)` / `gdFsFinish` | 1 / 1 | set up / tear down; read the root directory |
| `gdFsOpen(name, NULL)` → handle | 3 (+2 in SDK) | open by name in the current directory; NULL on failure |
| `gdFsGetFileSize(h, &bytes)` / `gdFsGetFileSctSize(h, &sectors)` | 1 / 3 | size from the directory record (2048-byte sectors) |
| `gdFsRead(h, nsct, buf)` | 3 | synchronous read of whole sectors into a 32-byte-aligned buffer; the game reads complete files |
| `gdFsClose(h)` / `gdFsGetStat(h)` | 3 / 1 | close; state (idle/reading/error) |
| `gdFsGetDrvStat()` | 9 | drive state polled every frame (tray open, no disc → the game's error screen): always "ready" |
| `gdFsEntryErrFuncAll(fn, obj)` | 1 | fatal-error callback (the game's handler exits to the BIOS) |
| `gdFsChangeDir(name)` | 1 | root only on this disc (flat directory) |
| `gdFsDaPlay(start, end, repeat)`, `gdFsDaPause`, `gdFsDaStop`, `gdFsDaRelease` (names low) | 1 each | **CDDA music**: play a range of audio tracks (04–47) with repeat, pause/stop, resume. Port: stream the CDDA tracks from the disc into the mixer (S7) |
| `gdFs_Api34`, `gdFs_Api58` | 1 each | generic drive commands used only on the fatal-error path: no-ops |

Game file loading: `0x8C012E40` (open, size, `syMalloc`, read, close: whole file to heap), `0x8C01311C` (the same
into the fixed buffer at 0x8CE40000), `0x8C0120CA` (load a PVM file and give it to `njLoadTexturePvmMemory`).

### 2.3 pd, pdLcd, pdVib, kd — S6

`pdInitPeripheral(0, recvbuf, sendbuf)` (buffers 0xC020 bytes, the SDK sample sizes), `pdExitPeripheral`, and
**`pdGetPeripheral(port)`** (5 sites) returning the per-port state. The game polls **4 ports**: a table at
0x8C066934 holds 0, 6, 12, 18 (ports A0, B0, C0, D0). Fields it reads (offsets in the returned structure, our
reading of the public layout: id 0, support 4, on 8, off 12, press 16, release 20, r 24, l 26, x1 28, y1 30, x2 32,
y2 34, ..., info 48): **+4 support, +8 on (buttons held), +0x10 press (Start to skip the movie), +0x1C/+0x1E
(stick X/Y, also thresholded into digital directions), +0x20/+0x22, +0x30 (device info: connected/type)**. No
read of +0x18/+0x1A (the analog triggers r/l in that layout) was found at the five call sites; if the layout
holds, the triggers are unused or read as digital bits. To confirm in S6 with the game's input code (open).

- **VMU LCD (pdLcd)**: `pdVmsLcdWrite(port, bitmap, 2)` and `pdVmsLcdIsReady(port)` on ports 1, 7, 13, 19
  (slot 1 of A–D): the game shows a picture on every connected VMU. Port: optional host panel.
- **Jump Pack (pdVib)**: `pdVibMxIsReady(port)`, `pdVibMxStart(port, param)`, `pdVibMxStop(port)`,
  `pdVibMxSetStopTime(port, 20)`: 1 site each, used by the player object code → `rtw_rumble(port, on/off)`.
- **kd** (0x8C0DD130–0x8C0DE2B0) is the Maple bus driver shared by pd, bu, pdLcd and pdVib (frame build, DMA
  start, `tas.b` locks); the HLE does not need it.

### 2.4 bu (VMU backup) — S6

| Function | HLE semantics |
|---|---|
| `buInit(15, 1, work, callback)` / `buExit` | drives A1–D1 (mask 15); completion callback |
| `buSetCompleteCallback(fn)` / `buSetProgressCallback(fn)` (low) | async completion / progress hooks |
| `buGetDiskInfo(drive, &info)`, `bu_GetBlocks(drive, kind)`, `bu_IsReady(drive)`, `bu_IsFormatted(drive)` | capacity, free/used blocks, connected, formatted (root block marker) |
| `bu_FindFile(drive, name)` | look a file up before saving |
| `buMakeBackupFileImage(dst, header)` | build a VMS file image: header (descriptions 16/32 bytes, icon count and speed, palette, icon bitmaps, eyecatch type) + CRC + data |
| `buSaveFile(drive, name, image, blocks, &time, flags)` / `buLoadFile(drive, name, buf, 0, 0)` | write / read the save file; the game polls `buStat(drive)` and `buGetLastError(drive)` until done |

The game saves **one file** (one name constant in game data at 0x8C06F368, one header at 0x8C1B822C, image built
at 0x8C1B8290), with a time stamp from its RTC helper; flags 0x80000000. Load and save paths: game functions
0x8C018E7C (load) and 0x8C018F7C (save). HLE: a VMS file system over the 128 KiB VMU image (retrowser
`save.kind: vmu`), synchronous with immediate completion callbacks.

### 2.5 sd (Manatee) — S7

The game drives the AICA sound driver through Manatee host commands (each wrapper writes a command record and
posts it; most check an "initialised" flag and report errors through one function):

- Driver and memory: `sdDrvInit(drv)` (downloads `MANATEE.DRV`, `SDRV` magic), `sdLibInit`, `sdSysFinish`,
  `sdSysServer` (every V-blank), `sdMemBlkCreate/SetPrm/Destroy`, `sdMultiUnitDownload` (an `SMLT` bank file into
  AICA RAM; uses `sdSndMemAllocateBank`).
- MIDI sequences: `sdMidiOpenPort`, `sdMidiClosePort`, `sdMidiPlay(port, bank, seq, fade)` (cmd 1),
  `sdMidiStop` (2), `sdMidiSetVol`/`sdMidiSetPan` (5/6, order low).
- One-shots: `sdShotOpenPort`, `sdShotClosePort`, `sdShotPlay(port, bank, no, prio)` (17), `sdShotStop` (18),
  `sdShotSetVol`/`SetPan` (21/22), `sdShotSetPitch` (24).
- Global: `sd_SetStereoMono('STRO'|'MONO')`, `sd_SetMasterVol(0..15)`, and four commands we have not named
  (`sd_Cmd53`, `sd_Cmd54`, `sd_CmdBytes`, `sd_CmdU8U8`, `sd_CmdReset`).

HLE (S7): implement the command semantics on our mixer (MLT banks: MIDI sequence + program banks, one-shot bank,
DSP effect programs) rather than emulating the ARM7 driver; music itself is CDDA (2.2).

### 2.6 Ninja (and the two direct Kamui calls) — S5

**System and frame.** `njInitSystem(mode, frame, count)` (mode 1 on VGA, 56 on TV; frame mode 1; count 1),
`njExitSystem`, `njInitVertexBuffer(op, om, tr, tm, pt)` (see 3), `nj_SetWorkSize`, `njWaitVSync` (end of frame:
close the lists, render, flip), `njSetVSyncFunction(fn)`, `njSetEORFunction(fn)`, `njSetBackColor`,
`njSetBorderColor`, `njPrint(loc, fmt, ...)` + `njPrintSize` (debug text, 1 site each).

**Matrix and vector maths (pure code, port as C).** `njInitMatrix`, `njPushMatrix`/`njPopMatrix`,
`njUnitMatrix`, `njGetMatrix`, `njSetMatrix`, `njMultiMatrix`, `njInvertMatrix`, `njTranslate`/`njTranslateV`,
`njScale`/`njScaleV`, `njRotateX/Y/Z`, `njRotateXYZ`, `njRotateZXY` (low), `njRotate(m, axis, ang)`,
`njCalcPoint`, `njCalcVector`, `njInnerProduct`, `njOuterProduct`, `njScalor`, `njScalor2`, `njUnitVector`,
`njSin`/`njCos`/`njTan` (angle units 65536 per turn, `fsca`), `njSqrt`, `njRandom`, `njProjectScreen`,
`njCalcScreen`. Matrices are 4×4 floats, row-major, current matrix pointer + stack depth in globals; `NULL` means
the current matrix. These must match the SH-4 results closely (`fsca`/`fsrra` approximations: decide tolerance in
S3 tests).

**Camera and screen.** `njSetScreen`, `njSetAspect`, `njSetView`, `njInitView`, `nj_InitCamera`,
`nj_SetCamera`, plus `njClip2D` (user clip rectangle, section 3).

**Textures.** `njInitTexture(list, 768)`, `njInitTextureBuffer`, `njLoadTexture(texlist)` (GBIX/PVRT files),
`njLoadTexturePvmMemory(buf, texlist)` (a PVM archive the game read into memory), `njReleaseTexture(texlist)`
(28 sites), `njSetTexture(texlist)` (58), `njTextureFilterMode`. HLE: decode PVR (VQ, twiddled, mipmaps) into
WebGL textures; keep the texlist structure (`{entries*, count}`, entries `{name*, attr, surface}`) because the game
reads it.

**2D.** `njDrawSprite2D(sprite, n, pri, attr)` (147 sites; attr 0x20 alpha, 0x22 alpha+colour),
`njDrawSprite3D(sprite, n, attr)` (26; 0x30/0x32/0x33 = alpha+scale(+colour)(+angle)), `njDrawPolygon`,
`njDrawLine2D`, `njDrawTriangle2D` (low), `njSetConstantMaterial`, `njSetConstantAttr`, `njControl3D`,
`njColorBlendingMode(src|dst, mode)` (52), `njFogEnable`/`njFogDisable`, `njSetFogColor`,
`njGenerateFogTable3`, `njSetFogTable` (writes the 128-entry PVR fog table).

**Models and motions: families.** The game draws with several Ninja draw families. Each family has its own light
globals, set through three setters (direction x,y,z; intensity pair; colour r,g,b) before drawing; members of a
family share those globals. Role names (SDK names to settle in S5 from behaviour: candidates in brackets):

| Family | Model / object | Motion (obj, motion, frame) | Shape motion (obj, motion, shape, frame) | Lights |
|---|---|---|---|---|
| L1 (chunk, most used) | `nj_ModelL1` (62), `nj_ObjectL1` (4) | `nj_MotionL1` | `nj_ShapeMotionL1`, `nj_ShapeMotionL1b` | `nj_LightL1Dir/Intensity/Color` [njCnkEasyDraw*, njCnkSetEasyLight*] |
| L2 (chunk, 4 indexed lights) | `nj_ModelL2` (4) | `nj_MotionL2` | `nj_ShapeMotionL2`, `nj_ShapeMotionL2b` | `nj_LightL2Dir/DirN/Color/ColorN/IntensityN/Update` [njCnk*Multi*] |
| L3 (chunk) | `nj_ModelL3` (5) | `nj_MotionL3`, `nj_MotionL3b` | `nj_ShapeMotionL3`, `nj_ShapeMotionL3b` | `nj_LightL3*` [njCnkSimpleDraw*] |
| L4 | `nj_ModelL4` | `nj_MotionL4` | `nj_ShapeMotionL4` | `nj_LightL4*` |
| B (basic NJS_MODEL, no family light) | `nj_ObjectB`, `nj_ModelB` | `nj_MotionB1/B2/B3` | `nj_ShapeMotionB1/B2/B3` | njCreateLight/njDeleteLight lights [njDrawObject, njDrawMotion, ...] |
| modifier volume | `njCnkModDrawModel` (1 site: the shadow model) | | | — |

Plus `nj_MotionSetup/StepA/B/C/End` (motion evaluation without drawing), `nj_ShapeOnly`, `nj_ReadChunkFile` (a
parser for NJ chunk files: `NJCM`, `NJTL`, `NJBM`, `NJCA`, `NMDM`, `NSSM`, `POF0`; 18 sites), `njInit3D`,
`njInitShape`, `nj_MemCopy4`, `nj_SetCallback`, `nj_SetColor3`, `nj_LightReset`.

HLE: one renderer that walks NJS_OBJECT trees (basic and chunk models), applies the motion / shape-motion
interpolation (frame float, keyframe types per motion flags), lights per family and emits WebGL draws per list
type. The draw calls are where most of the S5 work is.

### 2.7 CRI middleware (Sofdec) — later

`mwPlyInitSofdec`, `mwPlyFinishSofdec`, `mwPlyCalcWorkCprmSfd(...)` (work size, the game allocates it),
`mwPlyCreateSofdec(cprm)` (then the game calls the player through its function table: start by file name, get
status, stop, destroy), `mwPlySetOutVol` (low), `mw_PreInit`, `mw_SetErrFunc`, `mw_ExecServer`, `mw_ExecVsync`.
Used once, for `PEN_OP.SFD` (game function 0x8C01DEF8: plays until the end or Start). Port: skip the intro, or
decode MPEG-1 + ADX later. The movie frames reach a texture through the TA YUV converter (Kamui code at
0x8C11F972, called from the Sofdec player only).

## 3. Graphics checks (PowerVR / Kamui)

- **Lists used: opaque, opaque modifier volume, translucent.** The game's `njInitVertexBuffer` call (in
  0x8C010C28) passes op = −0x80000 (a special value; meaning to settle in S5), **om = 24 576**, **tr = 0x40000**,
  **tm = 0**, **pt = 0**: no translucent modifier volumes and **no punch-through list**. Kamui's per-list table
  (0x8C123040) holds all five list headers, but only lists with a non-zero size are opened.
- **Modifier volumes: yes, opaque, for cheap shadows.** `njSetCheapShadowMode(160)` writes the PVR shadow scale
  register (FPU_SHAD_SCALE, register offset 0x74: enable + intensity 160/256). `njCnkModDrawModel` (one site,
  0x8C0373B8, a shadow model) reaches the only Ninja code that builds modifier-volume headers (list type 1:
  0x81000000 / 0x81000040 in 0x8C0E4210/4258/42A0). Port: shadow volumes in WebGL (stencil) darkening by
  160/256, or a cheaper approximation.
- **Render to texture / frame-buffer read-back: no.** Kamui's render-to-texture path (0x8C11E220: programs
  FB_W_SOF1, line stride and clip for a target) is reached only from 0x8C11E9F2 and 0x8C11EFF6, which have no
  callers and no pointer references: dead code. Frame buffers are only flipped (0x8C11DBF6 writes FB_R_SOF/FB_W_SOF
  pairs). VRAM addresses are loaded only by Kamui's texture upload / memory set-up helpers (0x8C13FE82,
  0x8C1410FE, 0x8C141AFC–0x8C141E82, called from Kamui texture functions) and by one unreferenced Ninja function
  (0x8C0A29D4); no frame-buffer read-back path was found, but the helpers' direction (write vs read) was not
  checked one by one: confirm in S5. Texture writes come from the PVR loads and the Sofdec YUV upload.
- **Split screen: per-player screen + view + user tile clipping.** The main loop draws players 0–3 in turn
  (game 0x8C010DDE(n) → 0x8C011C7C): each sets `njSetScreen` (centre/size), `nj_SetCamera` and then
  `njClip2D(2, rect)` (0x8C0A254A), which writes a user-clip rectangle into every enabled list through Kamui
  (0x8C12390A per list type); the HUD pass uses a full-screen rectangle. Port: viewport + scissor per player.
- **Video modes**: `njInitSystem` mode 1 when the cable check reports VGA, else 56 (TV, interlaced 640×480 NTSC);
  frame-buffer mode 1, V-sync count 1 (60 Hz). PAL modes not used (US disc; the region byte check in 0x8C0623B4
  selects between the two).
- Fog: table fog (`njGenerateFogTable3` + `njSetFogTable`, `njSetFogColor`), toggled per pass.

## 4. Data files: the container format

All `*.BIN`/`*.DAT` containers (except 1ST_READ) are loaded by game function **0x8C012EBE**: it reads the whole
file to the heap (0x8C012E40), then walks the 16-byte records `{type, offset, count, extra}` until type 0. For
each record it turns `offset` into a pointer (file base + offset, function 0x8C012CF4) and calls the fix-up for
the type, which turns the internal file offsets of that structure into pointers (the file is position-independent:
every pointer field is an offset from the file start).

| Type | Fix-up | Structure (our reading) |
|---|---|---|
| 1 | 0x8C013188 | object tree, **basic models**: object 0x34 bytes (flags, model +4, pos/ang/scale, child +0x2C, sibling +0x30); model fix-up 0x8C0131D6 = NJS_MODEL (points, normals, meshsets with their own arrays, materials) |
| 2 | 0x8C013264 | motion: `{data*, frames, type flags (u16 at +8), ...}`; per-node key arrays counted from the type flags (pos/ang/scale/... bits); needs the node count (record `count`) |
| 3 | 0x8C013654 | texture list `{entries*, count}`, entries 12 bytes `{name*, attr, surface}` |
| 4 | 0x8C013696 | pointer to a type-1 object |
| 5 | 0x8C0136CE | pointer to a motion (node count from the record) |
| 6 | 0x8C01370E | pointer to a texture list |
| 7, 8 | none | raw data (only the record offset is relocated) |
| 9 | 0x8C013746 | action `{object*, motion*}` (basic object) |
| 10 | 0x8C013790 | pointer to an action |
| 11 | 0x8C0137D0 | `{action*, node count}` |
| 12 | 0x8C01380A | `{motion*, motion*}` (two motions, e.g. a motion link) |
| 13 | 0x8C013856 | pointer to a type-12 pair |
| 14 | 0x8C013896 | `{motion*, data*, node count}` |
| 15 | 0x8C0138E6 | object tree, **chunk models** (same object layout; model fix-up 0x8C013934 = chunk model `{vlist*, plist*, ...}`) |
| 16 | 0x8C013954 | pointer to a type-15 object |
| 17 | 0x8C01398C | action with a chunk object `{object*, motion*}` |
| 18 | 0x8C0139D6 | pointer to a type-17 action |
| 19 | 0x8C013A16 | `{chunk object*, motion*, node count}` |

Seen on the disc (S0): characters 15 (chunk objects), 5 (motions), 8 (raw); courses 7 ×14, 8, 1 (basic-model
objects); traps 14/16 pairs. PVM archives are read whole (0x8C0120CA) and handed to `njLoadTexturePvmMemory`; MLT
banks go to `sdMultiUnitDownload`; `.NJ`/`.NJM` files are parsed by `nj_ReadChunkFile` (18 sites). Port: the same
relocation in C (S4), then the S5 renderer reads the structures directly.

## 5. 0x8C23D900 is not code

The image tail after the zero block holds the **static-constructor table**: 0x8C23D900–0x8C23D90C are three
pointers to game functions (0x8C01212A, 0x8C038750, 0x8C03C2BC) and a terminator; 0x8C23D910/0x8C23D914 are the
begin/end of that list, 0x8C23D918/0x8C23D91C the begin/end of an **empty** destructor list. The game's `main`
wrapper (0x8C0164B4) runs the constructors (0x8C0164EC: call every entry from begin to end), the game, then the
destructors (0x8C016510). Ghidra folded the destructor loop's first load into a call to 0x8C23D900; at run time
that loop never runs. Nothing is copied there: the zero block 0x8C197090–0x8C23D900 is `.bss`, cleared at start-up
by the section-initialisation routine 0x8C0100D8 (range tables at 0x8C0668DC and 0x8C179974; the last cleared range
ends at 0x8C23D900), and the tail is initialised data (with 467 function-pointer words, the game's callback
fields). S2 removes the bogus function (`port/symbols`: region `data`).

## 6. Recompiler notes for S3

Counts over the whole image (game / C runtime / SDK), from `tools/ghidra/S2Recomp*.java`:

- **Indirect calls and jumps**: `jsr` through a literal-pool constant 4 750 / 133 / 5 807; register-indirect
  `jsr` (target loaded from memory: structure fields, tables, vtables) **604 / 30 / 1 633**; `jmp` through a
  literal 821 / 2 / 561 (tail calls), register-indirect `jmp` 5 / 24 / 92. A recompiler needs a run-time dispatch
  from guest address to translated function for all known entries (7 925), plus the BIOS vectors 0x8C0000B0,
  0xB8, 0xBC, 0xE0 (HLE). No literal call target lies outside the image.
- **Jump tables**: `braf` 11 game + 10 SDK (SHC switch tables, recovered by Ghidra for 19 of them); `jmp`-based
  tables 17 in the SDK (recovered).
- **Function-pointer data**: 2 795 words (480 targets, 505 tables of ≥ 3) in the SDK data area
  (vtables/handler tables); 467 words in the game's initialised data at the tail; none in the 0x8C066904–
  0x8C0998D4 data block.
- **FPU modes**: `fschg` 2 170 and `frchg` 136, **all in the SDK** (Ninja/Kamui paired-move and matrix code); the
  game code never switches FPU mode. `lds …,fpscr` 33 in the C runtime (double-precision helpers set PR around
  their work, values computed from the saved FPSCR) and 17 in the SDK; the one literal FPSCR value is 0x40001
  (round-to-zero, denormals-as-zero, single precision): the default state the translator assumes. 41 delay slots
  hold an `fschg`/`frchg` and 9 an FPSCR load: track FPU mode across delay slots.
- **Delay slots**: 44 066 delayed branches; no PC-relative load or `mova` in a delay slot; one delay slot is also
  a branch target (SDK 0x8C119FC4); 2 slots overwrite the jump register (0x8C0625EC rt, 0x8C12B700 SDK).
- **Self-modifying / copied code: none found.** No write references into function bodies; the start-up only
  clears `.bss`. But **code is entered through the P2 (uncached) mirror**: 10 flows jump to 0xAC0xxxxx aliases of
  the image (cache control in syCache at 0x8C0998D4–0x8C09A0B0, and calls to it from `syHwInit`,
  `mwPlyInitSofdec`, `mwPlyFinishSofdec`, plus sy 0x8C16F40C/0x8C16F4EA): map 0xA0000000-region addresses onto
  the 0x80000000 ones.
- **Cache / store-queue ops (SDK only)**: `pref` 3 922 (store-queue flushes to the TA and texture memory),
  `ocbwb` 1 050, `ocbi` 138, `ocbp` 7, `movca.l` 44: translate as no-ops, with store-queue writes routed to the
  HLE (the HLE replaces the TA paths anyway).
- **System / interrupts**: `rte` 3 (syFbr context switch 0x8C0E2E10, exception entries 0x8C12B420/0x8C12B56C),
  `ldc …,vbr` in 0x8C12B420/56C/700 (SDK) and one apparent `ldc …,vbr` inside game function 0x8C052D18 at
  0x8C052D1E (probably data decoded as code: check in S3), `ldc …,sr` 3 game (interrupt mask in
  `sbInitSystem`/`sbExitSystem`) / 6 rt / 175 SDK, banked-register loads in the fiber switch, `tas.b` 28 (kd
  locks). No `trapa`, `sleep` or `ldtlb`. **syFbr** (fibers: user-level context switching) is used by the CRI
  middleware only; the game itself is single-threaded with V-blank callbacks.
- **Integer division** goes through SHC runtime helpers (`div1` ×145 in rt); `mac` instructions are not used.

## 7. Region table changes (S2)

`port/symbols/regions_s2.tsv` overrides the S1 labels for 23 ranges; `tools/s2_apply.py` (run by
`tools/ghidra.sh export`) applies them and the names. Method for the system libraries: the call graph from the
identified API functions plus **data order**: each library's version string sits in the data section in the same
order as its code objects (syCache, gdFs, pd, syMalloc, syHw, syTmr, pdLcd, bu, syCfg, syBt, …, SJ, syChain, syInt,
sySq, kd, syBtCfg, syFbr, pdVib, fmMng), which matched every API function placed so far.

| Range | Was | Now |
|---|---|---|
| 0x8C062440–0x8C06245E, 0x8C063024–0x8C06315A, 0x8C063D54–0x8C063D9A | shc_rt | sy (cable check), bios_stub (GD, sysinfo, flash syscall wrappers) |
| 0x8C09CEC8–0x8C09ED2E | unknown + sy | **gdFs** (API + driver table) |
| 0x8C09ED34–0x8C09F41E, 0x8C09FD04–0x8C09FE68, 0x8C0A10F4–0x8C0A1262 | sy, Ninja | **pd**, pdLcd, pdVib API |
| 0x8C09FE68–0x8C0A0E7C | sy | **bu** API |
| 0x8C0DAE06–0x8C0DBE12 | Kamui | CRI_SJ (low) |
| 0x8C0DBE14–0x8C0DC6C0 | Kamui | **gdFs** (GD driver + ISO9660) |
| 0x8C0DC6C4–0x8C0DD0C0 | Kamui | sy (syChain/syInt/sySq) |
| 0x8C0DD130–0x8C0DE296 | Kamui | **kd** (Maple driver) |
| 0x8C0DE2B0–0x8C0E2BC8 | Ninja | **bu** core |
| 0x8C0E2BCC–0x8C0E3266 | Ninja | sy (syBtCfg, syFbr) |
| 0x8C0E3268–0x8C0E342C | Ninja | pd (pdVib) |
| 0x8C12B350–0x8C12B7B2 | pd | sy (exception entries) |
| 0x8C16F718–0x8C17994A | unknown | Ninja (strip emitters: store-queue output, called from Ninja) |
| 0x8C23D900 | unknown | data (constructor table) |

Function counts by label after S2: Ninja 1 573, CRI 1 003 (mw 316, SFD 353, ADX 268, SJ 66), Kamui 256, sy 227,
bu 195, sd 149, pd 129, gdFs 121, kd 56, shc_rt 110, bios_stub 20, **unknown 70** (was 213).

## 8. Open

- SDK names of the Ninja draw families (L1–L4, B) and their lights; `njRotateZXY`, `njInitView`/`njSetView`
  and the camera pair; the meaning of op = −0x80000 in `njInitVertexBuffer`.
- Manatee: the unnamed commands (53, 54, byte-array, 2-byte, reset); MIDI/shot vol/pan order.
- The CDDA function names (`gdFsDa*` are our guesses for play/pause/stop/release).
- pd structure: confirm the trigger fields (+0x18/+0x1A) are really unused.
- 70 functions still `unknown` (0x8C09C9E0–0x8C09CEC6, 0x8C129DDC–0x8C129E9A, 0x8C12B092–0x8C12B350); SJ
  boundaries inside CRI.
