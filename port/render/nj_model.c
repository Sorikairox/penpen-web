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
#include "det_math.h" /* S22: powf / sinf / cosf as our own code (native = wasm) */
#define R(i) HLE_R(i) /* S11: marks the argument registers read (rt.h) */

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

/* this SDK's njTranslate, njRotateXYZ/ZXY and njScale do nothing when the matrix argument is NULL (unlike the
 * documented "NULL = current matrix"): pass the current matrix itself */
#define CUR_M rd32(NJG_CURMATRIX)
static void gcall(SH4 *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, float f0, float f1, float f2) {
  SH4 save = *c;
  c->r[4] = a0; c->r[5] = a1; c->r[6] = a2; c->r[7] = a3;
  c->fr.f[4] = f0; c->fr.f[5] = f1; c->fr.f[6] = f2;
  c->pr = 0x8C000F00u;
  rt_call(c, fn);
  *c = save;
}

/* ---- lights (per family; one shared set for now: RENDER.md 5) ---- */
/* Families (docs/port/SDK.md 2.6): L1, L3, L4 = one directional light each, set by Dir (FR4-6, view space,
 * the direction the light travels), Intensity (FR4 diffuse, FR5 ambient) and Color (FR4-6); L2 = up to four
 * directional lights (Dir = light 0, DirN/ColorN/IntensityN(i) with the index in R4) plus an ambient colour
 * (L2Color), taken by L2Update. The game sets them all once per frame (S8 trace: L1/L3/L4 (0, .45, .89),
 * diffuse 1, ambient .4; L2 light 0 (0, -.71, -.71) white, lights 1-3 zero vectors, ambient .4 grey). The B
 * family (basic models) has no setter of its own: it uses the L1 values. */
typedef struct { float dir[3], dif, amb, col[3]; } light1_t;
static light1_t fam[4] = {{{0, 0, 1}, 1, .25f, {1, 1, 1}}, {{0, 0, 1}, 1, .25f, {1, 1, 1}}, {{0, 0, 1}, 1, .25f, {1, 1, 1}}, {{0, 0, 1}, 1, .25f, {1, 1, 1}}};
typedef struct { float dir[3], col[3], dif; } lightn_t;
static lightn_t l2[4] = {{{0, 0, 1}, {1, 1, 1}, 1}}, l2_set[4] = {{{0, 0, 1}, {1, 1, 1}, 1}};
static float l2_amb[3] = {.25f, .25f, .25f}, l2_amb_set[3] = {.25f, .25f, .25f};
static int cur_fam; /* family of the draw in progress: 0 L1, 1 L2, 2 L3, 3 L4 */
#define F3(dst) do { (dst)[0] = c->fr.f[4]; (dst)[1] = c->fr.f[5]; (dst)[2] = c->fr.f[6]; } while (0)
#define LIGHT_SETTERS(n, i) \
  static void h_l##n##dir(SH4 *c) { hle_logcall(c, ""); F3(fam[i].dir); R(0) = 0; } \
  static void h_l##n##int(SH4 *c) { hle_logcall(c, ""); fam[i].dif = c->fr.f[4]; fam[i].amb = c->fr.f[5]; R(0) = 0; } \
  static void h_l##n##col(SH4 *c) { hle_logcall(c, ""); F3(fam[i].col); R(0) = 0; }
LIGHT_SETTERS(1, 0)
LIGHT_SETTERS(3, 2)
LIGHT_SETTERS(4, 3)
static void h_l2dir(SH4 *c) { hle_logcall(c, ""); F3(l2_set[0].dir); R(0) = 0; }
static void h_l2dirn(SH4 *c) { hle_logcall(c, ""); if (R(4) < 4) F3(l2_set[R(4)].dir); R(0) = 0; }
static void h_l2coln(SH4 *c) { hle_logcall(c, ""); if (R(4) < 4) F3(l2_set[R(4)].col); R(0) = 0; }
static void h_l2intn(SH4 *c) { hle_logcall(c, ""); if (R(4) < 4) l2_set[R(4)].dif = c->fr.f[4]; R(0) = 0; }
static void h_l2col(SH4 *c) { hle_logcall(c, ""); F3(l2_amb_set); R(0) = 0; }
static void h_l2update(SH4 *c) { hle_logcall(c, ""); memcpy(l2, l2_set, sizeof l2); memcpy(l2_amb, l2_amb_set, sizeof l2_amb); R(0) = 0; }

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
/* shape motion: the two key arrays of points / normals around the frame and the blend between them */
static uint32_t shape_vert, shape_norm, shape_vert1, shape_norm1;
static float shape_t;
static void shape_get(uint32_t a0, uint32_t a1, uint32_t i, float *o) {
  uint32_t s0 = a0 + i * 12, s1 = a1 + i * 12;
  for (int k = 0; k < 3; k++) { float x = nj_f(s0 + 4u * (uint32_t)k); o[k] = a1 ? x + (nj_f(s1 + 4u * (uint32_t)k) - x) * shape_t : x; }
}
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
      if (shape_vert) shape_get(shape_vert, shape_vert1, i, p);
      if (nw >= 3) { uint32_t b = q + (uint32_t)pw * 4; nn[0] = nj_f(b); nn[1] = nj_f(b + 4); nn[2] = nj_f(b + 8); }
      else if (nw == 1) { uint32_t v = rd32(q + (uint32_t)pw * 4); /* 10:10:10 signed */
        nn[0] = (float)((int32_t)(v << 2) >> 22) / 511.0f; nn[1] = (float)((int32_t)(v << 12) >> 22) / 511.0f; nn[2] = (float)((int32_t)(v << 22) >> 22) / 511.0f; }
      if (shape_norm && nw) shape_get(shape_norm, shape_norm1, i, nn); /* xnorm renormalises */
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
typedef struct { float p[3], u, v; float c[4], s[3]; } cv_t; /* view-space vertex before projection; s = offset (specular) */
static void project(const cv_t *a, rd_vtx *o) {
  float dist = nj_f(NJG_SCREEN), cx = nj_f(NJG_SCREEN + 12), cy = nj_f(NJG_SCREEN + 16);
  float iw = 1.0f / a->p[2]; /* view space: +z forward (checked against the SDK's njCalcPoint) */
  o->x = cx + dist * a->p[0] * iw;
  o->y = cy + dist * a->p[1] * iw; /* view space y points down the screen */
  o->iw = iw; o->u = a->u; o->v = a->v;
  static const int sh[4] = {16, 8, 0, 24}; /* r g b a -> ARGB */
  uint32_t col = 0;
  for (int k = 0; k < 4; k++) { float f = a->c[k] < 0 ? 0 : a->c[k] > 1 ? 1 : a->c[k]; col |= (uint32_t)(f * 255.0f + 0.5f) << sh[k]; }
  o->col = col;
  uint32_t ofs = 0xFF000000u;
  for (int k = 0; k < 3; k++) { float f = a->s[k] < 0 ? 0 : a->s[k] > 1 ? 1 : a->s[k]; ofs |= (uint32_t)(f * 255.0f + 0.5f) << sh[k]; }
  o->ofs = ofs;
}
static void lerpv(const cv_t *a, const cv_t *b, float t, cv_t *o) {
  for (int k = 0; k < 3; k++) o->p[k] = a->p[k] + (b->p[k] - a->p[k]) * t;
  o->u = a->u + (b->u - a->u) * t; o->v = a->v + (b->v - a->v) * t;
  for (int k = 0; k < 4; k++) o->c[k] = a->c[k] + (b->c[k] - a->c[k]) * t;
  for (int k = 0; k < 3; k++) o->s[k] = a->s[k] + (b->s[k] - a->s[k]) * t;
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
/* specular (offset colour) of one light: the material's specular colour x (n.h)^exponent, h the half vector
 * between the light and the eye (view space: the eye looks along +z); the exponent is the specular colour's
 * alpha byte (S8; strip flag 0x02 ignores it; the L1 "easy" family has none) */
static float spec_term(const float *n, const float *ldir, const float *pos, float e) {
  float l[3] = {-ldir[0], -ldir[1], -ldir[2]}, vv[3] = {-pos[0], -pos[1], -pos[2]};
  float lv = sqrtf(vv[0] * vv[0] + vv[1] * vv[1] + vv[2] * vv[2]);
  if (lv < 1e-6f) return 0;
  float h[3] = {l[0] + vv[0] / lv, l[1] + vv[1] / lv, l[2] + vv[2] / lv}, lh = sqrtf(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
  if (lh < 1e-6f) return 0;
  float nh = (n[0] * h[0] + n[1] * h[1] + n[2] * h[2]) / lh;
  return nh > 0 ? det_powf(nh, e < 1 ? 1 : e) : 0;
}
static void shade_spec(const vb_t *v, const uint32_t *scol, uint32_t sflags, float *o, float *sp) {
  float d[4];
  argb4(diffuse(), d);
  sp[0] = sp[1] = sp[2] = 0;
  if (scol) { argb4(*scol, o); return; }
  if (v->hascol) { argb4(v->col, o); o[3] *= d[3]; return; }
  if ((sflags & 1) || !v->hasn) { for (int k = 0; k < 4; k++) o[k] = d[k]; return; }
  float spc[4];
  argb4(mat.spe, spc);
  int use_spec = cur_fam != 0 && !(sflags & 2) && (spc[0] + spc[1] + spc[2]) > 0;
  float e = (float)(mat.spe >> 24);
  if (cur_fam == 1) { /* L2: ambient colour + up to four lights */
    float k[3] = {l2_amb[0], l2_amb[1], l2_amb[2]};
    for (int i = 0; i < 4; i++) {
      float nl = -(v->n[0] * l2[i].dir[0] + v->n[1] * l2[i].dir[1] + v->n[2] * l2[i].dir[2]);
      if (nl > 0) for (int j = 0; j < 3; j++) k[j] += nl * l2[i].dif * l2[i].col[j];
      if (use_spec && (l2[i].dir[0] != 0 || l2[i].dir[1] != 0 || l2[i].dir[2] != 0)) {
        float st = spec_term(v->n, l2[i].dir, v->p, e);
        for (int j = 0; j < 3; j++) sp[j] += spc[j] * st * l2[i].col[j];
      }
    }
    for (int j = 0; j < 3; j++) o[j] = d[j] * k[j];
    o[3] = d[3];
    return;
  }
  const light1_t *L = &fam[cur_fam];
  float nl = -(v->n[0] * L->dir[0] + v->n[1] * L->dir[1] + v->n[2] * L->dir[2]); /* the light's direction of travel */
  float k = L->amb + (nl > 0 ? nl : 0) * L->dif;
  o[0] = d[0] * k * L->col[0]; o[1] = d[1] * k * L->col[1]; o[2] = d[2] * k * L->col[2]; o[3] = d[3];
  if (use_spec) { float st = spec_term(v->n, L->dir, v->p, e); for (int j = 0; j < 3; j++) sp[j] = spc[j] * st * L->col[j]; }
}
static void shade(const vb_t *v, const uint32_t *scol, uint32_t sflags, float *o) { float sp[3]; shade_spec(v, scol, sflags, o, sp); }

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
  if (hle_tracing()) rt_log("hle trace strip %08x type %u flags %x strips %u ctrl3d %x dif %08x amb %08x spe %08x blend %u/%u tex %u tflags %x",
                            a, t, sflags, ns, rd32(NJG_CTRL3D), diffuse(), mat.amb, mat.spe, mat.src, mat.dst, mat.texid, mat.tflags);
  /* specular needs the offset colour: decide per strip chunk (any lit vertex with a highlight) */
  if (cur_fam != 0 && !(sflags & 3) && (mat.spe & 0xFFFFFFu)) s.mode |= RD_M_OFFSET;
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
      shade_spec(vv, hs ? &scol : 0, sflags, o->c, o->s);
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
    } else if (t >= 64 && t <= 75) {
      /* njControl3D CNK_CONSTANT_ATTR: the SDK masks the strip header with njSetConstantAttr's AND/OR words */
      if (rd32(NJG_CTRL3D) & CTRL3D_CNK_CONSTANT_ATTR) fl = (((h & rd32(NJG_CATTR_AND)) | rd32(NJG_CATTR_OR)) >> 8) & 0xFF;
      strip_chunk(a, t, fl);
    }
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
  if (hle_tracing()) rt_log("hle trace model %08x at %d %d %d det %d/1000", m, (int)M[12], (int)M[13], (int)M[14],
                            (int)(1000 * (M[0] * (M[5] * M[10] - M[6] * M[9]) - M[1] * (M[4] * M[10] - M[6] * M[8]) + M[2] * (M[4] * M[9] - M[5] * M[8]))));
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
/* the arrays of the keys around frame f and the blend factor (linear between keys, as the SDK's shape
 * motion: wraps from the last key to the first over nbFrame) */
static uint32_t ptrkey(uint32_t keys, uint32_t nb, uint32_t nbf, float f, uint32_t *a1, float *t) {
  uint32_t k0, k1;
  *a1 = 0; *t = 0;
  if (!key_find(keys, nb, 8, nbf, f, &k0, &k1, t)) return 0;
  uint32_t a0 = rd32(k0 + 4);
  if (k1 != k0 && *t > 0) { *a1 = rd32(k1 + 4); if (!sh4_is_ram(*a1)) *a1 = 0; }
  return a0;
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
    uint32_t sv = 0, sn = 0, sv1 = 0, sn1 = 0;
    float st = 0, st2 = 0;
    if (an && an->shape) {
      uint32_t nbf = rd32(an->shape + 4);
      if (mdata_get(an->shape, (int)node, 0x10, &keys, &nb)) sv = ptrkey(keys, nb, nbf, an->frame, &sv1, &st);
      if (mdata_get(an->shape, (int)node, 0x20, &keys, &nb)) sn = ptrkey(keys, nb, nbf, an->frame, &sn1, &st2);
      if (!sv) st = st2;
    }
    if (hle_tracing()) rt_log("hle trace node %08x ev %x model %08x pos %d %d %d ang %x %x %x scl %d %d %d /1000 shape %08x %08x",
                              o, ev, model, (int)(pos[0] * 1000), (int)(pos[1] * 1000), (int)(pos[2] * 1000), ang[0], ang[1], ang[2],
                              (int)(scl[0] * 1000), (int)(scl[1] * 1000), (int)(scl[2] * 1000), sv, sn);
    gcall(c, G_PUSH, 0, 0, 0, 0, 0, 0, 0);
    if (!(ev & EV_UNIT_POS)) gcall(c, G_TRANSLATE, CUR_M, 0, 0, 0, pos[0], pos[1], pos[2]);
    if (!(ev & EV_UNIT_ANG)) gcall(c, (ev & EV_ZXY) ? G_ROTZXY : G_ROTXYZ, CUR_M, (uint32_t)ang[0], (uint32_t)ang[1], (uint32_t)ang[2], 0, 0, 0);
    if (!(ev & EV_UNIT_SCL)) gcall(c, G_SCALE, CUR_M, 0, 0, 0, scl[0], scl[1], scl[2]);
    shape_vert = sh4_is_ram(sv) ? sv : 0; shape_norm = sh4_is_ram(sn) ? sn : 0;
    shape_vert1 = shape_vert ? sv1 : 0; shape_norm1 = shape_norm ? sn1 : 0; shape_t = st;
    if (!(ev & EV_HIDE) && model) draw_model(model);
    shape_vert = shape_norm = shape_vert1 = shape_norm1 = 0;
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
  cv_t cvv = {{o[0], o[1], o[2]}, 0, 0, {1, 1, 1, 1}, {0, 0, 0}};
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
/* ---- modifier volumes (cheap shadow): njCnkModDrawModel draws a closed volume into the opaque modifier
 * volume list; volume chunks 56 (triangles), 57 (quads), 58 (strips): {nb | user count << 14}, then the
 * polygons' vertex indices, each followed by its user flags ---- */
static void volume_tri(uint32_t a, uint32_t b, uint32_t c) {
  if (a >= NVB || b >= NVB || c >= NVB) return;
  rd_state s;
  nj_state(&s, RD_OM, 0, 0);
  cv_t v[3];
  memset(v, 0, sizeof v);
  const uint32_t ix[3] = {a, b, c};
  for (int k = 0; k < 3; k++) { memcpy(v[k].p, vb[ix[k]].p, 12); v[k].u = v[k].v = 0; v[k].c[0] = v[k].c[1] = v[k].c[2] = v[k].c[3] = 0; }
  tri3(&s, &v[0], &v[1], &v[2], 0);
}
static void volume_plist(uint32_t a) {
  for (int guard = 0; guard < 10000; guard++) {
    uint32_t h = rd16(a), t = h & 0xFF;
    if (t == 0xFF) return;
    if (t == 0) { a += 2; continue; }
    if (t <= 7) { a += 2; continue; }
    if (t <= 15) { a += 4; continue; }
    uint32_t size = rd16(a + 2);
    if (t >= 56 && t <= 58) {
      uint32_t w = rd16(a + 4), nb = w & 0x3FFF, uf = w >> 14, q = a + 6;
      for (uint32_t i = 0; i < nb; i++) {
        if (t == 56) { volume_tri(rd16(q), rd16(q + 2), rd16(q + 4)); q += 6 + uf * 2; }
        else if (t == 57) { uint32_t i0 = rd16(q), i1 = rd16(q + 2), i2 = rd16(q + 4), i3 = rd16(q + 6);
          volume_tri(i0, i1, i2); volume_tri(i1, i3, i2); q += 8 + uf * 2; }
        else { int16_t len = (int16_t)rd16(q); int n = len < 0 ? -len : len; q += 2;
          uint32_t w0 = 0, w1 = 0;
          for (int j = 0; j < n; j++) { uint32_t ix = rd16(q); q += 2; if (j >= 2) { volume_tri(w0, w1, ix); q += uf * 2; } w0 = w1; w1 = ix; } }
      }
    }
    a += 4 + size * 2;
  }
}
static uint32_t n_volumes;
static void h_njCnkModDrawModel(SH4 *c) {
  hle_logcall(c, ""); cpu_now = c;
  uint32_t m = R(4);
  int obj = !sh4_is_ram(rd32(m)) && sh4_is_ram(rd32(m + 4)); /* the game passes an object (eval flags first) */
  uint32_t ev = obj ? rd32(m) : 7u;
  if (obj) {
    gcall(c, G_PUSH, 0, 0, 0, 0, 0, 0, 0);
    if (!(ev & EV_UNIT_POS)) gcall(c, G_TRANSLATE, CUR_M, 0, 0, 0, nj_f(m + 8), nj_f(m + 12), nj_f(m + 16));
    if (!(ev & EV_UNIT_ANG)) gcall(c, (ev & EV_ZXY) ? G_ROTZXY : G_ROTXYZ, CUR_M, rd32(m + 20), rd32(m + 24), rd32(m + 28), 0, 0, 0);
    if (!(ev & EV_UNIT_SCL)) gcall(c, G_SCALE, CUR_M, 0, 0, 0, nj_f(m + 32), nj_f(m + 36), nj_f(m + 40));
    m = rd32(m + 4);
  }
  if (sh4_is_ram(m)) {
    load_matrix();
    if (sh4_is_ram(rd32(m))) vlist(rd32(m));
    if (sh4_is_ram(rd32(m + 4))) volume_plist(rd32(m + 4));
    n_volumes++;
  }
  if (obj) gcall(c, G_POP, 1, 0, 0, 0, 0, 0, 0);
  R(0) = 0;
}
static void h_njSetCheapShadowMode(SH4 *c) { hle_logcall(c, ""); rd_shadow(R(4)); R(0) = 0; }

/* njDrawSprite3D(sprite, n, attr): the sprite's quad in the model's xy plane at sprite->p (y down as in
 * 2D, scaled by sx, sy; attr 1 rotates it by ang), through the current matrix, projected like a model */
static void h_njDrawSprite3D(SH4 *c) {
  hle_logcall(c, "");
  uint32_t sp = R(4), n = R(5), attr = R(6);
  uint32_t tl = rd32(sp + 0x18), ta = rd32(sp + 0x1C) + n * 20;
  if (tl) wr32(NJG_TEXLIST, tl);
  int16_t tsx = (int16_t)rd16(ta), tsy = (int16_t)rd16(ta + 2), cx = (int16_t)rd16(ta + 4), cy = (int16_t)rd16(ta + 6);
  float u1 = (float)(int16_t)rd16(ta + 8) / 256.0f, v1 = (float)(int16_t)rd16(ta + 10) / 256.0f;
  float u2 = (float)(int16_t)rd16(ta + 12) / 256.0f, v2 = (float)(int16_t)rd16(ta + 14) / 256.0f;
  uint32_t texid = (uint32_t)(int32_t)(int16_t)rd16(ta + 16);
  if (attr & 4) { float t = u1; u1 = u2; u2 = t; }
  if (attr & 8) { float t = v1; v1 = v2; v2 = t; }
  float px = nj_f(sp), py = nj_f(sp + 4), pz = nj_f(sp + 8), sx = nj_f(sp + 0xC), sy = nj_f(sp + 0x10);
  float x0 = (float)-cx * sx, y0 = (float)-cy * sy, x1 = (float)(tsx - cx) * sx, y1 = (float)(tsy - cy) * sy;
  float qx[4] = {x0, x1, x0, x1}, qy[4] = {y0, y0, y1, y1}, qu[4] = {u1, u2, u1, u2}, qv[4] = {v1, v1, v2, v2};
  float cs = 1, sn = 0;
  if (attr & 1) det_sincos_angle((int32_t)rd32(sp + 0x14), &sn, &cs);
  float col[4] = {1, 1, 1, 1};
  if (attr & 2) argb4(nj_argb_f(NJG_CMAT), col);
  load_matrix();
  cv_t v[4];
  memset(v, 0, sizeof v);
  for (int k = 0; k < 4; k++) {
    float m[3] = {px + qx[k] * cs - qy[k] * sn, py + qx[k] * sn + qy[k] * cs, pz};
    xform(m, v[k].p);
    v[k].u = qu[k]; v[k].v = qv[k];
    for (int j = 0; j < 4; j++) v[k].c[j] = col[j];
  }
  rd_state s;
  nj_state(&s, (attr & 0x20) ? RD_TR : RD_OP, nj_tex_of(tl ? tl : rd32(NJG_TEXLIST), texid), (attr & 0x20) != 0);
  tri3(&s, &v[0], &v[1], &v[2], 0);
  tri3(&s, &v[1], &v[3], &v[2], 0);
  R(0) = 0;
}

/* ---- basic models (S8). The game's "nj_ObjectB" (0x8C0C30A8, 1 site, called once per character in races and
 * in the attract demo) takes a basic NJS_MODEL, not an object: {points*, normals*, nbPoint, meshsets*, mats*,
 * u16 nbMeshset, u16 nbMat, center[3], r}; it saves the polygon context, checks nbPoint against the vertex
 * buffer, draws, restores. Meshset (24 bytes): {u16 type << 14 | material, u16 nbMesh, Sint16 *meshes,
 * attrs*, normals*, NJS_COLOR *vertcolor, NJS_TEX *vertuv} per corner; types 0 triangles, 1 quads (strip
 * order), 2 N-gons and 3 strips (count first, 0x8000 = reversed). Material (20 bytes): {diffuse ARGB,
 * specular ARGB, exponent, attr_texId, attrflags}; attrflags: src/dst blend 31-29/28-26, 0x02000000 ignore
 * light, 0x00800000 double side, 0x00400000 environment, 0x00200000 texture, 0x00100000 alpha,
 * 0x00080000 ignore specular, 0x40000/0x20000 flip U/V, 0x10000/0x8000 clamp U/V, 13-14 filter. Seen: a
 * 32-quad fading trail (128 points, vertex colours, texture 202, alpha, double side, ignore light). ---- */
static uint32_t n_basic;
static void basic_corner(uint32_t idx, uint32_t uvp, uint32_t vcp, uint32_t ci, uint32_t mattr, cv_t *o) {
  const vb_t *vv = &vb[idx < NVB ? idx : 0];
  memcpy(o->p, vv->p, 12);
  o->u = o->v = 0;
  if (uvp) { o->u = (float)(int16_t)rd16(uvp + ci * 4) / 256.0f; o->v = (float)(int16_t)rd16(uvp + ci * 4 + 2) / 256.0f; }
  if (mattr & 0x00400000u) { o->u = vv->n[0] * 0.5f + 0.5f; o->v = -vv->n[1] * 0.5f + 0.5f; }
  uint32_t sf = ((mattr & 0x02000000u) ? 1u : 0) | ((mattr & 0x00080000u) ? 2u : 0);
  if (vcp) { uint32_t col = rd32(vcp + ci * 4); shade_spec(vv, &col, sf, o->c, o->s); }
  else shade_spec(vv, 0, sf, o->c, o->s);
}
static void draw_basic(uint32_t m) {
  if (!sh4_is_ram(m)) return;
  load_matrix();
  uint32_t pts = rd32(m), nrm = rd32(m + 4), np = rd32(m + 8), ms = rd32(m + 12), mats = rd32(m + 16);
  uint32_t nms = rd16(m + 20), nmat = rd16(m + 22);
  if (np > NVB || !sh4_is_ram(pts)) return;
  for (uint32_t i = 0; i < np; i++) {
    float p[3] = {nj_f(pts + i * 12), nj_f(pts + i * 12 + 4), nj_f(pts + i * 12 + 8)};
    xform(p, vb[i].p);
    if (sh4_is_ram(nrm)) { float n[3] = {nj_f(nrm + i * 12), nj_f(nrm + i * 12 + 4), nj_f(nrm + i * 12 + 8)}; xnorm(n, vb[i].n); vb[i].hasn = 1; }
    else { vb[i].n[0] = vb[i].n[1] = 0; vb[i].n[2] = 1; vb[i].hasn = 0; }
    vb[i].hascol = 0;
  }
  for (uint32_t k = 0; k < nms && sh4_is_ram(ms); k++, ms += 24) {
    uint32_t tm = rd16(ms), type = tm >> 14, mi = tm & 0x3FFF, nb = rd16(ms + 2), me = rd32(ms + 4);
    uint32_t vcp = rd32(ms + 16), uvp = rd32(ms + 20);
    if (!sh4_is_ram(me)) continue;
    if (!sh4_is_ram(vcp)) vcp = 0;
    if (!sh4_is_ram(uvp)) uvp = 0;
    uint32_t ma = (sh4_is_ram(mats) && mi < nmat) ? mats + mi * 20 : 0;
    uint32_t mattr = ma ? rd32(ma + 16) : 0;
    mat_reset();
    if (ma) { mat.dif = rd32(ma); mat.spe = (rd32(ma + 4) & 0xFFFFFFu) | ((uint32_t)nj_f(ma + 8) & 0xFF) << 24; mat.texid = rd32(ma + 12) & 0x1FFF; }
    int alpha = (mattr & 0x00100000u) != 0;
    uint32_t tex = (mattr & 0x00600000u) ? nj_tex_of(rd32(NJG_TEXLIST), mat.texid) : 0;
    rd_state s;
    nj_state(&s, alpha ? RD_TR : RD_OP, tex, alpha);
    if (alpha) s.mode = (s.mode & ~0x3Fu) | RD_M_SRC(mattr >> 29) | RD_M_DST((mattr >> 26) & 7);
    s.mode &= ~RD_M_SHADE(3);
    s.mode |= RD_M_SHADE(alpha ? 3 : 1);
    if (mattr & 0x00010000u) s.mode |= RD_M_CLAMPU;
    if (mattr & 0x00008000u) s.mode |= RD_M_CLAMPV;
    if (mattr & 0x00040000u) s.mode |= RD_M_FLIPU;
    if (mattr & 0x00020000u) s.mode |= RD_M_FLIPV;
    if (!((mattr >> 13) & 3)) s.mode &= ~RD_M_FILTER;
    if (cur_fam != 0 && !(mattr & 0x02080000u) && (mat.spe & 0xFFFFFFu)) s.mode |= RD_M_OFFSET;
    int cull = (mattr & 0x00800000u) ? 0 : 1;
    uint32_t q = me, ci = 0;
    for (uint32_t pi = 0; pi < nb; pi++) {
      int n = type == 0 ? 3 : type == 1 ? 4 : 0, rev = 0;
      if (type >= 2) { uint32_t w = rd16(q); q += 2; n = (int)(w & 0x3FFF); rev = (w & 0x8000) != 0; }
      cv_t win[3];
      for (int j = 0; j < n; j++, ci++) {
        uint32_t idx = rd16(q); q += 2;
        if (j >= 3) { win[0] = win[1]; win[1] = win[2]; }
        cv_t *o = &win[j < 3 ? j : 2];
        basic_corner(idx, uvp, vcp, ci, mattr, o);
        if (j >= 2) {
          int odd = ((j - 2) & 1) ^ rev;
          if (odd) tri3(&s, &win[1], &win[0], &win[2], cull); else tri3(&s, &win[0], &win[1], &win[2], cull);
        }
      }
    }
  }
  n_basic++;
}
static void h_basic_model(SH4 *c) { hle_logcall(c, ""); cpu_now = c; cur_fam = 0; draw_basic(R(4)); R(0) = 0; }
void nj_model_log(void) {
  rt_log("render 3d models %u basic %u triangles %u near-clipped-out %u unknown vlist %u plist %u volumes %u", n_models, n_basic, n_tris3d, n_clipped, n_unknown_v, n_unknown_p, n_volumes);
}

#define FAMILY(n, i) \
  static void h_model_##n(SH4 *c) { cur_fam = i; h_model(c); } \
  static void h_motion_##n(SH4 *c) { cur_fam = i; h_motion(c); } \
  static void h_shapemotion_##n(SH4 *c) { cur_fam = i; h_shapemotion(c); }
FAMILY(l1, 0)
FAMILY(l2, 1)
FAMILY(l3, 2)
FAMILY(l4, 3)
static void h_object_l1(SH4 *c) { cur_fam = 0; h_object(c); }
const nj_impl_t nj_model_impls[] = {
  {"nj_ModelL1", h_model_l1}, {"nj_ModelL2", h_model_l2}, {"nj_ModelL3", h_model_l3}, {"nj_ModelL4", h_model_l4},
  {"nj_ObjectL1", h_object_l1}, {"nj_ObjectB", h_basic_model},
  {"nj_MotionL1", h_motion_l1}, {"nj_MotionL2", h_motion_l2}, {"nj_MotionL3", h_motion_l3}, {"nj_MotionL3b", h_motion_l3}, {"nj_MotionL4", h_motion_l4},
  {"nj_ShapeMotionL1", h_shapemotion_l1}, {"nj_ShapeMotionL1b", h_shapemotion_l1}, {"nj_ShapeMotionL2", h_shapemotion_l2},
  {"nj_ShapeMotionL2b", h_shapemotion_l2}, {"nj_ShapeMotionL3", h_shapemotion_l3}, {"nj_ShapeMotionL3b", h_shapemotion_l3},
  {"nj_ShapeMotionL4", h_shapemotion_l4},
  {"njDrawSprite3D", h_njDrawSprite3D}, {"njCnkModDrawModel", h_njCnkModDrawModel},
  {"njSetCheapShadowMode", h_njSetCheapShadowMode},
  {"nj_LightL1Dir", h_l1dir}, {"nj_LightL1Intensity", h_l1int}, {"nj_LightL1Color", h_l1col},
  {"nj_LightL3Dir", h_l3dir}, {"nj_LightL3Intensity", h_l3int}, {"nj_LightL3Color", h_l3col},
  {"nj_LightL4Dir", h_l4dir}, {"nj_LightL4Intensity", h_l4int}, {"nj_LightL4Color", h_l4col},
  {"nj_LightL2Dir", h_l2dir}, {"nj_LightL2DirN", h_l2dirn}, {"nj_LightL2ColorN", h_l2coln}, {"nj_LightL2IntensityN", h_l2intn},
  {"nj_LightL2Color", h_l2col}, {"nj_LightL2Update", h_l2update},
};
const int nj_model_nimpls = (int)(sizeof nj_model_impls / sizeof nj_model_impls[0]);
