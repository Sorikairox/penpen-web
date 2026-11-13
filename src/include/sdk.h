/* Pen Pen port, S11: the Katana SDK functions the hand-written game code calls.
 *
 * The SDK stays the original's (run as translated guest code) or the runtime's HLE (port/runtime/hle.c); the game
 * calls it through hw_call at the SDK function's address, exactly as the original's jsr did. Names and addresses
 * come from port/symbols/sdk_calls.tsv (S2 census); the argument counts from the SDK's public API and from the call
 * sites. */
#ifndef PENPEN_SDK_H
#define PENPEN_SDK_H

#include "hw.h"
/* S20: the SDK functions that are hand-written C (src/lib/) are called as C; the rest through hw_call */
#include "../lib/sdk_c.h"

/* ---- Shinobi (sy) ---- */
static inline gaddr syMalloc(uint32_t size) { return sy_malloc(size); }
static inline void syFree(gaddr block) { sy_free(block); }
static inline void syBtExit(void) { CALL0(0x8C0A10D0u); }          /* back to the Dreamcast's boot menu */
static inline void syCfgInit(gaddr work) { CALL1(0x8C0A0E84u, work); }
static inline void sy_Nop1(void) {} /* the library's empty hook (src/lib/sy/heap.c sy_nop1) */

/* ---- peripherals (pd) ---- */
static inline gaddr pdGetPeripheral(uint32_t port) { return CALL1(0x8C09F25Au, port); } /* -> PDS_PERIPHERAL */

/* ---- Ninja (nj) ---- */
static inline void njWaitVSync(void) { CALL0(0x8C0A1E30u); }       /* the frame ends here (the runtime yields) */
static inline void njSetVSyncFunction(gaddr fn) { CALL1(0x8C0A1FB4u, fn); }
static inline void njSetEORFunction(gaddr fn) { CALL1(0x8C0A1E0Eu, fn); }
static inline void njSetBackColor(uint32_t c1, uint32_t c2, uint32_t c3) { CALL3(0x8C0A1FEAu, c1, c2, c3); }
static inline void njSetBorderColor(uint32_t color) { CALL1(0x8C0A2990u, color); }
static inline void njSetFogColor(uint32_t color) { CALL1(0x8C0C6F68u, color); }
static inline void njSetFogTable(gaddr table) { CALL1(0x8C0C6F84u, table); }
static inline void njClip2D(int32_t mode, gaddr rect) { CALL2(0x8C0A254Au, mode, rect); }
static inline void njGetMatrix(gaddr m) { nj_get_matrix(m); }
static inline void njInvertMatrix(gaddr m) { nj_invert_matrix(m); }

/* ---- Ninja maths and drawing (S13). Float arguments in FR4..., a float result in FR0 (include/hw.h) ---- */
static inline f32 njScalor(gaddr v) { return nj_scalor(v); }       /* |v| */
static inline f32 njScalor2(gaddr v) { return nj_scalor2(v); }      /* |v|^2 */
static inline f32 njInnerProduct(gaddr a, gaddr b) { return nj_inner_product(a, b); }
static inline f32 njSqrt(f32 x) { return nj_sqrt(x); }
static inline f32 njUnitVector(gaddr v) { return nj_unit_vector(v); }  /* normalises v in place */
static inline f32 njSin(int32_t angle) { return nj_sin(angle); }  /* Ninja angle units */
static inline f32 njCos(int32_t angle) { return nj_cos(angle); }
/* the matrix functions take the matrix to change (0 = Ninja's current matrix) */
static inline void njPushMatrix(gaddr m) { nj_push_matrix(m); }
static inline void njPopMatrix(int32_t n) { nj_pop_matrix(n); }
static inline void njUnitMatrix(gaddr m) { nj_unit_matrix(m); }
static inline void njSetMatrix(gaddr m, gaddr src) { nj_set_matrix(m, src); }
static inline void njMultiMatrix(gaddr m, gaddr m2) { nj_multi_matrix(m, m2); }
static inline void njTranslateV(gaddr m, gaddr v) { nj_translate_v(m, v); }
static inline void njTranslate(gaddr m, f32 x, f32 y, f32 z) { nj_translate(m, x, y, z); }
static inline void njScale(gaddr m, f32 x, f32 y, f32 z) { nj_scale(m, x, y, z); }
static inline void njRotateX(gaddr m, int32_t a) { nj_rotate_x(m, a); }             /* Ninja angle units */
static inline void njRotateY(gaddr m, int32_t a) { nj_rotate_y(m, a); }
static inline void njRotateZ(gaddr m, int32_t a) { nj_rotate_z(m, a); }
static inline void njRotateXYZ(gaddr m, int32_t x, int32_t y, int32_t z) { nj_rotate_xyz(m, x, y, z); }
static inline void njCalcPoint(gaddr m, gaddr src, gaddr dst) { nj_calc_point(m, src, dst); }
static inline void njControl3D(uint32_t flags) { nj_control_3d(flags); }
static inline void njSetConstantMaterial(gaddr argb) { nj_set_constant_material(argb); }
static inline void njFogEnable(void) { CALL0(0x8C0C6CC0u); }
static inline void njFogDisable(void) { CALL0(0x8C0C6C90u); }
static inline int32_t njSetTexture(gaddr texlist) { return nj_set_texture(texlist); }
static inline void njReleaseTexture(gaddr texlist) { CALL1(0x8C0A441Cu, texlist); }
static inline void njDrawSprite2D(gaddr sprite, int32_t n, f32 pri, uint32_t attr) { FARGS1(pri); CALL3(0x8C0C60ECu, sprite, n, attr); }
static inline void njDrawSprite3D(gaddr sprite, int32_t n, uint32_t attr) { CALL3(0x8C0C61ACu, sprite, n, attr); }
static inline void njDrawPolygon(gaddr poly, int32_t n, int32_t trans) { CALL3(0x8C0C5E90u, poly, n, trans); }
/* the chunk-model and shape-motion drawers (S2 names) */
static inline void nj_ModelL1(gaddr model) { CALL1(0x8C0D0312u, model); }
static inline void nj_ShapeMotionL1(gaddr object, gaddr motion, gaddr shape, f32 frame) { FARGS1(frame); CALL3(0x8C0CC98Eu, object, motion, shape); }
/* reads a Ninja chunk file image already in memory (S2 name): the data, then two out-words (sizes) */
static inline gaddr nj_ReadChunkFile(gaddr data, gaddr out1, gaddr out2) { return nj_read_chunk_file(data, out1, out2); }

/* ---- sound (Manatee sd) ---- */
static inline void sdSysServer(void) { CALL0(0x8C09AF54u); }        /* the sound driver's per-frame service */

/* ---- GD-ROM file system (gdFs) ---- */
static inline int32_t gdFs_Api58(void) { return (int32_t)CALL0(0x8C09EC64u); }  /* request a drive-status read */
static inline gaddr gdFs_Api34(void) { return CALL0(0x8C09EBF2u); }             /* the drive's handle */
static inline int32_t gdFsGetStat(gaddr handle) { return (int32_t)CALL1(0x8C09E864u, handle); }
static inline int32_t gdFsGetDrvStat(void) { return (int32_t)CALL0(0x8C09EC4Cu); }
enum { GDD_STAT_BUSY_HANDLE = 4 };                                  /* gdFsGetStat: the request is still running */
enum { GDD_DRVSTAT_PLAY = 3, GDD_DRVSTAT_OPEN = 6, GDD_DRVSTAT_NODISC = 7, GDD_DRVSTAT_ERROR = 9 };

/* ---- CRI middleware ---- */
static inline void mw_PreInit(void) { CALL0(0x8C09C280u); }

#endif
