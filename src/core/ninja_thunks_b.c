/* Pen Pen port, S17 b05: the thunks each translation unit of the scenery code keeps to a Ninja function (SHC emits
 * a `jmp` to the SDK per unit: njScalor2 for the inline |v|^2 of its Vec3 class, and two of the drawers), and one to
 * the quaternion's identity. Each runs the SDK function with the registers as they came. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"

/* Ninja drawers the thunks below jump to */
/* nj_ObjectB: draw a basic-model object tree */
static inline void nj_ObjectB(gaddr object) { CALL1(0x8C0C30A8u, object); }
/* nj_ShapeMotionL1b: the blend of two motions and two shape motions (links) at `rate` */
static inline void nj_ShapeMotionL1b(gaddr object, gaddr motion_link, gaddr shape_link, f32 rate) {
  FARGS1(rate);
  CALL3(0x8C0C94CAu, object, motion_link, shape_link);
}

/* was func_8C0387D2, func_8C03C398, func_8C03D974, func_8C040708, func_8C04229E, func_8C043C6C, func_8C044990,
 * func_8C044D1A, func_8C045664, func_8C045EF0, func_8C04661C, func_8C046960, func_8C0474BC, func_8C04772C,
 * func_8C047E50, func_8C0483A4, func_8C0486F4, func_8C048A24, func_8C0491D0, func_8C049788, func_8C04AAEC:
 * |v|^2 (njScalor2) */
HW_EXPORT(8c0387d2, RETF(njScalor2(ARG0)))
HW_EXPORT(8c03c398, RETF(njScalor2(ARG0)))
HW_EXPORT(8c03d974, RETF(njScalor2(ARG0)))
HW_EXPORT(8c040708, RETF(njScalor2(ARG0)))
HW_EXPORT(8c04229e, RETF(njScalor2(ARG0)))
HW_EXPORT(8c043c6c, RETF(njScalor2(ARG0)))
HW_EXPORT(8c044990, RETF(njScalor2(ARG0)))
HW_EXPORT(8c044d1a, RETF(njScalor2(ARG0)))
HW_EXPORT(8c045664, RETF(njScalor2(ARG0)))
HW_EXPORT(8c045ef0, RETF(njScalor2(ARG0)))
HW_EXPORT(8c04661c, RETF(njScalor2(ARG0)))
HW_EXPORT(8c046960, RETF(njScalor2(ARG0)))
HW_EXPORT(8c0474bc, RETF(njScalor2(ARG0)))
HW_EXPORT(8c04772c, RETF(njScalor2(ARG0)))
HW_EXPORT(8c047e50, RETF(njScalor2(ARG0)))
HW_EXPORT(8c0483a4, RETF(njScalor2(ARG0)))
HW_EXPORT(8c0486f4, RETF(njScalor2(ARG0)))
HW_EXPORT(8c048a24, RETF(njScalor2(ARG0)))
HW_EXPORT(8c0491d0, RETF(njScalor2(ARG0)))
HW_EXPORT(8c049788, RETF(njScalor2(ARG0)))
HW_EXPORT(8c04aaec, RETF(njScalor2(ARG0)))

/* was func_8C038A40: draw an object tree (nj_ObjectB) */
HW_EXPORT(8c038a40, nj_ObjectB(ARG0))

/* was func_8C04992C: the blended shape motion draw (nj_ShapeMotionL1b) */
HW_EXPORT(8c04992c, nj_ShapeMotionL1b(ARG0, ARG1, ARG2, FARG0))

/* was func_8C03A4C2: a thunk to func_8C038822 (a copy of the quaternion's identity, core/vec.c) */
HW_EXPORT(8c03a4c2, RET(quat_identity(ARG0)))
