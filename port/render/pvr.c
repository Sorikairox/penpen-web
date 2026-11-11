/* Pen Pen port, S5: PVR texture decoding (pvr.h). Our own code.
 * Layouts (RENDER.md 3): twiddled = Morton order with y in the even and x in the odd address bits, on
 * min(w, h) squares placed one after the other along the longer side; VQ = a codebook of 2x2-texel
 * entries (4 x 16 bits, texel order (0,0) (0,1) (1,0) (1,1)) and one index byte per 2x2 block, the
 * indices twiddled at half size; small VQ: a shorter codebook (by width); mipmaps stored smallest first
 * (16-bit twiddled: the 1x1 texel at byte 6; VQ: the 1x1 index at byte 0 after the codebook); YUV422:
 * horizontal texel pairs share U (first word's low byte) and V (second word's low byte), Y in the high
 * bytes. */
#include <string.h>
#include "pvr.h"

static uint32_t rd32le(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t rd16le(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

static uint32_t twiddle(uint32_t x, uint32_t y) {
  uint32_t r = 0;
  for (int i = 0; i < 11; i++) r |= ((y >> i) & 1u) << (2 * i) | ((x >> i) & 1u) << (2 * i + 1);
  return r;
}
/* texel index in a twiddled w x h image (rectangles: squares of min(w, h) in sequence) */
static uint32_t tw_index(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
  uint32_t m = w < h ? w : h;
  uint32_t blk = (x / m) + (y / m); /* only one of them is non-zero */
  return blk * m * m + twiddle(x % m, y % m);
}
static int log2i(uint32_t v) { int n = 0; while ((1u << n) < v) n++; return n; }

static uint32_t px16(uint32_t c, int fmt) {
  uint32_t r, g, b, a;
  switch (fmt) {
  case PVR_ARGB1555:
    a = (c & 0x8000) ? 255 : 0; r = (c >> 10) & 31; g = (c >> 5) & 31; b = c & 31;
    r = r << 3 | r >> 2; g = g << 3 | g >> 2; b = b << 3 | b >> 2; break;
  case PVR_RGB555:
    a = 255; r = (c >> 10) & 31; g = (c >> 5) & 31; b = c & 31;
    r = r << 3 | r >> 2; g = g << 3 | g >> 2; b = b << 3 | b >> 2; break;
  case PVR_RGB565:
    a = 255; r = (c >> 11) & 31; g = (c >> 5) & 63; b = c & 31;
    r = r << 3 | r >> 2; g = g << 2 | g >> 4; b = b << 3 | b >> 2; break;
  case PVR_ARGB4444:
    a = (c >> 12) & 15; r = (c >> 8) & 15; g = (c >> 4) & 15; b = c & 15;
    a |= a << 4; r |= r << 4; g |= g << 4; b |= b << 4; break;
  default: /* bump maps and others: show as grey */
    a = 255; r = g = b = (c >> 8) & 255; break;
  }
  return r | g << 8 | b << 16 | a << 24;
}
static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
static uint32_t yuv(int y, int u, int v) {
  /* ITU-R BT.601, full range as the PVR's converter */
  u -= 128; v -= 128;
  int r = y + (int)(1.375f * (float)v), g = y - (int)(0.6875f * (float)v + 0.34375f * (float)u), b = y + (int)(1.71875f * (float)u);
  return (uint32_t)clamp8(r) | (uint32_t)clamp8(g) << 8 | (uint32_t)clamp8(b) << 16 | 0xFF000000u;
}

static const uint32_t mip16[] = {0x6, 0x8, 0x10, 0x30, 0xB0, 0x2B0, 0xAB0, 0x2AB0, 0xAAB0, 0x2AAB0, 0xAAAB0};
static const uint32_t mipvq[] = {0x0, 0x1, 0x2, 0x6, 0x16, 0x56, 0x156, 0x556, 0x1556, 0x5556, 0x15556};

static int parse(const uint8_t *p, uint32_t avail, pvr_info *in, const uint8_t **data, uint32_t *dlen) {
  uint32_t off = 0;
  in->gbix = 0xFFFFFFFFu;
  if (avail >= 16 && !memcmp(p, "GBIX", 4)) {
    uint32_t n = rd32le(p + 4);
    if (n >= 4) in->gbix = rd32le(p + 8);
    off = 8 + n;
    while (off + 4 <= avail && memcmp(p + off, "PVRT", 4) && off < 8 + n + 16) off++; /* padding */
  }
  if (off + 16 > avail || memcmp(p + off, "PVRT", 4)) return -1;
  uint32_t size = rd32le(p + off + 4);
  in->pixfmt = p[off + 8]; in->datafmt = p[off + 9];
  in->w = (int)rd16le(p + off + 12); in->h = (int)rd16le(p + off + 14);
  in->mip = in->datafmt == PVR_TW_MM || in->datafmt == PVR_VQ_MM || in->datafmt == PVR_PAL4_MM ||
            in->datafmt == PVR_PAL8_MM || in->datafmt == PVR_SVQ_MM || in->datafmt == PVR_TW_MM2 ||
            in->datafmt == PVR_BMP_MM || in->datafmt == PVR_RECT_MM;
  in->used = off + 8 + size;
  *data = p + off + 16;
  *dlen = size >= 8 ? size - 8 : 0;
  if (off + 16 + *dlen > avail) return -2;
  if (in->w <= 0 || in->h <= 0 || in->w > 1024 || in->h > 1024) return -1;
  return 0;
}
int pvr_info_of(const uint8_t *p, uint32_t avail, pvr_info *in) {
  const uint8_t *d; uint32_t n;
  return parse(p, avail, in, &d, &n);
}

int pvr_decode(const uint8_t *p, uint32_t avail, uint32_t *rgba, uint32_t max, pvr_info *in) {
  const uint8_t *d; uint32_t dlen;
  int r = parse(p, avail, in, &d, &dlen);
  if (r) return r;
  uint32_t w = (uint32_t)in->w, h = (uint32_t)in->h;
  if (w * h > max) return -4;
  int pf = in->pixfmt, df = in->datafmt;
  if (df == PVR_PAL4 || df == PVR_PAL4_MM || df == PVR_PAL8 || df == PVR_PAL8_MM) return -3;
  if (df == PVR_VQ || df == PVR_VQ_MM || df == PVR_SVQ || df == PVR_SVQ_MM) {
    uint32_t cb = 256;
    if (df == PVR_SVQ) cb = w <= 16 ? 16 : w == 32 ? 32 : w == 64 ? 128 : 256;
    if (df == PVR_SVQ_MM) cb = w <= 16 ? 16 : w == 32 ? 64 : 256;
    const uint8_t *book = d, *idx = d + cb * 8;
    if (in->mip) idx += mipvq[log2i(w)];
    if ((uint32_t)(idx - d) + (w / 2) * (h / 2) > dlen) return -2;
    for (uint32_t y = 0; y < h; y++)
      for (uint32_t x = 0; x < w; x++) {
        uint32_t k = idx[tw_index(x / 2, y / 2, w / 2 ? w / 2 : 1, h / 2 ? h / 2 : 1)];
        if (k >= cb) k = 0;
        const uint8_t *e = book + k * 8 + ((x & 1) * 2 + (y & 1)) * 2;
        uint32_t c = rd16le(e);
        if (pf == PVR_YUV422) { /* pairs inside the 2x2 entry: (0,y) with (1,y) */
          const uint8_t *e0 = book + k * 8 + (y & 1) * 2, *e1 = e0 + 4;
          rgba[y * w + x] = yuv((int)(c >> 8), (int)(rd16le(e0) & 255), (int)(rd16le(e1) & 255));
        } else rgba[y * w + x] = px16(c, pf);
      }
    return 0;
  }
  int twid = df == PVR_TW || df == PVR_TW_MM || df == PVR_TW_RECT || df == PVR_TW_MM2;
  uint32_t base = 0, stride = w;
  if (in->mip) base = twid ? mip16[log2i(w)] : 0;
  if (df == PVR_STRIDE || df == PVR_RECT || df == PVR_BMP || df == PVR_BMP_MM || df == PVR_RECT_MM) twid = 0;
  uint32_t bpp = pf == PVR_ARGB8888 ? 4 : 2;
  if (base + w * h * bpp > dlen) {
    /* some files put the top level at the end only: fall back to the last w*h texels */
    if (w * h * bpp > dlen) return -2;
    base = dlen - w * h * bpp;
  }
  const uint8_t *t = d + base;
  for (uint32_t y = 0; y < h; y++)
    for (uint32_t x = 0; x < w; x++) {
      uint32_t i = twid ? tw_index(x, y, w, h) : y * stride + x;
      if (bpp == 4) {
        uint32_t c = rd32le(t + i * 4); /* ARGB8888 */
        rgba[y * w + x] = ((c >> 16) & 255) | (c & 0xFF00u) | (c & 255) << 16 | (c & 0xFF000000u);
      } else if (pf == PVR_YUV422) {
        uint32_t x0 = x & ~1u;
        uint32_t i0 = twid ? tw_index(x0, y, w, h) : y * stride + x0, i1 = twid ? tw_index(x0 + 1, y, w, h) : y * stride + x0 + 1;
        uint32_t w0 = rd16le(t + i0 * 2), w1 = rd16le(t + i1 * 2), c = rd16le(t + i * 2);
        rgba[y * w + x] = yuv((int)(c >> 8), (int)(w0 & 255), (int)(w1 & 255));
      } else rgba[y * w + x] = px16(rd16le(t + i * 2), pf);
    }
  return 0;
}
