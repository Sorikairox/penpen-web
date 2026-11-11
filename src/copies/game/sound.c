/* Pen Pen port: identical copies of the functions src/game/sound.c exports (written by tools/copies.py from
 * port/symbols/copies.tsv: byte-identical C++ inline methods and thunks; do not edit, run the tool). Each
 * copy runs the same C as the copy it repeats (named in the comment above its lines). */
#include "../../game/sound.h"
#include "sdk.h"
#include "../../core/file.h"

/* copies of 8c01b158 */
HW_EXPORT(8c021d4a, sound_play((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))
HW_EXPORT(8c022b10, sound_play((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))
HW_EXPORT(8c0292b6, sound_play((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))
HW_EXPORT(8c02e8fe, sound_play((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))
/* copies of 8c01b17c */
HW_EXPORT(8c022b58, music_play((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))
HW_EXPORT(8c02f7d4, music_play((int32_t)ARG0, (int32_t)ARG1, (int32_t)ARG2))
/* copies of 8c01b182 */
HW_EXPORT(8c01d1d8, sound_play_port_sound(0))
HW_EXPORT(8c022b78, sound_play_port_sound(0))
/* copies of 8c01b1a2 */
HW_EXPORT(8c01d1f8, sound_play_port_sound(1))
HW_EXPORT(8c022b98, sound_play_port_sound(1))
/* copies of 8c01b1c2 */
HW_EXPORT(8c01d218, sound_play_port_sound(2))
HW_EXPORT(8c022bb8, sound_play_port_sound(2))
/* copies of 8c01b1e2 */
HW_EXPORT(8c022bd8, sound_play_port_sound(3))
/* copies of 8c022b5e */
HW_EXPORT(8c02f7da, music_stop())
