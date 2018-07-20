// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _BATTERY_TASK_H
#define _BATTERY_TASK_H


#include <stdint.h>


extern const uint32_t battery_task_stack_size;
void battery_task_main(void *task_params);


#endif
