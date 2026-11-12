/* Pen Pen port: identical copies of the functions src/game/house.c exports (written by tools/copies.py from
 * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each
 * copy runs the same C as the copy it repeats (named in the comment above its lines). */
#include "hw.h"
#include "sdk.h"
#include "../../core/vec.h"
#include "../../core/view.h"
#include "../../core/file.h"
#include "../../core/inline.h"
#include "../../core/ninja.h"
#include "../../game/sound.h"
#include "../../game/motion.h"
#include "../../game/menu.h"
#include "../../game/race_scene.h"

/* copies of 8c022730 */
HW_EXPORT(8c01d0fa, house_view_camera((int32_t)ARG0, ARG1, ARG2, FARG0, FARG1))
HW_EXPORT(8c028e6c, house_view_camera((int32_t)ARG0, ARG1, ARG2, FARG0, FARG1))
/* copies of 8c02257a */
HW_EXPORT(8c028e32, RETF(rt_floor(FARG0)))
/* copies of 8c022904 */
HW_EXPORT(8c02f6cc, RET(shape_motion_pair_draw(ARG0, ARG1, ARG2, (int32_t)ARG3)))
