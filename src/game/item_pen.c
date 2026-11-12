/* Pen Pen port, S15 (b01): the item pen's data (game/item_pen.c): its container (the shared models the effects and
 * the players' parts use, game/model.c, game/motion_player.c) and the two texture lists that go with it. */
#include "hw.h"
#include "../core/file.h"

#define ITEM_PEN_FILE      0x8C06CCDCu /* the container's file name */
#define ITEM_PEN_PVM       0x8C06CCECu /* its textures' PVM file name */
#define ASCII_PVM          0x8C06CD00u /* the text characters' PVM file name */
#define ASCII_TEXLIST      0x8C08C5C4u /* a static NJS_TEXLIST for them */
#define g_parts_container  GUEST_VAR(gaddr, 0x8C1B7E48u) /* the loaded container (motion_player.c's name) */
#define g_item_texlist     GUEST_VAR(gaddr, 0x8C1B7E40u) /* its first entry: the items' NJS_TEXLIST (model.c's
                                                            g_word_8c1b7e40) */
#define g_current_texlist  GUEST_VAR(gaddr, 0x8C1B7E44u) /* the texture list the text sprites use (sprite_text.c) */

/* func_8C0120CA (core/texture.c, not in a header): load the PVM file `name` into texlist */
static void texture_load(gaddr name, gaddr texlist, gaddr dir_name) { CALL3(0x8C0120CAu, name, texlist, dir_name); }

/* was func_8C018CBA: loads the item pen's container, its textures into the container's texture list, and the text
 * characters' textures into their static list (made the current one) */
void item_pen_load(void) {
  g_parts_container = container_load(ITEM_PEN_FILE, 0);
  g_item_texlist = (gaddr)((const ContainerEntry *)hw_ptr(g_parts_container))[0].offset;
  texture_load(ITEM_PEN_PVM, g_item_texlist, 0);
  g_current_texlist = ASCII_TEXLIST;
  texture_load(ASCII_PVM, ASCII_TEXLIST, 0);
}
HW_EXPORT(8c018cba, item_pen_load())
