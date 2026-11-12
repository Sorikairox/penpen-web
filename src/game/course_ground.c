/* Pen Pen port, S15 b07: the course's ground: which cell a position is on (a quad test per cell, game/course.c's
 * quad_contains), and the kind of the segment a cell is in. */
#include "hw.h"
#include "../core/vec.h"
#include "../core/contact.h"
#include "course.h"
#include "racer.h"

static const CourseGround *course_ground(const Course *c) { return (const CourseGround *)hw_ptr(c->entry64); }
static const CourseGroundStrip *ground_strip(const CourseGround *g) {
  return (const CourseGroundStrip *)hw_ptr(g->strip);
}

/* the offsets from a hint cell that course_cell_near tries first, in order (5 int32) */
#define g_cell_near_offsets ((const int32_t *)hw_ptr(0x8C0832F4u))
#define CELL_NEAR_TRIES 5

/* a Contacts and the word after it (4 after a reset; as Chain and Effect keep them) */
typedef struct ContactsSlot {
  Contacts contacts;       /* 0x00 */
  int32_t word;            /* 0x54 */
} ContactsSlot;
HW_OFFSET(ContactsSlot, word, 0x54);

/* func_8C0146BC (still translated): collide the contacts with a chunk model of the course */
static void contacts_test_chunk(gaddr contacts, gaddr chunk) { CALL2(0x8C0146BCu, contacts, chunk); }

/* func_8C041D54 / func_8C041D6C (game/course.c, static there): entry i of the course's word list 0 / 1 is set */
static int32_t list_entry_set_0(gaddr course, int32_t i) { return (int32_t)CALL2(0x8C041D54u, course, i); }
static int32_t list_entry_set_1(gaddr course, int32_t i) { return (int32_t)CALL2(0x8C041D6Cu, course, i); }

/* the chunks of the course's lists 0 and 1 at the cells around `cell` (cell - 1 .. cell + 1, those that are cells
 * of the course and have an entry), tested against the contacts */
static void course_test_chunks_around(gaddr course, gaddr contacts, int32_t cell) {
  const Course *c = course_at(course);
  int32_t i = cell - 1;
  for (int32_t n = 3; n > 0; n--, i++) {
    if (course_cell_valid(course, i) == 0) continue;
    if (list_entry_set_0(course, i) != 0)
      contacts_test_chunk(contacts, c->chunks[0] + (uint32_t)((const int32_t *)hw_ptr(c->lists[0]))[i]);
    if (list_entry_set_1(course, i) != 0)
      contacts_test_chunk(contacts, c->chunks[1] + (uint32_t)((const int32_t *)hw_ptr(c->lists[1]))[i]);
  }
}

/* was func_8C04181E: the kind of the segment cell `cell` is in (the first segment starting at or after it) */
int32_t course_segment_kind(gaddr course, int32_t cell) {
  return ((const CourseSegment *)hw_ptr(course_find_segment(course, cell)))->kind;
}
HW_EXPORT(8c04181e, RET(course_segment_kind(ARG0, (int32_t)ARG1)))

/* was func_8C041A7C: the cell whose quad holds the position (seen from above), searched from the first; -1 when
 * none does. (The original also copies the four corners of cell 0xBD into unused locals: a leftover with no
 * effect, not kept.) */
int32_t course_cell_search(gaddr course, gaddr pos) {
  const Course *c = course_at(course);
  const CourseGround *g = course_ground(c);
  gaddr vertices = g->vertices;
  gaddr quad = ground_strip(g)->quads;
  for (int32_t i = 0; i < c->cell_count; i++, quad += sizeof(CellQuad)) {
    if (quad_contains(course, pos, quad, vertices) != 0) return i;
  }
  return -1;
}
HW_EXPORT(8c041a7c, RET(course_cell_search(ARG0, ARG1)))

/* was func_8C041B34: the cell the position is on: first the cells at the five offsets from `hint` (those that are
 * cells of the course), else the search from the first cell */
int32_t course_cell_near(gaddr course, gaddr pos, int32_t hint) {
  const CourseGround *g = course_ground(course_at(course));
  gaddr quads = ground_strip(g)->quads;
  gaddr vertices = g->vertices;
  int32_t offsets[CELL_NEAR_TRIES]; /* the original copies the table to a local first */
  memcpy(offsets, g_cell_near_offsets, sizeof offsets);
  int32_t found = -1;
  for (int32_t k = 0; k < CELL_NEAR_TRIES; k++) {
    int32_t cell = offsets[k] + hint;
    if (course_cell_valid(course, cell) == 0) continue;
    if (quad_contains(course, pos, quads + (uint32_t)cell * sizeof(CellQuad), vertices) != 0) {
      found = cell;
      break;
    }
  }
  if (found == -1) found = course_cell_search(course, pos);
  return found;
}
HW_EXPORT(8c041b34, RET(course_cell_near(ARG0, ARG1, (int32_t)ARG2)))

/* was func_8C041880: a Collider's step on the course: its contacts emptied, its cell found again near the last
 * one (the last noted in word4), the course's chunks around it tested, and the kind of its segment */
void course_collider_update(gaddr course, gaddr collider) {
  Collider *k = (Collider *)hw_ptr(collider);
  k->contacts.count = 0;
  k->contacts_word = 4;
  int32_t hint = k->word4;
  k->word4 = k->contacts.cell;
  k->contacts.cell = course_cell_near(course, k->position, hint);
  course_test_chunks_around(course, member_addr(collider, Collider, contacts), k->contacts.cell);
  k->segment_kind = course_segment_kind(course, k->contacts.cell);
}
HW_EXPORT(8c041880, course_collider_update(ARG0, ARG1))

/* was func_8C041928: the same for a bare Contacts whose owner is the position: emptied, the cell (*cell, kept by
 * the caller) found again near the last one, the course's chunks around it tested */
void course_contacts_update(gaddr course, gaddr contacts, gaddr cell) {
  ContactsSlot *s = (ContactsSlot *)hw_ptr(contacts);
  s->contacts.count = 0;
  s->word = 4;
  int32_t *at = (int32_t *)hw_ptr(cell);
  int32_t found = course_cell_near(course, s->contacts.owner, *at);
  *at = found;
  course_test_chunks_around(course, contacts, found);
}
HW_EXPORT(8c041928, course_contacts_update(ARG0, ARG1, ARG2))
