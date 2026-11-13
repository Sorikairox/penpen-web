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
#include "../core/pad.h" /* S15 b07: Course.pad */

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
  f32 near;                /* 0x04: S15 b07: the fog's near distance */
  f32 far;                 /* 0x08: S15 b07: its far distance */
  f32 height;              /* 0x0C: the fog is on while the camera is below it (modes 2, 3) */
  Vec3 color;              /* 0x10: S15 b07: the background / fog colour (r, g, b, 0..255 as floats) */
} FogKey;
HW_OFFSET(FogKey, near, 0x04);
HW_OFFSET(FogKey, far, 0x08);
HW_OFFSET(FogKey, height, 0x0C);
HW_OFFSET(FogKey, color, 0x10);
HW_SIZE(FogKey, 0x1C);

/* S15 b07: a record of the weather track (20 bytes, sorted by cell): from `cell` on, the particles' kind, how many
 * and the constant force that drifts them */
typedef struct WeatherKey {
  int16_t cell;            /* 0x00 */
  int16_t kind;            /* 0x02: 0..3 (picks the texture animations and the particles' spread) */
  int16_t count;           /* 0x04: at most the CourseEnv's particle_max */
  uint8_t pad_06[2];
  Vec3 drift;              /* 0x08 */
} WeatherKey;
HW_OFFSET(WeatherKey, count, 0x04);
HW_OFFSET(WeatherKey, drift, 0x08);
HW_SIZE(WeatherKey, 0x14);

/* S15 b07: a record of a course's sound table (CourseEnv.course_table, 8 bytes, -1-terminated): while the camera's
 * cell is in first..last the sound plays on channel `flags & 0xF` (bit 7: the music player's, bit 6: not in
 * course variant 1) */
typedef struct CourseSoundKey {
  int16_t first_cell;      /* 0x00 */
  int16_t last_cell;       /* 0x02 */
  int16_t flags;           /* 0x04 */
  int16_t sound;           /* 0x06 */
} CourseSoundKey;
HW_SIZE(CourseSoundKey, 8);

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
  Vec3 last_eye;           /* 0x014: S15 b07: the eye when the weather particles were last placed */
  Vec3 vec20;              /* 0x020 */
  Vec3 vec2c;              /* 0x02C: S15 b07: its dot product with vec20 blends two fog keys (-1..1) */
  int32_t cell;            /* 0x038: the cell of the course the camera is over (the tracks are keyed by it) */
  union {
    Quat rot3c;            /* 0x03C: (constructed as a Quat) */
    Body1 fog_near;        /* 0x03C: S15 b07: the fog's near distance, eased toward the fog key's */
  };
  union {
    Quat rot4c;            /* 0x04C */
    Body1 fog_far;         /* 0x04C: S15 b07: its far distance */
  };
  union {
    Body body5c;           /* 0x05C */
    Body fog_color;        /* 0x05C: S15 b07: the background colour (position = r, g, b) */
  };
  int32_t fog_mode;        /* 0x08C: the mode of the fog key in use */
  gaddr fog_track;         /* 0x090: FogKey[] */
  int32_t no_fog;          /* 0x094: 1 in the game modes that switch the fog off (set-up modes other than 0, 4, 5) */
  Body lights[3];          /* 0x098: the light rig */
  gaddr light_track;       /* 0x128: {word, LightKey[]...}: read from +4 at set-up, walked as LightKey from 0 */
  gaddr particles;         /* 0x12C: Body[particle_max] */
  gaddr particle_anims;    /* 0x130: a texture animation number per particle */
  Vec3 drift;              /* 0x134: S15 b07: the weather key's drift (a force on every particle each step) */
  Vec3 push;               /* 0x140: the race scene adds to it (func_8C044EEA, a wind where the camera's word is
                            * 0x3A-0x3D) */
  int32_t particle_max;    /* 0x14C */
  int32_t particle_count;  /* 0x150: in use (the weather key's count, at most particle_max) */
  int32_t weather;         /* 0x154: the weather key's kind (2 = additive sprites) */
  int32_t last_weather;    /* 0x158: S15 b07: the kind the particles were placed for */
  uint32_t word15c;        /* 0x15C: S15 b07: cleared with word160 when the particles are placed */
  uint32_t word160;        /* 0x160 */
  f32 spread_xz;           /* 0x164: S15 b07: the particles are placed within eye.x, eye.z +- spread_xz */
  f32 spread_up;           /* 0x168: and eye.y - spread_down .. eye.y + spread_up */
  f32 spread_down;         /* 0x16C */
  gaddr weather_track;     /* 0x170: WeatherKey[] */
  CellKeys keys;            /* 0x174: S15 b07: word4 is the sky's model table, word8 its texture list */
  int32_t key_b;           /* 0x180: the b of the cell key in use (S15 b07: how the sky is drawn, 0 = none) */
  int32_t key_a;           /* 0x184: its a (S15 b07: the sky's model, in keys.word4) */
  int32_t fog_on;          /* 0x188 */
  int32_t random_used;     /* 0x18C: S15 b07 (was word18c): the random floats taken (wraps at 100) */
  gaddr random_next;       /* 0x190: S15 b07 (was table190): the next of a table of 100 random floats 0..1 */
  gaddr course_table;      /* 0x194: the course's table (by the set-up's course) */
  int32_t slots[5];        /* 0x198: -1 = empty */
  Quat rot1ac;             /* 0x1AC */
} CourseEnv;
HW_OFFSET(CourseEnv, eye, 0x008);
HW_OFFSET(CourseEnv, last_eye, 0x014);
HW_OFFSET(CourseEnv, fog_near, 0x03C);
HW_OFFSET(CourseEnv, fog_far, 0x04C);
HW_OFFSET(CourseEnv, fog_color, 0x05C);
HW_OFFSET(CourseEnv, no_fog, 0x094);
HW_OFFSET(CourseEnv, particles, 0x12C);
HW_OFFSET(CourseEnv, drift, 0x134);
HW_OFFSET(CourseEnv, last_weather, 0x158);
HW_OFFSET(CourseEnv, word160, 0x160);
HW_OFFSET(CourseEnv, spread_xz, 0x164);
HW_OFFSET(CourseEnv, spread_down, 0x16C);
HW_OFFSET(CourseEnv, key_b, 0x180);
HW_OFFSET(CourseEnv, random_used, 0x18C);
HW_OFFSET(CourseEnv, random_next, 0x190);
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
  union {
    uint32_t word24;       /* 0x0024 */
    Pad pad;               /* 0x0024: S15 b07: the pad whose X button cycles shown_view */
  };
  uint32_t word28;         /* 0x0028: S15 b07: 2 once the CourseEnvs are set up (func_8C041356) */
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
  union {
    uint32_t word5764;     /* 0x5764 */
    int32_t shown_view;    /* 0x5764: S15 b07: 0 at set-up; X steps it through the players */
  };
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

/* game/course.c */
int32_t course_row_bit(gaddr course, int32_t row, int32_t bit);
int32_t race_scene_view_bit(gaddr scene, int32_t bit);
f32 course_value(gaddr course, int32_t i);

f32 race_scene_start_value(gaddr scene);

/* game/course.c (S13; S14 b08 declares them for their callers) */
gaddr course_env_construct(gaddr env);
void course_release(gaddr course);
/* game/stage.c */
gaddr course_models(gaddr course);     /* &models: CourseModelSet[4][2] */
gaddr course_words(gaddr course);      /* &course_words */
gaddr course_env_words(gaddr course);  /* &env_word */

/* S15 b07: the course's ground (Course.entry64): its vertex array and the strip of cells, each a quad of four
 * 16-bit vertex indices (course_build.c reads the same data as CourseEntry64 / CourseStrip) */
typedef struct CourseGround {
  gaddr vertices;          /* 0x00: Vec3[] */
  uint8_t pad_04[0x0C - 0x04];
  gaddr strip;             /* 0x0C: CourseGroundStrip */
} CourseGround;
HW_OFFSET(CourseGround, strip, 0x0C);
typedef struct CourseGroundStrip {
  uint16_t word0;          /* 0x00 */
  uint16_t cell_count;     /* 0x02 */
  gaddr quads;             /* 0x04: int16_t[4] per cell */
} CourseGroundStrip;
HW_OFFSET(CourseGroundStrip, quads, 0x04);
typedef struct CellQuad {
  int16_t corner[4];
} CellQuad;
HW_SIZE(CellQuad, 8);

/* S15 b07: hand-written elsewhere, not in a header yet */
gaddr course_find_segment(gaddr course, int32_t cell);                       /* game/course.c */
int32_t course_cell_valid(gaddr course, int32_t i);                          /* game/course.c */
int32_t quad_contains(gaddr course, gaddr point, gaddr indices, gaddr vertices); /* game/course.c */
f32 blend_signed(f32 t, f32 a, f32 b);                                       /* game/course.c */
gaddr body_reset_set2(gaddr b);                                              /* game/course.c */
void course_env_free(gaddr env);                                             /* game/course.c */
void view_fog_enable(gaddr view);                                            /* game/course.c */
void view_fog_disable(gaddr view);                                           /* game/course.c */
void view_light_intensity_set(gaddr v);                                      /* game/camera.c */
void view_light_color_set(gaddr v);                                          /* game/camera.c */
void view_light_dir_set(gaddr v);                                            /* game/camera.c */
void course_env_update_fog(gaddr env);                                       /* game/camera.c */
void course_env_init_lights(gaddr env, gaddr track);                         /* game/camera.c */
void course_env_update_lights(gaddr env);                                    /* game/camera.c */
void course_env_draw_particles(gaddr env);                                   /* game/camera.c */
void course_env_set_keys(gaddr env, gaddr src);                              /* game/actor.c */
void course_env_update_keys(gaddr env);                                      /* game/actor.c */
void course_env_set_table(gaddr env, gaddr table);                           /* game/actor.c */
void course_env_place_particles(gaddr env);                                  /* game/course_env.c */
void course_env_update_weather(gaddr env);                                   /* S16 b05: game/course_env_weather.c */

/* S15 b07: game/course_ground.c */
int32_t course_segment_kind(gaddr course, int32_t cell);
int32_t course_cell_search(gaddr course, gaddr pos);
int32_t course_cell_near(gaddr course, gaddr pos, int32_t hint);
void course_collider_update(gaddr course, gaddr collider);
void course_contacts_update(gaddr course, gaddr contacts, gaddr cell);
void course_setup_envs(gaddr course);
void course_update_views(gaddr course);
/* S15 b07: game/course_env.c */
void course_env_apply_background(gaddr env);
void course_env_apply_lights(gaddr env);
void course_env_init_fog(gaddr env, gaddr track);
void course_env_init_fog_preset(gaddr env, gaddr track);
/* S15 c05: game/course_camera.c */
void course_tv_camera(gaddr course, gaddr eye, gaddr target, gaddr roll, gaddr fov);
void course_env_update_fog_blend(gaddr env);
void course_env_init_weather(gaddr env, gaddr track);
void course_env_place_particles(gaddr env);
void course_env_draw(gaddr env);
void course_env_update(gaddr env);
void course_env_setup(gaddr env, int32_t view, int32_t particle_max, gaddr tracks);
void course_env_draw_sky(gaddr env);
void course_env_update_sounds(gaddr env);

#endif
