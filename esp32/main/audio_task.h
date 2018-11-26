// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _AUDIO_TASK_H
#define _AUDIO_TASK_H


#include "app_task.h"


extern const app_task_descriptor audio_task_descriptor;


typedef enum {
	audio_task_sound_success1,
	audio_task_sound_success2,
	audio_task_sound_success3,
	audio_task_sound_error,
	audio_task_sound_tweet,
	audio_task_sound_low_battery
} audio_task_sound;

// Returns true if the sound was enqueued for playback.
// If the queue is full or not yet created, returns false.
bool audio_task_enqueue_sound(audio_task_sound sound);

// Removes all enqueued sounds
void audio_task_empty_queue(void);


#endif
