/* Pen Pen port, S18 b01: the race's text drawers (game/results_banner.c, game/ending_text.c, game/menu_lines.c),
 * which game/race_hud.c, game/race_scene.c and game/race_draw_three.c called through static wrappers. */
#ifndef PENPEN_RACE_TEXT_H
#define PENPEN_RACE_TEXT_H
#include "hw.h"

void results_draw_banner_four(void);         /* was func_8C0170F8 */
void ending_draw_banner(int32_t other_player); /* was func_8C01723E */
void ending_draw_item(int32_t item);         /* was func_8C0173E8 */
void pause_draw_menu_a(int32_t cursor);      /* was func_8C017AC8 */
void ending_draw_result_a(int32_t cursor);   /* was func_8C017D40 */
void ending_draw_result_b(int32_t cursor);   /* was func_8C018010 */
void ending_draw_result_c(int32_t cursor);   /* was func_8C0182D4 */
void ending_draw_result(int32_t cursor);     /* was func_8C01859C */
void ending_draw_result2(int32_t cursor);    /* was func_8C018774 */

#endif
