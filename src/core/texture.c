/* Pen Pen port, S13: loading a PVM texture archive from the disc (core/texture.c). The file is read whole into the
 * fixed area at 0x8CE40000 (file.h's FILE_FIXED_AREA) and handed to Ninja, which copies the textures to video
 * memory. Every GD-ROM call is retried until it succeeds, as file.c does. */
#include "hw.h"
#include "sdk.h"
#include "../core/file.h"

/* the GD-ROM file system (SDK) */
static gaddr gdFsOpen(gaddr name, gaddr dir_record) { return CALL2(0x8C09EB96u, name, dir_record); } /* 0 = not yet */
static void gdFsClose(gaddr handle) { CALL1(0x8C09E2B4u, handle); }
static void gdFsGetFileSctSize(gaddr handle, gaddr sectors_out) { CALL2(0x8C09E306u, handle, sectors_out); }
static int32_t gdFsRead(gaddr handle, int32_t sectors, gaddr buffer) { return (int32_t)CALL3(0x8C09E3DEu, handle, sectors, buffer); }
static void file_change_dir(gaddr dir_name) { CALL1(0x8C012D1Cu, dir_name); } /* func_8C012D1C: gdFsChangeDir until it succeeds */
static void njLoadTexturePvmMemory(gaddr pvm, gaddr texlist) { CALL2(0x8C0A3D5Cu, pvm, texlist); }

/* was func_8C0120CA: loads the PVM file `name` (from directory dir_name when it is not 0) into texlist */
void texture_load(gaddr name, gaddr texlist, gaddr dir_name) {
  if (dir_name != 0)
    file_change_dir(dir_name);
  gaddr handle;
  do
    handle = gdFsOpen(name, 0);
  while (handle == 0);
  gaddr sectors = hw_stack_alloc(4);
  gdFsGetFileSctSize(handle, sectors);
  while (gdFsRead(handle, (int32_t)U32_AT(sectors), FILE_FIXED_AREA) != 0) {
  }
  gdFsClose(handle);
  njLoadTexturePvmMemory(FILE_FIXED_AREA, texlist);
  hw_stack_free(4);
}
/* not exported (S13 difftest: differs from the original, to redo): HW_EXPORT(8c0120ca, texture_load(ARG0, ARG1, ARG2)) */

/* was func_8C01A404: releases the texture list an object holds at 0x30 */
void texture_release_member(gaddr obj) { njReleaseTexture(U32_AT(obj + 0x30)); }
HW_EXPORT(8c01a404, texture_release_member(ARG0))
