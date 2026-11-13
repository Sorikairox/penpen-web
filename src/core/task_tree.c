/* Pen Pen port, S17 (b01): the rest of the task tree's methods (core/task.h): adding siblings, the TaskTree that
 * holds its own root task, its destruction and the pause toggle. */
#include "task.h"
#include "sdk.h"
#include "../core/vec.h"

/* a TaskTree constructed with its root task inside it (the root's vtable is the caller's to set) */
typedef struct TaskTreeWithRoot {
  TaskTree tree; /* 0x00: tree.root points at root_task */
  Task root_task; /* 0x08 */
} TaskTreeWithRoot;
HW_OFFSET(TaskTreeWithRoot, root_task, 0x08);

/* was func_8C0102E2: put `first` (and the siblings chained after it) right after `task` among its siblings; the
 * siblings that followed `task` follow them. The new tasks take the task's parent and owner. */
void task_add_siblings(Task *task, Task *first) {
  if (task->next) {
    Task *last = first;
    while (last->next)
      last = task_at(last->next);
    last->next = task->next;
  }
  task->next = hw_addr(first);
  first->parent = task->parent;
  first->owner = task->owner;
}
HW_EXPORT(8c0102e2, task_add_siblings(task_at(ARG0), task_at(ARG1)))

/* was func_8C010442: the constructor of a TaskTree whose root task is its own member: the root is live, stops on
 * pause, has no links yet (its pause word and vtable are left alone); then the root is attached. Returns the tree. */
gaddr tasktree_construct(gaddr tree) {
  TaskTreeWithRoot *t = (TaskTreeWithRoot *)hw_ptr(tree);
  t->tree.root = member_addr(tree, TaskTreeWithRoot, root_task);
  Task *root = task_at(t->tree.root);
  root->linger = 0;
  root->active = 1;
  root->stops_on_pause = 1;
  root->child = 0;
  root->next = 0;
  root->parent = 0;
  root->owner = 0;
  tasktree_attach_root(tasktree_at(tree));
  return tree;
}
HW_EXPORT(8c010442, RET(tasktree_construct(ARG0)))

/* was func_8C010470: destroy every task below the root (its children and its siblings); the root's links are left
 * as they are */
void tasktree_destroy_tasks(TaskTree *tree) {
  if (task_at(tree->root)->child)
    task_destroy_tree(task_at(task_at(tree->root)->child));
  if (task_at(tree->root)->next)
    task_destroy_tree(task_at(task_at(tree->root)->next));
}
HW_EXPORT(8c010470, tasktree_destroy_tasks(tasktree_at(ARG0)))

/* was func_8C01049C: the TaskTree's destructor: its tasks destroyed, its memory freed when asked; returns it */
gaddr tasktree_destroy(gaddr tree, int32_t free_memory) {
  tasktree_destroy_tasks(tasktree_at(tree));
  if (free_memory)
    syFree(tree);
  return tree;
}
HW_EXPORT(8c01049c, RET(tasktree_destroy(ARG0, (int32_t)ARG1)))

/* was func_8C0104C0: the same without freeing (a tree that is a member of another object); returns it */
gaddr tasktree_destroy_member(gaddr tree) {
  tasktree_destroy_tasks(tasktree_at(tree));
  return tree;
}
HW_EXPORT(8c0104c0, RET(tasktree_destroy_member(ARG0)))

/* was func_8C0104FC: pause or unpause the whole tree (flip the root's pause word); returns the new state */
int32_t tasktree_toggle_pause(TaskTree *tree) {
  Task *root = task_at(tree->root);
  root->paused = root->paused == 0;
  return root->paused;
}
HW_EXPORT(8c0104fc, RET(tasktree_toggle_pause(tasktree_at(ARG0))))
