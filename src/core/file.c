/* Pen Pen port, S12: GD-ROM files and the CDDA calls (core/file.h). Every SDK request is repeated until the
 * drive takes it, as the original does. */
#include "file.h"
#include "sdk.h"

/* ---- the GD-ROM file system (gdFs) calls ---- */
static gaddr gdFsOpen(gaddr name, gaddr dir_record) { return CALL2(0x8C09EB96u, name, dir_record); } /* 0 = busy / not yet */
static void gdFsClose(gaddr handle) { CALL1(0x8C09E2B4u, handle); }
static void gdFsGetFileSize(gaddr handle, gaddr size_out) { CALL2(0x8C09E2ECu, handle, size_out); }
static void gdFsGetFileSctSize(gaddr handle, gaddr sectors_out) { CALL2(0x8C09E306u, handle, sectors_out); }
static int32_t gdFsRead(gaddr handle, int32_t sectors, gaddr buffer) { return (int32_t)CALL3(0x8C09E3DEu, handle, sectors, buffer); }
static void gdFsEntryErrFuncAll(gaddr func, gaddr obj) { CALL2(0x8C09EC1Eu, func, obj); }
static int32_t gdFsDaPause(void) { return (int32_t)CALL0(0x8C09ECACu); }

/* still translated (the runs never call them: a directory change, the error callback) */
static void file_change_dir(gaddr dir_name) { CALL1(0x8C012D1Cu, dir_name); } /* func_8C012D1C: gdFsChangeDir until it succeeds */

/* the main loop's globals this module resets (game/game.h has them too) */
#define g_exit_request GUEST_VAR(int32_t, 0x8C08AD48u)
#define g_drive_status GUEST_VAR(int32_t, 0x8C08AD4Cu)
#define FILE_ERROR_CALLBACK 0x8C012DA6u /* func_8C012DA6 (translated): a missing file (-27, -28) is let pass, any
                                         * other error leaves the game */

/* was func_8C012D04: at start-up: no exit request, no drive status yet, and the error callback for every request */
void file_init(void) {
  g_exit_request = 0;
  g_drive_status = 0;
  gdFsEntryErrFuncAll(FILE_ERROR_CALLBACK, 0);
}
HW_EXPORT(8c012d04, file_init())

/* open `name` (after changing to `dir_name` when there is one), retrying until the drive answers */
static gaddr file_open(gaddr name, gaddr dir_name) {
  if (dir_name)
    file_change_dir(dir_name);
  gaddr handle;
  do
    handle = gdFsOpen(name, 0);
  while (!handle);
  return handle;
}

/* was func_8C012E40: a whole file into a new block (whole sectors). The SDK writes the file's size and sector count
 * into the caller's frame: those two words live on the guest stack, where the original keeps them. */
gaddr file_load(gaddr name, gaddr dir_name) {
  gaddr handle = file_open(name, dir_name);
  gaddr frame = hw_stack_alloc(16); /* +0: sectors, +0xC: size in bytes (asked for, not used) */
  gdFsGetFileSize(handle, frame + 0xC);
  gdFsGetFileSctSize(handle, frame);
  int32_t sectors = GUEST_VAR(int32_t, frame);
  gaddr block = syMalloc((uint32_t)sectors << GD_SECTOR_SHIFT);
  while (gdFsRead(handle, GUEST_VAR(int32_t, frame), block) != 0) {
  }
  gdFsClose(handle);
  hw_stack_free(16);
  return block;
}
HW_EXPORT(8c012e40, RET(file_load(ARG0, ARG1)))

/* was func_8C01311C: a whole file into the fixed area at 0x8CE40000 */
gaddr file_load_fixed(gaddr name, gaddr dir_name) {
  gaddr handle = file_open(name, dir_name);
  gaddr frame = hw_stack_alloc(4); /* the sector count */
  gdFsGetFileSctSize(handle, frame);
  while (gdFsRead(handle, GUEST_VAR(int32_t, frame), FILE_FIXED_AREA) != 0) {
  }
  gdFsClose(handle);
  hw_stack_free(4);
  return FILE_FIXED_AREA;
}
HW_EXPORT(8c01311c, RET(file_load_fixed(ARG0, ARG1)))

/* was func_8C012EAC and its thunk func_8C013118 */
void file_free(gaddr block) {
  if (block)
    syFree(block);
}
HW_EXPORT(8c012eac, file_free(ARG0))
HW_EXPORT(8c013118, file_free(ARG0))

/* ---- CDDA ---- */
/* was func_8C012D5C */
void cd_pause(void) {
  while (gdFsDaPause() != 0) {
  }
}
HW_EXPORT(8c012d5c, cd_pause())

