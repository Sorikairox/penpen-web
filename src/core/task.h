/* Pen Pen port, S11: the task tree, the core of the game's object system.
 *
 * The game is written in C++ (Hitachi SHC). Every object that lives on screen (a scene, a menu, a racer, an effect)
 * derives from one base class, here `Task`: a node of a tree (first child + next sibling links) with a virtual
 * update, draw and destroy. Each frame the tree is updated from its root (task_update_tree) and drawn
 * (task_draw_tree); a task that is switched off (active = 0) switches its children off, waits until they are gone,
 * then unlinks itself and is destroyed.
 *
 * SHC's virtual tables hold one {function, this-adjustment} pair per method; a call passes `this + adjustment` (the
 * method's class may start before the Task part of the object, as in Game, game/game.h).
 *
 * A `TaskTree` is the holder of a tree: a pointer to its root task and how many update passes run per frame (the
 * main loop sets it to the number of V-blanks since the last frame, up to 5: the game catches up when it drops
 * frames). All layouts are the original's (checked below): translated code reads and writes the same objects. */
#ifndef PENPEN_TASK_H
#define PENPEN_TASK_H

#include "hw.h"

typedef struct VMethod {
  gaddr fn;            /* the method's code */
  int32_t this_adjust; /* added to the Task's address to make the method's `this` */
} VMethod;

typedef struct TaskVtbl {
  VMethod destroy;     /* (this, int free_memory) */
  VMethod update;      /* (this) once per update pass */
  VMethod draw;        /* (this) once per view */
} TaskVtbl;

typedef struct Task {
  int32_t linger;         /* 0x00: while inactive and childless, counts down; below 0 the task is destroyed */
  int32_t active;         /* 0x04: updated and drawn; 0 = being removed */
  int32_t stops_on_pause; /* 0x08: not updated while the tree is paused */
  int32_t paused;         /* 0x0C: the pause state handed down the tree this pass (the root's is the tree's) */
  gaddr child;            /* 0x10: first child (Task) */
  gaddr next;             /* 0x14: next sibling (Task) */
  gaddr parent;           /* 0x18 */
  gaddr owner;            /* 0x1C: the TaskTree (or object) the tree belongs to */
  gaddr vtable;           /* 0x20: TaskVtbl */
} Task;
HW_OFFSET(Task, child, 0x10);
HW_OFFSET(Task, vtable, 0x20);
HW_SIZE(Task, 0x24);

typedef struct TaskTree {
  gaddr root;              /* 0x00: the root Task */
  int32_t updates_per_frame; /* 0x04 */
} TaskTree;
HW_SIZE(TaskTree, 8);

static inline Task *task_at(gaddr a) { return (Task *)hw_ptr(a); }
static inline TaskTree *tasktree_at(gaddr a) { return (TaskTree *)hw_ptr(a); }

/* the task methods, as the original calls them */
uint32_t task_call_update(Task *task);
uint32_t task_call_draw(Task *task);
uint32_t task_call_destroy(Task *task, int32_t free_memory);

void task_init(Task *task, gaddr parent);
gaddr task_construct(gaddr task, gaddr parent);
void task_add_children(Task *parent, Task *first);
void task_update_tree(gaddr task, gaddr link, int32_t paused);
void task_draw_tree(Task *task);
void task_destroy_tree(Task *task);
int32_t task_is_paused(const Task *task);

TaskTree *tasktree_attach_root(TaskTree *tree);
void tasktree_update(gaddr tree);
void tasktree_set_pause(TaskTree *tree, int32_t paused);

#endif
