/* Pen Pen port: identical copies of the functions src/game/menu_penguin.c exports (written by tools/copies.py from
 * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each
 * copy runs the same C as the copy it repeats (named in the comment above its lines). */
#include "hw.h"
#include "sdk.h"
#include "../../game/motion.h"

/* copies of 8c01d178 */
HW_EXPORT(8c01f6da, RET(menu_penguin_construct(ARG0, (int32_t)ARG1, (int32_t)ARG2, ARG3, ARG4)))
/* copies of 8c01d1cc */
HW_EXPORT(8c01f708, motion_player_update(ARG0))
HW_EXPORT(8c02f78a, motion_player_update(ARG0))
/* copies of 8c0229b8 */
HW_EXPORT(8c01f70e, motion_player_draw(ARG0))
HW_EXPORT(8c02f790, motion_player_draw(ARG0))
