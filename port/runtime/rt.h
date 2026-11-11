/* Pen Pen port, S3: boot runtime shared declarations (native and wasm32). */
#ifndef RT_H
#define RT_H

#include "../recomp/rt_gen.h"

/* host interface (retrowser spec 4.10; native: disc_native.c, wasm: the host / node/run.mjs) */
#ifdef __wasm__
#define RTW_IMPORT(n) __attribute__((import_module("env"), import_name(n)))
#else
#define RTW_IMPORT(n)
#endif
RTW_IMPORT("rtw_disc_toc") int rtw_disc_toc(void *entries, int max);
RTW_IMPORT("rtw_disc_read") int rtw_disc_read(int track, int lba, int count, void *dst);
RTW_IMPORT("rtw_log") void rtw_log(const char *p, int len);

/* logging (no stdio in the wasm build): %d %u %x %08x %s %c */
void rt_log(const char *fmt, ...);
__attribute__((noreturn)) void rt_stop(const char *why);

/* disc + ISO9660 (disc.c) */
int disc_init(void);
int disc_read_lba(uint32_t lba, uint32_t n, void *dst); /* 2048-byte sectors of the data tracks */
int disc_find(const char *name, uint32_t *lba, uint32_t *size);
int disc_file_count(void);

/* HLE (hle.c) */
typedef void (*hle_fn)(SH4 *c);
hle_fn hle_find(uint32_t a);
void hle_init(SH4 *c);
void hle_summary(void);
extern int hle_max_frames;
extern int hle_verbose;

/* guest helpers */
extern SH4 *rt_cpu;
void rt_guest_call(SH4 *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
void rt_summary(void);
#define GUEST_SCRATCH 0x8C001000u /* the BIOS work area (no BIOS runs): HLE structures */

#endif
