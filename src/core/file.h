/* Pen Pen port, S12: files from the GD-ROM, the CDDA music calls, and the game's data containers.
 *
 * The game reads every file whole: file_load allocates a block of the file's sectors and reads it, file_load_fixed
 * reads into the fixed area at 0x8CE40000 (the sound driver at start-up). Every GD-ROM call is retried until it
 * succeeds (the SDK answers "busy" while another request runs).
 *
 * A data container (container_load) is a file that starts with a table of 16-byte entries {type, offset, a, b},
 * ended by type 0. Pointers inside the file are offsets from the file's start (0 or negative: no pointer); loading
 * turns each one into an address (reloc), following the Ninja structures each entry names: model trees (basic and
 * chunk), models, mesh sets, motions, actions, texture lists, plain pointer arrays. */
#ifndef PENPEN_FILE_H
#define PENPEN_FILE_H

#include "hw.h"

/* ---- the GD-ROM file system, as the game uses it ---- */
#define FILE_FIXED_AREA 0x8CE40000u    /* file_load_fixed's destination */
#define GD_SECTOR_SHIFT 11              /* 2048-byte sectors */
#define GDD_ERR_NOFILE_A (-27)          /* errors the game's error callback lets pass (gdFsEntryErrFuncAll) */
#define GDD_ERR_NOFILE_B (-28)

void file_init(void);
gaddr file_load(gaddr name, gaddr dir_name);
gaddr file_load_fixed(gaddr name, gaddr dir_name);
void file_free(gaddr block);

/* ---- CDDA (the music tracks on the disc) ---- */
void cd_pause(void);
/* still translated (no route's sampled frames reach them): func_8C012D38 play, func_8C012D72 stop,
 * func_8C012D88 release */
static inline void cd_play(int32_t first_track, int32_t last_track, int32_t repeat) {
  CALL3(0x8C012D38u, first_track, last_track, repeat);
}

/* ---- data containers ---- */
typedef struct ContainerEntry {
  int32_t type;   /* 0x0: CONTAINER_*, 0 = the end of the table */
  int32_t offset; /* 0x4: where the data is (an offset in the file, an address once loaded) */
  int32_t a, b;   /* 0x8, 0xC: counts (array length, nodes of a motion), by type */
} ContainerEntry;
HW_SIZE(ContainerEntry, 0x10);
enum ContainerType {
  CONTAINER_END = 0,
  CONTAINER_OBJECT = 1,             /* a basic model tree (NJS_OBJECT + NJS_MODEL) */
  CONTAINER_MOTION = 2,             /* a motion of `a` nodes */
  CONTAINER_TEXLIST = 3,
  CONTAINER_OBJECTS = 4,            /* `a` model trees */
  CONTAINER_MOTIONS = 5,            /* `a` motions of `b` nodes */
  CONTAINER_TEXLISTS = 6,           /* `a` texture lists */
  CONTAINER_DATA = 7,               /* plain data: only the address */
  CONTAINER_POINTERS = 8,           /* `a` pointers */
  CONTAINER_ACTION = 9,             /* a tree and its motion of `a` nodes */
  CONTAINER_ACTIONS = 10,           /* `a` actions of `b` nodes */
  CONTAINER_ACTION_LIST = 11,       /* `a` {action, nodes} pairs */
  CONTAINER_MOTION_PAIR = 12,       /* two motions of `a` nodes */
  CONTAINER_MOTION_PAIRS = 13,      /* `a` motion pairs of `b` nodes */
  CONTAINER_MOTION_PAIR_LIST = 14,  /* `a` {motion, motion, nodes} triples */
  CONTAINER_CNK_OBJECT = 15,        /* a chunk model tree (NJS_CNK_MODEL) */
  CONTAINER_CNK_OBJECTS = 16,       /* `a` chunk trees */
  CONTAINER_CNK_ACTION = 17,        /* a chunk tree and its motion of `a` nodes */
  CONTAINER_CNK_ACTIONS = 18,       /* `b` chunk actions of `b` nodes (the original uses `b` for both) */
  CONTAINER_CNK_ACTION_LIST = 19,   /* `a` {chunk tree, motion, nodes} triples */
  CONTAINER_TYPES = 20
};

/* the Ninja structures the fix-ups walk (pointer fields only; the rest stays as bytes) */
typedef struct NjObject {
  uint32_t evalflags;   /* 0x00 */
  gaddr model;          /* 0x04: NJS_MODEL or NJS_CNK_MODEL */
  uint32_t pos_ang_scl[9]; /* 0x08: floats */
  gaddr child;          /* 0x2C */
  gaddr sibling;        /* 0x30 */
} NjObject;
HW_OFFSET(NjObject, child, 0x2C);
HW_SIZE(NjObject, 0x34);

typedef struct NjModel {
  gaddr points;         /* 0x00 */
  gaddr normals;        /* 0x04 */
  int32_t nb_points;    /* 0x08 */
  gaddr meshsets;       /* 0x0C: NjMeshset[nb_meshsets] */
  gaddr materials;      /* 0x10 */
  uint16_t nb_meshsets; /* 0x14 */
  uint16_t nb_materials;/* 0x16 */
} NjModel;
HW_OFFSET(NjModel, nb_meshsets, 0x14);

typedef struct NjMeshset {
  uint16_t type_material; /* 0x00 */
  uint16_t nb_meshes;     /* 0x02 */
  gaddr meshes;           /* 0x04 */
  gaddr attrs;            /* 0x08 */
  gaddr normals;          /* 0x0C */
  gaddr vertex_colors;    /* 0x10 */
  gaddr vertex_uvs;       /* 0x14 */
} NjMeshset;
HW_SIZE(NjMeshset, 0x18);

typedef struct NjCnkModel {
  gaddr vlist;          /* 0x00 */
  gaddr plist;          /* 0x04 */
} NjCnkModel;

typedef struct NjTexName {
  gaddr filename;       /* 0x00 */
  uint32_t attr;        /* 0x04 */
  uint32_t texaddr;     /* 0x08 */
} NjTexName;
HW_SIZE(NjTexName, 0xC);

typedef struct NjTexList {
  gaddr textures;       /* 0x00: NjTexName[nb_textures] */
  uint32_t nb_textures; /* 0x04 */
} NjTexList;

typedef struct NjMotion {
  gaddr mdata;          /* 0x00: per node, `elements` key-array pointers then `elements` key counts */
  int32_t nb_frames;    /* 0x04 */
  uint16_t type;        /* 0x08: which elements the motion has (position, angle, scale, vector, shape, ...) */
  uint16_t interpolation; /* 0x0A */
} NjMotion;
HW_OFFSET(NjMotion, type, 0x08);

typedef struct NjAction {
  gaddr object;         /* 0x00 */
  gaddr motion;         /* 0x04 */
} NjAction;

#define MOTION_ELEMENTS_MAX 5          /* the fix-up handles motions of 1 to 5 elements */
#define MOTION_POINTER_KEYS 0x8C08AD54u /* the original's table of which elements have pointer keys (5 words) */

int32_t reloc(int32_t offset, gaddr base);
void fix_object_tree(gaddr object, gaddr base);
void fix_model(gaddr model, gaddr base);
void fix_meshset(gaddr meshset, gaddr base);
void fix_motion(gaddr motion, gaddr base, int32_t nodes);
void fix_texlist(gaddr texlist, gaddr base);
void fix_cnk_object_tree(gaddr object, gaddr base);
void fix_cnk_model(gaddr model, gaddr base);
gaddr container_load(gaddr name, gaddr dir_name);

#endif
