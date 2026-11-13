/* Pen Pen port, S17 (b02): mode 9's scene (0x90 bytes, constructor func_8C01E8DA -> func_8C01E50C, vtable
 * 0x8C23D9E4 {func_8C01E93A, ...}): by its update, a timed sequence of lines in the game's language (a table of
 * 12-byte entries per language whose first word is the line's time in seconds) over a background that fades in
 * phase by phase; Start on the last phase goes to mode 4. The name "staff roll" is a guess from that shape.
 * Also the destructor of its penguin (a MotionPlayer subclass) and the text label it draws its lines with. */
#include "hw.h"
#include "sdk.h"
#include "../core/task.h"
#include "../core/pad.h"
#include "../core/file.h"
#include "../core/contact.h"
#include "../core/inline.h"
#include "../core/sprite_text.h"
#include "game.h"
#include "sound.h"
#include "save.h"
#include "motion.h"
#include "staff_roll.h"

#define STAFF_ROLL_TEXLIST_B 0x8C08CDACu /* a static NJS_TEXLIST released with the scene */
#define STAFF_ROLL_MUSIC     0x2F
#define STAFF_ROLL_LAST_PHASE 4
#define PHASE_FRAMES         3900        /* the countdown of each phase before the last */
#define DRIVE_PLAYING        3           /* gdFsGetDrvStat: the drive is still playing */
#define NEXT_MODE            4

/* ---- the scene's update ---- */

/* fade += 1 / frames, at most 1 */
static void fade_step(f32 *fade, f32 frames) {
  f32 v = fadd(*fade, fdiv(F32(1.0f), frames));
  *fade = v;
  if (fgt(v, F32(1.0f)))
    *fade = F32(1.0f);
}

/* was func_8C01E970: the scene's update: the music started on the first update, both fades stepped, the phases
 * counted down (Start on the last one leaves for mode 4), the lines timed one after the other (-1 after the last),
 * and the frame counter */
void staff_roll_update(gaddr scene) {
  StaffRoll *s = staff_roll_at(scene);
  if (s->started == 0) {
    music_play(STAFF_ROLL_MUSIC, 0, 0);
    s->started++;
  }
  fade_step(&s->phase_fade, s->phase_fade_frames);
  fade_step(&s->line_fade, s->line_fade_frames);
  if (s->phase >= 0) {
    if (s->phase < STAFF_ROLL_LAST_PHASE) {
      int32_t left = s->countdown - 1;
      s->countdown = left;
      if (left < 0) {
        s->phase_fade_frames = F32((float)PHASE_FRAMES);
        s->countdown = PHASE_FRAMES;
        s->phase_fade = F32(0.0f);
        s->prev_phase = s->phase;
        s->phase++;
      }
    } else if (s->phase == STAFF_ROLL_LAST_PHASE) {
      s->phase_fade = F32(0.0f);
      s->prev_phase = s->phase;
      if (port_pressed(0, PAD_START)) {
        Game *game = game_at(s->task.owner);
        game->next_mode = NEXT_MODE;
        game->mode_arg = 0;
      }
    }
  }
  if (s->line >= 0) {
    f32 t = fsub(s->line_timer, F32(1.0f));
    s->line_timer = t;
    if (fgt(F32(0.0f), t)) {
      s->line_fade = F32(0.0f);
      s->prev_line = s->line;
      s->line = s->line + 1;
      int32_t language = game_language();
      const StaffRollLine *lines =
          (const StaffRollLine *)hw_ptr(STAFF_ROLL_LINES + (uint32_t)language * STAFF_ROLL_LANGUAGE_SIZE);
      t = fmul(f32_from_int(lines[s->line].seconds), F32(60.0f));
      s->line_timer = t;
      if (fgt(F32(0.0f), t))
        s->line = -1;
    }
  }
  s->frames = fadd(s->frames, F32(1.0f));
}
HW_EXPORT(8c01e970, staff_roll_update(ARG0))

/* ---- destruction ---- */

/* was func_8C01E900: the destructor's body: the model file freed, both texture lists released, the music stopped;
 * then it waits while the drive is playing */
void staff_roll_release(gaddr scene) {
  file_free(staff_roll_at(scene)->file);
  njReleaseTexture(staff_roll_at(scene)->texlist);
  njReleaseTexture(STAFF_ROLL_TEXLIST_B);
  music_stop();
  while (gdFsGetDrvStat() == DRIVE_PLAYING) {
  }
}
HW_EXPORT(8c01e900, staff_roll_release(ARG0))

/* was func_8C01E93A: the destructor: the body, the memory freed when asked; returns the scene */
gaddr staff_roll_destroy(gaddr scene, int32_t free_memory) {
  staff_roll_release(scene);
  if (free_memory)
    syFree(scene);
  return scene;
}
HW_EXPORT(8c01e93a, RET(staff_roll_destroy(ARG0, (int32_t)ARG1)))

/* was func_8C01E95E: the same without the free; returns the scene */
gaddr staff_roll_destroy_member(gaddr scene) {
  staff_roll_release(scene);
  return scene;
}
HW_EXPORT(8c01e95e, RET(staff_roll_destroy_member(ARG0)))

/* was func_8C01E410: the scene's penguin's destructor: its contacts freed, its member at 0xE0 destroyed (an empty
 * destructor, func_8C01E03A), the MotionPlayer part freed; the memory freed when asked; returns it */
gaddr roll_penguin_destroy(gaddr penguin, int32_t free_memory) {
  contacts_free(member_addr(penguin, RollPenguin, contacts));
  method_empty();
  motion_player_free(penguin);
  if (free_memory)
    syFree(penguin);
  return penguin;
}
HW_EXPORT(8c01e410, RET(roll_penguin_destroy(ARG0, (int32_t)ARG1)))

/* ---- the text label ---- */

/* was func_8C01E48C: a label drawn: its colour, its scale (in pixels) and spacing as the text style, then its
 * string at its position; text_draw's result */
int32_t text_label_draw(gaddr label) {
  TextLabel *l = text_label_at(label);
  f32 *color = (f32 *)hw_ptr(TEXT_COLOR_ARGB);
  color[1] = l->r;
  color[2] = l->g;
  color[3] = l->b;
  color[0] = l->a;
  text_set_scale(l->size_x, l->size_y, l->spacing);
  return text_draw(f32_to_int(l->x), f32_to_int(l->y), l->string);
}
HW_EXPORT(8c01e48c, RET(text_label_draw(ARG0)))
