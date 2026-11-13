/* Pen Pen port, S17 b04: the race scene's per-player sound requests on the sound front end's upper ports
 * (game/sound.h: 8 one-shot ports and 8 MIDI ports), by the game's mode (RaceSetup.mode):
 *  - the "channel" helpers give each player channels of its own on ports 4-7: one player (modes 0 and 4) channels
 *    0-2 on ports 4-6; two players (mode 1) channels 0-1 each, player i's channel c on port 4 + 2i + c, panned to
 *    its half of the screen; four players (modes 2 and 3) channel 0 each, on port 4 + i;
 *  - the MIDI senders and stoppers of the lower ports, which race_midi_play_by_mode (game/race_hud.c) picks among:
 *    the full screen (the player on view 0 on its port, the watched player on port + 2), the split screen (view 1
 *    on the port panned right, view 0 on port + 2 panned left), four views (player i on port + i, panned by i). */
#include "hw.h"
#include "sdk.h"
#include "sound.h"
#include "race_scene.h"

/* sdMidiStop (Manatee): stop a MIDI port */
static inline void sdMidiStop(gaddr port) { CALL1(0x8C09AA54u, port); }
/* sdShotStop (Manatee): stop a one-shot port */
static inline void sdShotStop(gaddr port) { CALL1(0x8C09AD80u, port); }

/* the game's mode (RaceSetup.mode) */
static int32_t scene_mode(gaddr scene) { return race_setup_at(race_scene_at(scene)->setup)->mode; }

/* was func_8C02E8F0: stop MIDI port `port` (no bound check) */
void race_midi_port_stop(int32_t port) { sdMidiStop(g_midi_port[port]); }
HW_EXPORT(8c02e8f0, race_midi_port_stop((int32_t)ARG0))

/* ---- a player's own channels (ports 4-7) ---- */

/* was func_8C02EDAA: play one-shot `sound` on player i's channel `channel` at full volume (two players: panned
 * left for player 0, right for player 1) */
void race_channel_sound_play(gaddr scene, int32_t i, int32_t channel, int32_t priority, int32_t sound) {
  switch (scene_mode(scene)) {
  case 0:
  case 4:
    if (i < 1 && channel < 3) sound_shot_request(channel + 4, priority, sound, SOUND_VOLUME_MAX, 0, 0);
    break;
  case 1:
    if (i < 2 && channel < 2)
      sound_shot_request(i * 2 + channel + 4, priority, sound, SOUND_VOLUME_MAX, i == 0 ? 127 : -127, 0);
    break;
  case 2:
  case 3:
    if (i < 4 && channel < 1) sound_shot_request(i + channel + 4, priority, sound, SOUND_VOLUME_MAX, 0, 0);
    break;
  }
}
HW_EXPORT(8c02edaa, race_channel_sound_play(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* was func_8C02EE6A: stop the one-shot on player i's channel `channel` (and drop its pending request) */
void race_channel_sound_stop(gaddr scene, int32_t i, int32_t channel) {
  int32_t port;
  switch (scene_mode(scene)) {
  case 0:
  case 4:
    if (i >= 1 || channel >= 3) return;
    port = channel + 4;
    break;
  case 1:
    if (i >= 2 || channel >= 2) return;
    port = i * 2 + channel + 4;
    break;
  case 2:
  case 3:
    if (i >= 4 || channel >= 1) return;
    port = i + channel + 4;
    break;
  default:
    return;
  }
  g_shot_req_sound[port] = -1;
  sdShotStop(g_shot_port[port]);
}
HW_EXPORT(8c02ee6a, race_channel_sound_stop(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C02EFEC: play MIDI `sequence` on player i's channel `channel` at full volume, centred (one or two
 * players only: four players have no MIDI channel) */
void race_channel_midi_play(gaddr scene, int32_t i, int32_t channel, int32_t priority, int32_t sequence) {
  switch (scene_mode(scene)) {
  case 0:
  case 4:
    if (i < 1 && channel < 3) sound_midi_request(channel + 4, priority, sequence, SOUND_VOLUME_MAX, 0);
    break;
  case 1:
    if (i < 2 && channel < 2) sound_midi_request(i * 2 + channel + 4, priority, sequence, SOUND_VOLUME_MAX, 0);
    break;
  }
}
HW_EXPORT(8c02efec, race_channel_midi_play(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* was func_8C02F076: stop the MIDI sequence on player i's channel `channel` (one or two players) */
void race_channel_midi_stop(gaddr scene, int32_t i, int32_t channel) {
  switch (scene_mode(scene)) {
  case 0:
  case 4:
    if (i < 1 && channel < 3) race_midi_port_stop(channel + 4);
    break;
  case 1:
    if (i < 2 && channel < 2) race_midi_port_stop(i * 2 + channel + 4);
    break;
  }
}
HW_EXPORT(8c02f076, race_channel_midi_stop(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* ---- the lower ports, by the screen layout ---- */

/* was func_8C02F186: the split screen's MIDI request: player i on view 1 on `port` panned right, on view 0 on
 * port + 2 panned left (full volume; player_view[i] indexed as given) */
void race_midi_send_split(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence) {
  RaceScene *s = race_scene_at(scene);
  if (s->player_view[i] == 1)
    sound_midi_request(port, priority, sequence, SOUND_VOLUME_MAX, 127);
  else if (s->player_view[i] == 0)
    sound_midi_request(port + 2, priority, sequence, SOUND_VOLUME_MAX, -127);
}
HW_EXPORT(8c02f186, race_midi_send_split(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* was func_8C02F1E8: the four views' MIDI request: players 0-3 on `port` + i (port 0 only), odd players panned
 * right, even ones left */
void race_midi_send_panned(gaddr scene, int32_t i, int32_t port, int32_t priority, int32_t sequence) {
  if (i < 4 && port == 0)
    sound_midi_request(port + i, priority, sequence, SOUND_VOLUME_MAX, (i & 1) ? 127 : -127);
}
HW_EXPORT(8c02f1e8, race_midi_send_panned(ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* was func_8C02F244: the full screen's MIDI stop: player i on view 0 stops `port`, the watched player port + 2 */
void race_midi_end(gaddr scene, int32_t i, int32_t port) {
  RaceScene *s = race_scene_at(scene);
  if (s->player_view[i] == 0)
    race_midi_port_stop(port);
  else if (s->watched == i)
    race_midi_port_stop(port + 2);
}
HW_EXPORT(8c02f244, race_midi_end(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C02F272: the split screen's: view 1 stops `port`, view 0 port + 2 */
void race_midi_end_split(gaddr scene, int32_t i, int32_t port) {
  RaceScene *s = race_scene_at(scene);
  if (s->player_view[i] == 1)
    race_midi_port_stop(port);
  else if (s->player_view[i] == 0)
    race_midi_port_stop(port + 2);
}
HW_EXPORT(8c02f272, race_midi_end_split(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C02F2A2: the four views': players 0-3 stop `port` + i (port 0 only) */
void race_midi_end_panned(gaddr scene, int32_t i, int32_t port) {
  if (i < 4 && port == 0) race_midi_port_stop(i + port);
}
HW_EXPORT(8c02f2a2, race_midi_end_panned(ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C02EF88: stop player i's MIDI sequence on `port` through the helper of the game's mode */
void race_midi_end_by_mode(gaddr scene, int32_t i, int32_t port) {
  switch (scene_mode(scene)) {
  case 0:
  case 4:
    race_midi_end(scene, i, port);
    break;
  case 1:
    race_midi_end_split(scene, i, port);
    break;
  case 2:
  case 3:
    race_midi_end_panned(scene, i, port);
    break;
  }
}
HW_EXPORT(8c02ef88, race_midi_end_by_mode(ARG0, (int32_t)ARG1, (int32_t)ARG2))
