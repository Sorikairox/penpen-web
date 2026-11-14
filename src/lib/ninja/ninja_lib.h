/* Pen Pen port, S20: the Katana SDK's Ninja library, hand-written (batch N2: render-state setters, trigonometry,
 * curves, fog table, view / camera / screen, chunk file reader). Every function works on the SDK's own globals at
 * their guest addresses, so the draw HLE (port/render/nj.h, nj_draw.c, nj_model.c) reads what it read before.
 *
 * Names: snake_case of the SDK name (sdk.h already uses the SDK names for its CALL wrappers). Where the census name
 * of port/symbols/sdk_calls.tsv was a guess that the code contradicts, the C name says what the code does and the
 * comment gives the census name. */
#ifndef PENPEN_NINJA_LIB_H
#define PENPEN_NINJA_LIB_H

#include "hw.h"

/* ---- types the SDK keeps in guest RAM ---- */

/* a RAM matrix: 16 floats in XMTRX's order (column j is m[4j .. 4j+3]) */
typedef struct NjMatrix {
  f32 m[16];
} NjMatrix;
HW_SIZE(NjMatrix, 0x40);

/* NJS_VIEW (njInitView / njSetView) */
typedef struct NjView {
  f32 px, py, pz;     /* 0x00: the eye */
  f32 vx, vy, vz;     /* 0x0C: the direction it looks in */
  int32_t roll;       /* 0x18: Ninja angle */
  int32_t ang;        /* 0x1C: the field of view (Ninja angle) */
  f32 n_clip, f_clip; /* 0x20: near / far planes (negative: view space looks down -z) */
  f32 side[3];        /* 0x28: (1, 0, 0) after njInitView; njSetView does not read it */
  f32 up[3];          /* 0x34: (0, 1, 0) after njInitView; njSetView does not read it */
} NjView;
HW_OFFSET(NjView, vx, 0x0C);
HW_OFFSET(NjView, roll, 0x18);
HW_OFFSET(NjView, ang, 0x1C);
HW_OFFSET(NjView, n_clip, 0x20);
HW_OFFSET(NjView, f_clip, 0x24);
HW_OFFSET(NjView, side, 0x28);
HW_OFFSET(NjView, up, 0x34);
HW_SIZE(NjView, 0x40);

/* the camera of nj_InitCamera / nj_SetCamera: its own matrix, then a view-like block */
typedef struct NjCamera {
  NjMatrix m;         /* 0x00: built by nj_SetCamera */
  f32 px, py, pz;     /* 0x40 */
  f32 vx, vy, vz;     /* 0x4C */
  int32_t roll;       /* 0x58 */
  uint32_t unk5c[5];  /* 0x5C: zeroed by nj_InitCamera, read by neither */
  f32 unk70;          /* 0x70: -1 after nj_InitCamera */
  f32 unk74;          /* 0x74: 0 after nj_InitCamera */
} NjCamera;
HW_OFFSET(NjCamera, px, 0x40);
HW_OFFSET(NjCamera, vx, 0x4C);
HW_OFFSET(NjCamera, roll, 0x58);
HW_OFFSET(NjCamera, unk5c, 0x5C);
HW_OFFSET(NjCamera, unk70, 0x70);
HW_SIZE(NjCamera, 0x78);

/* NJS_ARGB (njSetConstantMaterial) */
typedef struct NjArgb {
  f32 a, r, g, b;
} NjArgb;
HW_SIZE(NjArgb, 0x10);

/* ---- the SDK's globals ---- */
#define g_nj_texlist          GUEST_VAR(gaddr, 0x8C1823F0u)    /* njSetTexture: the current NJS_TEXLIST (NJG_TEXLIST) */
#define g_nj_texbuf_addr      GUEST_VAR(gaddr, 0x8C1EF488u)    /* njInitTextureBuffer: the buffer */
#define g_nj_texbuf_size      GUEST_VAR(uint32_t, 0x8C1EF48Cu) /*   and its size, rounded down to 2 KB */
#define g_nj_control3d        GUEST_VAR(uint32_t, 0x8C1EF2ECu) /* njControl3D flags (NJG_CTRL3D) */
#define g_nj_node_callback    GUEST_VAR(gaddr, 0x8C1C6F7Cu)    /* nj_SetCallback: called with each node a motion drawer passes */
#define g_nj_cattr_and        GUEST_VAR(uint32_t, 0x8C1EF320u) /* njSetConstantAttr masks (NJG_CATTR_AND / _OR) */
#define g_nj_cattr_or         GUEST_VAR(uint32_t, 0x8C1EF324u)
#define g_nj_cmaterial        GUEST_VAR(NjArgb, 0x8C1EF328u)   /* njSetConstantMaterial (NJG_CMAT) */
/* the screen and projection block (port/render/nj_model.c reads dist, cx, cy, near, far) */
#define g_nj_screen_dist      GUEST_VAR(f32, 0x8C1EF2A4u)      /* the projection distance */
#define g_nj_screen_w         GUEST_VAR(f32, 0x8C1EF2A8u)      /* the screen width (njSetView reads it) */
#define g_nj_screen_cx        GUEST_VAR(f32, 0x8C1EF2B0u)      /* the screen centre */
#define g_nj_screen_cy        GUEST_VAR(f32, 0x8C1EF2B4u)
#define g_nj_near             GUEST_VAR(f32, 0x8C1EF2D0u)      /* near / far clip distances (positive) */
#define g_nj_far              GUEST_VAR(f32, 0x8C1EF2D4u)
#define g_nj_aspect           GUEST_VAR(f32, 0x8C1EF2D8u)      /* the width is divided by it (njSetView) */
/* the matrix stack: the current matrix pointer (NJG_CURMATRIX) and the stack's first matrix */
#define g_nj_current_matrix   GUEST_VAR(gaddr, 0x8C1EF2B8u)
#define g_nj_matrix_base      GUEST_VAR(gaddr, 0x8C1EF2BCu)
#define g_nj_matrix_depth     GUEST_VAR(int32_t, 0x8C1EF35Cu)  /* the matrices on the stack (njInitMatrix: 1; the view setters reset it) */
#define G_NJ_UNIT_MATRIX      0x8C18F060u                      /* the SDK's constant unit matrix */
#define G_NJ_VIEW_MATRIX      0x8C23BDE0u                      /* njSetView builds the view's matrix here */
#define G_NJ_CAMERA_MATRIX    0x8C23B7E0u                      /* a copy of the stack's first matrix after a view */
#define g_nj_camera           GUEST_VAR(gaddr, 0x8C23B7C0u)    /* nj_SetCamera: the camera applied last */
#define G_NJ_MODF_INT_PART    0x8C1C6E48u                      /* njRandom's discarded whole part (a float) */

static inline NjMatrix *nj_matrix_at(gaddr a) { return (NjMatrix *)hw_ptr(a); }
static inline NjView *nj_view_at(gaddr a) { return (NjView *)hw_ptr(a); }
static inline NjCamera *nj_camera_at(gaddr a) { return (NjCamera *)hw_ptr(a); }

/* ---- render_state.c ---- */
int32_t nj_set_texture(gaddr texlist);
void nj_control_3d(uint32_t flags);
void nj_set_constant_attr(uint32_t and_mask, uint32_t or_mask);
void nj_set_constant_material(gaddr argb);
void nj_init_texture_buffer(gaddr buffer, uint32_t size);

/* ---- trig.c ---- */
f32 nj_sin(int32_t angle);
f32 nj_cos(int32_t angle);
f32 nj_tan(int32_t angle);
f32 nj_sqrt(f32 x);
f32 nj_fraction(f32 x);

/* ---- curve.c ---- */
void nj_bezier3(gaddr points, gaddr out, f32 t);
void nj_hermite3(gaddr points, gaddr out, f32 t);

/* ---- chunk.c ---- */
void nj_mem_copy4(gaddr dst, gaddr src, uint32_t words);
void nj_mem_copy1(gaddr dst, gaddr src, uint32_t bytes);
uint32_t nj_byte_swap32(uint32_t v);
gaddr nj_read_chunk_file(gaddr data, gaddr offset_io, gaddr type_out);

/* ---- fog.c ---- */
void nj_generate_fog_table3(gaddr table, f32 n, f32 f);

/* ---- view.c ---- */
void nj_init_view(gaddr view);
void nj_set_view(gaddr view);
void nj_clip_z(f32 n, f32 f);
void nj_set_screen(gaddr screen);
void nj_init_camera(gaddr camera);
void nj_set_camera(gaddr camera);
void nj_apply_camera(void);

#endif
