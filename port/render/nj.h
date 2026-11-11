/* Pen Pen port, S5: the Ninja drawing HLE (nj_draw.c: frame, state, textures, 2D; nj_model.c: 3D).
 * hle.c registers nj_impls[] in its table and calls the frame hooks from njWaitVSync. */
#ifndef NJ_H
#define NJ_H
#include "../runtime/rt.h"
#include "rd.h"

typedef struct { const char *name; hle_fn fn; } nj_impl_t;
extern const nj_impl_t nj_impls[];
extern const int nj_nimpls;
extern const nj_impl_t nj_model_impls[];
extern const int nj_model_nimpls;

void nj_frame_begin(uint32_t frame);
void nj_frame_end(void);
void nj_model_log(void);
void hle_logcall(SH4 *c, const char *extra); /* hle.c: count + log one HLE call */
uint32_t hle_frame(void);

/* Ninja globals kept by guest SDK code (pure setters run as guest code, RENDER.md 4) */
#define NJG_CTRL3D 0x8C1EF2ECu     /* njControl3D flags */
#define CTRL3D_CNK_CONSTANT_ATTR 0x800u /* chunk strip headers: (header & AND mask) | OR mask (njSetConstantAttr) */
#define CTRL3D_SHADOW 0x400u            /* polygons are affected by modifier volumes (PCW bit 7) */
#define CTRL3D_SHADOW_OPAQUE 0x1000u    /* the same, for polygons without alpha only */
#define NJG_CATTR_AND 0x8C1EF320u  /* njSetConstantAttr masks */
#define NJG_CATTR_OR 0x8C1EF324u
#define NJG_CMAT 0x8C1EF328u       /* njSetConstantMaterial: NJS_ARGB {a, r, g, b} floats */
#define NJG_TEXLIST 0x8C1823F0u    /* njSetTexture: the current NJS_TEXLIST */
#define NJG_MEMLIST 0x8C1823F4u    /* njInitTexture: NJS_TEXMEMLIST array (0x44 bytes each) and count */
#define NJG_MEMLIST_N 0x8C1823F8u
#define NJG_CURMATRIX 0x8C1EF2B8u  /* the current matrix pointer (njInitMatrix's stack) */

float nj_f(uint32_t a);
uint32_t nj_argb_f(uint32_t a); /* NJS_ARGB floats at a -> ARGB8888 */
/* textures: backend id of texture number n of a texlist (0 if none) and its size */
uint32_t nj_tex_of(uint32_t texlist, uint32_t n);
/* the current 2D/3D render state the draw functions share */
extern uint32_t nj_blend_src, nj_blend_dst; /* PVR codes */
extern int16_t nj_clip[4];
extern int nj_filter;
void nj_state(rd_state *s, int list, uint32_t tex, int use_alpha);

#endif
