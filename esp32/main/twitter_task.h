// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _TWITTER_TASK_H
#define _TWITTER_TASK_H


#include <stdint.h>


extern const uint32_t twitter_task_stack_size;
void twitter_task_main(void *task_params);


#endif
