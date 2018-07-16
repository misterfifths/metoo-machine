// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _AUDIO_TASK_H
#define _AUDIO_TASK_H


#include <stdint.h>


extern const uint32_t audio_task_stack_size;
void audio_task_main(void *task_params);


#endif
