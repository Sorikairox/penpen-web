/* Pen Pen port, S18 b01: the fragments of this batch's game region whose parent's hand-written C does their work
 * (hw.h: HW_EXPORT_FRAGMENT). */
#include "hw.h"

/* func_8C01DBD2: the body of func_8C01DBB8 (game/good_night.c, the GOOD NIGHT scene's draw) after its prologue:
 * r13 the scene, its 0x20-byte frame (the background sprite at r15), r14, fr13-fr15 and PR saved there */
HW_EXPORT_FRAGMENT(8c01dbd2, 8c01dbb8)
