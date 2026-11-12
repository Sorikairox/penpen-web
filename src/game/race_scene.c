/* Pen Pen port, S13 (batch b): the race scene's per-player helpers (0x8C02C780-0x8C02F4A8).
 *
 * The scene keeps up to four players: their objects at 0x4C (one word each), the view each player is shown on at
 * 0xBC (0 or 1; the sound helpers pick the player's port by it), the player each view shows at 0x9C, the player the
 * camera follows at 0xDC, a player watched from the other view at 0x11C and a per-player distance (float) at 0x45C.
 * `setup` (0x28) is the race's set-up object: its mode word at 0x2C (0 to 5: how many players and which game), the
 * course at 0x30 and a variant at 0x34. (Field names are from what these functions do with them.)
 *
 * Sound: a player's one-shot / MIDI requests go to port N (view 0) or N + 2 (the watched player on view 1: quieter
 * the farther it is: full volume at distance 0, silent from 250 on); some modes pan the ports left and right. */
#include "hw.h"
#include "sdk.h"
#include "sound.h"
#include "../core/inline.h"
#include "../core/view.h"

/* fields of the race scene (only these are used here) */
#define SCENE_SETUP(s)        U32_AT((s) + 0x28)    /* the race's set-up object */
#define SCENE_STATE(s)        U32_AT((s) + 0x2C)
#define SCENE_SUBSTATE(s)     U32_AT((s) + 0x34)
#define SCENE_STAGE(s)        U32_AT((s) + 0x38)    /* the stage object (its 16-bit attribute table at 0xB4) */
#define SCENE_PLAYER(s, i)    U32_AT((s) + 0x4C + 4u * (uint32_t)(i))
#define SCENE_VIEW_PLAYER(s, v) (*(int32_t *)hw_ptr((s) + 0x9C + 4u * (uint32_t)(v))) /* the player a view shows */
#define SCENE_PLAYER_VIEW(s, i) (*(int32_t *)hw_ptr((s) + 0xBC + 4u * (uint32_t)(i)))
#define SCENE_FOCUS(s)        (*(int32_t *)hw_ptr((s) + 0xDC)) /* the player the camera follows */
#define SCENE_PLAYER_ITEM(s, i) U32_AT((s) + 0xFC + 4u * (uint32_t)(i))
#define SCENE_WATCHED(s)       (*(int32_t *)hw_ptr((s) + 0x11C))
#define SCENE_DISTANCE(s, i)  F32_AT((s) + 0x45C + 4u * (uint32_t)(i))
#define SCENE_MUSIC_CUE(s)    (*(int32_t *)hw_ptr((s) + 0xA7C))
#define SCENE_RESULT(s)       U32_AT((s) + 0xB4C)
#define SCENE_SCORE16(s, i)   (*(int16_t *)hw_ptr((s) + 0xCC0 + 2u * (uint32_t)(i)))

#define STATE_RESULTS 3
#define STATE_ENDING 4

/* the music cues of every course: [course][kind 0..2][variant 0..4][cue] of 16-byte {attribute, track, fade,
 * repeat} */
#define MUSIC_CUES 0x8C067314u

/* ---- the players' objects (the accessors are class-neutral inline copies, core/inline.c) ---- */

/* was func_8C02C780: the address of player i's member at 0x2B8 */
gaddr race_player_member(gaddr scene, int32_t i) { return field_addr(SCENE_PLAYER(scene, i), 0x2B8); }
HW_EXPORT(8c02c780, RET(race_player_member(ARG0, (int32_t)ARG1)))
/* was func_8C02C7C8: player i's float at 0x32C */
f32 race_player_f32(gaddr scene, int32_t i) { return field_get_f32(SCENE_PLAYER(scene, i), 0x32C); }
HW_EXPORT(8c02c7c8, RETF(race_player_f32(ARG0, (int32_t)ARG1)))
/* was func_8C02C810: player i's word at 0x1C4 */
uint32_t race_player_word(gaddr scene, int32_t i) { return field_get(SCENE_PLAYER(scene, i), 0x1C4); }
HW_EXPORT(8c02c810, RET(race_player_word(ARG0, (int32_t)ARG1)))
/* was func_8C02C9B6: the word at 0x6C */
HW_EXPORT(8c02c9b6, RET(field_get(ARG0, 0x6C)))

/* was func_8C02E7AE: player i's 16-bit score while the results show, else 0 */
int32_t race_result_score(gaddr scene, int32_t i) {
  if (SCENE_STATE(scene) == STATE_RESULTS) return SCENE_SCORE16(scene, i);
  return 0;
}
HW_EXPORT(8c02e7ae, RET(race_result_score(ARG0, (int32_t)ARG1)))

/* ---- the ending, drawn per view ---- */

/* func_8C01723E / func_8C01859C / func_8C0173E8 / func_8C018774: the ending's drawers (still translated) */
static void ending_draw_banner(int32_t other_player) { CALL1(0x8C01723Eu, other_player); }
static void ending_draw_result(gaddr result) { CALL1(0x8C01859Cu, result); }
static void ending_draw_item(uint32_t item) { CALL1(0x8C0173E8u, item); }
static void ending_draw_result2(gaddr result) { CALL1(0x8C018774u, result); }

/* was func_8C02DAF0: in the ending, the view's banner (told whether this view is not the followed player's), or
 * the result on the followed player's view */
void race_ending_draw_a(gaddr scene) {
  if (SCENE_STATE(scene) != STATE_ENDING) return;
  if (SCENE_SUBSTATE(scene) == 1) {
    int32_t view = view_current_index();
    ending_draw_banner(view != SCENE_PLAYER_VIEW(scene, SCENE_FOCUS(scene)));
  } else if (SCENE_SUBSTATE(scene) == 3) {
    if (view_current_index() == SCENE_PLAYER_VIEW(scene, SCENE_FOCUS(scene))) ending_draw_result(SCENE_RESULT(scene));
  }
}
HW_EXPORT(8c02daf0, race_ending_draw_a(ARG0))

/* was func_8C02DC26: the same pass's second layer: no item on the followed player's view, the item of the player
 * another view shows (when it shows one), or the second result drawer */
void race_ending_draw_b(gaddr scene) {
  if (SCENE_STATE(scene) != STATE_ENDING) return;
  if (SCENE_SUBSTATE(scene) == 1) {
    if (view_current_index() == SCENE_PLAYER_VIEW(scene, SCENE_FOCUS(scene))) {
      ending_draw_item(0);
      return;
    }
    int32_t player = SCENE_VIEW_PLAYER(scene, view_current_index());
    if (player >= 0) ending_draw_item(SCENE_PLAYER_ITEM(scene, player));
  } else if (SCENE_SUBSTATE(scene) == 3) {
    if (view_current_index() == SCENE_PLAYER_VIEW(scene, SCENE_FOCUS(scene))) ending_draw_result2(SCENE_RESULT(scene));
  }
}
HW_EXPORT(8c02dc26, race_ending_draw_b(ARG0))

/* ---- sound per player ---- */

/* sdShotStop (Manatee): stop a one-shot port */
static inline void sdShotStop(gaddr port) { CALL1(0x8C09AD80u, port); }

/* the watched player's distance as a 0..1 fade (0 at the line, 1 from 250 on) */
static f32 distance_fade(gaddr scene, int32_t player) {
  f32 x = fdiv(SCENE_DISTANCE(scene, player), F32(250.0f));
  if (fgt(F32(0.0f), x)) x = F32(0.0f);
  if (fgt(x, F32(1.0f))) x = F32(1.0f);
  return x;
}

/* was func_8C02EA0C: stop player i's sound on port `port` (or on port + 2 for view 1), by the game's mode */
void race_sound_stop(gaddr scene, int32_t i, int32_t port) {
  int32_t mode = (int32_t)SCENE_STATE(SCENE_SETUP(scene));
  int32_t echo = port + 2;
  switch (mode) {
  case 0:
  case 4:
    if (SCENE_PLAYER_VIEW(scene, i) == 0) {
      g_shot_req_sound[port] = -1;
      sdShotStop(g_shot_port[port]);
    } else if (SCENE_WATCHED(scene) == i) {
      g_shot_req_sound[echo] = -1;
      sdShotStop(g_shot_port[echo]);
    }
    break;
  case 1:
    if (SCENE_PLAYER_VIEW(scene, i) == 0) {
      g_shot_req_sound[port] = -1;
      sdShotStop(g_shot_port[port]);
    } else if (SCENE_PLAYER_VIEW(scene, i) == 1) {
      g_shot_req_sound[echo] = -1;
      sdShotStop(g_shot_port[echo]);
    }
    break;
  case 2:
  case 3:
    if (i < 4 && port == 0) {
      int32_t p = i + port;
      g_shot_req_sound[p] = -1;
      sdShotStop(g_shot_port[p]);
    }
    break;
  default:
    break;
  }
}
HW_EXPORT(8c02ea0c, race_sound_stop(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C02EAF2: play a one-shot for player i: full volume on its port for view 0, or on port + 2 for the
 * watched player, faded by its distance (the 7th argument is the pitch, the 6th the pan) */
void race_sound_play(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sound, int32_t pan,
                     int32_t pitch) {
  if (port >= 4) return;
  if (SCENE_PLAYER_VIEW(scene, i) == 0) {
    sound_shot_request(port, priority, sound, SOUND_VOLUME_MAX, pan, pitch);
  } else if (SCENE_WATCHED(scene) == i) {
    f32 x = distance_fade(scene, SCENE_WATCHED(scene));
    int32_t volume = f32_to_int(fsub(F32(127.0f), fmul(x, F32(255.0f))));
    sound_shot_request(port + 2, priority, sound, volume, pan, pitch);
  }
}
HW_EXPORT(8c02eaf2, race_sound_play(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4, (int32_t)ARG5,
                                    (int32_t)ARG6))

/* was func_8C02EB94: change player i's sound: as given on view 0, or on port + 2 for the watched player with the
 * volume scaled down by its distance (through whole numbers, as the original rounds) */
void race_sound_adjust(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t volume, int32_t pan,
                       int32_t pitch) {
  if (port >= 4) return;
  if (SCENE_PLAYER_VIEW(scene, i) == 0) {
    sound_shot_adjust(port, priority, volume, pan, pitch);
  } else if (SCENE_WATCHED(scene) == i) {
    f32 keep = fsub(F32(1.0f), distance_fade(scene, SCENE_WATCHED(scene)));
    int32_t v = f32_to_int(fadd(f32_from_int(volume), F32(127.0f))); /* -127..127 to 0..254 */
    v = f32_to_int(fmul(f32_from_int(v), keep));
    v = f32_to_int(fsub(f32_from_int(v), F32(127.0f)));
    sound_shot_adjust(port + 2, priority, v, pan, pitch);
  }
}
HW_EXPORT(8c02eb94, race_sound_adjust(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4, (int32_t)ARG5,
                                      (int32_t)ARG6))

/* was func_8C02EC54: (port 0 only) a one-shot panned right for a player on view 1, left on port 2 for view 0 */
void race_sound_play_split(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sound, int32_t unused,
                           int32_t pitch) {
  if (port >= 1) return;
  if (SCENE_PLAYER_VIEW(scene, i) == 1)
    sound_shot_request(port, priority, sound, SOUND_VOLUME_MAX, 127, pitch);
  else if (SCENE_PLAYER_VIEW(scene, i) == 0)
    sound_shot_request(port + 2, priority, sound, SOUND_VOLUME_MAX, -127, pitch);
}
HW_EXPORT(8c02ec54, race_sound_play_split(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4,
                                          (int32_t)ARG5, (int32_t)ARG6))

/* was func_8C02ECC0: (port 0 only) a sound change: right on the port for view 0, left on port 2 for view 1 */
void race_sound_adjust_split(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t volume, int32_t unused,
                             int32_t pitch) {
  if (port >= 1) return;
  if (SCENE_PLAYER_VIEW(scene, i) == 0)
    sound_shot_adjust(port, priority, volume, 127, pitch);
  else if (SCENE_PLAYER_VIEW(scene, i) == 1)
    sound_shot_adjust(port + 2, priority, volume, -127, pitch);
}
HW_EXPORT(8c02ecc0, race_sound_adjust_split(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4,
                                            (int32_t)ARG5, (int32_t)ARG6))

/* the pan of player i's port in a split game: odd players right, even players left */
static int32_t player_pan(int32_t i) { return (i & 1) ? 127 : -127; }

/* was func_8C02ED1A: (offset 0 only) a one-shot on player i's own port, panned to its side */
void race_sound_play_panned(gaddr scene, int32_t i, int32_t offset, int32_t priority, int32_t sound, int32_t unused,
                            int32_t pitch) {
  if (i >= 4 || offset != 0) return;
  sound_shot_request(offset + i, priority, sound, SOUND_VOLUME_MAX, player_pan(i), pitch);
}
HW_EXPORT(8c02ed1a, race_sound_play_panned(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4,
                                           (int32_t)ARG5, (int32_t)ARG6))

/* was func_8C02ED6C: (offset 0 only) change the sound of player i's own port, panned to its side */
void race_sound_adjust_panned(gaddr scene, int32_t i, int32_t offset, int32_t priority, int32_t volume,
                              int32_t unused, int32_t pitch) {
  if (i >= 4 || offset != 0) return;
  sound_shot_adjust(offset + i, priority, volume, player_pan(i), pitch);
}
HW_EXPORT(8c02ed6c, race_sound_adjust_panned(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4,
                                             (int32_t)ARG5, (int32_t)ARG6))

/* was func_8C02F0F4: a MIDI sequence for player i: full volume on view 0, or (port 0 only) on port 2 for the
 * watched player, faded by its distance (the original passes 9 as the unused third argument there) */
void race_midi_play(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence) {
  if (SCENE_PLAYER_VIEW(scene, i) == 0) {
    sound_midi_request(port, priority, sequence, SOUND_VOLUME_MAX, 0);
  } else if (port == 0 && SCENE_WATCHED(scene) == i) {
    f32 x = distance_fade(scene, SCENE_WATCHED(scene));
    int32_t volume = f32_to_int(fsub(F32(127.0f), fmul(x, F32(255.0f))));
    sound_midi_request(port + 2, priority, sequence, volume, 0);
  }
}
HW_EXPORT(8c02f0f4, race_midi_play(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* ---- music cues ---- */

/* the cue the course's table holds for this kind of game, variant and step */
static gaddr music_cue(gaddr scene, gaddr setup, int32_t kind) {
  return MUSIC_CUES + field_get(setup, 0x30) * 0x780u + (uint32_t)kind * 0x280u + field_get(setup, 0x34) * 0x80u +
         (uint32_t)SCENE_MUSIC_CUE(scene) * 16u;
}

/* was func_8C02F2C4: when the ground under the followed player (or player 0 in modes 0 and 5) has the attribute
 * the next music cue waits for, start the cue's track (attribute 8 also plays sound 0x20) and step to the next */
void race_music_update(gaddr scene) {
  gaddr setup = SCENE_SETUP(scene);
  gaddr player;
  if (field_get(setup, 0x2C) == 0 || field_get(setup, 0x2C) == 5)
    player = SCENE_PLAYER(scene, 0);
  else
    player = SCENE_PLAYER(scene, SCENE_FOCUS(scene));
  int32_t cell = (int32_t)field_get(player, 0x1C4);
  int32_t attribute = (uint16_t)field_table16_get(SCENE_STAGE(scene), 0xB4, cell) & 0xF;
  if (attribute == 6) return;
  int32_t kind = 0;
  switch (field_get(setup, 0x2C)) {
  case 0: case 5: kind = 0; break;
  case 4: kind = 1; break;
  case 1: case 2: case 3: kind = 2; break;
  default: break;
  }
  gaddr cue = music_cue(scene, setup, kind);
  if ((int32_t)U32_AT(cue) != attribute) return;
  int32_t repeat = U32_AT(cue + 0xC) != 0 ? 1 : 0;
  music_play((int32_t)U32_AT(cue + 4), (int32_t)U32_AT(cue + 8), repeat);
  if (attribute == 8) sound_play(7, 0x20, 0xF);
  SCENE_MUSIC_CUE(scene) += 1;
}
HW_EXPORT(8c02f2c4, race_music_update(ARG0))
