/* Pen Pen port, S17 (b01): the C++ static destructors (game/main.c runs the constructors). */
#include "hw.h"

#define DTORS_BEGIN 0x8C23D918u /* -> the first entry of the destructor table */
#define DTORS_END   0x8C23D91Cu /* -> its end */

/* was func_8C016510: run the static destructors: pairs of words, the first of each called (as the constructors'
 * table); the end is read again after each call */
void run_static_destructors(void) {
  for (gaddr entry = GUEST_VAR(gaddr, DTORS_BEGIN); entry < GUEST_VAR(gaddr, DTORS_END); entry += 8)
    CALL0(GUEST_VAR(gaddr, entry));
}
HW_EXPORT(8c016510, run_static_destructors())
