/* Pen Pen port, S20: Ninja's memory copies and its chunk file reader (nj_ReadChunkFile: one chunk of a Ninja file
 * image already in memory, copied to a new block, its pointers relocated by the POF0 chunk that follows it). */
#include "hw.h"
#include "ninja_lib.h"
#include "../lib.h"

/* was func_8C0A1264 (nj_MemCopy4): copies `words` 32-bit words forward, one at a time (a count of 0 runs 2^32
 * times, as the original's decrement-and-test loop does) */
void nj_mem_copy4(gaddr dst, gaddr src, uint32_t words) {
  do {
    U32_AT(dst) = U32_AT(src);
    dst += 4;
    src += 4;
  } while (--words != 0);
}
HW_EXPORT(8c0a1264, nj_mem_copy4(ARG0, ARG1, ARG2))

/* was func_8C0A1284: the same, byte by byte */
void nj_mem_copy1(gaddr dst, gaddr src, uint32_t bytes) {
  do {
    GUEST_VAR(uint8_t, dst) = GUEST_VAR(uint8_t, src);
    dst += 1;
    src += 1;
  } while (--bytes != 0);
}
HW_EXPORT(8c0a1284, nj_mem_copy1(ARG0, ARG1, ARG2))

/* was func_8C0CF76A: the word with its four bytes reversed (chunk tags are stored big-endian) */
uint32_t nj_byte_swap32(uint32_t v) {
  return (v << 24) | ((v & 0x0000FF00u) << 8) | ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}
HW_EXPORT(8c0cf76a, RET(nj_byte_swap32(ARG0)))

/* the chunk tags (as nj_byte_swap32 makes them from the file's bytes) */
enum {
  CHUNK_NJIN = 0x4E4A494E, /* "NJIN" */
  CHUNK_NJCM = 0x4E4A434D, /* "NJCM": chunk model */
  CHUNK_NJBM = 0x4E4A424D, /* "NJBM": basic model */
  CHUNK_NJTL = 0x4E4A544C, /* "NJTL": texture list */
  CHUNK_NJLI = 0x4E4A4C49, /* "NJLI": light */
  CHUNK_NJCA = 0x4E4A4341, /* "NJCA": camera */
  CHUNK_NMDM = 0x4E4D444D, /* "NMDM": motion */
  CHUNK_NLIM = 0x4E4C494D, /* "NLIM": light motion */
  CHUNK_NCAM = 0x4E43414D, /* "NCAM": camera motion */
  CHUNK_NSSM = 0x4E53534D, /* "NSSM": shape motion */
  CHUNK_POF0 = 0x504F4630, /* "POF0": the pointer offsets of the chunk before it */
  CHUNK_NONE = 0x2A2A2A2A, /* "****": the tag reported for an unknown chunk */
};

/* the SDK's allocator (the original enters it through its second entry 0x8C09F4D2) */
static inline gaddr nj_malloc(uint32_t size) { return sy_malloc(size); }

/* a 4-byte field of the file, read byte by byte (it may be unaligned) in the CPU's byte order */
static uint32_t chunk_read32(gaddr src) {
  uint32_t v = 0;
  for (int i = 3; i >= 0; i--) v = (v << 8) | GUEST_VAR(uint8_t, src + (uint32_t)i);
  return v;
}

/* was func_8C0CF790 (nj_ReadChunkFile): reads the chunk at data + *offset_io: its tag goes to *type_out ("****" and
 * 0 returned for a tag it does not know), its body is copied to a new block (0 returned if the allocation fails).
 * A POF0 chunk right after it lists the block's pointer fields: each one that is not 0 gets the block's address
 * added. *offset_io advances past everything read (the tag after the body too, whatever it is). Returns the block.
 * The sizes are stored little-endian; the tags big-endian. */
gaddr nj_read_chunk_file(gaddr data, gaddr offset_io, gaddr type_out) {
  uint32_t *offset = (uint32_t *)hw_ptr(offset_io);
  uint32_t *type = (uint32_t *)hw_ptr(type_out);
  gaddr src = data + *offset;
  uint32_t tag = chunk_read32(src);
  src += 4;
  uint32_t size = chunk_read32(src);
  *offset += 8;
  src += 4;
  tag = nj_byte_swap32(tag);
  switch (tag) {
  case CHUNK_NJIN: case CHUNK_NJCM: case CHUNK_NJBM: case CHUNK_NJTL: case CHUNK_NJLI:
  case CHUNK_NJCA: case CHUNK_NMDM: case CHUNK_NLIM: case CHUNK_NCAM: case CHUNK_NSSM:
    *type = tag;
    break;
  default:
    *type = CHUNK_NONE;
    return 0;
  }

  gaddr block = nj_malloc(size);
  if (block == 0) return 0;
  nj_mem_copy1(block, src, size);
  *offset += size;
  src += size;

  /* the next chunk: only a POF0 is used */
  tag = chunk_read32(src);
  *offset += 4;
  src += 4;
  tag = nj_byte_swap32(tag);
  if (tag != CHUNK_POF0) return block;
  uint32_t left = chunk_read32(src); /* the POF0 body's size in bytes */
  src += 4;
  *offset += 4;

  /* the POF0 body: a run of steps (in words) from one pointer field to the next, 1, 2 or 4 bytes each by the top
   * two bits of the first byte (01: 6 bits, 10: 14 bits, 11: 30 bits, big-endian); 00 bytes are padding */
  uint32_t pos = 0;
  while (left > 0) {
    uint8_t b0 = GUEST_VAR(uint8_t, src);
    src += 1;
    *offset += 1;
    left -= 1;
    uint32_t step;
    switch (b0 & 0xC0) {
    case 0x00:
      continue;
    case 0x40:
      step = b0 & 0x3Fu;
      break;
    case 0x80: {
      uint8_t b1 = GUEST_VAR(uint8_t, src);
      src += 1;
      left -= 1;
      *offset += 1;
      step = ((uint32_t)(b0 & 0x3F) << 8) | b1;
      break;
    }
    default: { /* 0xC0 */
      uint8_t b1 = GUEST_VAR(uint8_t, src);
      src += 1;
      left -= 1;
      uint8_t b2 = GUEST_VAR(uint8_t, src);
      src += 1;
      left -= 1;
      uint8_t b3 = GUEST_VAR(uint8_t, src);
      src += 1;
      left -= 1;
      *offset += 3;
      step = ((uint32_t)(b0 & 0x3F) << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
      break;
    }
    }
    pos += step << 2;
    uint32_t *field = (uint32_t *)hw_ptr(block + pos);
    if (*field != 0) *field += block;
  }
  return block;
}
HW_EXPORT(8c0cf790, RET(nj_read_chunk_file(ARG0, ARG1, ARG2)))
