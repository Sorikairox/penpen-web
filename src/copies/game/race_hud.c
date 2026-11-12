/* Pen Pen port: identical copies of the functions src/game/race_hud.c exports (written by tools/copies.py from
 * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each
 * copy runs the same C as the copy it repeats (named in the comment above its lines). */
#include "hw.h"
#include "sdk.h"
#include "../../core/pad.h"
#include "../../core/task.h"
#include "../../core/view.h"
#include "../../core/ninja.h"
#include "../../core/sprite_text.h"
#include "../../game/game.h"
#include "../../game/sound.h"
#include "../../game/race_scene.h"
#include "../../game/racer.h"

/* copies of 8c02d4f0 */
HW_EXPORT(8c02d48c, race_pause_draw_a(ARG0))
