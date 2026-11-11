/* Pen Pen port, S11: the Katana SDK functions the hand-written game code calls.
 *
 * The SDK stays the original's (run as translated guest code) or the runtime's HLE (port/runtime/hle.c); the game
 * calls it through hw_call at the SDK function's address, exactly as the original's jsr did. Names and addresses
 * come from port/symbols/sdk_calls.tsv (S2 census); the argument counts from the SDK's public API and from the call
 * sites. */
#ifndef PENPEN_SDK_H
#define PENPEN_SDK_H

#include "hw.h"

/* ---- Shinobi (sy) ---- */
static inline gaddr syMalloc(uint32_t size) { return CALL1(0x8C09F4D6u, size); }
static inline void syFree(gaddr block) { CALL1(0x8C09F588u, block); }
static inline void syBtExit(void) { CALL0(0x8C0A10D0u); }          /* back to the Dreamcast's boot menu */
static inline void syCfgInit(gaddr work) { CALL1(0x8C0A0E84u, work); }
static inline void sy_Nop1(void) { CALL0(0x8C09FC94u); }

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
static inline void njGetMatrix(gaddr m) { CALL1(0x8C0C3230u, m); }
static inline void njInvertMatrix(gaddr m) { CALL1(0x8C0C3328u, m); }

/* ---- Ninja maths and drawing (S13). Float arguments in FR4..., a float result in FR0 (include/hw.h) ---- */
static inline f32 njScalor(gaddr v) { CALL1(0x8C0C487Cu, v); return hw_fret(); }       /* |v| */
static inline f32 njScalor2(gaddr v) { CALL1(0x8C0C4868u, v); return hw_fret(); }      /* |v|^2 */
static inline f32 njInnerProduct(gaddr a, gaddr b) { CALL2(0x8C0C3308u, a, b); return hw_fret(); }
static inline f32 njSqrt(f32 x) { FARGS1(x); CALL0(0x8C0C4BD0u); return hw_fret(); }
static inline f32 njUnitVector(gaddr v) { CALL1(0x8C0C4A4Cu, v); return hw_fret(); }  /* normalises v in place */
static inline f32 njSin(int32_t angle) { CALL1(0x8C0C4BC4u, angle); return hw_fret(); }  /* Ninja angle units */
static inline f32 njCos(int32_t angle) { CALL1(0x8C0C4A70u, angle); return hw_fret(); }
/* the matrix functions take the matrix to change (0 = Ninja's current matrix) */
static inline void njPushMatrix(gaddr m) { CALL1(0x8C0C3D90u, m); }
static inline void njPopMatrix(int32_t n) { CALL1(0x8C0C3D4Cu, n); }
static inline void njUnitMatrix(gaddr m) { CALL1(0x8C0C49E0u, m); }
static inline void njSetMatrix(gaddr m, gaddr src) { CALL2(0x8C0C4890u, m, src); }
static inline void njMultiMatrix(gaddr m, gaddr m2) { CALL2(0x8C0C3B30u, m, m2); }
static inline void njTranslateV(gaddr m, gaddr v) { CALL2(0x8C0C4904u, m, v); }
static inline void njTranslate(gaddr m, f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL1(0x8C0C4978u, m); }
static inline void njScale(gaddr m, f32 x, f32 y, f32 z) { FARGS3(x, y, z); CALL1(0x8C0C47BCu, m); }
static inline void njRotateX(gaddr m, int32_t a) { CALL2(0x8C0C3FE8u, m, a); }             /* Ninja angle units */
static inline void njRotateY(gaddr m, int32_t a) { CALL2(0x8C0C407Cu, m, a); }
static inline void njRotateZ(gaddr m, int32_t a) { CALL2(0x8C0C42ECu, m, a); }
static inline void njRotateXYZ(gaddr m, int32_t x, int32_t y, int32_t z) { CALL4(0x8C0C3E14u, m, x, y, z); }
static inline void njCalcPoint(gaddr m, gaddr src, gaddr dst) { CALL3(0x8C0C3168u, m, src, dst); }
static inline void njControl3D(uint32_t flags) { CALL1(0x8C0A966Cu, flags); }
static inline void njSetConstantMaterial(gaddr argb) { CALL1(0x8C0A967Cu, argb); }
static inline void njFogEnable(void) { CALL0(0x8C0C6CC0u); }
static inline void njFogDisable(void) { CALL0(0x8C0C6C90u); }
static inline int32_t njSetTexture(gaddr texlist) { return (int32_t)CALL1(0x8C0A4540u, texlist); }
static inline void njReleaseTexture(gaddr texlist) { CALL1(0x8C0A441Cu, texlist); }
static inline void njDrawSprite2D(gaddr sprite, int32_t n, f32 pri, uint32_t attr) { FARGS1(pri); CALL3(0x8C0C60ECu, sprite, n, attr); }
static inline void njDrawSprite3D(gaddr sprite, int32_t n, uint32_t attr) { CALL3(0x8C0C61ACu, sprite, n, attr); }
static inline void njDrawPolygon(gaddr poly, int32_t n, int32_t trans) { CALL3(0x8C0C5E90u, poly, n, trans); }
/* the chunk-model and shape-motion drawers (S2 names) */
static inline void nj_ModelL1(gaddr model) { CALL1(0x8C0D0312u, model); }
static inline void nj_ShapeMotionL1(gaddr object, gaddr motion, gaddr shape, f32 frame) { FARGS1(frame); CALL3(0x8C0CC98Eu, object, motion, shape); }
/* reads a Ninja chunk file image already in memory (S2 name): the data, then two out-words (sizes) */
static inline gaddr nj_ReadChunkFile(gaddr data, gaddr out1, gaddr out2) { return CALL3(0x8C0CF790u, data, out1, out2); }

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
