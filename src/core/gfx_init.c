/* Pen Pen port, S13: Ninja's set-up and the background / fog colour (core/gfx_init.c). */
#include "hw.h"
#include "sdk.h"
#include "../core/ninja.h"
#include "../core/view.h"
#include "../game/game.h"

/* the graphics state the set-up allocates (guest .bss) */
#define g_gfx_ready        GUEST_VAR(int32_t, 0x8C08A880u) /* the set-up ran */
#define g_matrix_stack     GUEST_VAR(gaddr, 0x8C08A884u)   /* njInitMatrix's stack (32 matrices) */
#define g_vertex_work      GUEST_VAR(gaddr, 0x8C08A888u)   /* njInit3D's work area (the fixed area at 0x8CE40000) */
#define g_texture_manage   GUEST_VAR(gaddr, 0x8C08A88Cu)   /* njInitTexture's table (0x300 entries) */
#define g_shape_work       GUEST_VAR(gaddr, 0x8C08A890u)   /* njInitShape's work area (0x8CF00000) */
#define g_control3d_base   GUEST_VAR(gaddr, 0x8C08A8A0u)   /* the njControl3D flags stack (core/ninja.c: its top) */
#define g_control3d_depth  GUEST_VAR(int32_t, 0x8C08A8A4u)
#define LIGHT_WORK         0x8C1B335Cu                     /* the light njCreateLight makes */

#define MATRIX_STACK_SIZE  0x800u   /* 32 NJS_MATRIX */
#define TEXTURE_ENTRIES    0x300
#define TEXTURE_MANAGE_SIZE 0xCC00u /* TEXTURE_ENTRIES * 0x44 */
#define VERTEX_WORK        0x8CE40000u
#define SHAPE_WORK         0x8CF00000u
#define TEXTURE_BUFFER     0x8CE40000u
#define TEXTURE_BUFFER_SIZE 0x1C0000u
#define BACKGROUND_DEFAULT 0xFFC5C5C5u
#define ASPECT_Y           0xC61C4000u /* -10000.0f */

/* Ninja's set-up calls (SDK; 0x8C0A2544 has no S2 name) */
static void njInitVertexBuffer(uint32_t op, uint32_t om, uint32_t tp, uint32_t tm, uint32_t pt) {
  CALL5(0x8C0A1B52u, op, om, tp, tm, pt);
}
static void njTextureFilterMode(int32_t mode) { CALL1(0x8C0A2200u, mode); }
static void njInitTextureBuffer(gaddr buffer, uint32_t size) { CALL2(0x8C0A4488u, buffer, size); }
static void njInitTexture(gaddr manage, int32_t entries) { CALL2(0x8C0A449Eu, manage, entries); }
static void njInitShape(gaddr work) { CALL1(0x8C0A6396u, work); }
static void njInit3D(gaddr work, int32_t n) { CALL2(0x8C0A963Cu, work, n); }
static void njSetConstantAttr(uint32_t and_attr, uint32_t or_attr) { CALL2(0x8C0A9672u, and_attr, or_attr); }
static void njInitMatrix(gaddr stack, int32_t n, int32_t flag) { CALL3(0x8C0C325Cu, stack, n, flag); }
static void njCreateLight(gaddr light, int32_t mode) { CALL2(0x8C0C4CA0u, light, mode); }
static void njSetAspect(f32 ax, f32 ay) { FARGS2(ax, ay); CALL0(0x8C0D19CCu); }
static void sdk_8C0A2544(uint32_t value) { CALL1(0x8C0A2544u, value); } /* a Ninja/PVR setting (0xA0 here) */
static void njGenerateFogTable3(gaddr table, f32 near, f32 far) { FARGS2(near, far); CALL1(0x8C0C6CF0u, table); }

/* was func_8C010C28: Ninja's set-up, once: matrix stack, vertex and texture buffers, 3D, shapes, the background
 * colour, a light, the njControl3D flags stack, filtering and constant attributes, one full-screen view, the
 * aspect */
void gfx_init(void) {
  if (g_gfx_ready != 0)
    return;
  g_matrix_stack = syMalloc(MATRIX_STACK_SIZE);
  g_texture_manage = syMalloc(TEXTURE_MANAGE_SIZE);
  g_vertex_work = VERTEX_WORK;
  g_shape_work = SHAPE_WORK;
  njInitTextureBuffer(TEXTURE_BUFFER, TEXTURE_BUFFER_SIZE);
  njInitVertexBuffer(0xFFF80000u, 0x6000, 0x40000, 0, 0);
  njInitMatrix(g_matrix_stack, 0x20, 0);
  njInit3D(g_vertex_work, 0x800);
  njInitShape(g_shape_work);
  njInitTexture(g_texture_manage, TEXTURE_ENTRIES);
  g_background_color = BACKGROUND_DEFAULT;
  g_fog_dirty = 0;
  njCreateLight(LIGHT_WORK, 2);
  gaddr flags = syMalloc(0x20);
  g_control3d_base = flags;
  U32_AT(flags) = 0;
  g_control3d_depth = 0;
  njTextureFilterMode(1);
  control3d_set(4);
  njSetConstantAttr(0xFFFF9FFFu, 0x2000);
  sdk_8C0A2544(0xA0);
  view_set_split(SPLIT_FULL);
  njSetAspect(F32(-1.0f), F32_BITS(ASPECT_Y));
  g_gfx_ready = 1;
}
HW_EXPORT(8c010c28, gfx_init())

/* was func_8C010F04: the background colour, and the fog table from near to far (sent at the end of the render) */
void background_set(uint32_t argb, f32 fog_near, f32 fog_far) {
  g_background_color = argb;
  njGenerateFogTable3(FOG_TABLE, fog_near, fog_far);
  g_fog_dirty = 1;
}
HW_EXPORT(8c010f04, background_set(ARG0, FARG0, FARG1))
