/* Pen Pen port, S11: the game's sound front end, between the game and the Manatee sound driver (sd) / CDDA.
 *
 * Game code never talks to the driver directly: during a frame it posts requests, and sound_update() (called by
 * game_update once per frame) sends them.
 *  - One-shot effects: 8 ports. A request names a port, a priority, the sound (a one-shot number of bank 0) and its
 *    volume, pan and pitch; the highest priority request of the frame wins (priority 0 always replaces). A request
 *    without a sound (sound_shot_adjust) only changes the playing sound's volume, pan or pitch. Volume, pan and pitch
 *    are sent only when they change.
 *  - MIDI sequences: 8 ports, the same scheme without pitch.
 *  - Music: one CDDA track. music_play asks for a track, with a fade of N frames (the CDDA volume ramps from 127 down
 *    to -127, then the track starts at -127 and comes up once the drive reports PLAY).
 * The "set" (sound_select_set) is the sound bank in use, reloaded when a scene needs another one. */
#ifndef PENPEN_SOUND_H
#define PENPEN_SOUND_H

#include "hw.h"

#define SOUND_PORTS 8
#define SOUND_VOLUME_MAX 127
#define CDDA_VOLUME_OFF (-127)

/* per port (guest .bss): one-shot ports */
#define g_shot_port         ((gaddr *)hw_ptr(0x8C1B3B64u))    /* sd port handles */
#define g_shot_req_priority ((int32_t *)hw_ptr(0x8C1B3B84u))  /* this frame's request (-1 = none) */
#define g_shot_req_sound    ((int32_t *)hw_ptr(0x8C1B3BA4u))  /* -1 = no new sound (adjust only) */
#define g_shot_pitch        ((int32_t *)hw_ptr(0x8C1B3BC4u))  /* wanted ... */
#define g_shot_volume       ((int32_t *)hw_ptr(0x8C1B3BE4u))
#define g_shot_pan          ((int32_t *)hw_ptr(0x8C1B3C04u))
#define g_shot_pitch_sent   ((int32_t *)hw_ptr(0x8C1B3C24u))  /* ... and last sent to the driver */
#define g_shot_volume_sent  ((int32_t *)hw_ptr(0x8C1B3C44u))
#define g_shot_pan_sent     ((int32_t *)hw_ptr(0x8C1B3C64u))
/* MIDI ports */
#define g_midi_port         ((gaddr *)hw_ptr(0x8C1B3B44u))
#define g_midi_req_priority ((int32_t *)hw_ptr(0x8C1B3C84u))
#define g_midi_req_sequence ((int32_t *)hw_ptr(0x8C1B3CA4u))
#define g_midi_volume       ((int32_t *)hw_ptr(0x8C1B3CC4u))
#define g_midi_pan          ((int32_t *)hw_ptr(0x8C1B3CE4u))
#define g_midi_volume_sent  ((int32_t *)hw_ptr(0x8C1B3D04u))
#define g_midi_pan_sent     ((int32_t *)hw_ptr(0x8C1B3D24u))
/* music */
#define g_music_requested   GUEST_VAR(int32_t, 0x8C1B3D44u)   /* CDDA track asked for (-1 = silence) */
#define g_music_playing     GUEST_VAR(int32_t, 0x8C1B3D48u)
#define g_music_repeat      GUEST_VAR(int32_t, 0x8C1B3D4Cu)
#define g_cdda_volume       GUEST_VAR(int32_t, 0x8C1B3D50u)   /* -127..127 as last sent (sd_Cmd53) */
#define g_music_fade_length GUEST_VAR(int32_t, 0x8C1B3D54u)
#define g_music_fade_left   GUEST_VAR(int32_t, 0x8C1B3D58u)
/* sound sets */
#define g_sound_set         GUEST_VAR(int32_t, 0x8C08C4C4u)   /* the set loaded (-1 = none yet) */
#define SOUND_SET_FILES     0x8C08C4B0u                       /* the bank file of each set (pointers to names) */
#define SOUND_DRIVER_FILE   0x8C066F70u                       /* the driver's file name (in the game's data) */

void sound_init(void);
void sound_update(void);
void sound_shot_request(int32_t port, int32_t priority, int32_t sound, int32_t volume, int32_t pan, int32_t pitch);
void sound_shot_adjust(int32_t port, int32_t priority, int32_t volume, int32_t pan, int32_t pitch);
void sound_midi_request(int32_t port, int32_t priority, int32_t sequence, int32_t volume, int32_t pan);
void sound_reset_requests(void);
void sound_select_set(int32_t set);
void music_play(int32_t track, int32_t fade_frames, int32_t repeat);
void music_stop(void);

#endif
