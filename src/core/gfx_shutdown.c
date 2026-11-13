/* Pen Pen port, S17 (b01): the graphics state undone (the counterpart of core/gfx_init.c), and a view's fog
 * switch. */
#include "hw.h"
#include "sdk.h"
#include "../core/view.h"

/* the graphics state gfx_init allocates (guest .bss, as core/gfx_init.c names it) */
#define g_matrix_stack     GUEST_VAR(gaddr, 0x8C08A884u)   /* njInitMatrix's stack */
#define g_texture_manage   GUEST_VAR(gaddr, 0x8C08A88Cu)   /* njInitTexture's table */
#define g_gfx_block_894    GUEST_VAR(gaddr, 0x8C08A894u)   /* another block freed here (gfx_init does not set it) */
#define g_control3d_base   GUEST_VAR(gaddr, 0x8C08A8A0u)   /* the njControl3D flags stack */
#define LIGHT_WORK         0x8C1B335Cu                     /* the light gfx_init creates */

static void njDeleteLight(gaddr light) { CALL1(0x8C0C4E20u, light); }

/* was func_8C010D98: at shut-down: the light deleted and the blocks freed (the flags stack's pointer is left as it
 * is, the others cleared) */
void gfx_shutdown(void) {
  njDeleteLight(LIGHT_WORK);
  syFree(g_control3d_base);
  syFree(g_matrix_stack);
  g_matrix_stack = 0;
  syFree(g_texture_manage);
  g_texture_manage = 0;
  syFree(g_gfx_block_894);
  g_gfx_block_894 = 0;
}
HW_EXPORT(8c010d98, gfx_shutdown())

/* was func_8C012054 (a View method): Ninja's fog on or off as this view wants it */
void view_fog_apply(View *view) {
  if (view->fog == 0)
    njFogDisable();
  else
    njFogEnable();
}
HW_EXPORT(8c012054, view_fog_apply(view_at(ARG0)))
