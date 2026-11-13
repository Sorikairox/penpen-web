/* Pen Pen port, S17 (b01): more of the GD-ROM calls the game wraps (core/file.c has the others): each repeats the
 * SDK call until it succeeds; the error handler the file system calls. */
#include "hw.h"
#include "sdk.h"
#include "../core/file.h"

static int32_t gdFsChangeDir(gaddr name) { return (int32_t)CALL1(0x8C09E29Cu, name); }
static int32_t gdFsDaStop(void) { return (int32_t)CALL0(0x8C09ECC4u); }
static int32_t gdFsDaRelease(int32_t arg) { return (int32_t)CALL1(0x8C09ECE6u, arg); }
/* func_8C01022C: the systems shut down (it masks the interrupts: stays translated) */
static void system_shutdown(void) { CALL0(0x8C01022Cu); }

#define FILE_ERROR_PASS_A (-27) /* the two gdFs errors the handler lets pass (core/file.c: a missing file) */
#define FILE_ERROR_PASS_B (-28)

/* was func_8C012D1C: change the current directory (until the drive accepts it) */
void file_change_dir(gaddr dir_name) {
  while (gdFsChangeDir(dir_name) != 0) {
  }
}
HW_EXPORT(8c012d1c, file_change_dir(ARG0))

/* was func_8C012D72 (and its thunk func_8C01607A): stop the CD audio */
void cd_stop(void) {
  while (gdFsDaStop() != 0) {
  }
}
HW_EXPORT(8c012d72, cd_stop())
HW_EXPORT(8c01607a, cd_stop())

/* was func_8C012D88: gdFsDaRelease (the SDK replays the saved play) until it succeeds; returns 1 */
int32_t cd_release(int32_t arg) {
  while (gdFsDaRelease(arg) != 0) {
  }
  return 1;
}
HW_EXPORT(8c012d88, RET(cd_release((int32_t)ARG0)))

/* was func_8C012DA6: the file system's error callback (object, error): errors -27 and -28 are let pass; any other
 * shuts the systems down and goes back to the boot menu */
void file_error(gaddr object, int32_t error) {
  if (error == FILE_ERROR_PASS_A || error == FILE_ERROR_PASS_B)
    return;
  system_shutdown();
  syBtExit();
}
HW_EXPORT(8c012da6, file_error(ARG0, (int32_t)ARG1))
