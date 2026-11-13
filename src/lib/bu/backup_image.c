/* Pen Pen port, S20: the backup library's file image (buMakeBackupFileImage): a VMS save file built from the game's
 * description of it (the header with its comments and application name, the icon palette and frames, an optional
 * eye-catch picture, the save data, padded to whole 512-byte blocks, the header's CRC over it all), and the
 * library's thunks the game calls (bu, pd, gdFs, the CRI middleware: each jumps to the library function the runtime
 * replaces). */
#include "../lib.h"

/* the game's description of the file (Katana's BUS_FILEINFO) */
typedef struct BuFileInfo {
  char vms_comment[0x12];   /* 0x00: 16 characters shown by the VMU (the rest: padding) */
  char btr_comment[0x22];   /* 0x12: 32 characters shown by the Dreamcast's file manager */
  char app_name[0x10];      /* 0x34 */
  gaddr icon_palette;       /* 0x44: 16 colours (32 bytes) */
  gaddr icon_data;          /* 0x48 */
  uint16_t icon_frames;     /* 0x4C: 1-3 */
  uint16_t icon_speed;      /* 0x4E */
  gaddr eyecatch_data;      /* 0x50 */
  uint16_t eyecatch_type;   /* 0x54: 0 none, 1-3 */
  uint16_t _56;
  gaddr save_data;          /* 0x58 */
  uint32_t save_size;       /* 0x5C */
} BuFileInfo;
HW_OFFSET(BuFileInfo, icon_palette, 0x44);
HW_OFFSET(BuFileInfo, icon_frames, 0x4C);
HW_OFFSET(BuFileInfo, eyecatch_type, 0x54);
HW_OFFSET(BuFileInfo, save_size, 0x5C);

/* the VMS file header */
typedef struct BuFileHeader {
  char vms_comment[0x10];   /* 0x00: space-padded */
  char btr_comment[0x20];   /* 0x10: space-padded */
  char app_name[0x10];      /* 0x30: zero-padded */
  uint16_t icon_frames;     /* 0x40 */
  uint16_t icon_speed;      /* 0x42 */
  uint16_t eyecatch_type;   /* 0x44 */
  uint16_t crc;             /* 0x46 */
  uint32_t save_size;       /* 0x48 */
  uint8_t _4c[0x14];
  uint8_t icon_palette[0x20]; /* 0x60 */
} BuFileHeader;
HW_OFFSET(BuFileHeader, icon_frames, 0x40);
HW_OFFSET(BuFileHeader, crc, 0x46);
HW_OFFSET(BuFileHeader, save_size, 0x48);
HW_OFFSET(BuFileHeader, icon_palette, 0x60);
HW_SIZE(BuFileHeader, 0x80);

#define g_icon_bytes     ((const uint32_t *)hw_ptr(0x8C181AF8u)) /* the icon data's size by frame count (& 3) */
#define g_eyecatch_bytes ((const uint32_t *)hw_ptr(0x8C181B08u)) /* the eye-catch's size by type (& 3) */
#define BU_BLOCK         0x200u
#define BU_BAD_INFO      (-0x100)

/* was func_8C0A0E38: the CRC of the VMS header (polynomial 0x1021, from 0): each byte sign-extended into bits 8-31 of a
 * 32-bit register, eight shifts; the register's upper half is not masked (only the low 16 bits are stored) */
uint32_t bu_crc(gaddr data, uint32_t n) {
  const int8_t *p = hw_ptr(data);
  uint32_t crc = 0;
  for (uint32_t i = 0; i < n; i++) {
    crc ^= (uint32_t)(int32_t)p[i] << 8;
    for (int bit = 0; bit < 8; bit++) crc = (crc & 0x8000u) ? (crc << 1) ^ 0x1021u : crc << 1;
  }
  return crc;
}
HW_EXPORT(8c0a0e38, RET(bu_crc(ARG0, ARG1)))

/* a string of the description into the header: up to max characters, to its terminating zero */
static void copy_text(char *dst, const char *src, int32_t max) {
  for (int32_t i = 0; src[i] != 0;) {
    dst[i] = src[i];
    if (++i >= max) break;
  }
}

/* was buMakeBackupFileImage (func_8C0A0AA0): the file image of info at image; returns its size in 512-byte blocks,
 * or -256 when the description is not usable (no icon, more than 3 frames, an eye-catch type above 3 or without its
 * data) */
int32_t bu_make_file_image(gaddr image, gaddr info_addr) {
  const BuFileInfo *info = hw_ptr(info_addr);
  BuFileHeader *h = hw_ptr(image);
  if ((int16_t)info->icon_frames == 0 || info->icon_frames > 3 || info->eyecatch_type > 3 || !info->icon_palette ||
      !info->icon_data || ((int16_t)info->eyecatch_type != 0 && !info->eyecatch_data))
    return BU_BAD_INFO;
  rt_memset(image, 0, 0x60);
  rt_memset(image + offsetof(BuFileHeader, vms_comment), ' ', sizeof h->vms_comment);
  rt_memset(image + offsetof(BuFileHeader, btr_comment), ' ', sizeof h->btr_comment);
  rt_memset(image + offsetof(BuFileHeader, app_name), 0, sizeof h->app_name);
  copy_text(h->vms_comment, info->vms_comment, sizeof h->vms_comment);
  copy_text(h->btr_comment, info->btr_comment, sizeof h->btr_comment);
  copy_text(h->app_name, info->app_name, sizeof h->app_name);

  uint32_t icon_bytes = g_icon_bytes[info->icon_frames & 3];
  uint32_t eyecatch_bytes = g_eyecatch_bytes[info->eyecatch_type & 3];
  uint32_t size = info->save_size + icon_bytes + eyecatch_bytes + sizeof(BuFileHeader);
  h->icon_speed = info->icon_speed;
  h->icon_frames = info->icon_frames;
  /* the parts after the header: the icon frames, the eye-catch, the save data */
  uint32_t icon_at = sizeof(BuFileHeader), eyecatch_at, data_at;
  if ((int16_t)info->eyecatch_type == 0 || !info->eyecatch_data) {
    h->eyecatch_type = 0;
    eyecatch_at = 0;
  } else {
    h->eyecatch_type = info->eyecatch_type;
    eyecatch_at = icon_bytes + sizeof(BuFileHeader);
  }
  if (!info->save_size || !info->save_data) {
    h->save_size = 0;
    data_at = 0;
  } else {
    h->save_size = info->save_size;
    data_at = icon_bytes + eyecatch_bytes + sizeof(BuFileHeader);
  }
  if (icon_bytes) {
    rt_memmove(image + offsetof(BuFileHeader, icon_palette), info->icon_palette, sizeof h->icon_palette);
    rt_memmove(image + icon_at, info->icon_data, icon_bytes);
  }
  if (eyecatch_bytes) rt_memmove(image + eyecatch_at, info->eyecatch_data, eyecatch_bytes);
  if (info->save_size) rt_memmove(image + data_at, info->save_data, info->save_size);
  uint32_t partial = size & (BU_BLOCK - 1);
  if (partial) rt_memset(image + size, 0, BU_BLOCK - partial);
  ((BuFileHeader *)hw_ptr(image))->crc = (uint16_t)bu_crc(image, size);
  return (int32_t)(size >> 9) + (partial ? 1 : 0);
}
HW_EXPORT(8c0a0aa0, RET(bu_make_file_image(ARG0, ARG1)))

/* ---- the thunks: each jumps to a library function the runtime replaces (port/runtime/hle_io.c, hle.c) ---- */
/* was func_8C0A00DA: buSetCompleteCallback (0x8C0DEA7C) */
uint32_t bu_set_complete_callback(gaddr fn) { return CALL1(0x8C0DEA7Cu, fn); }
HW_EXPORT(8c0a00da, RET(bu_set_complete_callback(ARG0)))
/* was func_8C0A00E0: buSetProgressCallback (0x8C0DEA98) */
uint32_t bu_set_progress_callback(gaddr fn) { return CALL1(0x8C0DEA98u, fn); }
HW_EXPORT(8c0a00e0, RET(bu_set_progress_callback(ARG0)))
/* was func_8C0A00F2: bu_IsReady (0x8C0DF09A) */
uint32_t bu_is_ready(int32_t drive) { return CALL1(0x8C0DF09Au, drive); }
HW_EXPORT(8c0a00f2, RET(bu_is_ready((int32_t)ARG0)))
/* was func_8C0A019C: bu_IsFormatted (0x8C0E12DE) */
uint32_t bu_is_formatted(int32_t drive) { return CALL1(0x8C0E12DEu, drive); }
HW_EXPORT(8c0a019c, RET(bu_is_formatted((int32_t)ARG0)))
/* was func_8C0A10F4: pdVibMxIsReady (0x8C0E3268) */
uint32_t pd_vib_is_ready(int32_t port) { return CALL1(0x8C0E3268u, port); }
HW_EXPORT(8c0a10f4, RET(pd_vib_is_ready((int32_t)ARG0)))
/* was func_8C09D4F2: gdFsInit (0x8C09EAD8): max open files, work area, max directory entries, directory buffer */
uint32_t gd_fs_init(uint32_t max_open, gaddr work, uint32_t max_dirent, gaddr dirbuf) {
  return CALL4(0x8C09EAD8u, max_open, work, max_dirent, dirbuf);
}
HW_EXPORT(8c09d4f2, RET(gd_fs_init(ARG0, ARG1, ARG2, ARG3)))
/* was func_8C09AFD8 / func_8C09AFDE / func_8C09AFE4: the CRI middleware's mw_ExecVsync, mw_SetErrFunc (callback, its
 * argument) and mw_ExecServer, the movie scene's (game/good_night.c) */
uint32_t mw_exec_vsync(void) { return CALL0(0x8C0D3356u); }
HW_EXPORT(8c09afd8, RET(mw_exec_vsync()))
uint32_t mw_set_err_func(gaddr fn, gaddr arg) { return CALL2(0x8C0D30CEu, fn, arg); }
HW_EXPORT(8c09afde, RET(mw_set_err_func(ARG0, ARG1)))
uint32_t mw_exec_server(void) { return CALL0(0x8C0D31E0u); }
HW_EXPORT(8c09afe4, RET(mw_exec_server()))
