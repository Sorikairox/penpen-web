/* Pen Pen port, S17 (b02): the Game's copies of the TaskTree's destruction (core/task_tree.c): a thunk to
 * tasktree_destroy_tasks and the two destructors built on it (the Game is a TaskTree, game/game.h). */
#include "hw.h"
#include "../core/task.h"

/* was func_8C019E2C: the thunk to tasktree_destroy_tasks (func_8C010470) */
HW_EXPORT(8c019e2c, tasktree_destroy_tasks(tasktree_at(ARG0)))
/* was func_8C019E32: as tasktree_destroy (func_8C01049C): the tasks destroyed, the memory freed when asked;
 * returns the tree */
HW_EXPORT(8c019e32, RET(tasktree_destroy(ARG0, (int32_t)ARG1)))
/* was func_8C019E56: as tasktree_destroy_member (func_8C0104C0): the tasks destroyed; returns the tree */
HW_EXPORT(8c019e56, RET(tasktree_destroy_member(ARG0)))
