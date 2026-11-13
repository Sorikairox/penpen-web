/* Pen Pen port, S17 (b01): thunks the compiler left in front of two Ninja functions (a jump with the arguments as
 * they came): the squared length of a vector (njScalor2, one copy per unit that uses it) and the cross product. */
#include "hw.h"
#include "sdk.h"

/* out = a x b; FR0 passed on as Ninja leaves it (its Float result) */
static f32 njOuterProduct(gaddr a, gaddr b, gaddr out) { CALL3(0x8C0C3D04u, a, b, out); return hw_fret(); }

/* was func_8C010BE2 (and its copies below): |v|^2 */
HW_EXPORT(8c010be2, RETF(njScalor2(ARG0)))
HW_EXPORT(8c0122b4, RETF(njScalor2(ARG0)))
HW_EXPORT(8c013ac0, RETF(njScalor2(ARG0)))
HW_EXPORT(8c0151de, RETF(njScalor2(ARG0)))
HW_EXPORT(8c0162f6, RETF(njScalor2(ARG0)))
HW_EXPORT(8c016588, RETF(njScalor2(ARG0)))
HW_EXPORT(8c016b3a, RETF(njScalor2(ARG0)))

/* was func_8C013ABA: out = a x b */
HW_EXPORT(8c013aba, RETF(njOuterProduct(ARG0, ARG1, ARG2)))

static void njScaleV(gaddr m, gaddr v) { CALL2(0x8C0C470Cu, m, v); }

/* was func_8C013BB2: scale the current matrix by the vector v */
void matrix_scale_v(gaddr v) { njScaleV(0, v); }
HW_EXPORT(8c013bb2, matrix_scale_v(ARG0))
