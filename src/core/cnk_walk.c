/* Pen Pen port, S15 (b01): walking a chunk model tree (NJS_OBJECT with NJS_CNK_MODELs) for its strips. */
#include "hw.h"
#include "../core/file.h"

#define NJD_EVAL_BREAK 0x10 /* the node's children are skipped */

static inline const NjObject *object_at(gaddr a) { return (const NjObject *)hw_ptr(a); }

/* func_8C0144F2 (still translated): the strips of one chunk model, for `context` */
static void cnk_model_strips(gaddr context, gaddr model) { CALL2(0x8C0144F2u, context, model); }

/* was func_8C0146BC: every model of the tree from `object` (the node, its children unless NJD_EVAL_BREAK, then its
 * siblings in turn) through cnk_model_strips; the flags are read before the model is handled */
void cnk_tree_strips(gaddr context, gaddr object) {
  for (;;) {
    uint32_t flags = object_at(object)->evalflags;
    if (object_at(object)->model != 0)
      cnk_model_strips(context, object_at(object)->model);
    if ((flags & NJD_EVAL_BREAK) == 0)
      cnk_tree_strips(context, object_at(object)->child);
    if (object_at(object)->sibling == 0)
      return;
    object = object_at(object)->sibling;
  }
}
HW_EXPORT(8c0146bc, cnk_tree_strips(ARG0, ARG1))
