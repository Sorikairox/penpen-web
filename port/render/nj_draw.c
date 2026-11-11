/* Pen Pen port, S5: Ninja drawing HLE, part 1: frame, render state, textures, 2D primitives.
 * Our own code, written from the Ninja API's public behaviour and from what the game's calls show
 * (docs/port/RENDER.md 4). Structures (guest memory, little-endian):
 *   NJS_TEXLIST {NJS_TEXNAME *textures; Uint32 n}, NJS_TEXNAME {name*, attr, texaddr} (12 bytes):
 *     texaddr = NJS_TEXMEMLIST* after a load (attr bit 24 NJD_TEXATTR_GLOBALINDEX: a global index)
 *   NJS_TEXMEMLIST (0x44 bytes, the array given to njInitTexture): +0 global index, +0x10 flags,
 *     +0x14 texinfo (we keep width/height at +0x20/+0x22), +0x40 count (u16), +0x42 error (u16)
 *   NJS_SPRITE {NJS_POINT3 p; Float sx, sy; Angle ang; NJS_TEXLIST *tlist; NJS_TEXANIM *tanim} (32)
 *   NJS_TEXANIM {Sint16 sx, sy, cx, cy, u1, v1, u2, v2, texid, attr} (20 bytes; u, v in 1/256)
 *   PVM archive: 'PVMH', header size, u16 flags, u16 count, entries {u16 id, name[28] if flags&8, u16
 *     format if &4, u16 dims if &2, u32 global index if &1}; the textures (GBIX + PVRT chunks) follow
 *     the header. */
#include <stdlib.h>
#include "nj.h"
#include "pvr.h"

#define R(i) c->r[i]
static uint32_t frame_no;

float nj_f(uint32_t a) { uint32_t u = rd32(a); float f; memcpy(&f, &u, 4); return f; }
static uint32_t f2b(float v) { int i = (int)(v * 255.0f + 0.5f); return (uint32_t)(i < 0 ? 0 : i > 255 ? 255 : i); }
uint32_t nj_argb_f(uint32_t a) {
  return f2b(nj_f(a)) << 24 | f2b(nj_f(a + 4)) << 16 | f2b(nj_f(a + 8)) << 8 | f2b(nj_f(a + 12));
}

/* ---- render state shared by the draw functions ---- */
uint32_t nj_blend_src = 4, nj_blend_dst = 5; /* src alpha, 1 - src alpha */
int16_t nj_clip[4] = {0, 0, RD_W, RD_H_};
int nj_filter = 1;
int nj_fog_on;
void nj_state(rd_state *s, int list, uint32_t tex, int use_alpha) {
  s->list = (uint32_t)list; s->tex = tex;
  s->mode = RD_M_SRC(nj_blend_src) | RD_M_DST(nj_blend_dst) | RD_M_ZFUNC(6) | RD_M_SHADE(3) | (nj_filter ? RD_M_FILTER : 0) |
            (use_alpha ? RD_M_ALPHA : 0) | (list == RD_TR ? 0 : RD_M_ZWRITE) | (nj_fog_on ? RD_M_FOG : 0);
  memcpy(s->clip, nj_clip, sizeof s->clip);
}

/* ---- frame ---- */
static uint32_t back_c[3] = {0xFF000000u, 0xFF000000u, 0xFF000000u};
void nj_frame_begin(uint32_t f) { frame_no = f; rd_begin(f); }
#define PVR_FOG_DENSITY 0xA05F80B8u
void nj_frame_end(void) { rd_back_color(back_c[0], back_c[1], back_c[2]); rd_fog_density(rd32(PVR_FOG_DENSITY)); rd_end(); }
static void h_njSetBackColor(SH4 *c) { hle_logcall(c, ""); back_c[0] = R(4); back_c[1] = R(5); back_c[2] = R(6); R(0) = 0; }
static void h_njSetBorderColor(SH4 *c) { hle_logcall(c, ""); R(0) = 0; } /* outside the picture: not shown */
/* njColorBlendingMode(target, mode): target 0 = source, 1 = destination; Kamui's blending modes */
static uint32_t blend_code(uint32_t mode, int dst) {
  switch (mode) {
  case 0: return dst ? 4 : 5;  /* both inverse source alpha */
  case 1: return dst ? 5 : 4;  /* both source alpha */
  case 2: return 6;            /* destination alpha */
  case 3: return 2;            /* destination colour (as a source factor) */
  case 4: return 7;            /* 1 - destination alpha */
  case 5: return 3;            /* 1 - destination colour */
  case 6: return 5;            /* 1 - source alpha */
  case 7: return 3;            /* 1 - source colour (as a destination factor) */
  case 8: return 4;            /* source alpha */
  case 9: return 2;            /* source colour */
  case 10: return 1;           /* one */
  default: return 0;           /* zero */
  }
}
static void h_njColorBlendingMode(SH4 *c) {
  hle_logcall(c, "");
  if (R(4) == 0) { nj_blend_src = blend_code(R(5), 0); if (R(5) <= 1) nj_blend_dst = blend_code(R(5), 1); }
  else if (R(4) == 1) nj_blend_dst = blend_code(R(5), 1);
  R(0) = 0;
}
static void h_njTextureFilterMode(SH4 *c) { hle_logcall(c, ""); nj_filter = R(4) != 0; R(0) = 0; }
/* njClip2D(mode, rect): the game passes {tile min, tile max, pixel min, pixel max} as NJS_POINT2s (32-pixel
 * tiles, inclusive: {0,0} {19,14} {0,0} {639,479} for the whole screen): the user clip of every list */
static void h_njClip2D(SH4 *c) {
  hle_logcall(c, "");
  float x0 = nj_f(R(5) + 16), y0 = nj_f(R(5) + 20), x1 = nj_f(R(5) + 24) + 1, y1 = nj_f(R(5) + 28) + 1;
  if (x1 > x0 && y1 > y0) {
    nj_clip[0] = (int16_t)(x0 < 0 ? 0 : x0); nj_clip[1] = (int16_t)(y0 < 0 ? 0 : y0);
    nj_clip[2] = (int16_t)(x1 > RD_W ? RD_W : x1); nj_clip[3] = (int16_t)(y1 > RD_H_ ? RD_H_ : y1);
  }
  R(0) = 0;
}
/* table fog: njSetFogColor(ARGB), njSetFogTable(Float[128]), the density register FOG_DENSITY written by
 * the SDK's njGenerateFogTable3 (guest code), njFogEnable/Disable per polygon (Kamui context +0x3C) */
static uint32_t fog_color;
static void h_njSetFogColor(SH4 *c) { hle_logcall(c, ""); fog_color = R(4); R(0) = 0; }
static void h_njSetFogTable(SH4 *c) {
  hle_logcall(c, "");
  float t[128];
  for (uint32_t i = 0; i < 128; i++) t[i] = nj_f(R(4) + 4 * i);
  rd_fog(fog_color, t, 128);
  R(0) = 0;
}
static void h_njFogEnable(SH4 *c) { hle_logcall(c, ""); nj_fog_on = 1; R(0) = 0; }
static void h_njFogDisable(SH4 *c) { hle_logcall(c, ""); nj_fog_on = 0; R(0) = 0; }

/* ---- textures ---- */
#define NMEM 1024
static uint32_t mem_base, mem_n, mem_id[NMEM]; /* backend id per memlist entry */
static void h_njInitTexture(SH4 *c) {
  hle_logcall(c, "");
  mem_base = R(4); mem_n = R(5) > NMEM ? NMEM : R(5);
  wr32(NJG_MEMLIST, mem_base); wr32(NJG_MEMLIST_N, mem_n);
  for (uint32_t i = 0; i < mem_n * 0x44; i += 4) wr32(mem_base + i, 0);
  for (uint32_t i = 0; i < NMEM; i++) { rd_tex_free(mem_id[i]); mem_id[i] = 0; }
  R(0) = 1;
}
static int mem_index(uint32_t texaddr) {
  if (!mem_base || texaddr < mem_base || (texaddr - mem_base) % 0x44) return -1;
  uint32_t i = (texaddr - mem_base) / 0x44;
  return i < mem_n ? (int)i : -1;
}
static int mem_by_gbix(uint32_t g) {
  for (uint32_t i = 0; i < mem_n; i++) if (rd16(mem_base + i * 0x44 + 0x40) && rd32(mem_base + i * 0x44) == g) return (int)i;
  return -1;
}
uint32_t nj_tex_of(uint32_t tl, uint32_t n) {
  if (!sh4_is_ram(tl) || n >= rd32(tl + 4)) return 0;
  uint32_t e = rd32(tl) + n * 12;
  if (!sh4_is_ram(e)) return 0;
  uint32_t attr = rd32(e + 4), ta = rd32(e + 8);
  int i = (attr & 0x01000000u) ? mem_by_gbix(ta & 0x03FFFFFFu) : mem_index(ta);
  return i >= 0 ? mem_id[i] : 0;
}
static uint32_t *pix; /* decode buffer: 1024 x 1024 */
static uint32_t n_tex_loaded, n_tex_failed;
/* load one PVR at guest address a (avail bytes) into a memlist entry: reuse by global index */
static int load_one(uint32_t a, uint32_t avail, uint32_t gbix_hint, uint32_t entry) {
  static uint8_t *buf; static uint32_t cbuf;
  if (avail > (8u << 20)) avail = 8u << 20;
  if (avail > cbuf) { free(buf); cbuf = avail; buf = malloc(cbuf); }
  for (uint32_t k = 0; k < avail; k++) buf[k] = rd8(a + k);
  pvr_info in;
  if (!pix) pix = malloc(1024 * 1024 * 4);
  int r = pvr_decode(buf, avail, pix, 1024 * 1024, &in);
  uint32_t g = in.gbix != 0xFFFFFFFFu ? in.gbix : gbix_hint;
  int i = g != 0xFFFFFFFFu ? mem_by_gbix(g) : -1;
  if (i >= 0) { /* already resident: share it */
    wr16(mem_base + (uint32_t)i * 0x44 + 0x40, (uint16_t)(rd16(mem_base + (uint32_t)i * 0x44 + 0x40) + 1));
    wr32(entry + 8, mem_base + (uint32_t)i * 0x44);
    return (int)(r ? 0 : in.used);
  }
  for (uint32_t k = 0; k < mem_n && i < 0; k++) if (!rd16(mem_base + k * 0x44 + 0x40)) i = (int)k;
  if (i < 0) { rt_log("render texture memlist full"); return -1; }
  uint32_t m = mem_base + (uint32_t)i * 0x44;
  for (uint32_t k = 0; k < 0x44; k += 4) wr32(m + k, 0);
  wr32(m, g); wr32(m + 0x10, 2); wr16(m + 0x40, 1);
  if (r == 0) {
    wr16(m + 0x20, (uint16_t)in.w); wr16(m + 0x22, (uint16_t)in.h);
    mem_id[i] = rd_tex_new(in.w, in.h, pix, g);
    n_tex_loaded++;
  } else {
    n_tex_failed++;
    if (n_tex_failed <= 16) rt_log("render texture not decoded: error %d pixel %d data %d %dx%d", r, in.pixfmt, in.datafmt, in.w, in.h);
    mem_id[i] = 0;
  }
  wr32(entry + 8, m);
  return r == 0 || r == -3 ? (int)in.used : -1;
}
/* njLoadTexturePvmMemory(pvm, texlist): the texlist gets the archive's count; entry i = texture i */
static void h_njLoadTexturePvmMemory(SH4 *c) {
  hle_logcall(c, "");
  uint32_t p = R(4), tl = R(5);
  if (rd32(p) != 0x484D5650u /* PVMH */) { R(0) = (uint32_t)-1; return; }
  uint32_t hsz = rd32(p + 4), flags = rd16(p + 8), n = rd16(p + 10);
  uint32_t esz = 2 + ((flags & 8) ? 28 : 0) + ((flags & 4) ? 2 : 0) + ((flags & 2) ? 2 : 0) + ((flags & 1) ? 4 : 0);
  uint32_t q = p + 8 + hsz, e = p + 12;
  wr32(tl + 4, n);
  uint32_t ents = rd32(tl);
  for (uint32_t i = 0; i < n; i++, e += esz) {
    uint32_t g = (flags & 1) ? rd32(e + esz - 4) : 0xFFFFFFFFu;
    /* find the next chunk (alignment padding between textures) */
    for (int k = 0; k < 64 && rd32(q) != 0x58494247u /* GBIX */ && rd32(q) != 0x54525650u /* PVRT */; k++) q++;
    uint32_t ent = ents + i * 12;
    wr32(ent + 4, rd32(ent + 4) & ~0x01000000u);
    int used = load_one(q, 4u << 20, g, ent);
    if (used <= 0) { R(0) = (uint32_t)-1; return; }
    q += (uint32_t)used;
  }
  R(0) = 1;
}
static void release_mem(uint32_t ta) {
  int i = mem_index(ta);
  if (i < 0) return;
  uint32_t m = mem_base + (uint32_t)i * 0x44, k = rd16(m + 0x40);
  if (!k) return;
  wr16(m + 0x40, (uint16_t)(k - 1));
  if (k == 1) { rd_tex_free(mem_id[i]); mem_id[i] = 0; wr32(m, 0); }
}
static void h_njReleaseTexture(SH4 *c) {
  hle_logcall(c, "");
  uint32_t tl = R(4);
  for (uint32_t i = 0; sh4_is_ram(tl) && i < rd32(tl + 4); i++) {
    uint32_t e = rd32(tl) + i * 12;
    uint32_t attr = rd32(e + 4), ta = rd32(e + 8);
    if (attr & 0x01000000u) { int k = mem_by_gbix(ta & 0x03FFFFFFu); if (k >= 0) release_mem(mem_base + (uint32_t)k * 0x44); }
    else release_mem(ta);
  }
  R(0) = 1;
}

/* ---- 2D ---- */
#define SPR_ANGLE 1u
#define SPR_COLOR 2u
#define SPR_HFLIP 4u
#define SPR_VFLIP 8u
#define SPR_ALPHA 0x20u
static uint32_t n_sprites;
/* njDrawSprite2D(sprite, n, pri, attr): pri in FR4 (1/w = 1/-pri, at most 1) */
static void h_njDrawSprite2D(SH4 *c) {
  hle_logcall(c, "");
  uint32_t sp = R(4), n = R(5), attr = R(6);
  float pri = c->fr.f[4];
  uint32_t tl = rd32(sp + 0x18), ta = rd32(sp + 0x1C) + n * 20;
  if (tl) wr32(NJG_TEXLIST, tl);
  int16_t tsx = (int16_t)rd16(ta), tsy = (int16_t)rd16(ta + 2), cx = (int16_t)rd16(ta + 4), cy = (int16_t)rd16(ta + 6);
  float u1 = (float)(int16_t)rd16(ta + 8) / 256.0f, v1 = (float)(int16_t)rd16(ta + 10) / 256.0f;
  float u2 = (float)(int16_t)rd16(ta + 12) / 256.0f, v2 = (float)(int16_t)rd16(ta + 14) / 256.0f;
  uint32_t texid = (uint32_t)(int32_t)(int16_t)rd16(ta + 16);
  if (attr & SPR_HFLIP) { float t = u1; u1 = u2; u2 = t; }
  if (attr & SPR_VFLIP) { float t = v1; v1 = v2; v2 = t; }
  float px = nj_f(sp), py = nj_f(sp + 4), sx = nj_f(sp + 0xC), sy = nj_f(sp + 0x10);
  float x0 = (float)-cx * sx, y0 = (float)-cy * sy, x1 = (float)(tsx - cx) * sx, y1 = (float)(tsy - cy) * sy;
  float iw = -pri >= 1.0f ? 1.0f / -pri : 1.0f;
  float qx[4] = {x0, x1, x0, x1}, qy[4] = {y0, y0, y1, y1}, qu[4] = {u1, u2, u1, u2}, qv[4] = {v1, v1, v2, v2};
  float cs = 1, sn = 0;
  if (attr & SPR_ANGLE) {
    float a = (float)(int32_t)rd32(sp + 0x14) * (6.28318530718f / 65536.0f);
    cs = __builtin_cosf(a); sn = __builtin_sinf(a);
  }
  uint32_t col = (attr & SPR_COLOR) ? nj_argb_f(NJG_CMAT) : 0xFFFFFFFFu;
  rd_vtx v[4];
  for (int k = 0; k < 4; k++) {
    v[k].x = px + qx[k] * cs - qy[k] * sn; v[k].y = py + qx[k] * sn + qy[k] * cs;
    v[k].iw = iw; v[k].u = qu[k]; v[k].v = qv[k]; v[k].col = col; v[k].ofs = 0;
  }
  rd_state s;
  nj_state(&s, (attr & SPR_ALPHA) ? RD_TR : RD_OP, nj_tex_of(tl ? tl : rd32(NJG_TEXLIST), texid), (attr & SPR_ALPHA) != 0);
  rd_strip(&s, v, 4, 0);
  n_sprites++;
  R(0) = 0;
}

const nj_impl_t nj_impls[] = {
  {"njSetBackColor", h_njSetBackColor}, {"njSetBorderColor", h_njSetBorderColor},
  {"njColorBlendingMode", h_njColorBlendingMode}, {"njTextureFilterMode", h_njTextureFilterMode},
  {"njClip2D", h_njClip2D}, {"njSetFogColor", h_njSetFogColor}, {"njSetFogTable", h_njSetFogTable},
  {"njFogEnable", h_njFogEnable}, {"njFogDisable", h_njFogDisable},
  {"njInitTexture", h_njInitTexture}, {"njLoadTexturePvmMemory", h_njLoadTexturePvmMemory},
  {"njReleaseTexture", h_njReleaseTexture}, {"njDrawSprite2D", h_njDrawSprite2D},
};
const int nj_nimpls = (int)(sizeof nj_impls / sizeof nj_impls[0]);
