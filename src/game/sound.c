/* Pen Pen port, S11: the sound front end (game/sound.h). */
#include "sound.h"
#include "sdk.h"
#include "../core/file.h"

/* ---- the Manatee driver (sd), CDDA and file calls this module makes ---- */
static void sdLibInit(void) { CALL3(0x8C09AEE4u, 0, 0, 0); }
static void sdDrvInit(gaddr block) { CALL1(0x8C09ADE4u, block); }
static void sdMemBlkCreate(gaddr handle_out) { CALL1(0x8C09A1B4u, handle_out); }
static void sdMemBlkSetPrm(gaddr block, gaddr data, uint32_t size, int32_t attr, gaddr callback) {
  CALL5(0x8C09A29Cu, block, data, size, attr, callback);
}
static void sdMemBlkDestroy(gaddr block) { CALL1(0x8C09A234u, block); }
static void sdMultiUnitDownload(gaddr block) { CALL1(0x8C09A59Cu, block); }
static void sdShotOpenPort(gaddr handle_out) { CALL1(0x8C09AB08u, handle_out); }
static void sdMidiOpenPort(gaddr handle_out) { CALL1(0x8C09A878u, handle_out); }
static void sdShotPlay(gaddr port, int32_t bank, int32_t sound, int32_t priority) { CALL4(0x8C09AB58u, port, bank, sound, priority); }
static void sdShotSetVol(gaddr port, int32_t volume, int32_t time) { CALL3(0x8C09AD14u, port, volume, time); }
static void sdShotSetPan(gaddr port, int32_t pan, int32_t time) { CALL3(0x8C09ABECu, port, pan, time); }
static void sdShotSetPitch(gaddr port, int32_t cents, int32_t time) { CALL3(0x8C09AC6Cu, port, cents, time); }
static void sdMidiPlay(gaddr port, int32_t bank, int32_t sequence, int32_t priority) { CALL4(0x8C09A8C0u, port, bank, sequence, priority); }
static void sdMidiSetVol(gaddr port, int32_t volume, int32_t time) { CALL3(0x8C09A9D4u, port, volume, time); }
static void sdMidiSetPan(gaddr port, int32_t pan, int32_t time) { CALL3(0x8C09A954u, port, pan, time); }
static void sd_SetMasterVol(int32_t volume) { CALL1(0x8C09A440u, volume); }
static void sd_SetStereoMono(uint32_t mode) { CALL1(0x8C09A49Cu, mode); }
static void sd_CmdU8U8(int32_t a, int32_t b) { CALL2(0x8C09A3B4u, a, b); }
static void sd_Cmd53(int32_t left, int32_t right) { CALL2(0x8C09A778u, left, right); } /* CDDA volume */
static void sd_Cmd54(int32_t left, int32_t right) { CALL2(0x8C09A6B0u, left, right); } /* CDDA pan */
static void syCfgGetSoundMode(gaddr mode_out) { CALL1(0x8C0A0ED6u, mode_out); }
#define SD_STEREO 0x4F525453u /* 'STRO' */
#define SD_MONO 0x4F4E4F4Du   /* 'MONO' */
#define GD_DRVSTAT_PLAY 3


/* was func_8C015BFC: start the driver (its program from the disc), open the 8 + 8 ports, silence the music, clear
 * every request */
void sound_init(void) {
  sdLibInit();
  gaddr driver = file_load_fixed(SOUND_DRIVER_FILE, 0);
  gaddr block = hw_stack_alloc(4); /* the driver writes the block handle there */
  GUEST_VAR(gaddr, block) = 0;
  sdMemBlkCreate(block);
  sdMemBlkSetPrm(GUEST_VAR(gaddr, block), driver, 0, -1, 0);
  sdDrvInit(GUEST_VAR(gaddr, block));
  sdMemBlkDestroy(GUEST_VAR(gaddr, block));
  hw_stack_free(4);

  for (int i = 0; i < SOUND_PORTS; i++) {
    sdShotOpenPort(hw_addr(&g_shot_port[i]));
    sdMidiOpenPort(hw_addr(&g_midi_port[i]));
  }
  g_music_requested = -1;
  g_music_playing = -1;
  g_cdda_volume = CDDA_VOLUME_OFF;
  g_music_fade_length = 0;
  g_music_fade_left = 0;
  for (int i = 0; i < SOUND_PORTS; i++) {
    g_shot_req_priority[i] = -1;
    g_midi_req_priority[i] = -1;
    g_shot_pitch[i] = g_shot_volume[i] = g_shot_pan[i] = 0;
    g_shot_pitch_sent[i] = g_shot_volume_sent[i] = g_shot_pan_sent[i] = 0;
    g_midi_volume[i] = g_midi_pan[i] = 0;
    g_midi_volume_sent[i] = g_midi_pan_sent[i] = 0;
  }
}
HW_EXPORT(8c015bfc, sound_init())

/* the music: the CDDA volume follows the drive (up while it plays), fades out before a change of track */
static void music_update(void) {
  if (g_music_requested == g_music_playing) {
    int32_t volume = gdFsGetDrvStat() == GD_DRVSTAT_PLAY ? SOUND_VOLUME_MAX : CDDA_VOLUME_OFF;
    if (g_cdda_volume != volume) {
      sd_Cmd53(volume, volume);
      g_cdda_volume = volume;
    }
  } else if (g_music_fade_left) {
    g_music_fade_left--;
    g_cdda_volume = 2 * SOUND_VOLUME_MAX * g_music_fade_left / g_music_fade_length - SOUND_VOLUME_MAX;
    sd_Cmd53(g_cdda_volume, g_cdda_volume);
  } else {
    /* faded out: change the track (it comes up once the drive plays it) */
    g_music_fade_length = 0;
    g_music_playing = g_music_requested;
    g_cdda_volume = CDDA_VOLUME_OFF;
    sd_Cmd53(CDDA_VOLUME_OFF, CDDA_VOLUME_OFF);
    if (g_music_requested < 0)
      cd_pause();
    else
      cd_play(g_music_playing, g_music_playing, g_music_repeat ? 15 : 0);
  }
}

/* was func_8C015D10: once per frame: the music, then this frame's requests to the 8 one-shot and 8 MIDI ports,
 * then the driver's service. (game_update passes two arguments this build does not use.) */
void sound_update(void) {
  music_update();
  for (int i = 0; i < SOUND_PORTS; i++) {
    if (g_shot_req_priority[i] >= 0) {
      if (g_shot_req_sound[i] >= 0)
        sdShotPlay(g_shot_port[i], 0, g_shot_req_sound[i], g_shot_req_priority[i]);
      if (g_shot_pitch_sent[i] != g_shot_pitch[i]) {
        sdShotSetPitch(g_shot_port[i], g_shot_pitch[i], 0);
        g_shot_pitch_sent[i] = g_shot_pitch[i];
      }
      if (g_shot_volume_sent[i] != g_shot_volume[i]) {
        sdShotSetVol(g_shot_port[i], g_shot_volume[i], 0);
        g_shot_volume_sent[i] = g_shot_volume[i];
      }
      if (g_shot_pan_sent[i] != g_shot_pan[i]) {
        sdShotSetPan(g_shot_port[i], g_shot_pan[i], 0);
        g_shot_pan_sent[i] = g_shot_pan[i];
      }
    }
    g_shot_req_priority[i] = -1;
    g_shot_req_sound[i] = -1;
  }
  for (int i = 0; i < SOUND_PORTS; i++) {
    if (g_midi_req_priority[i] >= 0) {
      if (g_midi_req_sequence[i] >= 0)
        sdMidiPlay(g_midi_port[i], 0, g_midi_req_sequence[i], g_midi_req_priority[i]);
      if (g_midi_volume_sent[i] != g_midi_volume[i]) {
        sdMidiSetVol(g_midi_port[i], g_midi_volume[i], 0);
        g_midi_volume_sent[i] = g_midi_volume[i];
      }
      if (g_midi_pan_sent[i] != g_midi_pan[i]) {
        sdMidiSetPan(g_midi_port[i], g_midi_pan[i], 0);
        g_midi_pan_sent[i] = g_midi_pan[i];
      }
    }
    g_midi_req_priority[i] = -1;
    g_midi_req_sequence[i] = -1;
  }
  sdSysServer();
}
HW_EXPORT(8c015d10, sound_update())

/* was func_8C015FFC: load a sound bank file and hand it to the driver */
static void sound_download_bank(gaddr file_name, int32_t arg) {
  gaddr data = file_load_fixed(file_name, (gaddr)arg);
  gaddr block = hw_stack_alloc(4);
  GUEST_VAR(gaddr, block) = 0;
  sdMemBlkCreate(block);
  sdMemBlkSetPrm(GUEST_VAR(gaddr, block), data, 0, -1, 0);
  sdMultiUnitDownload(GUEST_VAR(gaddr, block));
  sdMemBlkDestroy(GUEST_VAR(gaddr, block));
  hw_stack_free(4);
}
HW_EXPORT(8c015ffc, sound_download_bank(ARG0, (int32_t)ARG1))

/* was func_8C016036: ask for a CDDA track; the current one fades out over `fade_frames` first */
void music_play(int32_t track, int32_t fade_frames, int32_t repeat) {
  g_music_fade_left = fade_frames;
  g_music_fade_length = fade_frames;
  g_cdda_volume = SOUND_VOLUME_MAX;
  g_music_requested = track;
  g_music_repeat = repeat;
}
HW_EXPORT(8c016036, music_play((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))

/* was func_8C01604E: silence at once */
void music_stop(void) {
  g_music_playing = -1;
  g_music_requested = -1;
  g_cdda_volume = CDDA_VOLUME_OFF;
  sd_Cmd53(g_cdda_volume, CDDA_VOLUME_OFF);
  cd_pause();
}
HW_EXPORT(8c01604e, music_stop())

/* was func_8C01614C: a one-shot request (the original's third argument is not used) */
void sound_shot_request(int32_t port, int32_t priority, int32_t sound, int32_t volume, int32_t pan, int32_t pitch) {
  if (priority == 0 || priority > g_shot_req_priority[port]) {
    g_shot_req_priority[port] = priority;
    g_shot_req_sound[port] = sound;
    g_shot_pitch[port] = pitch;
    g_shot_volume[port] = volume;
    g_shot_pan[port] = pan;
  }
}
HW_EXPORT(8c01614c, sound_shot_request((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG3, (int32_t)ARG4, (int32_t)ARG5,
                                       (int32_t)ARG6))

/* was func_8C01617E: change the volume, pan and pitch of the port's sound without starting a new one */
void sound_shot_adjust(int32_t port, int32_t priority, int32_t volume, int32_t pan, int32_t pitch) {
  if (priority == 0 || priority > g_shot_req_priority[port]) {
    g_shot_req_priority[port] = priority;
    g_shot_pitch[port] = pitch;
    g_shot_volume[port] = volume;
    g_shot_pan[port] = pan;
  }
}
HW_EXPORT(8c01617e, sound_shot_adjust((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2, (int32_t)ARG3, (int32_t)ARG4))

/* was func_8C0161A8: a MIDI sequence request (the third argument is not used) */
void sound_midi_request(int32_t port, int32_t priority, int32_t sequence, int32_t volume, int32_t pan) {
  if (priority == 0 || priority > g_midi_req_priority[port]) {
    g_midi_req_priority[port] = priority;
    g_midi_req_sequence[port] = sequence;
    g_midi_volume[port] = volume;
    g_midi_pan[port] = pan;
  }
}
HW_EXPORT(8c0161a8, sound_midi_request((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG3, (int32_t)ARG4, (int32_t)ARG5))

/* was func_8C0161D4: no request, full volume, centre, no pitch change */
void sound_reset_requests(void) {
  for (int i = 0; i < SOUND_PORTS; i++) {
    g_shot_req_priority[i] = -1;
    g_shot_req_sound[i] = -1;
    g_shot_volume[i] = SOUND_VOLUME_MAX;
    g_shot_pan[i] = 0;
    g_shot_pitch[i] = 0;
    g_midi_req_priority[i] = -1;
    g_midi_req_sequence[i] = -1;
    g_midi_volume[i] = SOUND_VOLUME_MAX;
    g_midi_pan[i] = 0;
  }
}
HW_EXPORT(8c0161d4, sound_reset_requests())

/* was func_8C016A5A: switch to another sound set: silence, load its bank, then stereo or mono as the console's
 * settings say */
void sound_select_set(int32_t set) {
  if (g_sound_set == set)
    return;
  sound_reset_requests();
  sd_SetMasterVol(0);
  sd_Cmd53(CDDA_VOLUME_OFF, CDDA_VOLUME_OFF);
  sound_download_bank(((gaddr *)hw_ptr(SOUND_SET_FILES))[set], 0);
  sd_CmdU8U8(0, 0);
  gaddr mode = hw_stack_alloc(4);
  syCfgGetSoundMode(mode);
  if (GUEST_VAR(int32_t, mode) == 0) {
    sd_SetStereoMono(SD_STEREO);
    sd_Cmd54(-127, 127); /* CDDA left channel left, right channel right */
  } else {
    sd_SetStereoMono(SD_MONO);
    sd_Cmd54(0, 0);
  }
  hw_stack_free(4);
  sd_SetMasterVol(15);
  g_sound_set = set;
}
HW_EXPORT(8c016a5a, sound_select_set((int32_t)ARG0))
