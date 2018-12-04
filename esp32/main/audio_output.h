// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _AUDIO_OUTPUT_H
#define _AUDIO_OUTPUT_H


#include <stdbool.h>


void audio_init(void);

// If sync is true, this call will suspend the calling task (waiting on an internal queue)
// until the sound is finished playing.
// You probably want to go through the interface in audio_task.h, which lets you enqueue
// sounds in a fire-and-forget manner.
void play_sound(const unsigned char *samples, size_t samples_length, bool sync);


#endif
