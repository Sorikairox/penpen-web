/* Pen Pen port, S8: Manatee MIDI sequences (seq.c; docs/port/AUDIO.md 2.4). Ports 0..7 = the sd MIDI ports. */
#ifndef SEQ_H
#define SEQ_H
#include <stdint.h>
void seq_bank(uint32_t tag, int bank, uint32_t addr, uint32_t size); /* an SMSB / SMPB unit downloaded */
void seq_play(int port, int bank, int no);
void seq_stop(int port);
void seq_port(int port, int which, int v); /* 0 volume, 1 pan: -128..127 */
int seq_playing(int port);
void seq_advance(int n);                   /* n output samples */
void seq_log(void);
#endif
