/* Pen Pen port: identical copies of the functions src/game/racer_body.c exports (written by tools/copies.py from
 * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each
 * copy runs the same C as the copy it repeats (named in the comment above its lines). */
#include "hw.h"
#include "sdk.h"
#include "../../core/vec.h"
#include "../../core/contact.h"
#include "../../core/transform.h"
#include "../../game/racer.h"
#include "../../game/motion.h"
#include "../../game/effects.h"
#include "../../game/race_scene.h"
#include "../../game/sound.h"

/* copies of 8c030b20 */
HW_EXPORT(8c01e312, racer_effects_push_set_b(ARG0, ARG1))
