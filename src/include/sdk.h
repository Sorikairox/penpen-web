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

/* ---- sound (Manatee sd) ---- */
static inline void sdSysServer(void) { CALL0(0x8C09AF54u); }        /* the sound driver's per-frame service */

/* ---- GD-ROM file system (gdFs) ---- */
static inline int32_t gdFs_Api58(void) { return (int32_t)CALL0(0x8C09EC64u); }  /* request a drive-status read */
static inline gaddr gdFs_Api34(void) { return CALL0(0x8C09EBF2u); }             /* the drive's handle */
static inline int32_t gdFsGetStat(gaddr handle) { return (int32_t)CALL1(0x8C09E864u, handle); }
static inline int32_t gdFsGetDrvStat(void) { return (int32_t)CALL0(0x8C09EC4Cu); }
enum { GDD_STAT_BUSY_HANDLE = 4 };                                  /* gdFsGetStat: the request is still running */
enum { GDD_DRVSTAT_OPEN = 6, GDD_DRVSTAT_NODISC = 7, GDD_DRVSTAT_ERROR = 9 };

/* ---- CRI middleware ---- */
static inline void mw_PreInit(void) { CALL0(0x8C09C280u); }

#endif
