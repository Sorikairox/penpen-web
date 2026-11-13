/* Pen Pen port, S17 b04: the racer's effects (game/effects.h, the racer's child task): handing a frame to set A. */
#include "hw.h"
#include "../core/task.h"
#include "effects.h"

/* func_8C03A252: set A's push (still translated): (set, matrix, cell; radius, speed, heading in FR4-FR6) */
static void effect_set_a_push(gaddr set, gaddr m, uint32_t cell, f32 radius, f32 speed, f32 heading) {
  FARGS3(radius, speed, heading);
  CALL3(0x8C03A252u, set, m, cell);
}

/* was func_8C030B16: hand the racer's matrix, cell, radius, speed and heading to its effects' set A */
void racer_effects_set_a_push(gaddr fx, gaddr m, uint32_t cell, f32 radius, f32 speed, f32 heading) {
  effect_set_a_push(racer_effects_at(fx)->set_a, m, cell, radius, speed, heading);
}
HW_EXPORT(8c030b16, racer_effects_set_a_push(ARG0, ARG1, ARG2, FARG0, FARG1, FARG2))
