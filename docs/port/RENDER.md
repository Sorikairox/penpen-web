# Pen Pen TriIcelon: the renderer (S5)

The game's Ninja/Kamui drawing is replaced by HLE functions that build a **per-frame draw list in our own
format**; a **WebGL2 backend** in JavaScript draws it. In the retrowser module (`video: webgl`, the module in a
worker) the backend runs on the host's OffscreenCanvas; for tests the same backend runs in headless Chromium
on dumps of the draw list. Everything in `port/render/`; our own code, written from the public descriptions of
the PowerVR2, the Ninja API and the Ninja chunk format, and from what the game's calls show (S2 census,
`tools/ghidra.sh decomp` read in the terminal only).

## 1. Pieces

| File | What |
|---|---|
| `rd.h`, `rd.c` | draw list builder: screen-space vertices, lists, polygon state, strips to triangles, culling, translucent sort, packing, texture upload/free records |
| `pvr.h`, `pvr.c` | PVR texture decoding (PVRT + GBIX chunks) |
| `nj_draw.c` | frame hooks, render state (blending, back colour, user clip, filter, fog), textures (PVM, texmemlist), `njDrawSprite2D` |
| `nj_model.c` | 3D: chunk objects/models with motions and shape motions, lighting (L1 setters), `njDrawSprite3D`, cheap-shadow volumes |
| `nj.h` | shared declarations, the Ninja globals the HLE reads |
| `gl/render_gl.js` | WebGL2 backend (classic script: `self.PenpenGL.create(canvas).frame(buffer, ptr)`) |
| `shot/shot.html`, `shot/shot.mjs` | capture page + DevTools driver (headless Chromium in `docker/chromium`) |
| `tools/s5.sh` | `shots FRAMES [run args]`, `render`, `look NAMES` |

The module calls one import per frame, `env.penpen_gl_frame(ptr, len)`, from `njWaitVSync` (end of render). The
node stand-in host (`port/runtime/node/run.mjs`) implements it without GL: it keeps the uploaded textures and
writes **dumps** of chosen frames (`--shots F1,F2 --shot-dir DIR`). The retrowser glue (S9) will implement it as
`r.frame(memory.buffer, ptr)` on the worker's OffscreenCanvas.

## 2. The draw list format

Little-endian u32 words unless said. Offsets are relative to the header; texture pointers are addresses in the
same memory (the wasm memory; a dump file is its own memory with the header at 0).

- **Header (24 words)**: `'PPDL'`, version 1, frame, width 640, height 480, back colours 0/1/2 (ARGB), ncmd,
  cmd_off, nvert, vert_off, nup, up_off, nfree, free_off, fog colour, fog_off (128 floats, 0 = no table yet),
  opaque command count, translucent triangles, volume triangles, culled triangles, cheap shadow intensity |
  0x100, FOG_DENSITY register value.
- **Command (8 words)**: list (0 opaque, 1 opaque modifier volume, 2 translucent, 3 punch-through), first
  vertex, vertex count (triangles), texture id (0 none), mode, clip x0 | y0 << 16, clip x1 | y1 << 16, 0.
- **Mode bits**: 0–2 PVR src factor, 3–5 dst factor (0 zero, 1 one, 2 other colour, 3 1 − other, 4 src α,
  5 1 − src α, 6 dst α, 7 1 − dst α), 6 depth write, 7–9 PVR depth compare on 1/w (0 never … 6 gequal, 7
  always), 10 bilinear, 11/12 clamp U/V, 13/14 flip U/V, 15–16 texture shading (0 decal, 1 modulate, 2 decal
  alpha, 3 modulate alpha), 17 use vertex alpha, 18 ignore texture alpha, 19 offset colour, 20 fog, 21 PCW shadow bit (opaque modifier volumes darken it).
- **Vertex (28 bytes)**: x, y (pixels), 1/w, u, v (floats), base ARGB, offset ARGB.
- **Upload (4 words)**: id, width, height, pointer to RGBA8. **Free (1 word)**: id.

C side: strips become triangles (odd ones re-wound), screen-space culling, the opaque and volume lists keep the
submission order (merged into one command per run of equal state), the translucent list is **sorted per
triangle back to front by mean 1/w (stable)**: an approximation of the PVR's per-pixel autosort.

Backend: background plane (3 colours, the 4th extrapolated), then pass 0 opaque + punch-through, pass 1
modifier volumes (stencil invert where the volume face is in front of the stored depth; then the pixels with
odd parity × intensity/256), pass 2 translucent with blending, depth test, no depth write. Depth is
`gl_FragDepth = 1 / (1 + 1/w)` (from `gl_FragCoord.w`; PVR "greater 1/w" = GL "less"), so 2D (1/w ≤ 1) and 3D
share one buffer; `gl_Position.w = w` keeps texturing perspective-correct. Textures: one GL texture per upload,
mipmaps generated for power-of-two sizes.

## 3. PVR textures (`pvr.c`)

PVRT chunk (optionally after a GBIX chunk: global index): pixel format byte, data format byte, width, height.
Decoded to RGBA8, top mipmap level only:

- pixel formats ARGB1555, RGB565, ARGB4444, RGB555, YUV422 (pairs of texels share U/V), ARGB8888; bump maps as
  grey; **palettised (4/8 bpp) not supported** (needs the palette RAM; not met on the route so far).
- data formats: twiddled (Morton, y in even bits; rectangles = squares of min(w, h) in sequence), twiddled
  mipmaps (top level at 6 + 2·(w²−1)/3 bytes), VQ and small VQ (codebook of 2×2 entries, texel order (0,0)
  (0,1) (1,0) (1,1), indices twiddled at half size; small-VQ codebook size by width), VQ mipmaps, rectangle,
  stride, BMP. A size check falls back to "the top level is the last w·h texels".

`njLoadTexturePvmMemory(pvm, texlist)`: PVMH header (flags, count, per-entry name/format/dims/global index),
then the textures; the texlist gets the archive's count and entry i ↔ texture i; each entry's texaddr points
to an `NJS_TEXMEMLIST` entry (0x44 bytes) of the array given to `njInitTexture` (global index +0, flags +0x10,
our width/height +0x20/+0x22, count +0x40). Same global index = shared, counted. `njReleaseTexture` counts down
and frees. Texture lookup for drawing: current texlist (`njSetTexture`, guest code, global 0x8C1823F0) →
entry → memlist entry (or by global index when the entry has attr bit 24) → backend id. Decoding happens at
load time because the game reads the PVM into the texture buffer area and overwrites it later.

## 4. Render state and 2D (`nj_draw.c`)

- **Guest code for pure setters** (hle.c `guest[]`): `njSetTexture`, `njControl3D` (0x8C1EF2EC),
  `njSetConstantAttr` (0x8C1EF320/324), `njSetConstantMaterial` (NJS_ARGB floats at 0x8C1EF328),
  `njInitTextureBuffer`, `njGenerateFogTable3` (fills the table, writes FOG_DENSITY 0x5F80B8). The HLE reads
  their globals.
- `njColorBlendingMode(0 src | 1 dst, mode)`: Kamui modes 0 both-inverse-src-α, 1 both-src-α, 2 dst α,
  3 dst colour, 4 1 − dst α, 5 1 − dst colour, 6 1 − src α, 7 1 − src colour, 8 src α, 9 src colour, 10 one,
  11 zero → PVR factors.
- `njSetBackColor(c0, c1, c2)`: the background plane colours. `njSetBorderColor`: not shown.
- `njClip2D(mode, rect)`: **rect = {tile min, tile max, pixel min, pixel max}** as NJS_POINT2 (inclusive;
  {0,0} {19,14} {0,0} {639,479} = whole screen): the pixel rectangle becomes every following command's clip
  (split screen: per player).
- Fog: `njSetFogColor(ARGB)`, `njSetFogTable(Float[128])`, `njFogEnable/Disable` per polygon; the backend
  indexes the table like the PVR: z = clamp(1/w · density, 1, 256), index = 16 · ⌊log2 z⌋ + 4 mantissa bits,
  linear between entries.
- `njDrawSprite2D(sprite, n, attr)`, pri in FR4: NJS_SPRITE {p, sx, sy, ang, tlist, tanim}, **NJS_TEXANIM is
  20 bytes of Sint16** {sx, sy, cx, cy, u1, v1, u2, v2, texid, attr}, u/v in 1/256; quad (−cx..sx−cx) ×
  scale, 1/w = 1/−pri (at most 1); attr 1 angle, 2 colour (constant material), 4/8 flip, 0x20 alpha
  (translucent list, current blending); texture shading modulate-alpha. Sets the current texlist.

## 5. 3D (`nj_model.c`)

- **The game's `nj_Model*` calls pass an `NJS_CNK_OBJECT`** (eval flags, model, pos, ang, scl, child,
  sibling; usually with BREAK), not a bare model; a pointer in word 0 is taken as a bare model.
  `nj_ObjectL1` = object tree, `nj_Motion*` = (object, motion, frame in FR4), `nj_ShapeMotion*` = (object,
  motion, shape, frame in FR4). All families (L1–L4) use one implementation for now.
- **Node transforms by the SDK's own matrix code** (guest calls: njPushMatrix, njTranslate, njRotateXYZ /
  njRotateZXY, njScale, njPopMatrix on the current matrix whose pointer is at 0x8C1EF2B8): the HLE reads the
  resulting 4×4 (row vectors, translation in elements 12–14). **This SDK's njTranslate / njRotateXYZ /
  njRotateZXY / njScale return at once when the matrix argument is NULL** (not "NULL = current matrix"): the
  HLE passes the current matrix pointer itself. S5 passed NULL, so every node transform of a model tree was
  lost (S8 fix: the title's ice tube sat 544 units too close and unscaled, in front of the logo; the
  characters' limbs were all at their root).
- **Conventions, checked at run time** (`render check` line: our transform = the SDK's njCalcPoint): view space
  **+z forward, y down the screen**; screen = cx + dist·x/z, cy + dist·y/z with njSetScreen's {dist, w, h, cx,
  cy} at 0x8C1EF2A4 (554.3, 320, 240 here; 60° horizontal); near plane at 0x8C1EF2D0 (1.0; far 10000).
  Triangles are clipped against the near plane in view space before projection. Front faces are
  counter-clockwise on screen.
- Vertex lists: types 32–50 (position, normal, 10:10:10 normals, diffuse D8, user/Ninja flags); NF/VN_NF
  weighted vertices accumulate per index (flags & 3: first / middle / last, weight = high 16 bits / 255).
- Polygon lists: bits (1 blend, 4 cache the rest of the list, 5 draw a cached list), tiny (texture id 13 bits,
  filter, clamp/flip flags in the header byte), material (diffuse/ambient/specular ARGB, blend in the header
  byte), strips 64–75 (UV 1/256 or 1/1024, normals, colours, second UV, user flags after the 2nd vertex;
  negative length = reversed), volumes 56–58.
- Strip flags: 0x01 ignore light, 0x08 use alpha (translucent list, the material's blend factors), 0x10
  double sided (no culling), 0x40 environment map (UV from the view-space normal, the material's texture).
- Colour: strip colours, else vertex colours, else the **family's light** (S8): L1/L3/L4 one directional
  light each: diffuse × (ambient + max(0, −n·L) · intensity) × light colour (`nj_LightLnDir/Intensity/Color`,
  FR4–FR6); L2 four lights (`Dir` = light 0, `DirN/ColorN/IntensityN(i)`, ambient colour `L2Color`, taken by
  `L2Update`): diffuse × (ambient + Σ max(0, −n·Lᵢ) · intensityᵢ · colourᵢ). Values seen (set every frame):
  L1/L3/L4 (0, .45, .89), 1, ambient .4; L2 light 0 (0, −.71, −.71) white, lights 1–3 zero, ambient .4.
  **Specular** (L2/L3/L4, not the L1 "easy" family; strip flag 0x02 ignores it): the material's specular
  colour × (n·h)^e, e = the specular alpha byte, h = half vector of light and eye, into the offset colour
  (mode bit 19). The title tube (L3, specular 0x0BFFFFFF) shows highlight rings. `njControl3D` bit 0x10
  (constant material): the constant material replaces the diffuse colour, alpha included (the title's two ice
  layers cross-fade this way).
- Motions: NJS_MOTION {mdata, nbFrame, type, inp_fn}; per node n = popcount(type) pointers then n counts;
  pos/ang/scl keys {frame, 3 values} linear (angles as Sint32 differences), wrap from the last key to the first
  over nbFrame; a channel with keys overrides the UNIT flag. Shape motions: VERT/NORM keys {frame, pointer},
  **linear blend of the two keys around the frame** per point / normal (S8; normals renormalised).
- **Basic models** (S8): the game's `nj_ObjectB` (0x8C0C30A8, one site, once per character in races and the
  attract demo) takes a basic **NJS_MODEL** {points, normals, nbPoint, meshsets, mats, nbMeshset/nbMat,
  center, r}, not an object (the SDK saves the polygon context, checks nbPoint against the vertex buffer,
  draws, restores). Meshsets (24 bytes: type<<14 | material, nbMesh, meshes, attrs, normals, vertex colours,
  UVs per corner; triangles, quads in strip order, N-gons and strips with a count, 0x8000 = reversed),
  materials (20 bytes: diffuse, specular, exponent, attr_texId, attrflags with blend factors in bits 31–26,
  ignore light 0x02000000, double side 0x00800000, environment 0x00400000, texture 0x00200000, alpha
  0x00100000, ignore specular 0x00080000, flips/clamps 0x40000–0x8000, filter 13–14). It is the slide trail
  behind each character (32 quads, vertex colours fading out, texture 202, alpha). Lit with the L1 light.
- 2D (S8, nj_draw.c): `njDrawPolygon(vtx {x, y, z, ARGB}, n, trans)` = one strip, z used as 1/w as the SDK
  does; `njDrawTriangle2D/njDrawLine2D(NJS_POINT2COL, n, pri, attr)` with the attr bits read in the SDK (1
  fan, 2 connected, 0x40 translucent; 0x80000000 texture and 0x20 not modelled), lines as 1-pixel quads.
  None of them, nor `njPrint`, is called in 6 000 frames of the boot or of the S4 route.
- `njDrawSprite3D(sprite, n, attr)`: the sprite quad in the model xy plane through the current matrix.
- Cheap shadow: `njCnkModDrawModel(object)` (the shadow object) → its model's volume chunks into the opaque
  modifier volume list; `njSetCheapShadowMode(160)` intensity. **Only polygons with the PCW shadow bit are
  darkened** (S8): the SDK's polygon context builder (0x8C0E4340) sets PCW bit 7 when njControl3D has 0x400,
  or 0x1000 and the polygon has no alpha. The game draws the course with njControl3D 0xC04 and the
  characters with 0x004, so the player's body is not shaded. Backend: pass 0 writes stencil bit 7 from the
  polygon's mode bit 21, pass 1 the volume parity in bit 0, the darkening needs both.
- njControl3D 0x800 (CNK constant attr, S8): the SDK masks every chunk strip header with njSetConstantAttr's
  AND/OR words (seen: AND 0xFFFF9FFF OR 0x2000 on the course = no environment map, flat shading; OR 0x200 =
  ignore specular). A third global (0x8C1EF350, always 0 on the route) adds use-alpha in one draw family:
  not modelled.

## 6. Capture path (tests)

`tools/s5.sh shots 100,300,500 --frames 501 [--pad ...]`: builds the wasm (emsdk image), runs the boot in node in
a `--network none --read-only --cap-drop ALL` container that writes `penpen-build:/build/shots/dump/fNNNNN.ppdl`,
then the `penpen-chromium:1` image (Debian chromium + node 22, built from `docker/chromium/Dockerfile`; network
only at build time) renders each dump with `render_gl.js` (SwiftShader WebGL2) to `/build/shots/png/`, in a second
sandboxed container (static server on 127.0.0.1 inside it). `tools/s5.sh look fNNNNN` copies a PNG (magic
checked) to the session scratchpad for viewing; PNGs never enter the repo. Debug knobs: `LISTS=mask` (draw only
some lists), `CMDS=1` (per command: list, texture, mode, clip, triangles, box, 1/w range), `PROBE=x,y`
(triangles covering a pixel and their 1/w).

## 7. Results (S5) and known gaps

Route: `--pad 600:START,700:A,1100:START,1200:A,1500:A,1700:A,1760:A` (same frames as S4: menu 607, notice 706,
house 1104, course map 1563, race 1762).

| Frame | Seen |
|---|---|
| 100 | Infogrames logo (multicoloured rings, wordmark), fading in over the grey backdrop |
| 300 | GE (General Entertainment) logo, green, wordmark below |
| 500 | title: 3D "PenPen TriIcelon" logo (shape motion), a ring halo, a swirling env-mapped ice funnel (two cross-faded translucent layers), "PRESS START BUTTON", © 1998 1999 GE |
| 650 | mode menu: TRICELON / TIME TRIAL / VSICELON with the penguin cursor, the logo moved to the top left |
| 750 | the S4 "167-sprite screen" identified: a memory card notice ("To save your play data, insert a memory card with 5 blocks ... press Start button to begin the game now") over the menu |
| 1150 | "PenPen House": RACE / DRESS UP / RESULTS / GOOD NIGHT menu, the room (bed, radio, round windows, rug), six characters in 3D |
| 1600 | course map: "SWEETS", course owner Mrs. CREAM, a swirled cream cone in 3D, patterned backdrop |
| 2300 | race start: TIME 0'00"94, "GO!", lap/position 5/5 with a character portrait, START banner arch, checkered pink/mint ground, snowy hills, trees, sky, rivals ahead, the player (Sparky) from behind with a shadow |
| 3000 | 12.6 s in: the player still at the start line (the script presses nothing after 1760), rivals gone ahead, distance haze (fog) |

Known wrong or missing (to do):

- Translucency is sorted per triangle (no per-pixel autosort); no punch-through list is used by the game.
- Flat shading and ignore-ambient strip flags ignored; the L2 IntensityN second value (100 on the route) is
  not used; the specular model is our reading (no SDK constant checked).
- Spline motions (inp_fn) are interpolated linearly.
- `nj_ModelB` (1 site, never called on the routes), the B-family motions and `njPrint` not implemented.
- Palettised textures not decoded; `njLoadTexture` (GBIX/PVRT from a file) not implemented (1 site).
- `njCalcScreen`'s guest result did not match the formula in the self-check (its arguments are not
  (point, sx*, sy*)); the projection is validated by the pictures instead.
- Mipmaps are generated by GL, not taken from the files; mipmap D adjust ignored; bilinear only (no
  trilinear / super-sampling flags).
