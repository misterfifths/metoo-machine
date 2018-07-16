// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _TASK_COMMON_H
#define _TASK_COMMON_H


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


// Counting semaphore for one-way communication from the Twitter API stuff to the
// audio task. It's incremented every time we see a matching tweet, and the audio
// task waits on it to play a sound.
extern SemaphoreHandle_t tweet_semaphore;


// Initialize common low-level stuff.
// Called outside of the context of a task, before any are created.
void task_common_init(void);


#endif
