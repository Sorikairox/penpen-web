/* Pen Pen port, S18 b01: an inline Body setter of the staff roll's unit, and the fragments of this batch's core
 * region whose parent's hand-written C does their work (hw.h: HW_EXPORT_FRAGMENT). */
#include "hw.h"
#include "../core/vec.h"

/* was func_8C01E1A8: a Body's force = (x, y, z) (an inline method; roll_penguin_reset calls it). The original
 * stores x, y, z in that order (vec3_set: z first; distinct words, no other effect) and leaves no result. */
HW_EXPORT(8c01e1a8, vec3_set(FARG0, FARG1, FARG2, member_addr(ARG0, Body, force)))

/* func_8C011B1A: the rest of func_8C011AFC (core/view_layout.c, view_init_layout) after its first block copy: r14
 * the View, r4 the layout times 16, r5 the layout, r6 its clip entry; r14 and PR on its frame */
HW_EXPORT_FRAGMENT(8c011b1a, 8c011afc)
/* func_8C01E266: the rest of func_8C01E258 (core/inline_b02.c, material_set_e258) after the copy of the default
 * colour into its 0x10-byte local at r15: fr4-fr7 the colour, PR on its frame */
HW_EXPORT_FRAGMENT(8c01e266, 8c01e258)
/* func_8C015B52: the rest of func_8C015AA8 (core/sprite_pointer.c, sprite_pointer_draw) after its second njSin
 * call: fr0 the sine, fr12 / fr13 / fr15, r9 and r11-r14 its working values, its frame */
HW_EXPORT_FRAGMENT(8c015b52, 8c015aa8)
