/* Pen Pen port, S17 (b01): the music track brought back after the CD audio was released (game/sound.h). */
#include "hw.h"
#include "../core/file.h"
#include "../game/sound.h"


/* was func_8C016080: release the CD audio (cd_release(15)); when that reports 0, play the current track again,
 * repeated as music_play asked. (cd_release always returns 1: the replay below is the original's dead code.) */
void music_resume(void) {
  if (cd_release(15) != 0)
    return;
  cd_play(g_music_playing, g_music_playing, g_music_repeat ? 15 : 0);
}
HW_EXPORT(8c016080, music_resume())
