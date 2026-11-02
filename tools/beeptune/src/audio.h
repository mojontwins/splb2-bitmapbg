/* Beeptune - sound output (see audio.c) */
#ifndef AUDIO_H
#define AUDIO_H
#include <stdint.h>
#include <stddef.h>

/* starts playing 16-bit mono samples (copied: the caller may free them).
   Returns 0, or 1 with a message. Stops whatever was playing. */
int  audio_start(const int16_t *s, size_t n, int rate, char *msg, size_t msglen);
void audio_stop(void);
const char *audio_backend(void);

#endif
