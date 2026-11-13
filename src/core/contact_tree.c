/* Pen Pen port, S17 (b01): a basic model tree's triangles added to a Contacts set (core/contact.h). */
#include "hw.h"
#include "../core/file.h"

#define NJD_EVAL_BREAK 0x10 /* the node's children are skipped */

static inline const NjObject *object_at(gaddr a) { return (const NjObject *)hw_ptr(a); }

/* func_8C01431E (still translated): every triangle of one basic model's meshsets into the contacts */
static void contacts_add_model(gaddr contacts, gaddr model) { CALL2(0x8C01431Eu, contacts, model); }

/* was func_8C0144BC: every model of the tree from `object` (the node, its children unless NJD_EVAL_BREAK, then its
 * siblings in turn) through contacts_add_model; the flags are read after the model is handled */
void contacts_add_object_tree(gaddr contacts, gaddr object) {
  for (;;) {
    if (object_at(object)->model != 0)
      contacts_add_model(contacts, object_at(object)->model);
    if ((object_at(object)->evalflags & NJD_EVAL_BREAK) == 0)
      contacts_add_object_tree(contacts, object_at(object)->child);
    if (object_at(object)->sibling == 0)
      return;
    object = object_at(object)->sibling;
  }
}
HW_EXPORT(8c0144bc, contacts_add_object_tree(ARG0, ARG1))
