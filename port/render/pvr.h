/* Pen Pen port, S5: PowerVR2 texture decoding (PVRT files and the PVM archives that hold them).
 * Our own code, from the public descriptions of the PVR texture formats (docs/port/RENDER.md 3). */
#ifndef PVR_H
#define PVR_H
#include <stdint.h>

/* pixel formats */
enum { PVR_ARGB1555 = 0, PVR_RGB565 = 1, PVR_ARGB4444 = 2, PVR_YUV422 = 3, PVR_BUMP = 4, PVR_RGB555 = 5,
       PVR_ARGB8888 = 6 };
/* data formats */
enum { PVR_TW = 1, PVR_TW_MM = 2, PVR_VQ = 3, PVR_VQ_MM = 4, PVR_PAL4 = 5, PVR_PAL4_MM = 6, PVR_PAL8 = 7,
       PVR_PAL8_MM = 8, PVR_RECT = 9, PVR_RECT_MM = 10, PVR_STRIDE = 11, PVR_TW_RECT = 13, PVR_BMP = 14,
       PVR_BMP_MM = 15, PVR_SVQ = 16, PVR_SVQ_MM = 17, PVR_TW_MM2 = 18 };

typedef struct {
  int w, h, pixfmt, datafmt, mip;
  uint32_t gbix;   /* global index (GBIX chunk), 0xFFFFFFFF if none */
  uint32_t used;   /* bytes of the GBIX + PVRT chunks */
} pvr_info;

/* Decode the texture at p (a GBIX or PVRT chunk) into rgba (w*h words, bytes R G B A; at most max words).
 * Only the top mipmap level is decoded (the backend builds the others). Returns 0, or < 0:
 * -1 not a texture, -2 truncated, -3 format not supported (palettised: needs the palette RAM), -4 too big. */
int pvr_decode(const uint8_t *p, uint32_t avail, uint32_t *rgba, uint32_t max, pvr_info *info);
/* The header only (same return codes, no pixels). */
int pvr_info_of(const uint8_t *p, uint32_t avail, pvr_info *info);

#endif
