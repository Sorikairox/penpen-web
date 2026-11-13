/* Pen Pen port, S17 (b01): the sound front end shut down (game/sound.h). */
#include "hw.h"
#include "../game/sound.h"

static void sdShotClosePort(gaddr port) { CALL1(0x8C09AAB8u, port); }
static void sdMidiClosePort(gaddr port) { CALL1(0x8C09A828u, port); }
static void sdSysFinish(void) { CALL0(0x8C09AF10u); }

/* was func_8C015CD2 (and its thunk func_8C016A54): close the 8 one-shot and 8 MIDI ports (each pair in turn), then
 * stop the driver */
void sound_finish(void) {
  for (int i = 0; i < SOUND_PORTS; i++) {
    sdShotClosePort(g_shot_port[i]);
    sdMidiClosePort(g_midi_port[i]);
  }
  sdSysFinish();
}
HW_EXPORT(8c015cd2, sound_finish())
HW_EXPORT(8c016a54, sound_finish())
