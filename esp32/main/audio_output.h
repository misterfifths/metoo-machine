// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _AUDIO_OUTPUT_H
#define _AUDIO_OUTPUT_H


#include <stdbool.h>


void audio_init(void);

// If sync is true, this call doesn't return (it sleeps via vTaskDelay) until the
// sound is finished playing.
void play_sound(const unsigned char *samples, size_t samples_length, bool sync);


#endif
