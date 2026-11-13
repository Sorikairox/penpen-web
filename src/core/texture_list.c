/* Pen Pen port, S17 (b01): a texture list loaded by Ninja from its own file names (core/texture.c loads a PVM). */
#include "hw.h"
#include "sdk.h"

#include "../core/file.h" /* file_change_dir (core/cd.c) */
static int32_t njLoadTexture(gaddr texlist) { return (int32_t)CALL1(0x8C0A44F6u, texlist); }

/* was func_8C0120A0: change to directory dir_name when it is not 0, then njLoadTexture(texlist) until it reports
 * a count above 0 */
void texlist_load(gaddr texlist, gaddr dir_name) {
  if (dir_name != 0)
    file_change_dir(dir_name);
  while (njLoadTexture(texlist) <= 0) {
  }
}
HW_EXPORT(8c0120a0, texlist_load(ARG0, ARG1))
