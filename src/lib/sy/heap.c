/* Pen Pen port, S20: Shinobi's heap (syMallocInit, syMalloc, syFree), the cable-type port and the BIOS system-call
 * stub. The heap is a circular free list of blocks in 32-byte units, kept bit-exact because the game's pointers come
 * from it (and the per-frame log reads the list: port/runtime/hle.c heap_free): a block is a 32-byte header {next free
 * block, size in units} and its memory; syMalloc cuts the request from the end of the first free block that holds it
 * (the search starts after the rover, the block the last call stopped at), syFree puts a block back in address order
 * and merges it with the free neighbours it touches. Both run with every interrupt masked. */
#include "../lib.h"

typedef struct HeapBlock {
  gaddr next;     /* the next free block (the list is circular, through the sentinel) */
  uint32_t units; /* the block's size in 32-byte units, header included */
} HeapBlock;
HW_SIZE(HeapBlock, 8);

#define HEAP_UNIT       32u
#define g_heap_rover    GUEST_VAR(gaddr, 0x8C1C6754u) /* where the next search starts */
#define g_heap_start    GUEST_VAR(gaddr, 0x8C1C6758u)
#define g_heap_size     GUEST_VAR(uint32_t, 0x8C1C675Cu)
#define g_heap_start2   GUEST_VAR(gaddr, 0x8C1C6760u) /* the start again (read by syMallocFinish) */
#define HEAP_SENTINEL   0x8C1C6734u                   /* a block of 0 units that keeps the list from being empty */

static HeapBlock *block_at(gaddr b) { return (HeapBlock *)hw_ptr(b); }
static gaddr block_end(gaddr b) { return b + block_at(b)->units * HEAP_UNIT; }

/* was syFree (func_8C09F588): gives a block back (NULL: nothing) */
void sy_free(gaddr memory) {
  uint32_t level = sr_imask_raise();
  if (memory) {
    gaddr freed = memory - HEAP_UNIT;
    /* the free block after which it goes: the one below it whose next is above it, or the list's wrap-around
     * point (the highest block) when it lies past either end */
    gaddr p = g_heap_rover;
    while (!(freed > p && freed < block_at(p)->next)) {
      gaddr next = block_at(p)->next;
      if (p >= next && (freed > p || freed < next)) break;
      p = next;
    }
    HeapBlock *f = block_at(freed), *b = block_at(p);
    if (block_end(freed) == b->next) { /* it touches the next free block: they become one */
      f->units += block_at(b->next)->units;
      f->next = block_at(b->next)->next;
    } else {
      f->next = b->next;
    }
    if (block_end(p) == freed) { /* the free block before it touches it */
      b->units += f->units;
      b->next = f->next;
    } else {
      b->next = freed;
    }
    g_heap_rover = p;
  }
  sr_imask_set(level);
}
HW_EXPORT(8c09f588, sy_free(ARG0))

/* was syMalloc (func_8C09F4D6): size bytes (rounded up to the unit, plus the header), from the first free block
 * after the rover that holds them; NULL when none does */
gaddr sy_malloc(uint32_t size) {
  uint32_t level = (uint16_t)sr_imask_raise(); /* kept in a halfword of the frame */
  uint32_t units = ((size + 31u) >> 5) + 1u;
  gaddr start = g_heap_rover;
  gaddr prev = start;
  gaddr p = block_at(prev)->next;
  for (;;) {
    HeapBlock *b = block_at(p);
    if (b->units >= units) {
      if (b->units == units) {
        block_at(prev)->next = b->next; /* the whole block */
      } else {
        b->units -= units;              /* its end */
        p += b->units * HEAP_UNIT;
        block_at(p)->units = units;
      }
      g_heap_rover = prev;
      sr_imask_set(level);
      return p + HEAP_UNIT;
    }
    if (p == start) { /* round the whole list */
      sr_imask_set(level);
      return 0;
    }
    prev = p;
    p = b->next;
  }
}
HW_EXPORT(8c09f4d6, RET(sy_malloc(ARG0)))
/* was func_8C09F4D2: a second entry of syMalloc (a branch to it) */
HW_EXPORT(8c09f4d2, RET(sy_malloc(ARG0)))

/* was syMallocInit (func_8C09F428): the heap over [heap, heap + size), the start rounded up and the size down to the
 * unit; the list starts as the sentinel alone, and the whole heap is given to it as one freed block */
void sy_malloc_init(gaddr heap, uint32_t size) {
  heap = (heap + 31u) & ~31u;
  size &= ~31u;
  g_heap_start = heap;
  g_heap_start2 = heap;
  g_heap_size = size;
  g_heap_rover = HEAP_SENTINEL;
  block_at(HEAP_SENTINEL)->next = HEAP_SENTINEL;
  block_at(HEAP_SENTINEL)->units = 0;
  block_at(g_heap_start)->units = size >> 5;
  sy_free(g_heap_start + HEAP_UNIT);
}
HW_EXPORT(8c09f428, sy_malloc_init(ARG0, ARG1))

/* was sy_Nop1 (func_8C09FC94): an empty library hook */
void sy_nop1(void) {}
HW_EXPORT(8c09fc94, sy_nop1())

/* ---- the cable type ---- */
#define PORT_DATA_A 0xFF800030u /* PDTRA: bits 8-9 are the A/V cable's type */
#define CABLE_BITS  0x0300u
/* was func_8C062452: the cable bits of the port (bits 8-9, in place) */
uint32_t sy_cable_bits(void) { return hw_read16(PORT_DATA_A) & CABLE_BITS; }
HW_EXPORT(8c062452, RET(sy_cable_bits()))
/* was func_8C062440: the cable type (0: VGA, 2: RGB, 3: composite) */
uint32_t sy_cable_type(void) { return (sy_cable_bits() >> 8) & 3u; }
HW_EXPORT(8c062440, RET(sy_cable_type()))

/* ---- the BIOS ---- */
#define BIOS_SYSINFO_VECTOR 0x8C0000B0u /* the boot ROM's system-information call (the HLE's bios_sysinfo) */
/* was func_8C063150: the system-information call, function 3 (r7), with the caller's r4-r6 */
uint32_t sy_bios_sysinfo3(uint32_t a0, uint32_t a1, uint32_t a2) {
  return CALL4(GUEST_VAR(gaddr, BIOS_SYSINFO_VECTOR), a0, a1, a2, 3);
}
HW_EXPORT(8c063150, RET(sy_bios_sysinfo3(ARG0, ARG1, ARG2)))

/* ---- S20: hooks the library keeps (reached from the guest-code list: empty) ---- */
/* was syMallocFinish (func_8C09F45E): the heap's end (nothing to do) */
void sy_malloc_finish(void) {}
HW_EXPORT(8c09f45e, sy_malloc_finish())
/* was sy_Nop2 (func_8C09FCF2): an empty library hook */
void sy_nop2(void) {}
HW_EXPORT(8c09fcf2, sy_nop2())
