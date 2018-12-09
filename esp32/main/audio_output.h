// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _AUDIO_OUTPUT_H
#define _AUDIO_OUTPUT_H


#include <stdbool.h>


void audio_init(void);

// If sync is true, this call will suspend the calling task (waiting on an internal queue)
// until the sound is finished playing.
// Note that even with sync = false, the caller will be suspended while the data is
// written to the I2S bus. This can take a while for longer files.
// For that reason alone, you probably want to go through the interface in audio_task.h,
// which lets you enqueue sounds in a fire-and-forget manner.
void play_sound(const unsigned char *samples, size_t samples_length, bool sync);

// Suspends the caller until there is nothing playing.
// A little hacky; plays a tiny sample of silence and waits for it to complete.
// Useful if you need to ensure asynchronous audio is done before continuing.
void wait_for_silence(void);


#endif
