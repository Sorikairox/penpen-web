/* Pen Pen port, S17 (b02): screens of text shown around the memory card (0x8C018980-0x8C018D06): a fading line,
 * a four-line message in the game's language drawn at three places, and the release of the shared data
 * (texture lists and the parts container) loaded with them. */
#include "hw.h"
#include "sdk.h"
#include "../core/ninja.h"
#include "../core/sprite_text.h"
#include "../core/file.h"
#include "save.h"
#include "staff_roll.h" /* TextLabel, text_label_draw */

/* the text style of core/sprite_text.c */
#define SPRITE_SMALL     0x8C1B3AE8u
#define g_text_scale_x   GUEST_VAR(f32, 0x8C1B3B28u)
#define g_text_scale_y   GUEST_VAR(f32, 0x8C1B3B2Cu)
#define g_text_spacing   GUEST_VAR(f32, 0x8C1B3B30u) /* the style's third value (text_draw.c's name) */
#define g_text_color     ((f32 *)hw_ptr(0x8C1B3B34u)) /* a, r, g, b */

/* the shared data (game/model.c, game/motion_player.c) */
#define g_shared_texlist   GUEST_VAR(gaddr, 0x8C1B7E40u) /* the first entry of the parts container */
#define g_current_texlist  GUEST_VAR(gaddr, 0x8C1B7E44u) /* the texture list of the text sprites */
#define g_parts_container  GUEST_VAR(gaddr, 0x8C1B7E48u)

#define FADE_LINE_STRING  0x8C06C974u /* the string of the fading line */
#define CARD_MESSAGES     0x8C08C4F4u /* four strings per language */
#define CARD_MESSAGE_LINES 4

#define CONTROL3D_FLAGS   4
#define ATTR_AND          0xFFFF9FFFu /* njSetConstantAttr's masks */
#define ATTR_OR           0x2000u

static inline NjSprite *sprite_at(gaddr a) { return (NjSprite *)hw_ptr(a); }
static inline void njSetConstantAttr(uint32_t and_attr, uint32_t or_attr) { CALL2(0x8C0A9672u, and_attr, or_attr); }
/* func_8C016B7A (game/text_view_plain.c, no header): a string at (x, y) from the current view's corner */
static int32_t text_draw_at_view(int32_t x, int32_t y, gaddr string) { return (int32_t)CALL3(0x8C016B7Au, x, y, string); }


/* the text scale (already in 32-pixel units) and spacing, written as the inline setter does */
static void text_scale_put(f32 scale, f32 spacing) {
  g_text_scale_x = scale;
  sprite_at(SPRITE_SMALL)->sx = scale;
  g_text_scale_y = scale;
  sprite_at(SPRITE_SMALL)->sy = scale;
  g_text_spacing = spacing;
}

/* was func_8C018980: the fading line at (48, 380): colour (1, alpha, 0) at `alpha`, scale 1.25; text_draw's
 * result */
int32_t fade_line_draw(f32 alpha) {
  gaddr local = hw_stack_alloc(12); /* the original's one local word (4, never read), 12 bytes below its entry */
  U32_AT(local) = CONTROL3D_FLAGS;
  control3d_set(CONTROL3D_FLAGS);
  njSetConstantAttr(ATTR_AND, ATTR_OR);
  f32 *color = g_text_color;
  color[1] = F32(1.0f);
  color[2] = alpha;
  color[3] = F32(0.0f);
  color[0] = alpha;
  text_scale_put(F32(1.25f), F32(0.0f));
  hw_stack_free(12);
  return text_draw(48, 380, FADE_LINE_STRING);
}
HW_EXPORT(8c018980, RET(fade_line_draw(FARG0)))

/* the four lines of the card message in the game's language, from (x, y), `step` pixels apart: the style reset,
 * then scale 0.875 (28 pixels), spacing -2, white */
static void card_message_draw(int32_t x, int32_t y, int32_t step) {
  text_style_reset();
  text_scale_put(F32(0.875f), F32(-2.0f));
  f32 *color = g_text_color;
  color[1] = F32(1.0f);
  color[2] = F32(1.0f);
  color[3] = F32(1.0f);
  color[0] = F32(1.0f);
  const gaddr *lines = (const gaddr *)hw_ptr(CARD_MESSAGES + (uint32_t)game_language() * CARD_MESSAGE_LINES * 4u);
  for (int32_t i = 0; i < CARD_MESSAGE_LINES; i++, y += step)
    text_draw_at_view(x, y, lines[i]);
}

/* was func_8C0189E8: the card message at (160, 192), lines 50 pixels apart */
void card_message_draw_a(void) { card_message_draw(160, 192, 50); }
HW_EXPORT(8c0189e8, card_message_draw_a())

/* was func_8C018ADC: the card message at (160, 72), lines 32 pixels apart */
void card_message_draw_b(void) { card_message_draw(160, 72, 32); }
HW_EXPORT(8c018adc, card_message_draw_b())

/* was func_8C018B82: the card message at (20, 72), lines 32 pixels apart */
void card_message_draw_c(void) { card_message_draw(20, 72, 32); }
HW_EXPORT(8c018b82, card_message_draw_c())

/* was func_8C018CEC: the shared data released: both texture lists, then the parts container's file */
void shared_data_release(void) {
  njReleaseTexture(g_current_texlist);
  njReleaseTexture(g_shared_texlist);
  file_free(g_parts_container);
}
HW_EXPORT(8c018cec, shared_data_release())

#define CARD_SCREENS 0x8C08C5F4u /* per language, 8 screens: each an array of TextLabels ended by a null string */
#define CARD_SCREEN_COUNT 8

/* was func_8C019B30: screen n (0..7) of the memory card's texts in the game's language: each label of it drawn */
void card_screen_draw(int32_t n) {
  const gaddr *screens = (const gaddr *)hw_ptr(CARD_SCREENS + (uint32_t)game_language() * CARD_SCREEN_COUNT * 4u);
  for (gaddr label = screens[n]; text_label_at(label)->string != 0; label += sizeof(TextLabel))
    text_label_draw(label);
}
HW_EXPORT(8c019b30, card_screen_draw((int32_t)ARG0))
