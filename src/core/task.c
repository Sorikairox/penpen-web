/* Pen Pen port, S11: the task tree (core/task.h). Hand-written; each function names the original it replaces. */
#include "task.h"
#include "sdk.h"

/* ---- virtual calls ---- */

static uint32_t task_vcall(Task *task, const VMethod *m, int nargs, uint32_t arg) {
  return hw_call(m->fn, nargs, hw_addr(task) + (uint32_t)m->this_adjust, arg, 0, 0);
}
uint32_t task_call_update(Task *task) {
  return task_vcall(task, &((TaskVtbl *)hw_ptr(task->vtable))->update, 1, 0);
}
uint32_t task_call_draw(Task *task) {
  return task_vcall(task, &((TaskVtbl *)hw_ptr(task->vtable))->draw, 1, 0);
}
uint32_t task_call_destroy(Task *task, int32_t free_memory) {
  return task_vcall(task, &((TaskVtbl *)hw_ptr(task->vtable))->destroy, 2, (uint32_t)free_memory);
}

/* ---- building the tree ---- */

/* the Task part of a constructor (as func_8C0165C4 and its copy func_8C016B5C, which stay translated: the runs never
 * call them; Game's constructor has the same code inline). The pause state and the vtable are left to the caller. */
void task_init(Task *task, gaddr parent) {
  task->linger = 0;
  task->active = 1;
  task->stops_on_pause = 1;
  task->child = 0;
  task->next = 0;
  task->parent = parent;
  task->owner = 0;
}

/* was func_8C015200 (and its copies: the Task constructor every subclass inlines): a fresh task under `parent`;
 * returns the task */
gaddr task_construct(gaddr task, gaddr parent) {
  task_init(task_at(task), parent);
  return task;
}
HW_EXPORT(8c015200, RET(task_construct(ARG0, ARG1)))

/* was func_8C0102C0: make `first` (and the siblings chained after it) the first children of `parent`; the
 * parent's existing children follow them. The new tasks take the parent and its owner. */
void task_add_children(Task *parent, Task *first) {
  if (parent->child) {
    Task *last = first;
    while (last->next)
      last = task_at(last->next);
    last->next = parent->child;
  }
  parent->child = hw_addr(first);
  first->parent = hw_addr(parent);
  first->owner = parent->owner;
}
HW_EXPORT(8c0102c0, task_add_children(task_at(ARG0), task_at(ARG1)))

/* ---- the per-frame passes ---- */

/* was func_8C010306: one update pass over a task, its children and its following siblings (recursively).
 * `link` is the guest address of the pointer that holds this task (its parent's child field or its previous
 * sibling's next field, 0 for a root): a dead task is unlinked through it. Resumable: an update may end the frame
 * (a scene that loads its data while the screen keeps running). */
void task_update_tree(gaddr task_addr, gaddr link, int32_t paused) {
  HW_RESUMABLE(0x8C010306u, task_addr, link, paused);
  Task *task = task_at(task_addr);
  HW_RESUME_AT(0, 1, 2, 3);

  task->paused = paused;
  if (task->active && !(task->paused && task->stops_on_pause))
    HW_YIELDING(0, task_call_update(task));

  if (!task->active) {
    if (task->child) {
      /* switching a task off switches its children off; it goes once they are gone */
      for (gaddr c = task->child; c; c = task_at(c)->next)
        task_at(c)->active = 0;
    } else {
      task->linger--;
    }
  }

  if (task->child)
    HW_YIELDING(1, task_update_tree(task->child, task_addr + offsetof(Task, child), task->paused));
  if (task->next)
    HW_YIELDING(2, task_update_tree(task->next, task_addr + offsetof(Task, next), task->paused));

  if (task->linger < 0) {
    hw_write32(link, task->next); /* unlink (a root has no link: the original writes to address 0 then) */
    if (task_addr)
      HW_YIELDING(3, task_call_destroy(task, 1));
  }
}
HW_EXPORT_RESUMABLE(8c010306, task_update_tree(ARG0, ARG1, (int32_t)ARG2))

/* was func_8C0103B8: draw a task, its children and its following siblings (the siblings by iteration). */
void task_draw_tree(Task *task) {
  for (;;) {
    if (task->active)
      task_call_draw(task);
    if (task->child)
      task_draw_tree(task_at(task->child));
    if (!task->next)
      break;
    task = task_at(task->next);
  }
}
HW_EXPORT(8c0103b8, task_draw_tree(task_at(ARG0)))

/* was func_8C0103F0: destroy a task with all its children and following siblings, deepest first. */
void task_destroy_tree(Task *task) {
  if (task->child) {
    task_destroy_tree(task_at(task->child));
    task->child = 0;
  }
  if (task->next) {
    task_destroy_tree(task_at(task->next));
    task->next = 0;
  }
  task_call_destroy(task, 1); /* (the original first tests `task` for NULL, which a caller never passes) */
}
HW_EXPORT(8c0103f0, task_destroy_tree(task_at(ARG0)))

/* was func_8C019D0A */
int32_t task_is_paused(const Task *task) { return task->paused; }
HW_EXPORT(8c019d0a, RET(task_is_paused(task_at(ARG0))))

/* ---- task trees ---- */

/* was func_8C010430: the root belongs to the tree, starts unpaused, one update pass per frame. */
TaskTree *tasktree_attach_root(TaskTree *tree) {
  Task *root = task_at(tree->root);
  root->owner = hw_addr(tree);
  root->paused = 0;
  tree->updates_per_frame = 1;
  return tree;
}
HW_EXPORT(8c010430, RET(hw_addr(tasktree_attach_root(tasktree_at(ARG0)))))

/* was func_8C0104D2: this frame's update passes over the whole tree. Resumable (task_update_tree). */
void tasktree_update(gaddr tree_addr) {
  int32_t passes_left = 0;
  HW_RESUMABLE(0x8C0104D2u, tree_addr, passes_left);
  TaskTree *tree = tasktree_at(tree_addr);
  HW_RESUME_AT(0);

  for (passes_left = tree->updates_per_frame; passes_left > 0; passes_left--)
    HW_YIELDING(0, task_update_tree(tree->root, 0, task_at(tree->root)->paused));
}
HW_EXPORT_RESUMABLE(8c0104d2, tasktree_update(ARG0))

/* as func_8C010508 (stays translated: only the soft reset reaches it, never on the tested runs) */
void tasktree_set_pause(TaskTree *tree, int32_t paused) {
  task_at(tree->root)->paused = paused;
}
