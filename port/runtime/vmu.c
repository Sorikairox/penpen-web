/* Pen Pen port, S6: the VMU flash file system, on the 128 KiB image the host stores (retrowser save.kind
 * "vmu", the layout Flycast writes as vmu_save_A1.bin): 256 blocks of 512 bytes, little-endian words.
 *   block 255      system (root) block: 16 x 0x55 = formatted, colour, BCD time stamp, then the media
 *                  description at 0x40 (u16: last block 255, partition 0, root 255, FAT 254 / 1 block,
 *                  directory 253 / 13 blocks, icon, user blocks 200, 31, VMU game area 128)
 *   block 254      FAT: one u16 per block; 0xFFFC free, 0xFFFA last block of a chain, else the next block
 *   blocks 253-241 directory, a chain downwards: 13 x 16 entries of 32 bytes
 *                  (type 0x33 data / 0xCC game / 0 empty, copy flag, first block, 12-byte name, BCD time,
 *                  size in blocks, header offset in blocks)
 *   blocks 0-199   user blocks: data files are allocated from 199 downwards, a VMU game from 0 up
 *   blocks 200-240 unused
 * The bu HLE (hle_io.c) saves and loads the game's one file through these functions. */
#include <string.h>
#include "vmu.h"

uint8_t vmu_img[VMU_SIZE] __attribute__((aligned(16)));
uint32_t vmu_generation;

#define ROOT 255
#define FAT 254
#define DIR 253
#define DIR_BLOCKS 13
#define DIR_ENTRIES (DIR_BLOCKS * 16)
#define FREE 0xFFFCu
#define LAST 0xFFFAu

static uint8_t *blk(int b) { return vmu_img + b * VMU_BLOCK; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint16_t fat_get(int b) { return le16(blk(FAT) + 2 * b); }
static void fat_set(int b, uint32_t v) { put16(blk(FAT) + 2 * b, v); }
static uint8_t *dir_entry(int i) { return blk(DIR - i / 16) + (i % 16) * 32; }

const uint8_t *vmu_root(void) { return blk(ROOT); }

int vmu_blank(void) {
  for (int i = 0; i < VMU_SIZE; i++) if (vmu_img[i]) return 0;
  return 1;
}

int vmu_formatted(void) {
  for (int i = 0; i < 16; i++) if (blk(ROOT)[i] != 0x55) return 0;
  return 1;
}

void vmu_format(const uint8_t bcd[8]) {
  memset(vmu_img, 0, VMU_SIZE);
  uint8_t *r = blk(ROOT);
  memset(r, 0x55, 16);
  memcpy(r + 0x30, bcd, 8);
  static const uint16_t media[] = {255, 0, 255, FAT, 1, DIR, DIR_BLOCKS, 0, VMU_USER_BLOCKS, 31, VMU_EXEC_BLOCKS};
  for (unsigned i = 0; i < sizeof media / sizeof media[0]; i++) put16(r + 0x40 + 2 * i, media[i]);
  for (int b = 0; b < 256; b++) fat_set(b, FREE);
  fat_set(ROOT, LAST);
  fat_set(FAT, LAST);
  for (int b = DIR; b > DIR - DIR_BLOCKS + 1; b--) fat_set(b, (uint32_t)(b - 1));
  fat_set(DIR - DIR_BLOCKS + 1, LAST);
}

int vmu_free_blocks(void) {
  int n = 0;
  for (int b = 0; b < VMU_USER_BLOCKS; b++) n += fat_get(b) == FREE;
  return n;
}

int vmu_free_exec(void) {
  int n = 0;
  while (n < VMU_EXEC_BLOCKS && fat_get(n) == FREE) n++;
  return n;
}

static int name_eq(const uint8_t *e, const char *name) {
  for (int i = 0; i < 12; i++) {
    uint8_t want = (uint8_t)name[i];
    if (e[4 + i] != want) return 0;
    if (!want) return 1;
  }
  return 1;
}

int vmu_find(const char *name) {
  for (int i = 0; i < DIR_ENTRIES; i++) {
    uint8_t *e = dir_entry(i);
    if ((e[0] == 0x33 || e[0] == 0xCC) && name_eq(e, name)) return i;
  }
  return -1;
}

int vmu_file_blocks(int entry) { return le16(dir_entry(entry) + 0x18); }

int vmu_read(int entry, uint8_t *dst, int max_blocks) {
  const uint8_t *e = dir_entry(entry);
  int n = le16(e + 0x18), b = le16(e + 2), k = 0;
  for (; k < n && k < max_blocks && b < 256; k++) {
    memcpy(dst + k * VMU_BLOCK, blk(b), VMU_BLOCK);
    uint32_t nx = fat_get(b);
    if (nx == LAST) { k++; break; }
    b = (int)nx;
  }
  return k;
}

static void free_chain(int b) {
  for (int guard = 0; b < 256 && guard < 256; guard++) {
    uint32_t nx = fat_get(b);
    fat_set(b, FREE);
    if (nx == LAST || nx == FREE) break;
    b = (int)nx;
  }
}

int vmu_write(const char *name, const uint8_t *data, int blocks, const uint8_t bcd[8]) {
  if (blocks <= 0 || blocks > VMU_USER_BLOCKS) return -1;
  int e = vmu_find(name);
  /* the room the file needs, counting the blocks of the copy it replaces */
  int avail = vmu_free_blocks() + (e >= 0 ? vmu_file_blocks(e) : 0);
  if (avail < blocks) return -1;
  if (e < 0) {
    for (int i = 0; i < DIR_ENTRIES && e < 0; i++) if (dir_entry(i)[0] == 0) e = i;
    if (e < 0) return -2;
  } else {
    free_chain(le16(dir_entry(e) + 2));
  }
  /* allocate from the top of the user area down, chained in that order */
  int first = -1, prev = -1, done = 0;
  for (int b = VMU_USER_BLOCKS - 1; b >= 0 && done < blocks; b--) {
    if (fat_get(b) != FREE) continue;
    memcpy(blk(b), data + done * VMU_BLOCK, VMU_BLOCK);
    if (prev >= 0) fat_set(prev, (uint32_t)b); else first = b;
    fat_set(b, LAST);
    prev = b;
    done++;
  }
  uint8_t *d = dir_entry(e);
  memset(d, 0, 32);
  d[0] = 0x33; /* data file */
  d[1] = 0x00; /* copy allowed */
  put16(d + 2, (uint32_t)first);
  for (int i = 0; i < 12 && name[i]; i++) d[4 + i] = (uint8_t)name[i];
  memcpy(d + 0x10, bcd, 8);
  put16(d + 0x18, (uint32_t)blocks);
  put16(d + 0x1A, 0); /* the VMS header is the file's first block */
  vmu_generation++;
  return 0;
}

int vmu_check(void) {
  if (!vmu_formatted()) return -1;
  if (fat_get(ROOT) != LAST || fat_get(FAT) != LAST) return -2;
  int used[256] = {0};
  for (int i = 0; i < DIR_ENTRIES; i++) {
    const uint8_t *e = dir_entry(i);
    if (e[0] != 0x33 && e[0] != 0xCC) continue;
    int n = le16(e + 0x18), b = le16(e + 2), k = 0;
    while (k < n) {
      if (b >= VMU_USER_BLOCKS || used[b]) return -3;
      used[b] = 1;
      k++;
      uint32_t nx = fat_get(b);
      if (nx == LAST) break;
      if (nx == FREE) return -4;
      b = (int)nx;
    }
    if (k != n) return -5;
  }
  for (int b = 0; b < VMU_USER_BLOCKS; b++) if (!used[b] && fat_get(b) != FREE) return -6;
  return 0;
}
