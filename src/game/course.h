/* Pen Pen port, S14: the course of a race and each player's view of it (game/course.c, game/camera.c, game/actor.c,
 * game/stage.c).
 *
 * The race scene (game/race.h) builds one Course (0x5790 bytes, constructor func_8C041114 -> func_8C0409B8, vtable
 * 0x8C23DB5C, a Task) at its 0x38. The course loads the course file (its entries: the cell table, the segment list,
 * the actor placements, the tracks of the fog, lights and weather, ...), the course's chunk models and textures, and
 * keeps one CourseEnv per player view (fog, lights, weather particles, cell keys; func_8C042668 constructs them, four
 * at 0x4EE4). The ground is a strip of cells: each has a 16-bit attribute word (the low 4 bits are the ground kind,
 * bits 6 and 7 switch the fog on; 1 marks the start cell, 6 the finish) and the course is cut into segments (6-byte
 * records, sorted by their first cell, -1-terminated). */
#ifndef PENPEN_COURSE_H
#define PENPEN_COURSE_H

#include "../core/task.h"
#include "../core/vec.h"

/* a segment of the course: its first cell and its kind (3 = where the race starts, 1 = where it ends) */
typedef struct CourseSegment {
  int16_t first_cell; /* 0x00: the sort key; -1 ends the list */
  int16_t unknown2;   /* 0x02 */
  int16_t kind;       /* 0x04 */
} CourseSegment;
HW_SIZE(CourseSegment, 6);

/* a record of the fog track (0x1C bytes, sorted by cell): from `cell` on, the fog mode and its height */
typedef struct FogKey {
  int16_t cell;            /* 0x00 */
  int16_t mode;            /* 0x02: 0, 1 = by the ground's attribute; 2, 3 = by height */
  uint8_t pad_04[0x0C - 0x04];
  f32 height;              /* 0x0C: the fog is on while the camera is below it (modes 2, 3) */
  uint8_t pad_10[0x1C - 0x10];
} FogKey;
HW_OFFSET(FogKey, height, 0x0C);
HW_SIZE(FogKey, 0x1C);

/* a record of the light track (0x28 bytes, sorted by cell): the three points the light rig moves toward */
typedef struct LightKey {
  int16_t cell;            /* 0x00 */
  uint8_t pad_02[2];
  Vec3 points[3];          /* 0x04: for the rig's Bodies 0, 1, 2 */
} LightKey;
HW_OFFSET(LightKey, points, 0x04);
HW_SIZE(LightKey, 0x28);

/* a record of the cell-key track: 6 bytes {cell, a, b}, sorted by cell */
typedef struct CellKey {
  int16_t cell; /* 0x00 */
  int16_t a;    /* 0x02 */
  int16_t b;    /* 0x04 */
} CellKey;
HW_SIZE(CellKey, 6);

/* the cell keys of a course: the list and two words handed with it (copied together) */
typedef struct CellKeys {
  gaddr list;          /* 0x00: CellKey[] */
  uint32_t word4;      /* 0x04 */
  uint32_t word8;      /* 0x08 */
} CellKeys;
HW_SIZE(CellKeys, 12);

/* A player's view of the course (0x1BC bytes, constructor func_8C042668, set up by func_8C0427DC, updated by
 * func_8C042952): the camera's place on the course, the fog track (the fog of the player's View switches by the
 * ground or the height), the light rig (three Bodies that follow the light track), the weather particles (snow,
 * sparkles: up to `particle_max` Bodies drawn as 3D sprites) and the cell keys. */
typedef struct CourseEnv {
  int32_t view;            /* 0x000: the player and its View (g_views[view]) */
  int32_t active;          /* 0x004 */
  Vec3 eye;                /* 0x008: the camera's position (eye.y is compared with the fog's height) */
  uint8_t pad_014[0x020 - 0x014];
  Vec3 vec20;              /* 0x020 */
  Vec3 vec2c;              /* 0x02C */
  int32_t cell;            /* 0x038: the cell of the course the camera is over (the tracks are keyed by it) */
  Quat rot3c;              /* 0x03C */
  Quat rot4c;              /* 0x04C */
  Body body5c;             /* 0x05C */
  int32_t fog_mode;        /* 0x08C: the mode of the fog key in use */
  gaddr fog_track;         /* 0x090: FogKey[] */
  int32_t no_fog;          /* 0x094: 1 in the game modes that switch the fog off (set-up modes other than 0, 4, 5) */
  Body lights[3];          /* 0x098: the light rig */
  gaddr light_track;       /* 0x128: {word, LightKey[]...}: read from +4 at set-up, walked as LightKey from 0 */
  gaddr particles;         /* 0x12C: Body[particle_max] */
  gaddr particle_anims;    /* 0x130: a texture animation number per particle */
  uint8_t pad_134[0x140 - 0x134];
  Vec3 push;               /* 0x140: the race scene adds to it (func_8C044EEA, a wind where the camera's word is
                            * 0x3A-0x3D) */
  int32_t particle_max;    /* 0x14C */
  int32_t particle_count;  /* 0x150: in use (the weather key's count, at most particle_max) */
  int32_t weather;         /* 0x154: the weather key's kind (2 = additive sprites) */
  uint8_t pad_158[0x170 - 0x158];
  gaddr weather_track;     /* 0x170: 20-byte records {cell, kind, count, ...} */
  CellKeys keys;            /* 0x174 */
  int32_t key_b;           /* 0x180: the b of the cell key in use */
  int32_t key_a;           /* 0x184: its a */
  int32_t fog_on;          /* 0x188 */
  uint32_t word18c;        /* 0x18C */
  gaddr table190;          /* 0x190 */
  gaddr course_table;      /* 0x194: the course's table (by the set-up's course) */
  int32_t slots[5];        /* 0x198: -1 = empty */
  Quat rot1ac;             /* 0x1AC */
} CourseEnv;
HW_OFFSET(CourseEnv, eye, 0x008);
HW_OFFSET(CourseEnv, vec20, 0x020);
HW_OFFSET(CourseEnv, cell, 0x038);
HW_OFFSET(CourseEnv, rot3c, 0x03C);
HW_OFFSET(CourseEnv, body5c, 0x05C);
HW_OFFSET(CourseEnv, fog_mode, 0x08C);
HW_OFFSET(CourseEnv, lights, 0x098);
HW_OFFSET(CourseEnv, light_track, 0x128);
HW_OFFSET(CourseEnv, push, 0x140);
HW_OFFSET(CourseEnv, particle_max, 0x14C);
HW_OFFSET(CourseEnv, weather_track, 0x170);
HW_OFFSET(CourseEnv, keys, 0x174);
HW_OFFSET(CourseEnv, fog_on, 0x188);
HW_OFFSET(CourseEnv, course_table, 0x194);
HW_OFFSET(CourseEnv, slots, 0x198);
HW_OFFSET(CourseEnv, rot1ac, 0x1AC);
HW_SIZE(CourseEnv, 0x1BC);

#define COURSE_ENVS 4

/* models from the course's second file: three words of an entry and one of the next (0 where there is none) */
typedef struct CourseModelSet {
  gaddr model, motion, texlist; /* 0x00 */
  gaddr extra;                  /* 0x0C */
} CourseModelSet;
HW_SIZE(CourseModelSet, 16);

typedef struct Course {
  Task task;               /* 0x0000: vtable 0x8C23DB5C */
  uint32_t word24;         /* 0x0024 */
  uint32_t word28;         /* 0x0028 */
  int32_t course;          /* 0x002C: which course (indexes the per-course tables) */
  int32_t cell_count;      /* 0x0030 */
  int32_t variant;         /* 0x0034: 0, 1 or 2, by the set-up's mode and variant */
  gaddr file;              /* 0x0038: the course file (container) */
  gaddr file2;             /* 0x003C: the second file ("otpa") */
  gaddr chunks[4];         /* 0x0040: chunk models read from the course's model file */
  gaddr texlist;           /* 0x0050 */
  gaddr lists[4];          /* 0x0054: -1-terminated word lists (the last two may be 0) */
  gaddr entry64;           /* 0x0064 */
  gaddr cell_points;       /* 0x0068: 12 bytes per cell (three floats: the camera's path) */
  gaddr entries6c[4];      /* 0x006C */
  uint8_t pad_07c[0x084 - 0x07C];
  gaddr segments;          /* 0x0084: CourseSegment[] */
  gaddr actors;            /* 0x0088: the actor placements: 32-byte records, -1-terminated */
  gaddr fog_track;         /* 0x008C: FogKey[] (every CourseEnv gets these four at set-up) */
  gaddr light_track;       /* 0x0090: the light rig's data */
  gaddr weather_track;     /* 0x0094 */
  gaddr key_track;         /* 0x0098: CellKey[]; a CourseEnv copies the three words from here as its CellKeys */
  gaddr part_blocks_at;    /* 0x009C: = &part_blocks */
  gaddr bg_texlist;        /* 0x00A0 */
  gaddr part_blocks[4];    /* 0x00A4: COURSE_PART_COUNTS[course] chunk blocks */
  gaddr cell_attrs;        /* 0x00B4: uint16_t per cell */
  uint8_t pad_0b8[0x4EE0 - 0x0B8];
  int32_t env_count;       /* 0x4EE0: the CourseEnvs in use (1, 2 or 4 by the set-up's mode) */
  CourseEnv envs[COURSE_ENVS]; /* 0x4EE4 */
  uint32_t view_bits[4][10]; /* 0x55D4: 320 bits per view (course_row_bit), cleared at set-up */
  CourseModelSet models[4][2]; /* 0x5674: per env, two sets (zero where the course has none) */
  gaddr env_word[4];       /* 0x56F4 */
  uint32_t course_words[8]; /* 0x5704: from the course's static table */
  int32_t start_cell;      /* 0x5724 */
  int32_t finish_cell;     /* 0x5728 */
  int32_t start_segment;   /* 0x572C */
  int32_t finish_segment;  /* 0x5730 */
  Vec3 vec5734;            /* 0x5734 */
  Vec3 vec5740;            /* 0x5740 */
  uint8_t pad_574c[0x5764 - 0x574C];
  uint32_t word5764;       /* 0x5764 */
  uint8_t pad_5768[0x5770 - 0x5768];
  Vec3 vec5770;            /* 0x5770 */
  uint8_t pad_577c[0x5790 - 0x577C];
} Course;
HW_OFFSET(Course, word24, 0x0024);
HW_OFFSET(Course, course, 0x002C);
HW_OFFSET(Course, chunks, 0x0040);
HW_OFFSET(Course, lists, 0x0054);
HW_OFFSET(Course, segments, 0x0084);
HW_OFFSET(Course, fog_track, 0x008C);
HW_OFFSET(Course, key_track, 0x0098);
HW_OFFSET(Course, part_blocks, 0x00A4);
HW_OFFSET(Course, cell_attrs, 0x00B4);
HW_OFFSET(Course, env_count, 0x4EE0);
HW_OFFSET(Course, envs, 0x4EE4);
HW_OFFSET(Course, view_bits, 0x55D4);
HW_OFFSET(Course, models, 0x5674);
HW_OFFSET(Course, env_word, 0x56F4);
HW_OFFSET(Course, course_words, 0x5704);
HW_OFFSET(Course, start_segment, 0x572C);
HW_OFFSET(Course, vec5734, 0x5734);
HW_OFFSET(Course, word5764, 0x5764);
HW_OFFSET(Course, vec5770, 0x5770);
HW_SIZE(Course, 0x5790);
#define COURSE_VTABLE 0x8C23DB5Cu

static inline Course *course_at(gaddr a) { return (Course *)hw_ptr(a); }
static inline CourseEnv *course_env_at(gaddr a) { return (CourseEnv *)hw_ptr(a); }

/* the ground kind of a cell attribute word */
#define CELL_KIND(attr) ((attr) & 0xF)

#endif
