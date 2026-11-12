/* Pen Pen port, S13 (batch b): the race scene's per-player helpers (0x8C02C780-0x8C02F4A8).
 *
 * The scene (RaceScene, game/race_scene.h) keeps up to four players, the view each player is shown on (0 or 1;
 * the sound helpers pick the player's port by it), the player each view shows, the player the camera follows, a
 * player watched from the other view and a per-player distance; its RaceSetup gives the game's mode (0 to 5: how
 * many players and which game), the course and a variant.
 *
 * Sound: a player's one-shot / MIDI requests go to port N (view 0) or N + 2 (the watched player on view 1: quieter
 * the farther it is: full volume at distance 0, silent from 250 on); some modes pan the ports left and right. */
#include "hw.h"
#include "sdk.h"
#include "sound.h"
#include "../core/inline.h"
#include "../core/view.h"
#include "race_scene.h"
#include "game.h"
#include "racer.h"
#include "course.h"
#include "../core/task.h"

/* the music cues of every course: [course][kind 0..2][variant 0..4][cue 0..7] */
typedef struct MusicCue {
  int32_t attribute; /* 0x00: the ground attribute (low 4 bits) that starts it */
  int32_t track;     /* 0x04 */
  int32_t fade;      /* 0x08 */
  uint32_t repeat;   /* 0x0C */
} MusicCue;
HW_SIZE(MusicCue, 16);
typedef MusicCue MusicCueVariant[8];
typedef MusicCueVariant MusicCueKind[5];
typedef MusicCueKind MusicCueCourse[3];
HW_SIZE(MusicCueCourse, 0x780);
#define MUSIC_CUES 0x8C067314u

/* ---- the players (Racer, game/racer.h; through the class-neutral inline copies, core/inline.c) ---- */

/* was func_8C02C780: the address of player i's moving body */
gaddr race_player_body(gaddr scene, int32_t i) {
  return field_addr(race_scene_at(scene)->players[i], offsetof(Racer, body));
}
HW_EXPORT(8c02c780, RET(race_player_body(ARG0, (int32_t)ARG1)))
/* was func_8C02C7C8: player i's heading */
f32 race_player_heading(gaddr scene, int32_t i) {
  return field_get_f32(race_scene_at(scene)->players[i], offsetof(Racer, heading.value));
}
HW_EXPORT(8c02c7c8, RETF(race_player_heading(ARG0, (int32_t)ARG1)))
/* was func_8C02C810: player i's course cell (its contacts' cell) */
uint32_t race_player_cell(gaddr scene, int32_t i) {
  return field_get(race_scene_at(scene)->players[i], offsetof(Racer, contacts.cell));
}
HW_EXPORT(8c02c810, RET(race_player_cell(ARG0, (int32_t)ARG1)))
/* was func_8C02C9B6: the scene's word at 0x6C */
uint32_t race_scene_word6c(gaddr scene) { return race_scene_at(scene)->word6c; }
HW_EXPORT(8c02c9b6, RET(race_scene_word6c(ARG0)))

/* was func_8C02E7AE: player i's 16-bit score while the results show, else 0 */
int32_t race_result_score(gaddr scene, int32_t i) {
  RaceScene *s = race_scene_at(scene);
  if (s->state == RACE_STATE_RESULTS) return s->scores[i];
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
  RaceScene *s = race_scene_at(scene);
  if (s->state != RACE_STATE_ENDING) return;
  if (s->substate == 1) {
    int32_t view = view_current_index();
    ending_draw_banner(view != s->player_view[s->focus]);
  } else if (s->substate == 3) {
    if (view_current_index() == s->player_view[s->focus]) ending_draw_result(s->result);
  }
}
HW_EXPORT(8c02daf0, race_ending_draw_a(ARG0))

/* was func_8C02DC26: the same pass's second layer: no item on the followed player's view, the item of the player
 * another view shows (when it shows one), or the second result drawer */
void race_ending_draw_b(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  if (s->state != RACE_STATE_ENDING) return;
  if (s->substate == 1) {
    if (view_current_index() == s->player_view[s->focus]) {
      ending_draw_item(0);
      return;
    }
    int32_t player = s->view_player[view_current_index()];
    if (player >= 0) ending_draw_item(s->player_item[player]);
  } else if (s->substate == 3) {
    if (view_current_index() == s->player_view[s->focus]) ending_draw_result2(s->result);
  }
}
HW_EXPORT(8c02dc26, race_ending_draw_b(ARG0))

/* ---- sound per player ---- */

/* sdShotStop (Manatee): stop a one-shot port */
static inline void sdShotStop(gaddr port) { CALL1(0x8C09AD80u, port); }

/* the watched player's distance as a 0..1 fade (0 at the line, 1 from 250 on) */
static f32 distance_fade(gaddr scene, int32_t player) {
  RaceScene *s = race_scene_at(scene);
  f32 x = fdiv(s->values[0][player], F32(250.0f));
  if (fgt(F32(0.0f), x)) x = F32(0.0f);
  if (fgt(x, F32(1.0f))) x = F32(1.0f);
  return x;
}

/* was func_8C02EA0C: stop player i's sound on port `port` (or on port + 2 for view 1), by the game's mode */
void race_sound_stop(gaddr scene, int32_t i, int32_t port) {
  RaceScene *s = race_scene_at(scene);
  int32_t mode = (int32_t)race_setup_at(s->setup)->mode;
  int32_t echo = port + 2;
  switch (mode) {
  case 0:
  case 4:
    if (s->player_view[i] == 0) {
      g_shot_req_sound[port] = -1;
      sdShotStop(g_shot_port[port]);
    } else if (s->watched == i) {
      g_shot_req_sound[echo] = -1;
      sdShotStop(g_shot_port[echo]);
    }
    break;
  case 1:
    if (s->player_view[i] == 0) {
      g_shot_req_sound[port] = -1;
      sdShotStop(g_shot_port[port]);
    } else if (s->player_view[i] == 1) {
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
  RaceScene *s = race_scene_at(scene);
  if (port >= 4) return;
  if (s->player_view[i] == 0) {
    sound_shot_request(port, priority, sound, SOUND_VOLUME_MAX, pan, pitch);
  } else if (s->watched == i) {
    f32 x = distance_fade(scene, s->watched);
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
  RaceScene *s = race_scene_at(scene);
  if (port >= 4) return;
  if (s->player_view[i] == 0) {
    sound_shot_adjust(port, priority, volume, pan, pitch);
  } else if (s->watched == i) {
    f32 keep = fsub(F32(1.0f), distance_fade(scene, s->watched));
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
  RaceScene *s = race_scene_at(scene);
  if (port >= 1) return;
  if (s->player_view[i] == 1)
    sound_shot_request(port, priority, sound, SOUND_VOLUME_MAX, 127, pitch);
  else if (s->player_view[i] == 0)
    sound_shot_request(port + 2, priority, sound, SOUND_VOLUME_MAX, -127, pitch);
}
HW_EXPORT(8c02ec54, race_sound_play_split(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4,
                                          (int32_t)ARG5, (int32_t)ARG6))

/* was func_8C02ECC0: (port 0 only) a sound change: right on the port for view 0, left on port 2 for view 1 */
void race_sound_adjust_split(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t volume, int32_t unused,
                             int32_t pitch) {
  RaceScene *s = race_scene_at(scene);
  if (port >= 1) return;
  if (s->player_view[i] == 0)
    sound_shot_adjust(port, priority, volume, 127, pitch);
  else if (s->player_view[i] == 1)
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
  RaceScene *s = race_scene_at(scene);
  if (s->player_view[i] == 0) {
    sound_midi_request(port, priority, sequence, SOUND_VOLUME_MAX, 0);
  } else if (port == 0 && s->watched == i) {
    f32 x = distance_fade(scene, s->watched);
    int32_t volume = f32_to_int(fsub(F32(127.0f), fmul(x, F32(255.0f))));
    sound_midi_request(port + 2, priority, sequence, volume, 0);
  }
}
HW_EXPORT(8c02f0f4, race_midi_play(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* ---- music cues ---- */

/* the cue the course's table holds for this kind of game, variant and step */
static gaddr music_cue(gaddr scene, gaddr setup, int32_t kind) {
  RaceScene *s = race_scene_at(scene);
  const RaceSetup *r = race_setup_at(setup);
  return MUSIC_CUES + (uint32_t)r->course * (uint32_t)sizeof(MusicCueCourse) +
         (uint32_t)kind * (uint32_t)sizeof(MusicCueKind) + (uint32_t)r->variant * (uint32_t)sizeof(MusicCueVariant) +
         (uint32_t)s->music_cue * (uint32_t)sizeof(MusicCue);
}

/* was func_8C02F2C4: when the ground under the followed player (or player 0 in modes 0 and 5) has the attribute
 * the next music cue waits for, start the cue's track (attribute 8 also plays sound 0x20) and step to the next */
void race_music_update(gaddr scene) {
  RaceScene *s = race_scene_at(scene);
  gaddr setup = s->setup;
  const RaceSetup *r = race_setup_at(setup);
  gaddr player;
  if (r->mode == 0 || r->mode == 5)
    player = s->players[0];
  else
    player = s->players[s->focus];
  int32_t cell = racer_at(player)->contacts.cell;
  int32_t attribute = CELL_KIND(((const uint16_t *)hw_ptr(course_at(s->course)->cell_attrs))[cell]);
  if (attribute == 6) return;
  int32_t kind = 0;
  switch (r->mode) {
  case 0: case 5: kind = 0; break;
  case 4: kind = 1; break;
  case 1: case 2: case 3: kind = 2; break;
  default: break;
  }
  const MusicCue *cue = (const MusicCue *)hw_ptr(music_cue(scene, setup, kind));
  if (cue->attribute != attribute) return;
  int32_t repeat = cue->repeat != 0 ? 1 : 0;
  music_play(cue->track, cue->fade, repeat);
  if (attribute == 8) sound_play(7, 0x20, 0xF);
  s->music_cue += 1;
}
HW_EXPORT(8c02f2c4, race_music_update(ARG0))

/* was func_8C02F734 (and 13 identical copies): the RaceSetup being played */
gaddr race_setup_get(void) { return g_race_setup; }
HW_EXPORT(8c02f734, RET(race_setup_get()))

/* ---- scene constructors ----
 * Each stores its class's vtable in the Task, then runs the class's construction (still translated: it loads files
 * and can end frames, so these are resumable), and returns the scene. */
static gaddr scene_construct(uint32_t self, gaddr scene, uint32_t arg, gaddr vtable, gaddr init, int nargs) {
  HW_RESUMABLE(self, scene, arg, vtable, init, nargs);
  HW_RESUME_AT(0);
  task_at(scene)->vtable = vtable;
  HW_YIELDING_RET(0, hw_call(init, nargs, scene, arg, 0, 0), 0);
  return scene;
}
/* was func_8C01A3EE: mode 0's scene (the logos): vtable 0x8C23D93C, construction func_8C01A386 */
HW_EXPORT_RESUMABLE(8c01a3ee, RET(scene_construct(0x8C01A3EEu, ARG0, 0, 0x8C23D93Cu, 0x8C01A386u, 1)))
/* was func_8C01DDE2: mode 3's scene: vtable 0x8C23D99C, construction func_8C01DD7A */
HW_EXPORT_RESUMABLE(8c01dde2, RET(scene_construct(0x8C01DDE2u, ARG0, 0, 0x8C23D99Cu, 0x8C01DD7Au, 1)))
/* was func_8C01A8DE: mode 7's RaceSetup (argument: the game's mode): vtable 0x8C23D954, construction func_8C01A6B6 */
HW_EXPORT_RESUMABLE(8c01a8de, RET(scene_construct(0x8C01A8DEu, ARG0, ARG1, 0x8C23D954u, 0x8C01A6B6u, 2)))
/* was func_8C0232C6: the SetupMenu (0x424 bytes; argument: the RaceSetup): vtable 0x8C23DA5C, construction
 * func_8C022D80 */
HW_EXPORT_RESUMABLE(8c0232c6, RET(scene_construct(0x8C0232C6u, ARG0, ARG1, 0x8C23DA5Cu, 0x8C022D80u, 2)))
/* was func_8C0297AC: the RaceScene (0xD2C bytes; argument: the RaceSetup): vtable 0x8C23DA74, construction
 * func_8C0293F0 */
HW_EXPORT_RESUMABLE(8c0297ac, RET(scene_construct(0x8C0297ACu, ARG0, ARG1, 0x8C23DA74u, 0x8C0293F0u, 2)))

/* ---- the RaceSetup's update: switching what it shows ---- */

/* func_8C01AA58: before the race scene (still translated) */
static void race_setup_before_race(gaddr setup) { CALL1(0x8C01AA58u, setup); }

enum { SETUP_MENU_SIZE = 0x424, RACE_SCENE_SIZE = 0xD2C, COURSE_MAP_SIZE = 0x10C };

/* was func_8C01A950: when another state is asked for (RaceSetup.requested), the current scene is destroyed and the
 * new one built under the set-up: 0 the SetupMenu, 1 the RaceScene (after func_8C01AA58), 2 a third scene
 * (func_8C02FC12: vtable 0x8C23DA8C, construction func_8C02F850; no route asks for it);
 * 3 to 6 leave mode 7 for Game mode 4 (GOOD NIGHT), 9, 1 or 2. The constructors load files and end frames
 * (resumable) */
void race_setup_update(gaddr setup) {
  gaddr block = 0, scene = 0;
  HW_RESUMABLE(0x8C01A950u, setup, block, scene);
  HW_RESUME_AT(0, 1, 2, 3);
  RaceSetup *s = race_setup_at(setup);
  Game *owner = game_at(s->task.owner);
  if (s->state == s->requested) return;
  if (s->task.child != 0) {
    task_destroy_tree(task_at(s->task.child));
    s->task.child = 0;
  }
  s->state = s->requested;
  switch (s->requested) {
  case 0:
    block = syMalloc(SETUP_MENU_SIZE);
    if (block == 0) { scene = 0; break; }
    HW_YIELDING(0, scene = scene_construct(0x8C0232C6u, block, setup, 0x8C23DA5Cu, 0x8C022D80u, 2));
    break;
  case 1:
    HW_YIELDING(1, race_setup_before_race(setup));
    block = syMalloc(RACE_SCENE_SIZE);
    if (block == 0) { scene = 0; break; }
    HW_YIELDING(2, scene = scene_construct(0x8C0297ACu, block, setup, 0x8C23DA74u, 0x8C0293F0u, 2));
    break;
  case 2:
    block = syMalloc(COURSE_MAP_SIZE);
    if (block == 0) { scene = 0; break; }
    HW_YIELDING(3, scene = CALL2(0x8C02FC12u, block, setup));
    break;
  case 3: owner->next_mode = MODE_4; owner->mode_arg = 0; return;
  case 4: owner->next_mode = MODE_9; owner->mode_arg = 0; return;
  case 5: owner->next_mode = MODE_1; owner->mode_arg = 0; return;
  case 6: owner->next_mode = MODE_TITLE; owner->mode_arg = 0; return;
  default: return;
  }
  task_add_children(task_at(setup), task_at(scene));
}
HW_EXPORT_RESUMABLE(8c01a950, race_setup_update(ARG0))
