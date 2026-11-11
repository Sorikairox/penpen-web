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
RTW_IMPORT("rtw_log") int rtw_log(const char *p, int len);

/* logging (no stdio in the wasm build): %d %u %x %08x %s %c */
void rt_log(const char *fmt, ...);
__attribute__((noreturn)) void rt_stop(const char *why);

/* disc + ISO9660 (disc.c) */
int disc_init(void);
int disc_read_lba(uint32_t lba, uint32_t n, void *dst); /* 2048-byte sectors of the data tracks */
int disc_find(const char *name, uint32_t *lba, uint32_t *size);
int disc_find_ex(const char *name, uint32_t *lba, uint32_t *size, int *index);
int disc_file_count(void);
int disc_track(int num, uint32_t *lba, uint32_t *count, int *flags); /* S7: CDDA tracks */

/* HLE (hle.c) */
typedef void (*hle_fn)(SH4 *c);
hle_fn hle_find(uint32_t a);  /* HLE function for a (counted), else 0 (guest code or unknown) */
int hle_known(uint32_t a);    /* a has an HLE function (not counted) */
uint32_t hle_tmu_count(void); /* the frame-driven TMU0 count (syTmrGetCount, TCNT0 reads) */
uint32_t hle_rtc(void);       /* S8: the RTC, seconds since 1950-01-01 local: the start value + the frames' time */
extern uint32_t hle_rtc_base;
int hle_set_pad_script(const char *s);
int hle_set_watch(const char *s);
extern int hle_digest_every, hle_frame_log;
uint32_t rt_interp_calls(void);
void hle_init(SH4 *c);
void hle_summary(void);
void hle_ring_dump(void);
uint32_t hle_frame(void);
extern int hle_max_frames;
extern int hle_verbose;
extern int hle_trace_lo, hle_trace_hi;
int hle_tracing(void);

/* guest helpers */
extern SH4 *rt_cpu;
void rt_guest_call(SH4 *c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
void rt_summary(void);
void rt_rew_done(void);   /* the yielding HLE was reached again by the rewind */
int rt_rw_depth(void);    /* deepest call chain saved so far */
/* S11: hand-written code (src/include/hw.h, hw.c) and its in-run differential test (difftest.c) */
extern int rt_sandbox, hwt_recording, hwt_enabled, rt_hle_depth;
void rt_unw_save_blob(uint32_t fn, int site, uint32_t t, const void *blob, int n);
int rt_rew_pop_blob(uint32_t fn, uint32_t *t, void *blob, int n);
void hwt_sandbox_call(SH4 *c, uint32_t a);
__attribute__((noreturn)) void hwt_sandbox_abort(int code, uint32_t at);
void hwt_before_hle(SH4 *c);
void hwt_record_hle(SH4 *c, uint32_t a);
uint32_t hwt_io_read(uint32_t a);
void hwt_record_io(uint32_t a, uint32_t v);
void hwt_io_write(uint32_t a, uint32_t v, int size);
int hwt_begin(SH4 *c, uint32_t addr);
void hwt_end(int sample);
void hwt_abandon(void);
void hwt_summary(void);
int hwt_set(const char *opt);
extern int hle_digest_nostack;
/* S11: the argument registers the running HLE function touched (bits 4-7); the difftest compares only those */
extern uint32_t hle_arg_mask;
static inline uint32_t *hle_reg(SH4 *c, int i) { hle_arg_mask |= 1u << i; return &c->r[i]; }
#define HLE_R(i) (*hle_reg(c, (i)))
#define GUEST_SCRATCH 0x8C001000u /* the BIOS work area (no BIOS runs): HLE structures */

#endif
