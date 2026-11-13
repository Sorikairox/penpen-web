/* Pen Pen port, S17 (b01): the container fix-ups core/fixup.c left translated: arrays of model trees and texture
 * lists, actions (a model tree and its motion), motion pairs and their arrays (core/file.h). Each pointer that is not
 * 0 is relocated in place (reloc), then the structure it points to is fixed. */
#include "file.h"

/* the entries of the arrays these walk */
typedef struct ActionListEntry {
  gaddr action;  /* 0x0: NjAction */
  int32_t nodes; /* 0x4: the motion's node count */
} ActionListEntry;
HW_SIZE(ActionListEntry, 8);

typedef struct MotionPair {
  gaddr first, second; /* 0x0, 0x4: NjMotion (one node count for both) */
} MotionPair;
HW_SIZE(MotionPair, 8);

static gaddr *word_at(gaddr a) { return (gaddr *)hw_ptr(a); }

/* a pointer field: relocated in place; its new value */
static gaddr fix(gaddr *field, gaddr base) {
  *field = (gaddr)reloc((int32_t)*field, base);
  return *field;
}

/* was func_8C013696: `count` model trees */
void fix_objects(gaddr array, gaddr base, int32_t count) {
  for (; count > 0; count--, array += sizeof(gaddr))
    if (*word_at(array))
      fix_object_tree(fix(word_at(array), base), base);
}
HW_EXPORT(8c013696, fix_objects(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C01370E: `count` texture lists */
void fix_texlists(gaddr array, gaddr base, int32_t count) {
  for (; count > 0; count--, array += sizeof(gaddr))
    if (*word_at(array))
      fix_texlist(fix(word_at(array), base), base);
}
HW_EXPORT(8c01370e, fix_texlists(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C013746: an action: its model tree and its motion of `nodes` nodes */
void fix_action(gaddr action, gaddr base, int32_t nodes) {
  NjAction *a = (NjAction *)hw_ptr(action);
  if (a->object)
    fix_object_tree(fix(&a->object, base), base);
  if (a->motion)
    fix_motion(fix(&a->motion, base), base, nodes);
}
HW_EXPORT(8c013746, fix_action(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C013790: `count` actions, all of `nodes` nodes */
void fix_actions(gaddr array, gaddr base, int32_t count, int32_t nodes) {
  for (; count > 0; count--, array += sizeof(gaddr))
    if (*word_at(array))
      fix_action(fix(word_at(array), base), base, nodes);
}
HW_EXPORT(8c013790, fix_actions(ARG0, ARG1, (int32_t)ARG2, (int32_t)ARG3))

/* was func_8C0137D0: `count` {action, node count} entries */
void fix_action_list(gaddr array, gaddr base, int32_t count) {
  for (; count > 0; count--, array += sizeof(ActionListEntry)) {
    ActionListEntry *e = (ActionListEntry *)hw_ptr(array);
    if (e->action)
      fix_action(fix(&e->action, base), base, e->nodes);
  }
}
HW_EXPORT(8c0137d0, fix_action_list(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C01380A: two motions of `nodes` nodes */
void fix_motion_pair(gaddr pair, gaddr base, int32_t nodes) {
  MotionPair *p = (MotionPair *)hw_ptr(pair);
  if (p->first)
    fix_motion(fix(&p->first, base), base, nodes);
  if (p->second)
    fix_motion(fix(&p->second, base), base, nodes);
}
HW_EXPORT(8c01380a, fix_motion_pair(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C013856: `count` motion pairs, all of `nodes` nodes */
void fix_motion_pairs(gaddr array, gaddr base, int32_t count, int32_t nodes) {
  for (; count > 0; count--, array += sizeof(gaddr))
    if (*word_at(array))
      fix_motion_pair(fix(word_at(array), base), base, nodes);
}
HW_EXPORT(8c013856, fix_motion_pairs(ARG0, ARG1, (int32_t)ARG2, (int32_t)ARG3))

/* was func_8C01398C: a chunk-model action: its chunk tree and its motion of `nodes` nodes */
void fix_cnk_action(gaddr action, gaddr base, int32_t nodes) {
  NjAction *a = (NjAction *)hw_ptr(action);
  if (a->object)
    fix_cnk_object_tree(fix(&a->object, base), base);
  if (a->motion)
    fix_motion(fix(&a->motion, base), base, nodes);
}
HW_EXPORT(8c01398c, fix_cnk_action(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C0139D6: chunk-model actions; the fourth argument is both their count and their node count (the third
 * is not read) */
void fix_cnk_actions(gaddr array, gaddr base, int32_t unused, int32_t count) {
  int32_t nodes = count;
  for (; count > 0; count--, array += sizeof(gaddr))
    if (*word_at(array))
      fix_cnk_action(fix(word_at(array), base), base, nodes);
}
HW_EXPORT(8c0139d6, fix_cnk_actions(ARG0, ARG1, (int32_t)ARG2, (int32_t)ARG3))
