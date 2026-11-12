/* Pen Pen port: identical copies of the functions src/game/model.c exports (written by tools/copies.py from
 * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each
 * copy runs the same C as the copy it repeats (named in the comment above its lines). */
#include "hw.h"
#include "sdk.h"
#include "../../core/vec.h"
#include "../../core/inline.h"
#include "../../core/file.h"
#include "../../game/motion.h"
#include "../../game/effects.h"

/* copies of 8c01d1a6 */
HW_EXPORT(8c01f6b4, RET(motion_player_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c02298c, RET(motion_player_delete(ARG0, (int32_t)ARG1)))
HW_EXPORT(8c02f764, RET(motion_player_delete(ARG0, (int32_t)ARG1)))
/* copies of 8c01e308 */
HW_EXPORT(8c030b02, racer_effects_reset_ribbon(ARG0, FARG0, ARG1))
