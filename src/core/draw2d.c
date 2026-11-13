/* Pen Pen port, S17 (b01): small drawing helpers: a 2D line and a 2D triangle with optional vertex colours (white
 * when none are given), and a marker model drawn at a point with a size. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

/* Ninja's NJS_POINT2COL: the points, their colours, texture coordinates and their number */
typedef struct NjPoint2Col {
  gaddr p;      /* 0x00: NJS_POINT2[num] */
  gaddr col;    /* 0x04: NJS_COLOR[num] */
  gaddr tex;    /* 0x08 */
  uint32_t num; /* 0x0C */
} NjPoint2Col;
HW_SIZE(NjPoint2Col, 0x10);

/* the frames of the two (the original's layout and depth: the white colours, the NJS_POINT2COL, the saved PR) */
typedef struct LineFrame {
  uint32_t white[2];  /* 0x00: NJS_COLOR (argb bytes) per point */
  NjPoint2Col pc;     /* 0x08 */
  uint32_t saved_pr;  /* 0x18 */
} LineFrame;
HW_OFFSET(LineFrame, pc, 0x08);
HW_SIZE(LineFrame, 0x1C);
typedef struct TriangleFrame {
  uint32_t white[3];  /* 0x00 */
  NjPoint2Col pc;     /* 0x0C */
  uint32_t saved_pr;  /* 0x1C */
} TriangleFrame;
HW_OFFSET(TriangleFrame, pc, 0x0C);
HW_SIZE(TriangleFrame, 0x20);

#define WHITE 0xFFFFFFFFu
#define LINE_ATTR 0x40u
#define TRIANGLE_ATTR 0x60u
#define MARKER_MODEL 0x8C08AD14u /* an NJS_MODEL of the game's data, drawn as a marker */

/* Ninja (SDK): n NJS_POINT2COL sets, the priority in FR4 */
static void njDrawLine2D(gaddr pc, int32_t n, f32 pri, uint32_t attr) { FARGS1(pri); CALL3(0x8C0C56E0u, pc, n, attr); }
static void njDrawTriangle2D(gaddr pc, int32_t n, f32 pri, uint32_t attr) {
  FARGS1(pri);
  CALL3(0x8C0C5BD0u, pc, n, attr);
}
static void nj_ModelB(gaddr model) { CALL1(0x8C0C310Au, model); }

/* was func_8C011A00: a 2D line through two points at priority `pri`; colors = 0 draws it white */
void line_draw_2d(gaddr points, gaddr colors, f32 pri) {
  gaddr frame = hw_stack_alloc(sizeof(LineFrame));
  LineFrame *f = (LineFrame *)hw_ptr(frame);
  if (colors == 0) {
    f->white[1] = WHITE;
    f->white[0] = WHITE;
    colors = frame;
  }
  f->pc.p = points;
  f->pc.col = colors;
  f->pc.tex = 0;
  f->pc.num = 2;
  njDrawLine2D(member_addr(frame, LineFrame, pc), 1, pri, LINE_ATTR);
  hw_stack_free(sizeof(LineFrame));
}
HW_EXPORT(8c011a00, line_draw_2d(ARG0, ARG1, FARG0))

/* was func_8C011A44: the same for a triangle of three points */
void triangle_draw_2d(gaddr points, gaddr colors, f32 pri) {
  gaddr frame = hw_stack_alloc(sizeof(TriangleFrame));
  TriangleFrame *f = (TriangleFrame *)hw_ptr(frame);
  if (colors == 0) {
    f->white[2] = WHITE;
    f->white[1] = WHITE;
    f->white[0] = WHITE;
    colors = frame;
  }
  f->pc.p = points;
  f->pc.col = colors;
  f->pc.tex = 0;
  f->pc.num = 3;
  njDrawTriangle2D(member_addr(frame, TriangleFrame, pc), 1, pri, TRIANGLE_ATTR);
  hw_stack_free(sizeof(TriangleFrame));
}
HW_EXPORT(8c011a44, triangle_draw_2d(ARG0, ARG1, FARG0))

/* a point with a size (the marker's centre and radius) */
typedef struct Sphere {
  Vec3 center;  /* 0x00 */
  f32 radius;   /* 0x0C */
} Sphere;
HW_OFFSET(Sphere, radius, 0x0C);

/* was func_8C011A94: the marker model drawn at the sphere's centre, scaled to its diameter */
void sphere_draw_marker(gaddr sphere) {
  njPushMatrix(0);
  njTranslateV(0, sphere);
  f32 d = fmul(((const Sphere *)hw_ptr(sphere))->radius, fadd(F32(1.0f), F32(1.0f)));
  njScale(0, d, d, d);
  nj_ModelB(MARKER_MODEL);
  njPopMatrix(1);
}
HW_EXPORT(8c011a94, sphere_draw_marker(ARG0))
