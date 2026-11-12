/* Pen Pen port: identical copies of the functions src/game/actor_misc.c exports (written by tools/copies.py from
 * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each
 * copy runs the same C as the copy it repeats (named in the comment above its lines). */
#include "hw.h"
#include "sdk.h"
#include "../../core/vec.h"
#include "../../core/contact.h"
#include "../../core/view.h"
#include "../../core/ninja.h"
#include "../../game/game.h"
#include "../../game/actor.h"
#include "../../game/course.h"
#include "../../game/race_scene.h"
#include "../../game/scenery.h"

/* copies of 8c04d7f4 */
HW_EXPORT(8c0291f6, RET(race_scene_find_cell(ARG0, ARG1, (int32_t)ARG2)))
HW_EXPORT(8c046b78, RET(race_scene_find_cell(ARG0, ARG1, (int32_t)ARG2)))
/* copies of 8c05045a */
HW_EXPORT(8c040942, RET(race_scene_view_focus(ARG0, (int32_t)ARG1)))
