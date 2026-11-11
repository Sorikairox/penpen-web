/* Pen Pen port, S3: the disc as the game sees it, over the retrowser disc interface (rtw_disc_toc /
 * rtw_disc_read: 2048-byte user data per data sector, LBAs of a .gdi, high-density area from LBA 45000).
 * ISO9660: the primary volume descriptor is at LBA 45016 (track 3); the directory records point into the
 * last data track (track 48 on this disc). One flat root directory. */
#include "rt.h"

#define HD_BASE 45000u
#define MAXT 99
static int32_t toc[MAXT][4];
static int ntoc;

int disc_init(void) {
  ntoc = rtw_disc_toc(toc, MAXT);
  if (ntoc < 0) return -1;
  if (ntoc > MAXT) ntoc = MAXT;
  int data = 0;
  for (int i = 0; i < ntoc; i++) data += toc[i][3] & 1;
  rt_log("boot disc tracks %d data %d", ntoc, data);
  return ntoc;
}

int disc_read_lba(uint32_t lba, uint32_t n, void *dst) {
  uint8_t *d = dst;
  while (n) {
    int t = -1;
    for (int i = 0; i < ntoc; i++)
      if ((toc[i][3] & 1) && lba >= (uint32_t)toc[i][1] && lba < (uint32_t)(toc[i][1] + toc[i][2])) t = i;
    if (t < 0) return -2;
    uint32_t k = n > 256 ? 256 : n;
    int r = rtw_disc_read(toc[t][0], (int)lba, (int)k, d);
    if (r <= 0) return -3;
    lba += (uint32_t)r; n -= (uint32_t)r; d += (uint32_t)r * 2048;
  }
  return 0;
}

/* root directory, cached */
static uint8_t dirbuf[64 * 2048];
static uint32_t dirlen;

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int load_root(void) {
  static uint8_t pvd[2048];
  if (dirlen) return 0;
  if (disc_read_lba(HD_BASE + 16, 1, pvd)) return -1;
  if (pvd[0] != 1 || pvd[1] != 'C' || pvd[2] != 'D' || pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1') return -2;
  uint32_t lba = le32(pvd + 156 + 2), size = le32(pvd + 156 + 10);
  if (size > sizeof dirbuf) size = sizeof dirbuf;
  if (disc_read_lba(lba, (size + 2047) / 2048, dirbuf)) return -3;
  dirlen = size;
  return 0;
}

static int upper(int ch) { return ch >= 'a' && ch <= 'z' ? ch - 32 : ch; }

/* walk the root directory; name == NULL counts the files */
static int walk(const char *name, uint32_t *lba, uint32_t *size) {
  if (load_root()) return -1;
  int count = 0;
  for (uint32_t p = 0; p < dirlen;) {
    uint32_t len = dirbuf[p];
    if (!len) { p = (p / 2048 + 1) * 2048; continue; }
    const uint8_t *r = dirbuf + p;
    uint32_t nl = r[32];
    const char *nm = (const char *)r + 33;
    p += len;
    if (nl == 1 && (nm[0] == 0 || nm[0] == 1)) continue;
    if (r[25] & 2) continue; /* directory */
    count++;
    if (!name) continue;
    uint32_t i = 0;
    for (; i < nl && nm[i] != ';' && name[i]; i++)
      if (upper(nm[i]) != upper(name[i])) break;
    if ((i == nl || nm[i] == ';') && !name[i]) { *lba = le32(r + 2); *size = le32(r + 10); return 0; }
  }
  return name ? -4 : count;
}

int disc_find(const char *name, uint32_t *lba, uint32_t *size) { return walk(name, lba, size); }
int disc_file_count(void) { return walk(0, 0, 0); }
