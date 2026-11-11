/* Pen Pen port, S5: Ninja drawing HLE, part 2: 3D chunk models (NJCM) with motions and shape motions.
 * Our own code, from the public description of the Ninja chunk format (docs/port/RENDER.md 5).
 *
 * Objects: NJS_CNK_OBJECT {evalflags, model*, pos[3], ang[3] (Angle), scl[3], child*, sibling*} (0x34);
 *   NJS_CNK_MODEL {Sint32 *vlist; Sint16 *plist; center[3]; r}. The node transform (translate, rotate
 *   XYZ or ZXY, scale) is done by the SDK's own matrix functions (guest code, njPushMatrix ... on the
 *   current matrix of njInitMatrix's stack), so the HLE reads exactly the matrices the game sees.
 * Vertex list (Sint32 chunks): {type, flags, size in words} {index offset, count} then the vertices into
 *   Ninja's vertex buffer (here: view space position + normal + colour per index). Weighted (NF) vertices
 *   accumulate over the nodes (flags & 3: 0 first, 1 middle, 2 last).
 * Polygon list (Sint16 chunks): bits (blend, mipmap, exponent, cache/draw list), tiny (texture id),
 *   material (diffuse/ambient/specular ARGB), strip (index + UV / normal / colour per vertex, user flags),
 *   volume (modifier volumes), end.
 * Projection: the screen of njSetScreen (dist, cx, cy at 0x8C1EF2A4...) with near clipping in view space. */
#include <math.h>
#include "nj.h"
#define R(i) c->r[i]

/* SDK guest functions used for the node transforms */
#define G_PUSH 0x8C0C3D90u
#define G_POP 0x8C0C3D4Cu
#define G_TRANSLATE 0x8C0C4978u
#define G_ROTXYZ 0x8C0C3E14u
#define G_ROTZXY 0x8C0C4118u
#define G_SCALE 0x8C0C47BCu
#define G_CALCPOINT 0x8C0C3168u
#define G_CALCSCREEN 0x8C0C4B28u
#define NJG_SCREEN 0x8C1EF2A4u /* dist, w, h, cx, cy */
#define NJG_NEARFAR 0x8C1EF2D0u

static void gcall(SH4 *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, float f0, float f1, float f2) {
  SH4 save = *c;
  c->r[4] = a0; c->r[5] = a1; c->r[6] = a2; c->r[7] = a3;
  c->fr.f[4] = f0; c->fr.f[5] = f1; c->fr.f[6] = f2;
  c->pr = 0x8C000F00u;
  rt_call(c, fn);
  *c = save;
}

/* ---- lights (per family; one shared set for now: RENDER.md 5) ---- */
static float l_dir[3] = {0, 0, 1}, l_dif = 1.0f, l_amb = 0.25f, l_col[3] = {1, 1, 1};
static void h_lightdir(SH4 *c) { hle_logcall(c, ""); l_dir[0] = c->fr.f[4]; l_dir[1] = c->fr.f[5]; l_dir[2] = c->fr.f[6]; R(0) = 0; }
static void h_lightint(SH4 *c) { hle_logcall(c, ""); l_dif = c->fr.f[4]; l_amb = c->fr.f[5]; R(0) = 0; }
static void h_lightcol(SH4 *c) { hle_logcall(c, ""); l_col[0] = c->fr.f[4]; l_col[1] = c->fr.f[5]; l_col[2] = c->fr.f[6]; R(0) = 0; }

/* ---- vertex buffer ---- */
#define NVB 32768
typedef struct { float p[3], n[3]; uint32_t col; uint8_t hascol, hasn; } vb_t;
static vb_t vb[NVB];
static float M[16]; /* current matrix (row vectors: p' = p * M, translation in 12..14) */
static void load_matrix(void) { uint32_t p = rd32(NJG_CURMATRIX); for (int i = 0; i < 16; i++) M[i] = nj_f(p + 4u * (uint32_t)i); }
static void xform(const float *p, float *o) {
  o[0] = p[0] * M[0] + p[1] * M[4] + p[2] * M[8] + M[12];
  o[1] = p[0] * M[1] + p[1] * M[5] + p[2] * M[9] + M[13];
  o[2] = p[0] * M[2] + p[1] * M[6] + p[2] * M[10] + M[14];
}
static void xnorm(const float *n, float *o) {
  o[0] = n[0] * M[0] + n[1] * M[4] + n[2] * M[8];
  o[1] = n[0] * M[1] + n[1] * M[5] + n[2] * M[9];
  o[2] = n[0] * M[2] + n[1] * M[6] + n[2] * M[10];
  float l = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
  if (l > 1e-12f) { o[0] /= l; o[1] /= l; o[2] /= l; }
}
static uint32_t shape_vert, shape_norm; /* shape motion: replacement point / normal arrays for this node */
static uint32_t n_unknown_v, n_unknown_p;
static void vlist(uint32_t a) {
  for (int guard = 0; guard < 4096; guard++) {
    uint32_t h = rd32(a), t = h & 0xFF, fl = (h >> 8) & 0xFF, sz = h >> 16;
    if (t == 0xFF) return;
    if (t < 32 || t > 50) { n_unknown_v++; return; }
    uint32_t w1 = rd32(a + 4), off = w1 & 0xFFFF, n = w1 >> 16;
    uint32_t q = a + 8;
    int pw = (t == 32 || t == 33) ? 4 : 3;                    /* position words */
    int nw = (t == 33) ? 4 : (t >= 41 && t <= 47) ? 3 : (t >= 48) ? 1 : 0; /* normal words */
    int ex = (t == 32 || t == 33 || t == 34 || t == 41 || t == 48) ? 0 : 1; /* one extra word */
    uint32_t stride = (uint32_t)(pw + nw + ex) * 4;
    for (uint32_t i = 0; i < n; i++, q += stride) {
      float p[3] = {nj_f(q), nj_f(q + 4), nj_f(q + 8)}, nn[3] = {0, 0, 0};
      uint32_t idx = off + i, extra = ex ? rd32(q + (uint32_t)(pw + nw) * 4) : 0;
      if (shape_vert) { uint32_t s = shape_vert + i * 12; p[0] = nj_f(s); p[1] = nj_f(s + 4); p[2] = nj_f(s + 8); }
      if (nw >= 3) { uint32_t b = q + (uint32_t)pw * 4; nn[0] = nj_f(b); nn[1] = nj_f(b + 4); nn[2] = nj_f(b + 8); }
      else if (nw == 1) { uint32_t v = rd32(q + (uint32_t)pw * 4); /* 10:10:10 signed */
        nn[0] = (float)((int32_t)(v << 2) >> 22) / 511.0f; nn[1] = (float)((int32_t)(v << 12) >> 22) / 511.0f; nn[2] = (float)((int32_t)(v << 22) >> 22) / 511.0f; }
      if (shape_norm && nw) { uint32_t s = shape_norm + i * 12; nn[0] = nj_f(s); nn[1] = nj_f(s + 4); nn[2] = nj_f(s + 8); }
      float wgt = 1.0f;
      int mode = -1;
      if (t == 37 || t == 44) { idx = off + (extra & 0xFFFF); wgt = (float)(extra >> 16) / 255.0f; mode = (int)(fl & 3); }
      if (idx >= NVB) continue;
      vb_t *v = &vb[idx];
      float tp[3], tn[3];
      xform(p, tp);
      if (nw) xnorm(nn, tn); else { tn[0] = tn[1] = 0; tn[2] = 1; }
      if (mode <= 0) {
        for (int k = 0; k < 3; k++) { v->p[k] = tp[k] * (mode == 0 ? wgt : 1.0f); v->n[k] = tn[k] * (mode == 0 ? wgt : 1.0f); }
        v->hasn = nw != 0; v->hascol = 0;
      } else {
        for (int k = 0; k < 3; k++) { v->p[k] += tp[k] * wgt; v->n[k] += tn[k] * wgt; }
      }
      if (t == 35 || t == 42 || t == 49) { v->col = extra; v->hascol = 1; }
    }
    a += 4 + sz * 4;
  }
}

/* ---- polygon list ---- */
typedef struct {
  uint32_t dif, amb, spe, src, dst, texid, flags, tflags, filter;
} mat_t;
static mat_t mat;
static uint32_t cache[16];
static uint32_t n_tris3d, n_models, n_clipped;

static float near_z(void) { float n = nj_f(NJG_NEARFAR); return n > 0.001f ? n : 1.0f; }
typedef struct { float p[3], u, v; float c[4]; } cv_t; /* view-space vertex before projection */
static void project(const cv_t *a, rd_vtx *o) {
  float dist = nj_f(NJG_SCREEN), cx = nj_f(NJG_SCREEN + 12), cy = nj_f(NJG_SCREEN + 16);
  float iw = 1.0f / a->p[2]; /* view space: +z forward (checked against the SDK's njCalcPoint) */
  o->x = cx + dist * a->p[0] * iw;
  o->y = cy + dist * a->p[1] * iw; /* view space y points down the screen */
  o->iw = iw; o->u = a->u; o->v = a->v;
  static const int sh[4] = {16, 8, 0, 24}; /* r g b a -> ARGB */
  uint32_t col = 0;
  for (int k = 0; k < 4; k++) { float f = a->c[k] < 0 ? 0 : a->c[k] > 1 ? 1 : a->c[k]; col |= (uint32_t)(f * 255.0f + 0.5f) << sh[k]; }
  o->col = col; o->ofs = 0;
}
static void lerpv(const cv_t *a, const cv_t *b, float t, cv_t *o) {
  for (int k = 0; k < 3; k++) o->p[k] = a->p[k] + (b->p[k] - a->p[k]) * t;
  o->u = a->u + (b->u - a->u) * t; o->v = a->v + (b->v - a->v) * t;
  for (int k = 0; k < 4; k++) o->c[k] = a->c[k] + (b->c[k] - a->c[k]) * t;
}
/* one triangle in view space: near clip (z >= near is visible), project, emit */
static void tri3(const rd_state *s, const cv_t *a, const cv_t *b, const cv_t *c, int cull) {
  float nz = near_z();
  const cv_t *in[3] = {a, b, c};
  cv_t poly[4];
  int n = 0;
  for (int i = 0; i < 3; i++) {
    const cv_t *p = in[i], *q = in[(i + 1) % 3];
    int pin = p->p[2] >= nz, qin = q->p[2] >= nz;
    if (pin) poly[n++] = *p;
    if (pin != qin) { lerpv(p, q, (nz - p->p[2]) / (q->p[2] - p->p[2]), &poly[n]); poly[n].p[2] = nz; n++; }
  }
  if (n < 3) { n_clipped++; return; }
  rd_vtx v[4];
  for (int i = 0; i < n; i++) project(&poly[i], &v[i]);
  rd_vtx t[3] = {v[0], v[1], v[2]};
  rd_strip(s, t, 3, cull);
  if (n == 4) { rd_vtx t2[3] = {v[0], v[2], v[3]}; rd_strip(s, t2, 3, cull); }
  n_tris3d++;
}

/* colours as floats r, g, b, a */
static void argb4(uint32_t col, float *o) {
  o[0] = (float)((col >> 16) & 255) / 255.0f; o[1] = (float)((col >> 8) & 255) / 255.0f;
  o[2] = (float)(col & 255) / 255.0f; o[3] = (float)(col >> 24) / 255.0f;
}
/* the colour of a vertex: pre-lit colour, or the easy light: diffuse * (ambient + max(0, n.L) * intensity) */
#define CTRL_CONSTANT_MATERIAL 0x10u /* njControl3D: the constant material replaces the diffuse colour */
static uint32_t diffuse(void) { return (rd32(NJG_CTRL3D) & CTRL_CONSTANT_MATERIAL) ? nj_argb_f(NJG_CMAT) : mat.dif; }
static void shade(const vb_t *v, const uint32_t *scol, uint32_t sflags, float *o) {
  float d[4];
  argb4(diffuse(), d);
  if (scol) { argb4(*scol, o); return; }
  if (v->hascol) { argb4(v->col, o); o[3] *= d[3]; return; }
  if ((sflags & 1) || !v->hasn) { for (int k = 0; k < 4; k++) o[k] = d[k]; return; }
  float nl = -(v->n[0] * l_dir[0] + v->n[1] * l_dir[1] + v->n[2] * l_dir[2]); /* the light's direction of travel */
  float k = l_amb + (nl > 0 ? nl : 0) * l_dif;
  o[0] = d[0] * k * l_col[0]; o[1] = d[1] * k * l_col[1]; o[2] = d[2] * k * l_col[2]; o[3] = d[3];
}

static void strip_chunk(uint32_t a, uint32_t t, uint32_t sflags) {
  uint32_t hdr2 = rd16(a + 4), ns = hdr2 & 0x3FFF, uoff = hdr2 >> 14;
  int uv = (t == 65 || t == 68 || t == 71 || t == 74) ? 1 : (t == 66 || t == 69 || t == 72 || t == 75) ? 2 : 0;
  int vn = t == 67 || t == 68 || t == 69, d8 = t == 70 || t == 71 || t == 72, uv2 = t >= 73;
  float uvs = uv == 2 ? 1.0f / 1024.0f : 1.0f / 256.0f;
  int alpha = (sflags & 8) != 0;
  uint32_t tex = 0;
  if (uv || (sflags & 0x40)) tex = nj_tex_of(rd32(NJG_TEXLIST), mat.texid); /* 0x40: environment map */
  rd_state s;
  nj_state(&s, alpha ? RD_TR : RD_OP, tex, alpha);
  if (alpha) s.mode = (s.mode & ~0x3Fu) | RD_M_SRC(mat.src) | RD_M_DST(mat.dst);
  s.mode &= ~RD_M_SHADE(3);
  s.mode |= RD_M_SHADE(alpha ? 3 : 1);
  if (mat.tflags & 0x20) s.mode |= RD_M_CLAMPU;
  if (mat.tflags & 0x10) s.mode |= RD_M_CLAMPV;
  if (mat.tflags & 0x80) s.mode |= RD_M_FLIPU;
  if (mat.tflags & 0x40) s.mode |= RD_M_FLIPV;
  if (!mat.filter) s.mode &= ~RD_M_FILTER;
  int cull = (sflags & 0x10) ? 0 : 1; /* drop clockwise-on-screen triangles (y down) */
  uint32_t q = a + 6;
  for (uint32_t i = 0; i < ns; i++) {
    int16_t len = (int16_t)rd16(q); q += 2;
    int n = len < 0 ? -len : len;
    cv_t win[3];
    for (int j = 0; j < n; j++) {
      uint32_t idx = rd16(q); q += 2;
      if (j >= 3) { win[0] = win[1]; win[1] = win[2]; }
      cv_t *o = &win[j < 3 ? j : 2];
      float u = 0, v = 0;
      if (uv) { u = (float)(int16_t)rd16(q) * uvs; v = (float)(int16_t)rd16(q + 2) * uvs; q += 4; }
      if (vn) q += 6;
      uint32_t scol = 0; int hs = 0;
      if (d8) { scol = rd16(q) | (uint32_t)rd16(q + 2) << 16; q += 4; hs = 1; }
      if (uv2) q += 4;
      if (j >= 2) q += uoff * 2;
      const vb_t *vv = &vb[idx < NVB ? idx : 0];
      o->p[0] = vv->p[0]; o->p[1] = vv->p[1]; o->p[2] = vv->p[2];
      if (sflags & 0x40) { u = vv->n[0] * 0.5f + 0.5f; v = -vv->n[1] * 0.5f + 0.5f; } /* environment map */
      o->u = u; o->v = v;
      shade(vv, hs ? &scol : 0, sflags, o->c);
      if (j >= 2) {
        /* strip triangle k = j - 2: even k (a b c), odd k reversed; a negative length flips them all */
        int odd = ((j - 2) & 1) ^ (len < 0);
        if (odd) tri3(&s, &win[1], &win[0], &win[2], cull); else tri3(&s, &win[0], &win[1], &win[2], cull);
      }
    }
  }
}

static void plist(uint32_t a, int depth) {
  for (int guard = 0; guard < 100000; guard++) {
    uint32_t h = rd16(a), t = h & 0xFF, fl = h >> 8;
    if (t == 0xFF) return;
    if (t == 0) { a += 2; continue; }
    if (t <= 7) { /* bits */
      if (t == 1) { mat.src = (fl >> 3) & 7; mat.dst = fl & 7; }
      else if (t == 4) { if (fl < 16) cache[fl] = a + 2; return; }  /* cache the rest of the list */
      else if (t == 5) { if (fl < 16 && cache[fl] && depth < 4) plist(cache[fl], depth + 1); }
      a += 2; continue;
    }
    if (t <= 15) { /* tiny: texture id */
      uint32_t d = rd16(a + 2);
      mat.texid = d & 0x1FFF; mat.filter = (d >> 14) != 0; mat.tflags = fl;
      a += 4; continue;
    }
    uint32_t size = rd16(a + 2);
    if (t <= 31) { /* material */
      mat.src = (fl >> 3) & 7; mat.dst = fl & 7;
      uint32_t q = a + 4;
      if (t & 1) { mat.dif = rd16(q) | (uint32_t)rd16(q + 2) << 16; q += 4; }
      if (t & 2) { mat.amb = rd16(q) | (uint32_t)rd16(q + 2) << 16; q += 4; }
      if (t & 4) { mat.spe = rd16(q) | (uint32_t)rd16(q + 2) << 16; q += 4; }
    } else if (t >= 64 && t <= 75) strip_chunk(a, t, fl);
    else if (t >= 56 && t <= 58) { /* modifier volume: not drawn yet */ }
    else n_unknown_p++;
    a += 4 + size * 2;
  }
}

static void self_check(SH4 *c, uint32_t model);
static SH4 *cpu_now;
static void draw_model(uint32_t m) {
  if (!sh4_is_ram(m)) return;
  self_check(cpu_now, m);
  load_matrix();
  uint32_t vl = rd32(m), pl = rd32(m + 4);
  if (sh4_is_ram(vl)) vlist(vl);
  if (sh4_is_ram(pl)) plist(pl, 0);
  n_models++;
}
static void mat_reset(void) {
  mat.dif = 0xFFFFFFFFu; mat.amb = 0; mat.spe = 0; mat.src = 4; mat.dst = 5; mat.texid = 0; mat.flags = 0; mat.tflags = 0; mat.filter = 1;
}

/* ---- motion ---- */
/* NJS_MOTION {mdata*, nbFrame, u16 type, u16 inp_fn}; per node {ptr[n], count[n]} for the n set type
 * bits (pos, ang, scl, vec, vert, norm, ...); keys {frame, 3 values} (16 bytes) or {frame, ptr} (8) */
static int bitcount(uint32_t v) { int n = 0; while (v) { n += (int)(v & 1); v >>= 1; } return n; }
static int key_find(uint32_t keys, uint32_t nb, uint32_t stride, uint32_t nbf, float f, uint32_t *k0, uint32_t *k1, float *t) {
  if (!nb || !sh4_is_ram(keys)) return 0;
  uint32_t i = 0;
  while (i + 1 < nb && (float)rd32(keys + (i + 1) * stride) <= f) i++;
  *k0 = keys + i * stride;
  float f0 = (float)rd32(*k0), f1;
  if (i + 1 < nb) { *k1 = keys + (i + 1) * stride; f1 = (float)rd32(*k1); }
  else { *k1 = keys; f1 = (float)nbf; }
  *t = f1 > f0 ? (f - f0) / (f1 - f0) : 0.0f;
  if (*t < 0) *t = 0;
  if (*t > 1) *t = 1;
  return 1;
}
typedef struct { uint32_t mot, shape; float frame; int node; } anim_t;
static int mdata_get(uint32_t mot, int node, uint32_t bit, uint32_t *keys, uint32_t *nb) {
  if (!mot) return 0;
  uint32_t md = rd32(mot), type = rd16(mot + 8);
  if (!(type & bit) || !sh4_is_ram(md)) return 0;
  int n = bitcount(type & 0x3FFF), slot = bitcount(type & (bit - 1));
  uint32_t e = md + (uint32_t)node * (uint32_t)n * 8;
  *keys = rd32(e + (uint32_t)slot * 4); *nb = rd32(e + (uint32_t)(n + slot) * 4);
  return *nb && sh4_is_ram(*keys);
}
static void interp3f(uint32_t keys, uint32_t nb, uint32_t nbf, float f, float *o) {
  uint32_t k0, k1; float t;
  if (!key_find(keys, nb, 16, nbf, f, &k0, &k1, &t)) return;
  for (int i = 0; i < 3; i++) { float a = nj_f(k0 + 4 + 4u * (uint32_t)i), b = nj_f(k1 + 4 + 4u * (uint32_t)i); o[i] = a + (b - a) * t; }
}
static void interp3a(uint32_t keys, uint32_t nb, uint32_t nbf, float f, int32_t *o) {
  uint32_t k0, k1; float t;
  if (!key_find(keys, nb, 16, nbf, f, &k0, &k1, &t)) return;
  for (int i = 0; i < 3; i++) {
    int32_t a = (int32_t)rd32(k0 + 4 + 4u * (uint32_t)i), b = (int32_t)rd32(k1 + 4 + 4u * (uint32_t)i);
    o[i] = a + (int32_t)((float)(b - a) * t);
  }
}
static uint32_t ptrkey(uint32_t keys, uint32_t nb, uint32_t nbf, float f) {
  uint32_t k0, k1; float t;
  if (!key_find(keys, nb, 8, nbf, f, &k0, &k1, &t)) return 0;
  return rd32((t < 0.5f ? k0 : k1) + 4); /* nearest key (shape blending: RENDER.md 7) */
}

#define EV_UNIT_POS 1u
#define EV_UNIT_ANG 2u
#define EV_UNIT_SCL 4u
#define EV_HIDE 8u
#define EV_BREAK 0x10u
#define EV_ZXY 0x20u
static void draw_object(SH4 *c, uint32_t o, anim_t *an, int depth) {
  for (int guard = 0; sh4_is_ram(o) && guard < 512; guard++) {
    uint32_t ev = rd32(o), model = rd32(o + 4), node = an ? (uint32_t)an->node++ : 0;
    float pos[3] = {nj_f(o + 8), nj_f(o + 12), nj_f(o + 16)}, scl[3] = {nj_f(o + 32), nj_f(o + 36), nj_f(o + 40)};
    int32_t ang[3] = {(int32_t)rd32(o + 20), (int32_t)rd32(o + 24), (int32_t)rd32(o + 28)};
    uint32_t keys, nb;
    shape_vert = shape_norm = 0;
    if (an && an->mot) {
      uint32_t nbf = rd32(an->mot + 4);
      if (mdata_get(an->mot, (int)node, 1, &keys, &nb)) { interp3f(keys, nb, nbf, an->frame, pos); ev &= ~EV_UNIT_POS; }
      if (mdata_get(an->mot, (int)node, 2, &keys, &nb)) { interp3a(keys, nb, nbf, an->frame, ang); ev &= ~EV_UNIT_ANG; }
      if (mdata_get(an->mot, (int)node, 4, &keys, &nb)) { interp3f(keys, nb, nbf, an->frame, scl); ev &= ~EV_UNIT_SCL; }
    }
    uint32_t sv = 0, sn = 0;
    if (an && an->shape) {
      uint32_t nbf = rd32(an->shape + 4);
      if (mdata_get(an->shape, (int)node, 0x10, &keys, &nb)) sv = ptrkey(keys, nb, nbf, an->frame);
      if (mdata_get(an->shape, (int)node, 0x20, &keys, &nb)) sn = ptrkey(keys, nb, nbf, an->frame);
    }
    gcall(c, G_PUSH, 0, 0, 0, 0, 0, 0, 0);
    if (!(ev & EV_UNIT_POS)) gcall(c, G_TRANSLATE, 0, 0, 0, 0, pos[0], pos[1], pos[2]);
    if (!(ev & EV_UNIT_ANG)) gcall(c, (ev & EV_ZXY) ? G_ROTZXY : G_ROTXYZ, 0, (uint32_t)ang[0], (uint32_t)ang[1], (uint32_t)ang[2], 0, 0, 0);
    if (!(ev & EV_UNIT_SCL)) gcall(c, G_SCALE, 0, 0, 0, 0, scl[0], scl[1], scl[2]);
    shape_vert = sh4_is_ram(sv) ? sv : 0; shape_norm = sh4_is_ram(sn) ? sn : 0;
    if (!(ev & EV_HIDE) && model) draw_model(model);
    shape_vert = shape_norm = 0;
    uint32_t child = rd32(o + 0x2C);
    if (!(ev & EV_BREAK) && child && depth < 64) draw_object(c, child, an, depth + 1);
    gcall(c, G_POP, 1, 0, 0, 0, 0, 0, 0);
    if (ev & EV_BREAK) return;
    o = rd32(o + 0x30);
  }
}

/* one-time check of our matrix and projection conventions against the SDK's own njCalcPoint/njCalcScreen */
static void self_check(SH4 *c, uint32_t model) {
  static int done;
  if (done || !sh4_is_ram(model) || !sh4_is_ram(rd32(model))) return;
  done = 1;
  uint32_t vl = rd32(model), p = vl + 8, s = GUEST_SCRATCH + 0xA00;
  for (int k = 0; k < 3; k++) wr32(s + 4u * (uint32_t)k, rd32(p + 4u * (uint32_t)k));
  gcall(c, G_CALCPOINT, 0, s, s + 0x10, 0, 0, 0, 0);
  gcall(c, G_CALCSCREEN, s, s + 0x20, s + 0x24, 0, 0, 0, 0);
  load_matrix();
  float in[3] = {nj_f(s), nj_f(s + 4), nj_f(s + 8)}, o[3];
  xform(in, o);
  cv_t cvv = {{o[0], o[1], o[2]}, 0, 0, {1, 1, 1, 1}};
  rd_vtx pv;
  project(&cvv, &pv);
  rt_log("render check calcpoint sdk (%d, %d, %d) ours (%d, %d, %d) x1000; calcscreen sdk (%d, %d) ours (%d, %d)",
         (int)(nj_f(s + 0x10) * 1000), (int)(nj_f(s + 0x14) * 1000), (int)(nj_f(s + 0x18) * 1000),
         (int)(o[0] * 1000), (int)(o[1] * 1000), (int)(o[2] * 1000), (int)nj_f(s + 0x20), (int)nj_f(s + 0x24), (int)pv.x, (int)pv.y);
}

/* nj_ModelL1/L2/L3 (model), nj_ObjectL1/B (object), nj_Motion* (object, motion, frame in FR4),
 * nj_ShapeMotion* (object, motion, shape, frame in FR4) */
/* nj_Model*: the game passes an NJS_CNK_OBJECT (eval flags, model, pos ...; usually with BREAK: this node
 * only); a word 0 that is a pointer means a bare NJS_CNK_MODEL (vlist first) */
static void h_model(SH4 *c) {
  hle_logcall(c, ""); cpu_now = c; mat_reset();
  uint32_t w0 = rd32(R(4));
  if (sh4_is_ram(w0)) draw_model(R(4)); else draw_object(c, R(4), 0, 0);
  R(0) = 0;
}
static void h_object(SH4 *c) { hle_logcall(c, ""); cpu_now = c; mat_reset(); draw_object(c, R(4), 0, 0); R(0) = 0; }
static void h_motion(SH4 *c) {
  hle_logcall(c, ""); cpu_now = c;
  anim_t an = {R(5), 0, c->fr.f[4], 0};
  mat_reset(); draw_object(c, R(4), &an, 0); R(0) = 0;
}
static void h_shapemotion(SH4 *c) {
  hle_logcall(c, ""); cpu_now = c;
  anim_t an = {R(5), R(6), c->fr.f[4], 0};
  mat_reset(); draw_object(c, R(4), &an, 0); R(0) = 0;
}
void nj_model_log(void) {
  rt_log("render 3d models %u triangles %u near-clipped-out %u unknown vlist %u plist %u", n_models, n_tris3d, n_clipped, n_unknown_v, n_unknown_p);
}

const nj_impl_t nj_model_impls[] = {
  {"nj_ModelL1", h_model}, {"nj_ModelL2", h_model}, {"nj_ModelL3", h_model}, {"nj_ModelL4", h_model},
  {"nj_ObjectL1", h_object},
  {"nj_MotionL1", h_motion}, {"nj_MotionL2", h_motion}, {"nj_MotionL3", h_motion}, {"nj_MotionL3b", h_motion}, {"nj_MotionL4", h_motion},
  {"nj_ShapeMotionL1", h_shapemotion}, {"nj_ShapeMotionL1b", h_shapemotion}, {"nj_ShapeMotionL2", h_shapemotion},
  {"nj_ShapeMotionL2b", h_shapemotion}, {"nj_ShapeMotionL3", h_shapemotion}, {"nj_ShapeMotionL3b", h_shapemotion},
  {"nj_ShapeMotionL4", h_shapemotion},
  {"nj_LightL1Dir", h_lightdir}, {"nj_LightL1Intensity", h_lightint}, {"nj_LightL1Color", h_lightcol},
};
const int nj_model_nimpls = (int)(sizeof nj_model_impls / sizeof nj_model_impls[0]);
