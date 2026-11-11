/* Pen Pen port, S12: data containers: loading one turns the offsets inside it into addresses (core/file.h). */
#include "file.h"

/* guest words */
#define W(a) GUEST_VAR(int32_t, (a))
#define AT(type, a) ((type *)hw_ptr(a))

/* was func_8C012CF4: an offset from the file's start, as an address; 0 and negative values are not pointers */
int32_t reloc(int32_t offset, gaddr base) {
  return offset > 0 ? offset + (int32_t)base : offset;
}
HW_EXPORT(8c012cf4, RET(reloc((int32_t)ARG0, ARG1)))

/* a pointer field: relocated in place; its new value */
static gaddr fix(gaddr field, gaddr base) {
  W(field) = reloc(W(field), base);
  return (gaddr)W(field);
}

/* ---- basic models ---- */

/* was func_8C01322C */
void fix_meshset(gaddr meshset, gaddr base) {
  fix(meshset + offsetof(NjMeshset, meshes), base);
  fix(meshset + offsetof(NjMeshset, attrs), base);
  fix(meshset + offsetof(NjMeshset, normals), base);
  fix(meshset + offsetof(NjMeshset, vertex_colors), base);
  fix(meshset + offsetof(NjMeshset, vertex_uvs), base);
}
HW_EXPORT(8c01322c, fix_meshset(ARG0, ARG1))

/* was func_8C0131D6 */
void fix_model(gaddr model, gaddr base) {
  fix(model + offsetof(NjModel, points), base);
  fix(model + offsetof(NjModel, normals), base);
  fix(model + offsetof(NjModel, materials), base);
  fix(model + offsetof(NjModel, meshsets), base);
  for (int32_t i = 0; i < AT(NjModel, model)->nb_meshsets; i++)
    fix_meshset(AT(NjModel, model)->meshsets + (gaddr)i * sizeof(NjMeshset), base);
}
HW_EXPORT(8c0131d6, fix_model(ARG0, ARG1))

/* was func_8C013188: a model tree: each node's model, its children (recursively) and its siblings (in a loop) */
void fix_object_tree(gaddr object, gaddr base) {
  for (;;) {
    NjObject *node = AT(NjObject, object);
    if (node->model)
      fix_model(fix(object + offsetof(NjObject, model), base), base);
    if (node->child)
      fix_object_tree(fix(object + offsetof(NjObject, child), base), base);
    if (!node->sibling)
      break;
    object = fix(object + offsetof(NjObject, sibling), base);
  }
}
HW_EXPORT(8c013188, fix_object_tree(ARG0, ARG1))

/* ---- motions ---- */

/* was func_8C013264: a motion of `nodes` nodes. The type's low bits say which elements each node has: position,
 * angle, scale, vector (1, 2, 4, 8) have keys of plain values, shape and vertex (0x10, 0x20) keys that hold a pointer
 * (the original starts from a table of five flags at 0x8C08AD54 and marks those). Each node's entry holds the
 * elements' key-array pointers, then their key counts. Only motions of 1 to 5 elements are fixed. */
void fix_motion(gaddr motion, gaddr base, int32_t nodes) {
  gaddr mdata = fix(motion + offsetof(NjMotion, mdata), base);
  int32_t pointer_keys[16] = {0}; /* the original's array has 5: a 6-element motion would overrun it (never seen) */
  for (int i = 0; i < MOTION_ELEMENTS_MAX; i++)
    pointer_keys[i] = W(MOTION_POINTER_KEYS + 4u * (uint32_t)i);
  int elements = 0;
  for (int bit = 0; bit <= 12; bit++) {
    uint32_t element = AT(NjMotion, motion)->type & (1u << bit);
    if (element == 1 || element == 2 || element == 4 || element == 8) {
      elements++;
    } else if (element == 0x10 || element == 0x20) {
      pointer_keys[elements] = 1;
      elements++;
    }
  }
  if (elements < 1 || elements > MOTION_ELEMENTS_MAX)
    return;
  for (int32_t node = 0; node < nodes; node++) {
    gaddr entry = mdata + (gaddr)(node * elements * 8);
    for (int e = 0; e < elements; e++) {
      gaddr keys = fix(entry + 4u * (uint32_t)e, base);
      if (!pointer_keys[e])
        continue;
      uint32_t nb_keys = (uint32_t)W(entry + 4u * (uint32_t)(elements + e));
      for (uint32_t k = 0; k < nb_keys; k++)
        fix(keys + 8 * k + 4, base); /* a key: {frame, pointer} */
    }
  }
}
HW_EXPORT(8c013264, fix_motion(ARG0, ARG1, (int32_t)ARG2))

/* ---- textures ---- */

/* was func_8C013654: a texture list: the names' file-name pointers */
void fix_texlist(gaddr texlist, gaddr base) {
  fix(texlist + offsetof(NjTexList, textures), base);
  for (uint32_t i = 0; i < AT(NjTexList, texlist)->nb_textures; i++)
    fix(AT(NjTexList, texlist)->textures + i * sizeof(NjTexName) + offsetof(NjTexName, filename), base);
}
HW_EXPORT(8c013654, fix_texlist(ARG0, ARG1))

/* ---- chunk models ---- */

/* was func_8C013934 */
void fix_cnk_model(gaddr model, gaddr base) {
  fix(model + offsetof(NjCnkModel, vlist), base);
  fix(model + offsetof(NjCnkModel, plist), base);
}
HW_EXPORT(8c013934, fix_cnk_model(ARG0, ARG1))

/* was func_8C0138E6: as fix_object_tree, with chunk models */
void fix_cnk_object_tree(gaddr object, gaddr base) {
  for (;;) {
    NjObject *node = AT(NjObject, object);
    if (node->model)
      fix_cnk_model(fix(object + offsetof(NjObject, model), base), base);
    if (node->child)
      fix_cnk_object_tree(fix(object + offsetof(NjObject, child), base), base);
    if (!node->sibling)
      break;
    object = fix(object + offsetof(NjObject, sibling), base);
  }
}
HW_EXPORT(8c0138e6, fix_cnk_object_tree(ARG0, ARG1))

/* ---- still translated: the fix-ups of container types no route loads (they stay the translation's until a run
 * reaches them: the difftest could not test them) ---- */
static void fix_objects(gaddr array, gaddr base, int32_t count) { CALL3(0x8C013696u, array, base, count); }
static void fix_texlists(gaddr array, gaddr base, int32_t count) { CALL3(0x8C01370Eu, array, base, count); }
static void fix_action(gaddr action, gaddr base, int32_t nodes) { CALL3(0x8C013746u, action, base, nodes); }
static void fix_actions(gaddr array, gaddr base, int32_t count, int32_t nodes) { CALL4(0x8C013790u, array, base, count, nodes); }
static void fix_action_list(gaddr array, gaddr base, int32_t count) { CALL3(0x8C0137D0u, array, base, count); }
static void fix_motion_pair(gaddr pair, gaddr base, int32_t nodes) { CALL3(0x8C01380Au, pair, base, nodes); }
static void fix_motion_pairs(gaddr array, gaddr base, int32_t count, int32_t nodes) { CALL4(0x8C013856u, array, base, count, nodes); }
static void fix_cnk_action(gaddr action, gaddr base, int32_t nodes) { CALL3(0x8C01398Cu, action, base, nodes); }
/* func_8C0139D6 takes its count from the fourth argument and passes it on as the node count; the third is unused */
static void fix_cnk_actions(gaddr array, gaddr base, int32_t unused, int32_t count) { CALL4(0x8C0139D6u, array, base, unused, count); }

/* ---- arrays: `count` pointers (or tuples), each fixed when not 0 ---- */

/* was func_8C0136CE */
static void fix_motions(gaddr array, gaddr base, int32_t count, int32_t nodes) {
  for (; count > 0; count--, array += 4)
    if (W(array))
      fix_motion(fix(array, base), base, nodes);
}
HW_EXPORT(8c0136ce, fix_motions(ARG0, ARG1, (int32_t)ARG2, (int32_t)ARG3))

/* was func_8C013896: {motion, motion, nodes} triples */
static void fix_motion_pair_list(gaddr array, gaddr base, int32_t count) {
  for (; count > 0; count--, array += 12) {
    if (W(array))
      fix_motion(fix(array, base), base, W(array + 8));
    if (W(array + 4))
      fix_motion(fix(array + 4, base), base, W(array + 8));
  }
}
HW_EXPORT(8c013896, fix_motion_pair_list(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C013954 */
static void fix_cnk_objects(gaddr array, gaddr base, int32_t count) {
  for (; count > 0; count--, array += 4)
    if (W(array))
      fix_cnk_object_tree(fix(array, base), base);
}
HW_EXPORT(8c013954, fix_cnk_objects(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C013A16: {chunk tree, motion, nodes} triples */
static void fix_cnk_action_list(gaddr array, gaddr base, int32_t count) {
  for (; count > 0; count--, array += 12) {
    if (W(array))
      fix_cnk_object_tree(fix(array, base), base);
    if (W(array + 4))
      fix_motion(fix(array + 4, base), base, W(array + 8));
  }
}
HW_EXPORT(8c013a16, fix_cnk_action_list(ARG0, ARG1, (int32_t)ARG2))

/* ---- the container ---- */

/* was func_8C012EBE: load a container (file_load) and fix every entry of its table */
gaddr container_load(gaddr name, gaddr dir_name) {
  gaddr base = file_load(name, dir_name);
  for (gaddr e = base; W(e); e += sizeof(ContainerEntry)) {
    ContainerEntry *entry = AT(ContainerEntry, e);
    if ((uint32_t)(entry->type - 1) >= CONTAINER_TYPES - 1)
      continue; /* an unknown type is skipped */
    gaddr data = fix(e + offsetof(ContainerEntry, offset), base);
    switch (entry->type) {
    case CONTAINER_OBJECT: fix_object_tree(data, base); break;
    case CONTAINER_MOTION: fix_motion(data, base, entry->a); break;
    case CONTAINER_TEXLIST: fix_texlist(data, base); break;
    case CONTAINER_OBJECTS: fix_objects(data, base, entry->a); break;
    case CONTAINER_MOTIONS: fix_motions(data, base, entry->a, entry->b); break;
    case CONTAINER_TEXLISTS: fix_texlists(data, base, entry->a); break;
    case CONTAINER_DATA: break;
    case CONTAINER_POINTERS:
      for (int32_t i = 0; i < entry->a; i++)
        if (W(data + 4u * (uint32_t)i))
          fix(data + 4u * (uint32_t)i, base);
      break;
    case CONTAINER_ACTION: fix_action(data, base, entry->a); break;
    case CONTAINER_ACTIONS: fix_actions(data, base, entry->a, entry->b); break;
    case CONTAINER_ACTION_LIST: fix_action_list(data, base, entry->a); break;
    case CONTAINER_MOTION_PAIR: fix_motion_pair(data, base, entry->a); break;
    case CONTAINER_MOTION_PAIRS: fix_motion_pairs(data, base, entry->a, entry->b); break;
    case CONTAINER_MOTION_PAIR_LIST: fix_motion_pair_list(data, base, entry->a); break;
    case CONTAINER_CNK_OBJECT: fix_cnk_object_tree(data, base); break;
    case CONTAINER_CNK_OBJECTS: fix_cnk_objects(data, base, entry->a); break;
    case CONTAINER_CNK_ACTION: fix_cnk_action(data, base, entry->a); break;
    case CONTAINER_CNK_ACTIONS: fix_cnk_actions(data, base, entry->a, entry->b); break;
    case CONTAINER_CNK_ACTION_LIST: fix_cnk_action_list(data, base, entry->a); break;
    }
  }
  return base;
}
HW_EXPORT(8c012ebe, RET(container_load(ARG0, ARG1)))
