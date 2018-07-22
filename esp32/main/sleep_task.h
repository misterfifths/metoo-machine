// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _SLEEP_TASK_H
#define _SLEEP_TASK_H


#include <stdint.h>


extern const uint32_t sleep_task_stack_size;
void sleep_task_main(void *task_params);


#endif
