/* Pen Pen port, S18 b06: game/results_text.c, the results screen's pulsing lines of text, declared for their
 * callers (game/race_draw.c, game/race_hud.c). */
#ifndef PENPEN_RESULTS_TEXT_H
#define PENPEN_RESULTS_TEXT_H
#include "hw.h"

/* was func_8C016D60 / func_8C016E98 / func_8C016FD4: each steps the pulse and draws its line in the game's
 * language (the draw's result, or the language when it is out of 0-6) */
int32_t results_pulse_draw(void);
int32_t results_draw_banner_one(void);
int32_t results_draw_banner_split(void);

#endif
