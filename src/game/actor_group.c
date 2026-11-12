/* Pen Pen port, S15 b08: the race scene's ActorGroup (game/actor.h): its construction (the course's model chunks,
 * texture lists and container loaded), building one actor per placement of the course, and drawing the actors seen
 * by the current view. The group's other methods are in game/actor.c (func_8C044600 tears it down, func_8C0446B0
 * calls every actor's per-frame method). */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/vec.h"
#include "../core/view.h"
#include "../core/file.h"
#include "../core/ninja.h"
#include "actor.h"
#include "course.h"
#include "race_scene.h"
#include "scenery.h"

/* per course (RaceSetup.course), the names of the files the group loads */
#define g_actor_chunk_files     (&GUEST_VAR(gaddr, 0x8C0971D4u)) /* the models: an NJTL chunk then NJCM chunks */
#define g_actor_container_files (&GUEST_VAR(gaddr, 0x8C0971E4u)) /* a data container */
#define g_actor_chunk_textures  (&GUEST_VAR(gaddr, 0x8C0971F4u)) /* the PVM of the chunk file's texture list */
#define g_actor_container_textures (&GUEST_VAR(gaddr, 0x8C097204u)) /* the PVM of the container's texture list */

#define CHUNK_TAG_NJCM 0x4E4A434Du /* 'NJCM' as nj_ReadChunkFile reports it: a chunk model */
#define ACTOR_GROUP_VTABLE 0x8C23DB74u
#define NJD_CONTROL_3D_FLAG_400 0x400u /* the njControl3D flag the actors are drawn with */
#define g_control3d_top GUEST_VAR(gaddr, 0x8C08A8A0u) /* -> the current njControl3D flags word (core/ninja.c's stack) */

/* func_8C0120CA (core/texture.c, not in a header): load the PVM file `name` into texlist */
static void texture_load(gaddr name, gaddr texlist, gaddr dir_name) { CALL3(0x8C0120CAu, name, texlist, dir_name); }
/* func_8C043D4C: the actor factory: a new actor of placement type `type` (0 when out of memory) */
static gaddr actor_factory(gaddr group, int32_t type) { return CALL2(0x8C043D4Cu, group, type); }

/* was func_8C04443E: the actor's cell = the course cell `pos` is on, searched from `hint` */
void actor_set_cell(gaddr actor, gaddr pos, int32_t hint) {
  Actor *a = actor_at(actor);
  a->cell = course_find_cell(race_scene_at(a->scene)->course, pos, hint);
}
HW_EXPORT(8c04443e, actor_set_cell(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C044464: whether the actor's cell is in the current view's course row (1 or 0) */
int32_t actor_in_view(gaddr actor) {
  Actor *a = actor_at(actor);
  int32_t cell = a->cell;
  gaddr scene = a->scene;
  int32_t view = view_current_index();
  return course_row_bit(race_scene_at(scene)->course, view, cell);
}
HW_EXPORT(8c044464, RET(actor_in_view(ARG0)))

/* was func_8C0444A8: the group's set-up: a fresh Task; as many views as the split mode makes; no blocks; then the
 * course's files: the chunk file (its texture list chunk, then every model chunk into the blocks, as long as the
 * chunks are NJCM), their textures, the container and its textures. Returns the group. */
gaddr actor_group_init(gaddr group) {
  ActorGroup *g = actor_group_at(group);
  g->task.linger = 0;
  g->task.active = 1;
  g->task.stops_on_pause = 1;
  g->task.child = 0;
  g->task.next = 0;
  g->task.parent = 0;
  g->task.owner = 0;
  g->word428 = 0;
  int32_t course = race_setup_at(g_race_setup)->course;
  switch (g_split_mode) {
  case 0: g->word434 = 1; break;
  case 1:
  case 2: g->word434 = 2; break;
  case 3: g->word434 = 4; break;
  default: break;
  }
  for (int32_t i = 0; i < (int32_t)(sizeof g->blocks / sizeof g->blocks[0]); i++) g->blocks[i] = 0;

  /* nj_ReadChunkFile's two out-parameters, on the guest stack as the original's frame has them: the chunk's tag at
   * r15, the read offset at r15+4 (0 before the file is loaded) */
  gaddr tag = hw_stack_alloc(8);
  gaddr offset = tag + 4;
  GUEST_VAR(uint32_t, offset) = 0;
  gaddr data = file_load_fixed(g_actor_chunk_files[course], 0);
  g->texlist = nj_ReadChunkFile(data, offset, tag);
  /* every model chunk into a block (the original has no bound on the 0x46 blocks) */
  int32_t n = 0;
  do {
    g->blocks[n] = nj_ReadChunkFile(data, offset, tag);
    n++;
  } while (GUEST_VAR(uint32_t, tag) == CHUNK_TAG_NJCM);
  hw_stack_free(8);

  texture_load(g_actor_chunk_textures[course], g->texlist, 0);
  g->file = container_load(g_actor_container_files[course], 0);
  texture_load(g_actor_container_textures[course], ((const ContainerEntry *)hw_ptr(g->file))[0].offset, 0);
  return group;
}
HW_EXPORT(8c0444a8, RET(actor_group_init(ARG0)))

/* was func_8C0445B0: the group's constructor */
gaddr actor_group_construct(gaddr group) {
  actor_group_at(group)->task.vtable = ACTOR_GROUP_VTABLE;
  actor_group_init(group);
  return group;
}
HW_EXPORT(8c0445b0, RET(actor_group_construct(ARG0)))

/* was func_8C04467A: the group's deleting destructor */
gaddr actor_group_delete(gaddr group, int32_t free_memory) {
  actor_group_release(group);
  if (free_memory) syFree(group);
  return group;
}
HW_EXPORT(8c04467a, RET(actor_group_delete(ARG0, (int32_t)ARG1)))

/* was func_8C0447BC: hand a new actor what it needs: the scene; its texture list and model lists (a placement with
 * param bit 16 takes them from the group's container: kind k = param bits 12-15 has its entries 2k+1 and 2k+2; else
 * the group's texture list, and its blocks as the model list); its index and the number of views; its cell (from the placement's position);
 * then its init method with the placement */
void actor_group_attach(gaddr group, gaddr actor, gaddr placement, int32_t index) {
  ActorGroup *g = actor_group_at(group);
  Actor *a = actor_at(actor);
  const ActorPlacement *p = (const ActorPlacement *)hw_ptr(placement);
  a->scene = g->task.parent;
  if (p->param & 0x10000u) {
    int32_t kind = (int32_t)(p->param & 0xF000u) >> 12;
    a->texlist = g->file;
    gaddr entry = g->file + (gaddr)((2 * kind + 1) * (int32_t)sizeof(ContainerEntry));
    a->model_set = entry;
    a->word1c = entry + (gaddr)sizeof(ContainerEntry);
  } else {
    a->texlist = g->texlist;
    a->models = member_addr(group, ActorGroup, blocks);
  }
  a->index = index;
  a->word0c = g->word434;
  gaddr pos = hw_stack_alloc(sizeof(Vec3));
  vec3_set(p->position.x, p->position.y, p->position.z, pos);
  actor_set_cell(actor, pos, 0);
  hw_stack_free(sizeof(Vec3));
  const VMethod *init = &actor_vtbl(actor)->init;
  hw_call(init->fn, 2, actor + (uint32_t)init->this_adjust, placement, 0, 0);
}
HW_EXPORT(8c0447bc, actor_group_attach(ARG0, ARG1, ARG2, (int32_t)ARG3))

/* was func_8C0446EC: build one actor per placement of the course (up to the -1 type) and attach it (the original
 * has no bound on the ACTOR_MAX slots) */
void actor_group_populate(gaddr group) {
  ActorGroup *g = actor_group_at(group);
  g->count = 0;
  gaddr placement = course_at(race_scene_at(g->task.parent)->course)->actors;
  while (((const ActorPlacement *)hw_ptr(placement))->type != -1) {
    gaddr actor = actor_factory(group, ((const ActorPlacement *)hw_ptr(placement))->type);
    g->actors[g->count] = actor;
    actor_group_attach(group, actor, placement, g->count);
    placement += sizeof(ActorPlacement);
    g->count++;
  }
}
HW_EXPORT(8c0446ec, actor_group_populate(ARG0))

/* was func_8C04489A: draw the actors for the current view: with njControl3D flag 0x400 added, each actor that is
 * always drawn (flags bit 0) or whose cell the view's course row has: the group's texture list, then its
 * per-view draw method */
void actor_group_draw_view(gaddr group) {
  ActorGroup *g = actor_group_at(group);
  control3d_push();
  control3d_set(GUEST_VAR(uint32_t, g_control3d_top) | NJD_CONTROL_3D_FLAG_400);
  for (int32_t i = 0; i < g->count; i++) {
    if ((actor_at(g->actors[i])->flags & 1) || actor_in_view(g->actors[i])) {
      njSetTexture(g->texlist);
      gaddr actor = g->actors[i];
      const VMethod *draw = &actor_vtbl(actor)->draw_view;
      hw_call(draw->fn, 1, actor + (uint32_t)draw->this_adjust, 0, 0, 0);
    }
  }
  control3d_pop();
}
HW_EXPORT(8c04489a, actor_group_draw_view(ARG0))
