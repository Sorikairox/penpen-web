/* Pen Pen port, S17 (r1): the racer's sounds of an update (func_8C035FA2, run by racer_base_update while the race
 * scene is in its state 3).
 *
 * First the one-shot of this update on port 0, by priority of the event: a step of race progress (the jingle of the
 * item it holds and the progress MIDI on port 1), the "going backwards" warning (its 4th and 8th time), the start
 * of action 4, of the hit (action 3) and of the dash (action 2) (each with the character's voice, MIDI, when it wears
 * no costume), a new contact with another racer or with an object (pitched by the speed of the contact), the mark
 * B, word178, or the button press (a random voice, and the stroke of state 3, 4 or 5). Many sounds have a second
 * version on the cells whose attribute has bit 0x10 (`marked` below).
 * Then the loop of port 1, while the state stays: in state 3 a step sound when word554 says a step was made; in
 * state 4 the sliding sound of the special ground (started when the ground or the button changes, else its volume
 * by the speed and, on ground 3, its pitch), stopped off special ground; the first player also pans the CDDA by how
 * far its motion turns from its heading (sd_CmdBytes). A change of state stops port 1. word554 is cleared. */
#include "hw.h"
#include "sdk.h"
#include "../core/vec.h"
#include "../core/inline.h"   /* racer_cell, race_scene_cell_attr */
#include "../core/ninja.h"    /* vec3_normalized */
#include "race_scene.h"
#include "scenery.h"          /* random_timer_scaled */
#include "racer.h"

#define PORT_SHOT 0              /* the one-shots of events */
#define PORT_LOOP 1              /* the loops of the ground (and the progress MIDI) */
#define CELL_MARKED 0x10         /* a cell attribute bit: the sounds' second version */

/* the character's voices (MIDI sequences), an int32 per character */
#define VOICE_HIT     ((const int32_t *)hw_ptr(0x8C067274u)) /* action 3 starts */
#define VOICE_PRESS_A ((const int32_t *)hw_ptr(0x8C067294u)) /* the button, 1 in 20 */
#define VOICE_PRESS_B ((const int32_t *)hw_ptr(0x8C0672B4u)) /* the button, 1 in 20 of the rest */
#define VOICE_DASH    ((const int32_t *)hw_ptr(0x8C0672D4u)) /* action 2 starts */
#define VOICE_KIND4   ((const int32_t *)hw_ptr(0x8C0672F4u)) /* action 4 starts (an object of kind 4) */

#define CONTACT_SPEED_MIN F32_BITS(0x3EED097Au) /* 0.46296296 (25 / 54) */
#define CONTACT_SPEED_REF F32_BITS(0x3F6D097Au) /* 0.9259259 (25 / 27): also mark B's threshold */
#define CONTACT_PITCH_LOW F32(-1024.0f)
#define CONTACT_PITCH_RATE F32(2048.0f)
#define VOICE_ODDS        F32(20.0f)            /* random_timer_scaled's range: below 1 is 1 in 20 */
#define RUN_SPEED_REF     F32_BITS(0x406D097Au) /* 3.7037036 (100 / 27) */
#define RUN_PITCH_RATE    F32(512.0f)
#define SLIDE_SPEED_REF   F32_BITS(0x40B1C71Bu) /* 5.5555553 (50 / 9) */
#define SLIDE_VOLUME_LOW  F32(-40.0f)
#define SLIDE_VOLUME_RATE F32(40.0f)
#define SLIDE_VOLUME_OFF  (-127)                /* off the special ground's contact */
#define SLIDE_PITCH_LOW   F32(-1024.0f)
#define SLIDE_PITCH_RATE  F32(1024.0f)
#define SLIDE_PITCH_ICE   (-0x1800)             /* ground 4's fixed pitch */
#define PAN_LOW           F32(-64.0f)
#define PAN_RATE          F32(32.0f)

/* the original's frame (0x30 bytes under PR): sd_CmdBytes is handed its byte buffer at the same place */
typedef struct RacerSoundFrame {
  f32 side;             /* 0x00: the turn of the motion against the heading (cross product's y) */
  Vec3 axis;            /* 0x04: the draw matrix's third row (where the racer faces) */
  Vec3 direction;       /* 0x10: the moving body's velocity made unit */
  uint8_t cmd[4];       /* 0x1C: sd_CmdBytes's bytes: the first is the pan */
  uint8_t rest[0x34 - 0x20]; /* 0x20: the volume / pitch, the cell flag, `this`, PR */
} RacerSoundFrame;
HW_OFFSET(RacerSoundFrame, axis, 0x04);
HW_OFFSET(RacerSoundFrame, direction, 0x10);
HW_OFFSET(RacerSoundFrame, cmd, 0x1C);
HW_SIZE(RacerSoundFrame, 0x34);

/* SDK sd_CmdBytes (0x8C09A348): sound driver host command 0x88 with the bytes at buf */
static inline void sd_CmdBytes(gaddr buf) { CALL1(0x8C09A348u, buf); }

/* a one-shot of this racer (no pan) */
static void racer_sound(gaddr self, int32_t port, int32_t priority, int32_t sound, int32_t pitch) {
  const Racer *r = racer_at(self);
  race_sound_play_by_mode(r->scene, r->index, port, priority, sound, 0, pitch);
}

/* a MIDI sequence of this racer (priority 0) */
static void racer_midi(gaddr self, int32_t port, int32_t sequence) {
  const Racer *r = racer_at(self);
  race_midi_play_by_mode(r->scene, r->index, port, 0, sequence);
}

/* the loop of port 1 at a volume and pitch */
static void racer_loop_adjust(gaddr self, int32_t volume, int32_t pitch) {
  const Racer *r = racer_at(self);
  race_sound_adjust_by_mode(r->scene, r->index, PORT_LOOP, 0, volume, 0, pitch);
}

/* the pitch of a contact sound: -1024 at rest, + 2048 per CONTACT_SPEED_REF of speed */
static int32_t contact_pitch(f32 speed) {
  return f32_to_int(fmac(CONTACT_PITCH_RATE, fdiv(speed, CONTACT_SPEED_REF), CONTACT_PITCH_LOW));
}

/* the pitch of the run sounds of state 4: 512 per RUN_SPEED_REF of f434 */
static int32_t run_pitch(const Racer *r) {
  return f32_to_int(fmul(fdiv(r->f434, RUN_SPEED_REF), RUN_PITCH_RATE));
}

/* a step of race progress: the item's jingle (by the game's mode) and the progress MIDI */
static void racer_progress_sounds(gaddr self) {
  const Racer *r = racer_at(self);
  int32_t item;
  switch (race_setup_at(g_race_setup)->mode) {
  case 0:
    if (r->index != 0) break;
    item = (int32_t)race_scene_at(r->scene)->player_item[r->index];
    switch (item) {
    case 0: racer_sound(self, PORT_SHOT, 0xF, 1, 0); break;
    case 1: racer_sound(self, PORT_SHOT, 0xF, 2, 0); break;
    case 2: racer_sound(self, PORT_SHOT, 0xF, 3, 0); break;
    default: racer_sound(self, PORT_SHOT, 0xF, 4, 0); break;
    }
    break;
  case 1:
    item = (int32_t)race_scene_at(r->scene)->player_item[r->index];
    switch (item) {
    case 0: racer_sound(self, PORT_SHOT, 0xF, 1, 0); break;
    case 1: racer_sound(self, PORT_SHOT, 0xF, 4, 0); break;
    default: break;
    }
    break;
  case 2:
    item = (int32_t)race_scene_at(r->scene)->player_item[r->index];
    switch (item) {
    case 0: racer_sound(self, PORT_SHOT, 0xF, 1, 0); break;
    case 1: racer_sound(self, PORT_SHOT, 0xF, 2, 0); break;
    case 2: racer_sound(self, PORT_SHOT, 0xF, 4, 0); break;
    default: break;
    }
    break;
  case 3:
    item = (int32_t)race_scene_at(r->scene)->player_item[r->index];
    switch (item) {
    case 0: racer_sound(self, PORT_SHOT, 0xF, 1, 0); break;
    case 1: racer_sound(self, PORT_SHOT, 0xF, 2, 0); break;
    case 2: racer_sound(self, PORT_SHOT, 0xF, 3, 0); break;
    case 3: racer_sound(self, PORT_SHOT, 0xF, 4, 0); break;
    default: break;
    }
    break;
  default: /* mode 4 and the others: none */
    break;
  }
  switch (r->word1a8) {
  case 2: racer_midi(self, PORT_LOOP, 0); break;
  case 3: racer_midi(self, PORT_LOOP, 1); break;
  case 4: racer_midi(self, PORT_LOOP, 2); break;
  case 5: racer_midi(self, PORT_LOOP, 3); break;
  default: break;
  }
}

/* the button was pressed: a voice now and then, and the stroke of the state */
static void racer_press_sounds(gaddr self, int marked) {
  const Racer *r = racer_at(self);
  if (fgt(F32(1.0f), random_timer_scaled(VOICE_ODDS))) {
    if (r->costume == 0) racer_midi(self, PORT_SHOT, VOICE_PRESS_A[r->character]);
  } else if (fgt(F32(1.0f), random_timer_scaled(VOICE_ODDS)) && r->costume == 0) {
    racer_midi(self, PORT_SHOT, VOICE_PRESS_B[r->character]);
  }
  switch (r->state) {
  case 3:
    if (r->ground_touch != 0) racer_sound(self, PORT_SHOT, 8, marked ? 0x33 : 0x17, 0);
    break;
  case 4:
    switch (r->ground) {
    case 2:
      racer_sound(self, PORT_SHOT, 7, 0x1D, run_pitch(r));
      break;
    case 4:
      if (marked) {
        racer_sound(self, PORT_SHOT, 7, r->word174 != 0 ? 0x37 : 0x36, run_pitch(r));
      } else {
        racer_sound(self, PORT_SHOT, 7, r->word174 != 0 ? 0x23 : 0x1B, run_pitch(r));
      }
      break;
    case 3:
      racer_sound(self, PORT_SHOT, 7, 0x1D, run_pitch(r));
      break;
    default:
      break;
    }
    break;
  case 5:
    racer_sound(self, PORT_SHOT, 8, 0x1F, 0);
    break;
  default:
    break;
  }
}

/* the one-shot of this update, by priority of the event */
static void racer_event_sounds(gaddr self, int marked) {
  const Racer *r = racer_at(self);
  if (r->backwards != 0 && r->backwards_prev == 0) {
    switch (r->backwards_count) {
    case 4: racer_sound(self, PORT_SHOT, 0xF, 0x3C, 0); break;
    case 8: racer_sound(self, PORT_SHOT, 0xF, 0x3D, 0); break;
    default: break;
    }
  } else if (r->on_kind4_new != 0) {
    racer_sound(self, PORT_SHOT, 0xE, marked ? 0x34 : 0x10, 0);
    if (r->costume == 0) racer_midi(self, PORT_SHOT, VOICE_KIND4[r->character]);
  } else if (r->action == 3 && r->action_step == 0) {
    racer_sound(self, PORT_SHOT, 0xD, 0xF, 0);
    if (r->costume == 0) racer_midi(self, PORT_SHOT, VOICE_HIT[r->character]);
  } else if (r->action == 2 && r->action_step == 0) {
    racer_sound(self, PORT_SHOT, 0xC, marked ? 0x35 : 0x18, 0);
    if (r->costume == 0) racer_midi(self, PORT_SHOT, VOICE_DASH[r->character]);
  } else if (r->racer_contact != -1 && r->racer_contact != r->racer_contact_prev &&
             fgt(r->racer_speed, CONTACT_SPEED_MIN)) {
    racer_sound(self, PORT_SHOT, 0xB, marked ? 0x38 : 9, contact_pitch(r->racer_speed));
  } else if (r->object_contact != -1 && r->object_contact != r->object_contact_prev &&
             fgt(r->speed, CONTACT_SPEED_MIN) && !feq(r->object_grip, F32(0.0f))) {
    /* (pitched by racer_speed, as the original does) */
    racer_sound(self, PORT_SHOT, 0xB, marked ? 0x38 : 9, contact_pitch(r->racer_speed));
  } else if (r->mark_b_on != 0 && fgt(r->mark_b_value, CONTACT_SPEED_REF)) {
    if (r->state == 5) {
      racer_sound(self, PORT_SHOT, 0xA, 0x22, 0);
    } else {
      racer_sound(self, PORT_SHOT, 0xA, marked ? 0x39 : 0xA, 0);
    }
  } else if (r->word178 != 0) {
    racer_sound(self, PORT_SHOT, 9, r->state == 5 ? 0x21 : 0x2B, 0);
  } else if (r->edges[0].pressed != 0) {
    racer_press_sounds(self, marked);
  }
}

/* state 3, a step made (word554): the step sound of the ground */
static void racer_step_sound(gaddr self, int marked) {
  const Racer *r = racer_at(self);
  int alt = r->word144 != 0;
  if (r->ground == 2) {
    racer_sound(self, PORT_LOOP, 8, alt ? 0xB : 0xC, 0);
  } else if (r->word17c != 0) {
    racer_sound(self, PORT_LOOP, 8, alt ? 0x13 : 0x14, 0);
  } else if (r->word174 != 0) {
    if (fgt(r->vec534.y, F32(0.0f))) {
      racer_sound(self, PORT_LOOP, 8, alt ? 0x2C : 0x2D, 0);
    } else if (marked) {
      racer_sound(self, PORT_LOOP, 8, alt ? 0x3A : 0x3B, 0);
    } else {
      racer_sound(self, PORT_LOOP, 8, alt ? 0x15 : 0x16, 0);
    }
  } else if (marked) {
    racer_sound(self, PORT_LOOP, 8, alt ? 0x31 : 0x32, 0);
  } else {
    racer_sound(self, PORT_LOOP, 8, alt ? 0x11 : 0x12, 0);
  }
}

/* state 4: the slide of the special ground on port 1, and the first player's CDDA pan */
static void racer_slide_sound(gaddr self, gaddr frame) {
  const Racer *r = racer_at(self);
  if (r->ground == 0) {
    race_sound_stop(r->scene, r->index, PORT_LOOP);
  } else if (r->ground != r->ground_prev || r->state != r->state_prev || r->edges[0].pressed != 0) {
    /* (state == state_prev always holds here: the caller checked it) */
    switch (r->ground) {
    case 2:
      racer_sound(self, PORT_LOOP, 0, 0x1A, 0);
      break;
    case 4:
      if (r->word174 != 0) {
        racer_sound(self, PORT_LOOP, 0, 0x1C, 0);
      } else if (r->word17c != 0) {
        racer_sound(self, PORT_LOOP, 0, 0x19, 0);
      }
      break;
    case 3:
      racer_sound(self, PORT_LOOP, 0, 0x19, 0);
      break;
    default:
      break;
    }
    return;
  }

  int32_t volume;
  if (r->ground_touch != 0) {
    volume = f32_to_int(fmac(SLIDE_VOLUME_RATE, fdiv(r->f434, SLIDE_SPEED_REF), SLIDE_VOLUME_LOW));
  } else {
    volume = SLIDE_VOLUME_OFF;
  }
  switch (r->ground) {
  case 2:
    racer_loop_adjust(self, volume, 0);
    break;
  case 4:
    racer_loop_adjust(self, volume, SLIDE_PITCH_ICE);
    break;
  case 3:
    racer_loop_adjust(self, volume,
                      f32_to_int(fmac(SLIDE_PITCH_RATE, fdiv(r->f434, RUN_SPEED_REF), SLIDE_PITCH_LOW)));
    break;
  default:
    break;
  }

  if (r->index == 0) {
    RacerSoundFrame *f = (RacerSoundFrame *)hw_ptr(frame);
    vec3_normalized(member_addr(self, Racer, body.body.velocity), member_addr(frame, RacerSoundFrame, direction));
    vec3_copy(member_addr(frame, RacerSoundFrame, axis), member_addr(self, Racer, matrix3a8.m[8]));
    f32 side = vec3_cross_y(member_addr(frame, RacerSoundFrame, direction), member_addr(frame, RacerSoundFrame, axis));
    f->side = side;
    f->cmd[0] = (uint8_t)f32_to_int(fmac(PAN_RATE, side, PAN_LOW));
    sd_CmdBytes(member_addr(frame, RacerSoundFrame, cmd));
  }
}

/* was func_8C035FA2: the racer's sounds of this update (see the top of the file) */
void racer_play_sounds(gaddr self) {
  const Racer *r = racer_at(self);
  if (race_scene_at(r->scene)->state != RACE_STATE_RESULTS) return;
  gaddr frame = hw_stack_alloc(sizeof(RacerSoundFrame));
  int marked = ((uint32_t)race_scene_cell_attr(r->scene, (int32_t)racer_cell(self)) & 0xFFFFu & CELL_MARKED) != 0;

  if (r->progress_prev == 1 || r->word1a8 == 1 || r->word1a8 == r->progress_prev || r->word1a8 >= 6) {
    racer_event_sounds(self, marked);
  } else {
    racer_progress_sounds(self);
  }

  if (r->state != r->state_prev) {
    race_sound_stop(r->scene, r->index, PORT_LOOP);
  } else {
    switch (r->state) {
    case 3:
      if (r->word554 != 0) racer_step_sound(self, marked);
      break;
    case 4:
      racer_slide_sound(self, frame);
      break;
    default: /* 5 and the others: none */
      break;
    }
  }
  racer_at(self)->word554 = 0;
  hw_stack_free(sizeof(RacerSoundFrame));
}
HW_EXPORT(8c035fa2, racer_play_sounds(ARG0))
