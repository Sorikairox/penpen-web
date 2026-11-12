/* Pen Pen port, S14 (batch b08): the Course's construction and deletion (func_8C0409B8, func_8C041114,
 * func_8C0411EC, func_8C040984).
 *
 * The constructor picks the course's variant from the race set-up (0, 1 or 2), loads the course file (a data
 * container: the cell strip, the word lists, the camera path, the segments, the actors, the four tracks, the cell
 * attributes), marks the start and finish segments and cells for the set-up's mode and variant, loads the model
 * file (the texture list, four chunk models, the background's texture list and the part blocks, read one after the
 * other as Ninja chunks), the textures, the second file ("otpa": per CourseEnv two model sets, by the course's set
 * flags) and the course's static words. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"
#include "../core/task.h"
#include "../core/file.h"
#include "course.h"
#include "race_scene.h"

/* ---- calls into code that is still translated ---- */

/* func_8C0120CA (core/texture.c, not declared in a header): load the textures of `name` into `texlist` */
static void texture_load(gaddr name, gaddr texlist, gaddr dir_name) { CALL3(0x8C0120CAu, name, texlist, dir_name); }

/* func_8C062800 (SHC runtime): construct `n` objects of `size` bytes at `p` with the constructor `ctor` */
static void array_construct(int32_t n, uint32_t size, gaddr p, gaddr ctor) { CALL4(0x8C062800u, n, size, p, ctor); }

/* ---- the files' tables ---- */

#define COURSE_SHARED_TEXLIST 0x8C09703Cu      /* the texture list the courses share */
#define COURSE_SHARED_TEXTURES 0x8C084480u     /* its file name (a .pvm) */
#define COURSE_FILE_NAMES 0x8C096DE0u          /* per variant, a name per course: the course file (container) */
#define COURSE_MODEL_FILE_NAMES 0x8C096E1Cu    /* per variant, per course: the model file (chunks) */
#define COURSE_TEXTURE_NAMES 0x8C096E58u       /* per variant, per course: the course's textures */
#define COURSE_FILE2_NAMES 0x8C096E64u         /* per course: the second file ("otpa") */
#define COURSE_SET_FLAGS 0x8C096E74u           /* per course, a 16-bit word per CourseEnv: the model sets it has */
#define COURSE_STATIC_WORDS 0x8C096E94u        /* per course, the 8 words of Course.course_words */
#define COURSE_BG_TEXTURE_NAMES 0x8C096F14u    /* per course: the background's textures */
#define COURSE_PART_COUNTS ((const int32_t *)hw_ptr(0x8C0832E4u)) /* part blocks per course */
#define COURSE_ENV_CONSTRUCT 0x8C040984u       /* CourseEnv's constructor (a jump to func_8C042668) */

/* a name of a per-variant table (a pointer per variant to a name per course) */
static gaddr variant_course_name(gaddr table, int32_t variant, int32_t course) {
  gaddr names = ((const gaddr *)hw_ptr(table))[variant];
  return ((const gaddr *)hw_ptr(names))[course];
}
static gaddr course_name(gaddr table, int32_t course) { return ((const gaddr *)hw_ptr(table))[course]; }

/* ---- the course file ---- */

/* the entries are core/file.h's ContainerEntry: `offset` is the data's address once loaded */

/* the course file's entries, in order */
enum {
  CF_LIST1, CF_LIST2, CF_LIST0, CF_LIST3, CF_STRIP_HOLDER, CF_CELL_POINTS, CF_ENTRY6C, CF_ENTRY70 = CF_ENTRY6C + 1,
  CF_ENTRY74, CF_ENTRY78, CF_SEGMENTS, CF_ACTORS, CF_TRACKS /* 4 entries: the fog, light, weather and key tracks */,
  CF_CELL_ATTRS = CF_TRACKS + 4
};

/* entry CF_STRIP_HOLDER's data: its second word is the course's entry64, whose 0x0C points to the strip header */
typedef struct CourseStripHolder {
  uint32_t word0;   /* 0x00 */
  gaddr entry64;    /* 0x04: Course.entry64 */
} CourseStripHolder;
typedef struct CourseEntry64 {
  uint8_t pad_00[0x0C];
  gaddr strip;      /* 0x0C */
} CourseEntry64;
HW_OFFSET(CourseEntry64, strip, 0x0C);
typedef struct CourseStrip {
  uint16_t word0;       /* 0x00 */
  uint16_t cell_count;  /* 0x02 */
} CourseStrip;

static const ContainerEntry *container_entries(gaddr file) { return (const ContainerEntry *)hw_ptr(file); }

/* the set-up's mode, variant and course (func_8C040930 / 34 / 38 on race_setup_get()) */
static int32_t setup_mode(void) { return race_setup_at(race_setup_get())->mode; }
static int32_t setup_variant(void) { return race_setup_at(race_setup_get())->variant; }
static int32_t setup_course(void) { return race_setup_at(race_setup_get())->course; }

/* which of the course's three variants the set-up's mode and variant play */
static int32_t course_variant_for_setup(void) {
  switch (setup_mode()) {
  case 0: case 5: return setup_variant() == 0 ? 1 : 0;
  case 4: return 1;
  case 2: case 3: return 2;
  default: return 0;
  }
}

/* the first and the end segment of the race (an end segment of 8 = the whole course) */
static void course_race_segments(int32_t *first, int32_t *end) {
  *first = 0;
  *end = 8;
  switch (setup_mode()) {
  case 0: case 5:
    switch (setup_variant()) {
    case 0: case 1: case 4: *end = 6; break;
    case 3: *end = 8; break;
    default: break;
    }
    break;
  case 1: case 2: case 3: case 4:
    switch (setup_variant()) {
    case 0: *first = 0; *end = 8; break;
    case 1: *first = 0; *end = 2; break;
    case 2: *first = 2; *end = 4; break;
    case 3: *first = 4; *end = 6; break;
    case 4: *first = 6; *end = 8; break;
    default: break;
    }
    break;
  default:
    break;
  }
}

/* the out-parameters of nj_ReadChunkFile live in the original's frame (0x70 bytes below the entry's r15) */
typedef struct CourseBuildFrame {
  uint8_t pad_00[0x3C];
  uint32_t chunk_offset;  /* 0x3C: where the next chunk starts (0 before the first) */
  uint32_t chunk_aux;     /* 0x40 */
  uint8_t pad_44[0x70 - 0x44];
} CourseBuildFrame;
HW_OFFSET(CourseBuildFrame, chunk_offset, 0x3C);
HW_SIZE(CourseBuildFrame, 0x70);

/* the next chunk of the model file */
static gaddr read_chunk(gaddr data, gaddr frame) {
  return nj_ReadChunkFile(data, member_addr(frame, CourseBuildFrame, chunk_offset),
                          member_addr(frame, CourseBuildFrame, chunk_aux));
}

/* was func_8C0409B8: the Course's construction (see the file's comment); returns it */
gaddr course_construct_body(gaddr course) {
  gaddr frame = hw_stack_alloc(sizeof(CourseBuildFrame));
  Course *c = course_at(course);
  task_construct(course, 0);
  word_construct(member_addr(course, Course, word24), 0);
  array_construct(COURSE_ENVS, sizeof(CourseEnv), member_addr(course, Course, envs), COURSE_ENV_CONSTRUCT);
  method_self(member_addr(course, Course, vec5734)); /* the Vec3 constructors (empty) */
  method_self(member_addr(course, Course, vec5740));
  method_self(member_addr(course, Course, vec5770));
  c->word28 = 0;
  int32_t variant = course_variant_for_setup();
  c->variant = variant;
  texture_load(COURSE_SHARED_TEXTURES, COURSE_SHARED_TEXLIST, 0);
  c->course = setup_course();

  /* the course file */
  gaddr file = container_load(variant_course_name(COURSE_FILE_NAMES, variant, c->course), 0);
  const ContainerEntry *e = container_entries(file);
  c->file = file;
  c->lists[1] = e[CF_LIST1].offset;
  c->lists[2] = e[CF_LIST2].offset;
  c->lists[0] = e[CF_LIST0].offset;
  c->lists[3] = e[CF_LIST3].offset;
  const CourseStripHolder *holder = (const CourseStripHolder *)hw_ptr((gaddr)e[CF_STRIP_HOLDER].offset);
  c->entry64 = holder->entry64;
  c->cell_count = ((const CourseStrip *)hw_ptr(((const CourseEntry64 *)hw_ptr(c->entry64))->strip))->cell_count;
  c->texlist = 0;
  c->cell_points = e[CF_CELL_POINTS].offset;
  c->entries6c[0] = e[CF_ENTRY6C].offset;
  c->entries6c[1] = e[CF_ENTRY70].offset;
  c->entries6c[2] = e[CF_ENTRY74].offset;
  c->entries6c[3] = e[CF_ENTRY78].offset;
  c->segments = e[CF_SEGMENTS].offset;
  c->actors = e[CF_ACTORS].offset;
  c->fog_track = e[CF_TRACKS + 0].offset;
  c->light_track = e[CF_TRACKS + 1].offset;
  c->weather_track = e[CF_TRACKS + 2].offset;
  c->key_track = e[CF_TRACKS + 3].offset;
  c->cell_attrs = e[CF_CELL_ATTRS].offset;

  /* the race's start and finish: segment kinds 3 and 1, cell kinds 1 and 6 */
  int32_t first, end;
  course_race_segments(&first, &end);
  c->start_segment = first / 2;
  c->finish_segment = end / 2;
  CourseSegment *segments = (CourseSegment *)hw_ptr(c->segments);
  c->start_cell = segments[first].first_cell;
  c->finish_cell = segments[end - 1].first_cell + 1;
  segments[first].kind = 3;
  segments[end].kind = 1;
  uint16_t *attrs = (uint16_t *)hw_ptr(c->cell_attrs);
  attrs[c->start_cell] = (uint16_t)((attrs[c->start_cell] & 0xFFF0u) | 1u);
  attrs[c->finish_cell] = (uint16_t)((attrs[c->finish_cell] & 0xFFF0u) | 6u);

  /* in variant 1 the lane marks (kinds 8, 9) take the kind of the last lane cell (0-5) before them */
  if (c->variant == 1) {
    uint16_t *attr = (uint16_t *)hw_ptr(c->cell_attrs);
    uint32_t last_kind = 2;
    for (int32_t i = 0; i < c->cell_count; i++, attr++) {
      uint32_t kind = CELL_KIND(*attr);
      if (kind <= 5)
        last_kind = kind;
      else if (kind == 8 || kind == 9)
        *attr = (uint16_t)((*attr & 0xFFF0u) | (uint16_t)last_kind);
    }
  }

  /* the model file: chunks one after the other */
  CourseBuildFrame *f = (CourseBuildFrame *)hw_ptr(frame);
  f->chunk_offset = 0;
  gaddr models = file_load_fixed(variant_course_name(COURSE_MODEL_FILE_NAMES, variant, c->course), 0);
  c->texlist = read_chunk(models, frame);
  c->chunks[0] = read_chunk(models, frame);
  c->chunks[1] = read_chunk(models, frame);
  c->chunks[2] = read_chunk(models, frame);
  c->chunks[3] = read_chunk(models, frame);
  c->bg_texlist = read_chunk(models, frame);
  c->part_blocks_at = member_addr(course, Course, part_blocks);
  for (int32_t i = 0; i < COURSE_PART_COUNTS[c->course]; i++) c->part_blocks[i] = read_chunk(models, frame);
  texture_load(variant_course_name(COURSE_TEXTURE_NAMES, variant, c->course), c->texlist, 0);

  /* the second file: per CourseEnv two model sets; a set flag's low nibble says whether the set has its three
   * entries (model, motion, texture list; the first set's third entry's `a` is the env's word), the next nibble
   * whether it has the extra entry */
  gaddr file2 = container_load(course_name(COURSE_FILE2_NAMES, c->course), 0);
  const ContainerEntry *e2 = container_entries(file2);
  c->file2 = file2;
  const uint16_t *set_flags = (const uint16_t *)hw_ptr(COURSE_SET_FLAGS) + (uint32_t)c->course * 4u;
  for (int32_t env = 0; env < COURSE_ENVS; env++) {
    uint16_t flags = set_flags[env];
    for (int32_t set = 0; set < 2; set++) {
      CourseModelSet *m = &c->models[env][set];
      if ((flags & 0xF) != 0) {
        m->model = e2[0].offset;
        m->motion = e2[1].offset;
        if (set == 0) c->env_word[env] = (gaddr)e2[2].a;
        m->texlist = e2[2].offset;
        e2 += 3;
      } else {
        m->model = 0;
        m->motion = 0;
        m->texlist = 0;
      }
      flags = (uint16_t)(flags >> 4);
      if ((flags & 0xF) != 0) {
        m->extra = e2[0].offset;
        e2 += 1;
      } else {
        m->extra = 0;
      }
      flags = (uint16_t)(flags >> 4);
    }
  }
  const uint32_t *words = (const uint32_t *)hw_ptr(COURSE_STATIC_WORDS) + (uint32_t)c->course * 8u;
  for (int32_t i = 0; i < 8; i++) c->course_words[i] = words[i];
  texture_load(course_name(COURSE_BG_TEXTURE_NAMES, c->course), c->bg_texlist, 0);
  hw_stack_free(sizeof(CourseBuildFrame));
  return course;
}
HW_EXPORT(8c0409b8, RET(course_construct_body(ARG0)))

/* was func_8C041114: the Course's constructor: its vtable, then the construction */
gaddr course_construct(gaddr course) {
  course_at(course)->task.vtable = COURSE_VTABLE;
  course_construct_body(course);
  return course;
}
HW_EXPORT(8c041114, RET(course_construct(ARG0)))

/* was func_8C0411EC: the Course's deleting destructor (vtable 0x8C23DB5C's first method) */
gaddr course_delete(gaddr course, int32_t free_memory) {
  course_release(course);
  if (free_memory != 0) syFree(course);
  return course;
}
HW_EXPORT(8c0411ec, RET(course_delete(ARG0, (int32_t)ARG1)))

/* was func_8C040984: a jump to CourseEnv's constructor (the one the course's array construction is given) */
HW_EXPORT(8c040984, RET(course_env_construct(ARG0)))
