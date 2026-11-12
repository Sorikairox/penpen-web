/* Pen Pen port, S14 (b05): the shared 2D sprites' helpers of core/sprite_text.c (exported there; the header
 * declares them for their callers). */
#ifndef PENPEN_SPRITE_TEXT_H
#define PENPEN_SPRITE_TEXT_H
#include "hw.h"

/* was func_8C015826: a digit of `value` at (x, y), `size` pixels, through the small or big sprite */
void text_draw_digit_sized(int32_t x, int32_t y, int32_t value, int32_t size, uint32_t attr);
/* was func_8C0292DA / func_8C029306: cell n of the small / big sprite at (x, y), scale s */
void sprite_small_draw(int32_t n, uint32_t attr, f32 x, f32 y, f32 scale);
void sprite_big_draw(int32_t n, uint32_t attr, f32 x, f32 y, f32 scale);
/* S15 b04: was func_8C022A78 (and its copy func_8C029332): the style back to its default (white, scale 1) */
void text_style_reset(void);

#endif
