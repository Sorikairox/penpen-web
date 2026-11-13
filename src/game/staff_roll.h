/* Pen Pen port, S17 (b02): mode 9's scene (game/staff_roll.c), its penguin and the text label it draws with. */
#ifndef PENPEN_STAFF_ROLL_H
#define PENPEN_STAFF_ROLL_H

#include "hw.h"
#include "../core/task.h"
#include "motion.h"

typedef struct StaffRoll {
  Task task;                  /* 0x00: vtable 0x8C23D9E4 */
  int32_t started;            /* 0x24: the music was started (the first update) */
  int32_t countdown;          /* 0x28: frames left in the phase (3900 from the second phase on) */
  uint8_t unknown2c[0x34 - 0x2C];
  int32_t phase;              /* 0x34: 0..4 (4 waits for Start), negative: stopped */
  int32_t prev_phase;         /* 0x38 */
  f32 phase_fade;             /* 0x3C: 0 to 1 over phase_fade_frames */
  f32 phase_fade_frames;      /* 0x40 */
  int32_t line;               /* 0x44: the line shown (-1 after the last) */
  int32_t prev_line;          /* 0x48 */
  f32 line_fade;              /* 0x4C: 0 to 1 over line_fade_frames */
  f32 line_fade_frames;       /* 0x50 */
  f32 line_timer;             /* 0x54: frames left for the line */
  gaddr texlist;              /* 0x58: released with the scene */
  uint8_t unknown5c[0x64 - 0x5C];
  f32 frames;                 /* 0x64: +1 every update */
  gaddr file;                 /* 0x68: the model file (file_free) */
  uint8_t unknown6c[0x90 - 0x6C];
} StaffRoll;
HW_OFFSET(StaffRoll, started, 0x24);
HW_OFFSET(StaffRoll, phase, 0x34);
HW_OFFSET(StaffRoll, phase_fade_frames, 0x40);
HW_OFFSET(StaffRoll, line, 0x44);
HW_OFFSET(StaffRoll, line_timer, 0x54);
HW_OFFSET(StaffRoll, texlist, 0x58);
HW_OFFSET(StaffRoll, frames, 0x64);
HW_OFFSET(StaffRoll, file, 0x68);
HW_SIZE(StaffRoll, 0x90);
static inline StaffRoll *staff_roll_at(gaddr a) { return (StaffRoll *)hw_ptr(a); }

/* the scene's penguin: a MotionPlayer (its first word points at the part at 0x1A4, as the menus' penguins do),
 * constructed by func_8C01E364 (vtables at 0xDC and 0x1A0) */
typedef struct RollPenguin {
  MotionPlayer player;        /* 0x000 */
  uint8_t unknown_d8[0xE0 - sizeof(MotionPlayer)];
  uint8_t member_e0[0x144 - 0xE0]; /* 0x0E0: a member with an empty destructor */
  uint8_t contacts[0x19C - 0x144]; /* 0x144: core/contact.h's contact set (contacts_free) */
  gaddr extra;                /* 0x19C: a 0x3C-byte object of func_8C038C1C, or 0 */
  uint8_t unknown1a0[0x1A4 - 0x1A0];
} RollPenguin;
HW_OFFSET(RollPenguin, member_e0, 0xE0);
HW_OFFSET(RollPenguin, contacts, 0x144);
HW_OFFSET(RollPenguin, extra, 0x19C);

/* a line of text with its style (the scene draws its lines through it) */
typedef struct TextLabel {
  f32 r, g, b, a;             /* 0x00 */
  f32 x, y;                   /* 0x10: the position (truncated to pixels) */
  f32 size_x, size_y;         /* 0x18: in pixels (text_set_scale) */
  f32 spacing;                /* 0x20 */
  gaddr string;               /* 0x24: Shift-JIS */
} TextLabel;
HW_OFFSET(TextLabel, x, 0x10);
HW_OFFSET(TextLabel, spacing, 0x20);
HW_SIZE(TextLabel, 0x28);
static inline TextLabel *text_label_at(gaddr a) { return (TextLabel *)hw_ptr(a); }

#define TEXT_COLOR_ARGB 0x8C1B3B34u /* the text style's colour {a, r, g, b} (core/sprite_text.c) */

void staff_roll_update(gaddr scene);
void staff_roll_release(gaddr scene);
gaddr staff_roll_destroy(gaddr scene, int32_t free_memory);
gaddr staff_roll_destroy_member(gaddr scene);
gaddr roll_penguin_destroy(gaddr penguin, int32_t free_memory);
int32_t text_label_draw(gaddr label);

#endif
