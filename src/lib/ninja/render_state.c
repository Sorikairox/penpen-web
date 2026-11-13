/* Pen Pen port, S20: Ninja's render-state setters. They only store the game's choice in the SDK's globals; the draw
 * HLE (port/render/nj_draw.c, nj_model.c) reads them when it draws, so every store is the original's. */
#include "hw.h"
#include "ninja_lib.h"

/* was func_8C0A4540 (njSetTexture): makes texlist the current texture list; 1, or -1 (nothing stored) for NULL */
int32_t nj_set_texture(gaddr texlist) {
  if (texlist == 0) return -1;
  g_nj_texlist = texlist;
  return 1;
}
HW_EXPORT(8c0a4540, RET(nj_set_texture(ARG0)))

/* was func_8C0A966C (njControl3D): the 3D drawing flags */
void nj_control_3d(uint32_t flags) { g_nj_control3d = flags; }
HW_EXPORT(8c0a966c, nj_control_3d(ARG0))

/* was func_8C0A9672 (njSetConstantAttr): the AND / OR masks applied to chunk strip headers */
void nj_set_constant_attr(uint32_t and_mask, uint32_t or_mask) {
  g_nj_cattr_and = and_mask;
  g_nj_cattr_or = or_mask;
}
HW_EXPORT(8c0a9672, nj_set_constant_attr(ARG0, ARG1))

/* was func_8C0A967C (njSetConstantMaterial): copies the NJS_ARGB colour (four floats, moved as they are) */
void nj_set_constant_material(gaddr argb) {
  const NjArgb *src = (const NjArgb *)hw_ptr(argb);
  NjArgb *dst = &g_nj_cmaterial;
  dst->a = src->a;
  dst->r = src->r;
  dst->g = src->g;
  dst->b = src->b;
}
HW_EXPORT(8c0a967c, nj_set_constant_material(ARG0))

/* was func_8C0A4488 (njInitTextureBuffer): the texture loading buffer and its size, rounded down to 2 KB */
void nj_init_texture_buffer(gaddr buffer, uint32_t size) {
  g_nj_texbuf_addr = buffer;
  g_nj_texbuf_size = (size >> 11) << 11;
}
HW_EXPORT(8c0a4488, nj_init_texture_buffer(ARG0, ARG1))

/* was func_8C0A2544 (a thunk): njSetCheapShadowMode (Kamui, 0x8C12197C, one argument: the shadow intensity),
 * with the registers as they came; its r0 comes back unchanged */
static inline uint32_t njSetCheapShadowMode(uint32_t mode) { return CALL1(0x8C12197Cu, mode); }
HW_EXPORT(8c0a2544, RET(njSetCheapShadowMode(ARG0)))
