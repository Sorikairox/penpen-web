/* Pen Pen port, S5: the per-frame draw list (our format), built by the Ninja HLE (nj_*.c) and consumed by
 * the WebGL2 backend (port/render/gl/render_gl.js) in the retrowser worker, or written to a dump file by the
 * capture path (node/run.mjs --shots) and rendered by the same backend in headless Chromium.
 * Format and rules: docs/port/RENDER.md 2.
 *
 * The model is the PowerVR2's: vertices are already in screen space (x, y in 640x480 pixels, 1/w for depth
 * and perspective-correct texturing), each primitive goes to one of the lists (opaque, opaque modifier
 * volume, translucent, punch-through) with a polygon state (texture, blending, shading, depth, user clip).
 * The C side expands strips to triangles, culls, and sorts the translucent list; the backend only draws. */
#ifndef RD_H
#define RD_H
#include <stdint.h>

#define RD_W 640
#define RD_H_ 480

enum { RD_OP = 0, RD_OM = 1, RD_TR = 2, RD_PT = 3 };

/* PVR blend factors (TSP SRC/DST_ALPHA_INSTR): 0 zero, 1 one, 2 "other" colour (dst for src, src for dst),
 * 3 1 - other colour, 4 src alpha, 5 1 - src alpha, 6 dst alpha, 7 1 - dst alpha */
/* mode word of a command (RD_M_*) */
#define RD_M_SRC(x) ((uint32_t)(x) & 7u)
#define RD_M_DST(x) (((uint32_t)(x) & 7u) << 3)
#define RD_M_ZWRITE (1u << 6)
#define RD_M_ZFUNC(x) (((uint32_t)(x) & 7u) << 7) /* PVR ISP depth compare: 0 never 1 less 2 equal 3 lequal
                                                    4 greater 5 notequal 6 gequal 7 always (on 1/w) */
#define RD_M_FILTER (1u << 10)                      /* bilinear */
#define RD_M_CLAMPU (1u << 11)
#define RD_M_CLAMPV (1u << 12)
#define RD_M_FLIPU (1u << 13)
#define RD_M_FLIPV (1u << 14)
#define RD_M_SHADE(x) (((uint32_t)(x) & 3u) << 15)  /* 0 decal, 1 modulate, 2 decal alpha, 3 modulate alpha */
#define RD_M_ALPHA (1u << 17)                       /* use the vertex alpha (else 1) */
#define RD_M_NOTEXA (1u << 18)                      /* ignore the texture's alpha */
#define RD_M_OFFSET (1u << 19)                      /* add the offset (specular) colour */
#define RD_M_FOG (1u << 20)                         /* table fog */
#define RD_M_SHADOW (1u << 21)                      /* PCW "shadow": opaque modifier volumes darken it */

typedef struct { float x, y, iw, u, v; uint32_t col, ofs; } rd_vtx; /* 28 bytes; colours ARGB */

typedef struct {
  uint32_t list, tex, mode;      /* tex: backend texture id (0 = none) */
  int16_t clip[4];               /* user clip x0, y0, x1, y1 (inclusive-exclusive, pixels) */
} rd_state;

/* frame */
void rd_begin(uint32_t frame);
void rd_back_color(uint32_t c0, uint32_t c1, uint32_t c2);
void rd_fog(uint32_t color, const float *table128, int n); /* the PVR fog table (128 entries, 0..1 density) */
void rd_fog_density(uint32_t reg);   /* the PVR FOG_DENSITY register value */
void rd_shadow(uint32_t intensity);  /* opaque modifier volumes darken by intensity/256 (cheap shadow) */
void rd_end(void);                    /* closes the frame: sort, pack, submit (penpen_gl_frame) */
/* primitives: n vertices as a strip (PVR order: v0 v1 v2, v1 v2 v3 ...) or a triangle list */
void rd_strip(const rd_state *s, const rd_vtx *v, int n, int cull); /* cull: 0 none, 1 cw, 2 ccw (screen) */
void rd_tris(const rd_state *s, const rd_vtx *v, int n);
void rd_clip_default(rd_state *s);

/* textures: RGBA8 (R first in memory), kept in wasm memory until the next frame is submitted */
uint32_t rd_tex_new(int w, int h, const uint32_t *rgba, uint32_t key); /* returns id >= 1 */
void rd_tex_free(uint32_t id);
int rd_tex_size(uint32_t id, int *w, int *h);

/* stats of the last frame (logged by the HLE) */
typedef struct { uint32_t cmds, tris, verts, culled, uploads, textures, op, tr, om; } rd_stats_t;
extern rd_stats_t rd_stats;

#endif
