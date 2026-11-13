/* Pen Pen port, S17 b03: the TrapSet, a 0x558-byte Task a scene makes (func_8C01FE58's caller chain: allocated,
 * constructed with func_8C021614, hung under the scene, then filled by func_8C021718): a model container of the
 * course's objects ("OBJTRAP" files by layout number) and up to 256 objects placed from a layout table. Only its
 * construction and destruction are here; the methods that call the placed objects' virtual methods stay translated
 * (the objects' class is not identified yet). */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/file.h"
#include "../core/inline.h"

typedef struct TrapSet {
  Task task;                /* 0x000 */
  gaddr objects[256];       /* 0x024: the placed objects (func_8C01F780 makes them) */
  int32_t object_count;     /* 0x424 */
  int32_t word428;          /* 0x428: 0 at construction */
  int32_t layout;           /* 0x42C: picks the file names (1 at construction) */
  gaddr file;               /* 0x430: the container (container_load); its first entry's data is the texlist */
  int32_t word434;          /* 0x434: 1 at construction */
  int32_t shown;            /* 0x438: 0 = the objects are not updated or drawn */
  gaddr texlist;            /* 0x43C */
  uint8_t pad_440[0x558 - 0x440];
} TrapSet;
HW_OFFSET(TrapSet, object_count, 0x424);
HW_OFFSET(TrapSet, layout, 0x42C);
HW_OFFSET(TrapSet, file, 0x430);
HW_OFFSET(TrapSet, shown, 0x438);
HW_SIZE(TrapSet, 0x558);
#define TRAP_SET_VTABLE    0x8C23D9FCu
#define TRAP_SET_BINS      ((const gaddr *)hw_ptr(0x8C0971E4u)) /* the container's file name by layout */
#define TRAP_SET_PVMS      ((const gaddr *)hw_ptr(0x8C097204u)) /* its textures' file name by layout */

static inline TrapSet *trap_set_at(gaddr a) { return (TrapSet *)hw_ptr(a); }

/* func_8C0120CA (core/texture.c, not declared in a header): loads a texture file into a texlist */
static void texture_load(gaddr name, gaddr texlist, int32_t flags) { CALL3(0x8C0120CAu, name, texlist, flags); }

/* the container's first entry's data (its texlist) */
static gaddr trap_set_file_texlist(const TrapSet *t) {
  return (gaddr)((const ContainerEntry *)hw_ptr(t->file))->offset;
}

/* was func_8C0215C4: the body: a Task with no parent, layout 1, the container and its textures loaded */
gaddr trap_set_init(gaddr self) {
  TrapSet *t = trap_set_at(self);
  task_construct(self, 0);
  t->layout = 1;
  t->word434 = 1;
  t->word428 = 0;
  t->file = container_load(TRAP_SET_BINS[t->layout], 0);
  texture_load(TRAP_SET_PVMS[t->layout], trap_set_file_texlist(t), 0);
  return self;
}
HW_EXPORT(8c0215c4, RET(trap_set_init(ARG0)))

/* was func_8C021614: the constructor (the vtable, then the body) */
gaddr trap_set_construct(gaddr self) {
  trap_set_at(self)->task.vtable = TRAP_SET_VTABLE;
  trap_set_init(self);
  return self;
}
HW_EXPORT(8c021614, RET(trap_set_construct(ARG0)))

/* was func_8C02162A: the destructor's body: the textures released, the container freed, then the Task's (empty)
 * destructor body */
void trap_set_destroy(gaddr self) {
  njReleaseTexture(trap_set_file_texlist(trap_set_at(self)));
  file_free(trap_set_at(self)->file);
  method_empty();
}
HW_EXPORT(8c02162a, trap_set_destroy(ARG0))

/* was func_8C021658: the virtual destructor (frees the object when asked) */
gaddr trap_set_delete(gaddr self, int32_t free_memory) {
  trap_set_destroy(self);
  if (free_memory != 0)
    syFree(self);
  return self;
}
HW_EXPORT(8c021658, RET(trap_set_delete(ARG0, (int32_t)ARG1)))

/* was func_8C02167C: the destructor without freeing */
gaddr trap_set_destroy_self(gaddr self) {
  trap_set_destroy(self);
  return self;
}
HW_EXPORT(8c02167c, RET(trap_set_destroy_self(ARG0)))
