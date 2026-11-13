/* Pen Pen port, S18 b06: game/text_view_plain.c's export, declared for its callers (new header). */
#ifndef PENPEN_TEXT_VIEW_PLAIN_H
#define PENPEN_TEXT_VIEW_PLAIN_H
#include "hw.h"

/* was func_8C016B7A: draws `string` at (x, y) from the current view's top-left corner; text_draw's result */
int32_t text_draw_at_view(int32_t x, int32_t y, gaddr string);

#endif
