/* Pen Pen port, S5: the draw list builder (rd.h). Our own code. Buffer layout (little-endian, all u32
 * unless said; offsets relative to the header, texture pointers are addresses in the same memory: in a
 * dump file the file is that memory, header at 0): docs/port/RENDER.md 2.
 *   header 24 words: 'PPDL', version, frame, width, height, back colour 0/1/2, ncmd, cmd_off, nvert,
 *     vert_off, nup, up_off, nfree, free_off, fog colour, fog_off (128 floats, 0 = fog not set), stats x6
 *   command 8 words: list, first vertex, vertex count (triangles), texture id, mode, clip x0|y0<<16,
 *     clip x1|y1<<16, 0
 *   vertex 7 words: x, y, 1/w, u, v (floats), base ARGB, offset ARGB
 *   upload 4 words: id, width, height, pointer to RGBA8 (width*height*4 bytes)
 *   free 1 word: id
 * The opaque list keeps the submission order (merged into one command per state run), the translucent list
 * is sorted back to front per triangle (mean 1/w, stable), as an approximation of the PVR's per-pixel
 * autosort. The opaque modifier volume list is counted and dropped for now (RENDER.md 5). */
#include <stdlib.h>
#include <string.h>
#include "rd.h"

#ifdef __wasm__
__attribute__((import_module("env"), import_name("penpen_gl_frame"))) void penpen_gl_frame(const void *p, int len);
#else
static void penpen_gl_frame(const void *p, int len) { (void)p; (void)len; }
#endif

rd_stats_t rd_stats;

typedef struct { rd_state s; uint32_t first, count; } cmd_t;
typedef struct { uint32_t st; float key; uint32_t seq; rd_vtx v[3]; } tri_t;

static rd_vtx *vtx; static uint32_t nvtx, cvtx;
static cmd_t *cmd; static uint32_t ncmd, ccmd;
static tri_t *trs; static uint32_t ntr, ctr;
static rd_state *sts; static uint32_t nst, cst;
static uint32_t frame_no, back[3] = {0xFF000000u, 0xFF000000u, 0xFF000000u}, fog_col;
static float fog_tab[128]; static int fog_set;
static uint32_t n_om, n_culled;

#define GROW(p, n, c, k) do { if ((n) + (k) > (c)) { uint32_t nc = (c) ? (c) * 2 : 1024; while (nc < (n) + (k)) nc *= 2; \
  p = realloc(p, (size_t)nc * sizeof *(p)); c = nc; } } while (0)

/* textures */
typedef struct { int w, h, live; uint32_t key; uint32_t *rgba; } tex_t; /* rgba until uploaded */
static tex_t *tex; static uint32_t ntex, ctex;
static uint32_t *ups; static uint32_t nups, cups;   /* ids to upload with the next frame */
static uint32_t *frees; static uint32_t nfrees, cfrees;

uint32_t rd_tex_new(int w, int h, const uint32_t *rgba, uint32_t key) {
  uint32_t id = 0;
  for (uint32_t i = 1; i < ntex; i++) if (!tex[i].live && !tex[i].rgba) { id = i; break; }
  if (!id) { if (!ntex) ntex = 1; GROW(tex, ntex, ctex, 1); id = ntex++; }
  tex[id].w = w; tex[id].h = h; tex[id].live = 1; tex[id].key = key;
  tex[id].rgba = malloc((size_t)w * (size_t)h * 4);
  memcpy(tex[id].rgba, rgba, (size_t)w * (size_t)h * 4);
  GROW(ups, nups, cups, 1); ups[nups++] = id;
  return id;
}
void rd_tex_free(uint32_t id) {
  if (!id || id >= ntex || !tex[id].live) return;
  tex[id].live = 0;
  /* not yet uploaded: drop the pending upload; else the backend deletes it */
  for (uint32_t i = 0; i < nups; i++) if (ups[i] == id) { ups[i] = ups[--nups]; free(tex[id].rgba); tex[id].rgba = 0; return; }
  GROW(frees, nfrees, cfrees, 1); frees[nfrees++] = id;
}
int rd_tex_size(uint32_t id, int *w, int *h) {
  if (!id || id >= ntex || !tex[id].live) return -1;
  *w = tex[id].w; *h = tex[id].h;
  return 0;
}

void rd_clip_default(rd_state *s) { s->clip[0] = 0; s->clip[1] = 0; s->clip[2] = RD_W; s->clip[3] = RD_H_; }

void rd_begin(uint32_t frame) {
  frame_no = frame;
  nvtx = ncmd = ntr = nst = 0;
  n_om = n_culled = 0;
}
void rd_back_color(uint32_t c0, uint32_t c1, uint32_t c2) { back[0] = c0; back[1] = c1; back[2] = c2; }
void rd_fog(uint32_t color, const float *t, int n) {
  fog_col = color;
  for (int i = 0; i < 128; i++) fog_tab[i] = i < n ? t[i] : 1.0f;
  fog_set = 1;
}

static int same_state(const rd_state *a, const rd_state *b) {
  return a->list == b->list && a->tex == b->tex && a->mode == b->mode && !memcmp(a->clip, b->clip, sizeof a->clip);
}
static void op_tri(const rd_state *s, const rd_vtx *a, const rd_vtx *b, const rd_vtx *c) {
  if (!ncmd || !same_state(&cmd[ncmd - 1].s, s) || cmd[ncmd - 1].first + cmd[ncmd - 1].count != nvtx) {
    GROW(cmd, ncmd, ccmd, 1);
    cmd[ncmd].s = *s; cmd[ncmd].first = nvtx; cmd[ncmd].count = 0; ncmd++;
  }
  GROW(vtx, nvtx, cvtx, 3);
  vtx[nvtx++] = *a; vtx[nvtx++] = *b; vtx[nvtx++] = *c;
  cmd[ncmd - 1].count += 3;
}
static uint32_t state_id(const rd_state *s) {
  if (nst && same_state(&sts[nst - 1], s)) return nst - 1;
  GROW(sts, nst, cst, 1);
  sts[nst] = *s;
  return nst++;
}
static void tri(const rd_state *s, const rd_vtx *a, const rd_vtx *b, const rd_vtx *c, int cull) {
  if (cull) {
    float area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y); /* > 0: clockwise on screen (y down) */
    if ((cull == 1 && area > 0) || (cull == 2 && area < 0)) { n_culled++; return; }
  }
  if (s->list == RD_OM) { n_om++; return; }
  if (s->list == RD_TR) {
    GROW(trs, ntr, ctr, 1);
    tri_t *t = &trs[ntr];
    t->st = state_id(s); t->seq = ntr; t->key = (a->iw + b->iw + c->iw) * (1.0f / 3.0f);
    t->v[0] = *a; t->v[1] = *b; t->v[2] = *c;
    ntr++;
    return;
  }
  op_tri(s, a, b, c); /* opaque and punch-through: submission order */
}
void rd_strip(const rd_state *s, const rd_vtx *v, int n, int cull) {
  for (int i = 0; i + 2 < n; i++) {
    /* odd triangles of a strip have the opposite winding: keep the facing of the first */
    if (i & 1) tri(s, &v[i + 1], &v[i], &v[i + 2], cull);
    else tri(s, &v[i], &v[i + 1], &v[i + 2], cull);
  }
}
void rd_tris(const rd_state *s, const rd_vtx *v, int n) {
  for (int i = 0; i + 2 < n; i += 3) tri(s, &v[i], &v[i + 1], &v[i + 2], 0);
}

static int tri_cmp(const void *pa, const void *pb) {
  const tri_t *a = pa, *b = pb;
  if (a->key < b->key) return -1; /* far (small 1/w) first */
  if (a->key > b->key) return 1;
  return a->seq < b->seq ? -1 : a->seq > b->seq;
}

static uint8_t *out; static uint32_t cout;
void rd_end(void) {
  uint32_t op_cmds = ncmd;
  if (ntr) {
    qsort(trs, ntr, sizeof *trs, tri_cmp);
    for (uint32_t i = 0; i < ntr; i++) op_tri(&sts[trs[i].st], &trs[i].v[0], &trs[i].v[1], &trs[i].v[2]);
  }
  /* pack */
  uint32_t hdr = 24 * 4, csz = ncmd * 32, vsz = nvtx * 28, usz = nups * 16, fsz = nfrees * 4, gsz = fog_set ? 512 : 0;
  uint32_t total = hdr + csz + vsz + usz + fsz + gsz;
  if (total > cout) { free(out); cout = total * 2; out = malloc(cout); }
  uint32_t *h = (uint32_t *)out;
  memset(h, 0, hdr);
  h[0] = 0x4C445050u; h[1] = 1; h[2] = frame_no; h[3] = RD_W; h[4] = RD_H_;
  h[5] = back[0]; h[6] = back[1]; h[7] = back[2];
  h[8] = ncmd; h[9] = hdr; h[10] = nvtx; h[11] = hdr + csz; h[12] = nups; h[13] = hdr + csz + vsz;
  h[14] = nfrees; h[15] = hdr + csz + vsz + usz; h[16] = fog_col; h[17] = fog_set ? hdr + csz + vsz + usz + fsz : 0;
  h[18] = op_cmds; h[19] = ntr; h[20] = n_om; h[21] = n_culled;
  uint32_t *cw = (uint32_t *)(out + hdr);
  for (uint32_t i = 0; i < ncmd; i++, cw += 8) {
    cw[0] = cmd[i].s.list; cw[1] = cmd[i].first; cw[2] = cmd[i].count; cw[3] = cmd[i].s.tex; cw[4] = cmd[i].s.mode;
    cw[5] = (uint32_t)(uint16_t)cmd[i].s.clip[0] | (uint32_t)(uint16_t)cmd[i].s.clip[1] << 16;
    cw[6] = (uint32_t)(uint16_t)cmd[i].s.clip[2] | (uint32_t)(uint16_t)cmd[i].s.clip[3] << 16; cw[7] = 0;
  }
  memcpy(out + hdr + csz, vtx, vsz);
  uint32_t *uw = (uint32_t *)(out + hdr + csz + vsz);
  for (uint32_t i = 0; i < nups; i++, uw += 4) {
    tex_t *t = &tex[ups[i]];
    uw[0] = ups[i]; uw[1] = (uint32_t)t->w; uw[2] = (uint32_t)t->h; uw[3] = (uint32_t)(uintptr_t)t->rgba;
  }
  memcpy(out + hdr + csz + vsz + usz, frees, fsz);
  if (fog_set) memcpy(out + hdr + csz + vsz + usz + fsz, fog_tab, 512);
  rd_stats.cmds = ncmd; rd_stats.verts = nvtx; rd_stats.tris = nvtx / 3; rd_stats.culled = n_culled;
  rd_stats.uploads = nups; rd_stats.op = op_cmds; rd_stats.tr = ntr; rd_stats.om = n_om;
  rd_stats.textures = 0;
  for (uint32_t i = 1; i < ntex; i++) rd_stats.textures += tex[i].live;
  penpen_gl_frame(out, (int)total);
  /* uploaded: the backend has its own copy now */
  for (uint32_t i = 0; i < nups; i++) { free(tex[ups[i]].rgba); tex[ups[i]].rgba = 0; }
  nups = 0; nfrees = 0;
}
