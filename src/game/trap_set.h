/* Pen Pen port, S18 b02: the TrapSet (S17 b03 declared it in game/trap_set.c; moved here and extended).
 *
 * A 0x558-byte Task a scene makes (the CastScene, game/cast_scene.h: allocated, constructed with func_8C021614, hung
 * under the scene, then filled by func_8C021718): the course's objects of a layout, built like the race's
 * ActorGroup builds its actors (game/actor.h: the same factory, the same Actor base and placements; the layout of
 * the group's fields too): a container of the objects' models ("OBJTRAP" files by layout number) and up to 256
 * Actors placed from a -1-terminated ActorPlacement list of the layout. */
#ifndef PENPEN_TRAP_SET_H
#define PENPEN_TRAP_SET_H
#include "hw.h"
#include "../core/task.h"

typedef struct TrapSet {
  Task task;                /* 0x000 */
  gaddr objects[256];       /* 0x024: the placed Actors (the factory func_8C01F780 makes them) */
  int32_t object_count;     /* 0x424 */
  int32_t word428;          /* 0x428: 0 at construction */
  int32_t layout;           /* 0x42C: picks the file names and the placements (1 at construction) */
  gaddr file;               /* 0x430: the container (container_load); its first entry's data is the texlist */
  int32_t word434;          /* 0x434: 1 at construction; handed to each object (Actor.word0c) */
  int32_t shown;            /* 0x438: 0 = the objects are not updated or drawn (the CastScene's trap mode) */
  gaddr texlist;            /* 0x43C: set before each object's per-view draw */
  uint8_t pad_440[0x558 - 0x440];
} TrapSet;
HW_OFFSET(TrapSet, objects, 0x024);
HW_OFFSET(TrapSet, object_count, 0x424);
HW_OFFSET(TrapSet, layout, 0x42C);
HW_OFFSET(TrapSet, file, 0x430);
HW_OFFSET(TrapSet, word434, 0x434);
HW_OFFSET(TrapSet, shown, 0x438);
HW_OFFSET(TrapSet, texlist, 0x43C);
HW_SIZE(TrapSet, 0x558);

/* the placements (ActorPlacement lists, -1-terminated) by layout */
#define g_trap_set_placements ((const gaddr *)hw_ptr(0x8C092D4Cu))

static inline TrapSet *trap_set_at(gaddr a) { return (TrapSet *)hw_ptr(a); }

/* game/trap_set.c */
gaddr trap_set_init(gaddr self);
gaddr trap_set_construct(gaddr self);
void trap_set_destroy(gaddr self);
gaddr trap_set_delete(gaddr self, int32_t free_memory);
gaddr trap_set_destroy_self(gaddr self);
/* S18 b02: game/trap_set_objects.c */
void trap_set_set_shown(gaddr self, int32_t shown);
void trap_set_populate(gaddr self);
void trap_set_reattach(gaddr self);
void trap_set_attach(gaddr self, gaddr actor, gaddr placement, int32_t index);
void trap_set_draw(gaddr self);
void trap_set_draw_view(gaddr self);

#endif
