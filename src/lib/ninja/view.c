/* Pen Pen port, S20: Ninja's view, camera, screen and clip setters.
 *
 * A view (NJS_VIEW) or a camera becomes a matrix: start from the unit matrix, rotate by -roll about z, then by the
 * pitch about x and the heading about y that turn the view direction onto -z (both from acosf, in Ninja angle
 * units), then translate by -eye. Applying it resets Ninja's matrix stack to its first matrix, which becomes
 * scale(-1, -1, 1) times that matrix, and loads it into XMTRX (with a copy at G_NJ_CAMERA_MATRIX).
 *
 * The matrix functions it uses are matrix.h's (transform.c, matrix.c). */
#include "hw.h"
#include "sdk.h"
#include "ninja_lib.h"
#include "matrix.h"
#include "../lib.h"

/* NJS_SCREEN (njSetScreen) */
typedef struct NjScreen {
  f32 dist;   /* 0x00 */
  f32 w, h;   /* 0x04: not copied by njSetScreen */
  f32 cx, cy; /* 0x0C */
} NjScreen;
HW_OFFSET(NjScreen, cx, 0x0C);
HW_SIZE(NjScreen, 0x14);

#define NJ_RAD_TO_ANGLE  F32_BITS(0x4622F986u) /* 65536 / (2 pi) */
#define NJ_CLIP_MIN      F32_BITS(0xC77FFF00u) /* -65535: the farthest plane njSetAspect keeps */
#define NJ_VIEW_FAR      F32_BITS(0xC76A6000u) /* -60000: njInitView's far plane */
#define NJ_VIEW_FOV      0x2AAA                /* njInitView's field of view: 60 degrees */


/* an angle in radians as Ninja units, kept to 16 bits */
static int32_t angle_of(f32 radians) { return f32_to_int(fmul(radians, NJ_RAD_TO_ANGLE)) & 0xFFFF; }

/* -a with the CPU's wrap-around (NEG) */
static int32_t neg_angle(int32_t a) { return (int32_t)(0u - (uint32_t)a); }

/* XMTRX (the FPU's back bank) from / to a RAM matrix, element by element in the same order */
static void xmtrx_load(gaddr m) {
  f32 *x = hw_xmtrx();
  const NjMatrix *src = nj_matrix_at(m);
  for (int i = 0; i < 16; i++) x[i] = src->m[i];
}
static void xmtrx_store(gaddr m) {
  const f32 *x = hw_xmtrx();
  NjMatrix *dst = nj_matrix_at(m);
  for (int i = 0; i < 16; i++) dst->m[i] = x[i];
}

/* the view matrix's last steps, shared by njSetView and nj_SetCamera's tail: the matrix stack is reset to its
 * first matrix (XMTRX gets what that matrix held, then it becomes the unit matrix), which becomes
 * scale(-1, -1, 1) times `view_matrix`; XMTRX gets the result, and G_NJ_CAMERA_MATRIX a copy */
static void apply_view_matrix(gaddr view_matrix) {
  gaddr base = g_nj_matrix_base;
  g_nj_matrix_depth = 1; /* the matrix stack back to its base */
  xmtrx_load(base);
  *nj_matrix_at(base) = *nj_matrix_at(G_NJ_UNIT_MATRIX);
  g_nj_current_matrix = base;
  nj_scale(base, F32(-1.0f), F32(-1.0f), F32(1.0f));
  nj_multi_matrix(base, view_matrix);
  xmtrx_load(base);
  xmtrx_store(G_NJ_CAMERA_MATRIX);
}

/* was func_8C0CF4AC (njInitView): the default view: at the origin looking down -z, no roll, a 60-degree field of
 * view, planes at -1 and -60000 */
void nj_init_view(gaddr view) {
  NjView *v = nj_view_at(view);
  v->up[2] = F32(0.0f);
  v->up[1] = F32(1.0f);
  v->up[0] = F32(0.0f);
  v->side[2] = F32(0.0f);
  v->side[1] = F32(0.0f);
  v->side[0] = F32(1.0f);
  v->f_clip = NJ_VIEW_FAR;
  v->n_clip = F32(-1.0f);
  v->ang = NJ_VIEW_FOV;
  v->roll = 0;
  v->vz = F32(-1.0f);
  v->vy = F32(0.0f);
  v->vx = F32(0.0f);
  v->pz = F32(0.0f);
  v->py = F32(0.0f);
  v->px = F32(0.0f);
}
HW_EXPORT(8c0cf4ac, nj_init_view(ARG0))

/* was func_8C0CF4EC (njSetView): builds the view's matrix at G_NJ_VIEW_MATRIX and applies it; the projection
 * distance from the field of view (width / aspect / (2 tan(ang / 2))) and the clip planes from the view */
void nj_set_view(gaddr view) {
  const NjView *v = nj_view_at(view);
  *nj_matrix_at(G_NJ_VIEW_MATRIX) = *nj_matrix_at(G_NJ_UNIT_MATRIX);
  nj_rotate_z(G_NJ_VIEW_MATRIX, neg_angle(v->roll));

  /* pitch: the angle between the direction and its projection on the xz plane (its cosine clamped to [-1, 1]) */
  f32 vx = v->vx, vy = v->vy, vz = v->vz;
  f32 xz2 = fmac(vz, vz, fmul(vx, vx));
  f32 xz = fsqrt(xz2);
  f32 c = fmul(xz, fsrra(fmac(xz, xz, fmul(vy, vy))));
  if (fgt(c, F32(1.0f))) {
    c = F32(1.0f);
  } else if (fgt(F32(-1.0f), c)) {
    c = F32(-1.0f);
  }
  int32_t pitch = angle_of(rt_acosf_checked(c));
  nj_rotate_x(G_NJ_VIEW_MATRIX, fgt(F32(0.0f), vy) ? -pitch : pitch);

  /* heading: from the direction's z over its xz length (none when the direction is vertical) */
  if (!feq(F32(0.0f), xz2)) {
    c = fmul(fsrra(xz2), vz);
    if (fgt(c, F32(1.0f))) {
      c = F32(1.0f);
    } else if (fgt(F32(-1.0f), c)) {
      c = F32(-1.0f);
    }
    int32_t heading = angle_of(rt_acosf_checked(c));
    nj_rotate_y(G_NJ_VIEW_MATRIX, fgt(F32(0.0f), vx) ? heading : -heading);
  }
  nj_translate(G_NJ_VIEW_MATRIX, fneg(v->px), fneg(v->py), fneg(v->pz));

  apply_view_matrix(G_NJ_VIEW_MATRIX);

  f32 t = nj_tan(v->ang >> 1);
  g_nj_screen_dist = fdiv(fdiv(g_nj_screen_w, g_nj_aspect), fadd(t, t));
  g_nj_near = fneg(v->n_clip);
  g_nj_far = fneg(v->f_clip);
}
HW_EXPORT(8c0cf4ec, nj_set_view(ARG0))

/* was func_8C0D19CC (census name njSetAspect: the code sets the clip planes, as njClipZ): each plane is kept in
 * [-65535, -1] and stored as a distance */
void nj_clip_z(f32 n, f32 f) {
  f32 cn = n, cf = f;
  if (fgt(n, F32(-1.0f))) cn = F32(-1.0f);
  if (fgt(NJ_CLIP_MIN, n)) cn = NJ_CLIP_MIN;
  if (fgt(f, F32(-1.0f))) cf = F32(-1.0f);
  if (fgt(NJ_CLIP_MIN, f)) cf = NJ_CLIP_MIN;
  g_nj_near = fneg(cn);
  g_nj_far = fneg(cf);
}
HW_EXPORT(8c0d19cc, nj_clip_z(FARG0, FARG1))

/* was func_8C0D1A7C (njSetScreen): the projection distance and the screen centre (not its size) */
void nj_set_screen(gaddr screen) {
  const NjScreen *s = (const NjScreen *)hw_ptr(screen);
  g_nj_screen_dist = s->dist;
  g_nj_screen_cx = s->cx;
  g_nj_screen_cy = s->cy;
}
HW_EXPORT(8c0d1a7c, nj_set_screen(ARG0))

/* was func_8C0D1A10 (nj_InitCamera): a camera at the origin looking down -z, its matrix the unit matrix */
void nj_init_camera(gaddr camera) {
  NjCamera *cam = nj_camera_at(camera);
  cam->unk74 = F32(0.0f);
  cam->unk70 = F32(-1.0f);
  for (int i = 4; i >= 0; i--) cam->unk5c[i] = 0;
  cam->roll = 0;
  cam->vz = F32(-1.0f);
  cam->vy = F32(0.0f);
  cam->vx = F32(0.0f);
  cam->pz = F32(0.0f);
  cam->py = F32(0.0f);
  cam->px = F32(0.0f);
  cam->m = *nj_matrix_at(G_NJ_UNIT_MATRIX);
}
HW_EXPORT(8c0d1a10, nj_init_camera(ARG0))

/* was func_8C10C698: applies the camera nj_SetCamera chose (g_nj_camera's matrix) as the view */
void nj_apply_camera(void) { apply_view_matrix(g_nj_camera); }
HW_EXPORT(8c10c698, nj_apply_camera())

/* was func_8C0D1A94 (nj_SetCamera): builds the camera's own matrix (as njSetView does, without the clamps of the
 * cosines nor the vertical-direction check), makes it the current camera and applies it */
void nj_set_camera(gaddr camera) {
  NjCamera *cam = nj_camera_at(camera);
  cam->m = *nj_matrix_at(G_NJ_UNIT_MATRIX);
  nj_rotate_z(camera, neg_angle(cam->roll));

  f32 vx = cam->vx, vy = cam->vy, vz = cam->vz;
  f32 xz = fsqrt(fmac(vz, vz, fmul(vx, vx)));
  f32 c = fmul(fsrra(fmac(xz, xz, fmul(vy, vy))), xz);
  int32_t pitch = angle_of(rt_acosf_checked(c));
  nj_rotate_x(camera, fgt(F32(0.0f), vy) ? -pitch : pitch);

  int32_t heading = angle_of(rt_acosf_checked(fdiv(vz, xz)));
  nj_rotate_y(camera, fgt(F32(0.0f), vx) ? heading : -heading);
  nj_translate(camera, fneg(cam->px), fneg(cam->py), fneg(cam->pz));

  g_nj_camera = camera;
  nj_apply_camera();
}
HW_EXPORT(8c0d1a94, nj_set_camera(ARG0))
