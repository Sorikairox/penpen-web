/* Pen Pen port, S20: the start-up's library registry (functions.tsv region `unknown`, called by the boot code): every
 * SDK library linked into the program carries a 32-byte information record (a 16-byte name at +0x10); the start-up
 * walks the program's table of them and lists each record once in a registry at the top of the boot area's free
 * RAM, which other code (the debugger, the BIOS) can read. */
#include "../lib.h"

typedef struct LibRecord {
  uint8_t info[0x10];
  char name[0x10];          /* 0x10 */
} LibRecord;
HW_SIZE(LibRecord, 0x20);

typedef struct LibRegistryEntry {
  gaddr record;             /* a LibRecord */
  uint32_t zero;
} LibRegistryEntry;
typedef struct LibRegistry {
  int32_t count;
  uint32_t _04;
  LibRegistryEntry entry[]; /* 0x08 */
} LibRegistry;
HW_OFFSET(LibRegistry, entry, 8);

#define g_registry          GUEST_VAR(gaddr, 0x8C00FFF0u)    /* the registry, set on first use */
#define g_registry_capacity GUEST_VAR(uint32_t, 0x8C00FFF4u)
#define REGISTRY_HOME       0x8C00FC00u
#define REGISTRY_CAPACITY   0x78u
#define TABLE_RECORDS       0x20u       /* a table's records follow its 32-byte header */
#define TABLE_MAGIC         0x8C179DC8u /* the name of a table's first record (the table's mark) */
#define TABLE_END_NAME      0x8C179DD8u /* the name of the record that ends a table */
#define NAME_LEN            0x10u

/* was func_8C09CA0E: the registry (placed at its home the first time); *capacity = its capacity */
gaddr lib_registry(uint32_t *capacity) {
  if (g_registry == 0) {
    g_registry = REGISTRY_HOME;
    g_registry_capacity = REGISTRY_CAPACITY;
  }
  *capacity = g_registry_capacity;
  return g_registry;
}
HW_EXPORT(8c09ca0e, RET(lib_registry((uint32_t *)hw_ptr(ARG0))))

/* was func_8C09CAB2: the index of the listed record with that 16-byte name, or -2 */
int32_t lib_registry_find(gaddr name) {
  uint32_t capacity;
  LibRegistry *r = hw_ptr(lib_registry(&capacity));
  for (int32_t i = 0; i < r->count; i++)
    if (rt_memcmp(name, r->entry[i].record + offsetof(LibRecord, name), NAME_LEN) == 0) return i;
  return -2;
}
HW_EXPORT(8c09cab2, RET(lib_registry_find(ARG0)))

/* was func_8C09CC20: lists a record unless one of its name is listed (interrupts masked meanwhile); returns 0 */
int32_t lib_registry_add(gaddr record) {
  uint32_t level = sr_imask_raise();
  uint32_t capacity;
  LibRegistry *r = hw_ptr(lib_registry(&capacity));
  if (lib_registry_find(record + offsetof(LibRecord, name)) < 0) {
    r->entry[r->count].record = record;
    r->entry[r->count].zero = 0;
    r->count++;
  }
  sr_imask_set(level);
  return 0;
}
HW_EXPORT(8c09cc20, RET(lib_registry_add(ARG0)))

/* was func_8C09CD78: lists every record of a table (its first record must carry the mark: -1 otherwise), from the
 * second one up to the end record; returns 0 */
int32_t lib_registry_add_all(gaddr table) {
  gaddr records = table + TABLE_RECORDS;
  if (rt_memcmp(records + offsetof(LibRecord, name), TABLE_MAGIC, NAME_LEN) != 0) return -1;
  for (uint32_t i = 1;; i++) {
    gaddr record = records + i * sizeof(LibRecord);
    if (rt_memcmp(record + offsetof(LibRecord, name), TABLE_END_NAME, NAME_LEN) == 0) return 0;
    lib_registry_add(record);
  }
}
HW_EXPORT(8c09cd78, RET(lib_registry_add_all(ARG0)))

/* ---- S20: the registry undone (after main: no route reaches it) ---- */
/* was func_8C09CA30: the index of a listed record (by address), or -3 */
int32_t lib_registry_index(gaddr record) {
  uint32_t capacity;
  LibRegistry *r = hw_ptr(lib_registry(&capacity));
  for (int32_t i = 0; i < r->count; i++)
    if (r->entry[i].record == record) return i;
  return -3;
}
HW_EXPORT(8c09ca30, RET(lib_registry_index(ARG0)))

/* was func_8C09CC92: takes a record off the list (the later entries move down, the freed last entry is cleared);
 * 0, or -2 when it is not listed (interrupts masked meanwhile) */
int32_t lib_registry_remove(gaddr record) {
  uint32_t level = sr_imask_raise();
  uint32_t capacity;
  LibRegistry *r = hw_ptr(lib_registry(&capacity));
  int32_t result, i = lib_registry_index(record);
  if (i >= 0) {
    r->count--;
    for (; i < r->count; i++) {
      r->entry[i].record = r->entry[i + 1].record;
      r->entry[i].zero = r->entry[i + 1].zero;
    }
    r->entry[i].record = 0;
    r->entry[i].zero = 0;
    result = 0;
  } else {
    result = -2;
  }
  sr_imask_set(level);
  return result;
}
HW_EXPORT(8c09cc92, RET(lib_registry_remove(ARG0)))

/* was func_8C09CDF6: takes every record of a table off the list (as lib_registry_add_all walks it); 0, or -1 when
 * the table lacks its mark */
int32_t lib_registry_remove_all(gaddr table) {
  gaddr records = table + TABLE_RECORDS;
  if (rt_memcmp(records + offsetof(LibRecord, name), TABLE_MAGIC, NAME_LEN) != 0) return -1;
  for (uint32_t i = 1;; i++) {
    gaddr record = records + i * sizeof(LibRecord);
    if (rt_memcmp(record + offsetof(LibRecord, name), TABLE_END_NAME, NAME_LEN) == 0) return 0;
    lib_registry_remove(record);
  }
}
HW_EXPORT(8c09cdf6, RET(lib_registry_remove_all(ARG0)))
